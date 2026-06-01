#include "plugin.hpp"

struct Base : Module {
    enum ParamIds { NUM_PARAMS };
    enum InputIds  { NUM_INPUTS };
    enum OutputIds {
        PAGE_CV_OUTPUT,
        PAGE_TRIG_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        ENUMS(PAGE_LIGHT, 8 * 2),   // 2 channels per light: GreenRedLight
        NUM_LIGHTS
    };

    midi::InputQueue midiInput;
    midi::Output     midiOutput;

    int  currentPage      = 0;
    int  pageCount        = 0;   // discovered each frame from expander chain
    bool pageSelectMode   = false;

    // One-frame pulse for page-change trigger output
    dsp::PulseGenerator pageTrigger;

    // Cached LED state we last sent to the Launchpad, to avoid unnecessary messages
    uint8_t sentLeds[64]  = {};
    bool    ledsDirty     = true;  // send full refresh on first frame
    bool    repaintNeeded = false; // set when exiting page-select; cleared after signalling Buttons64

    Base() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configOutput(PAGE_CV_OUTPUT,   "Active page (1 V/page)");
        configOutput(PAGE_TRIG_OUTPUT, "Page-change trigger");

        // Allocate expander buffers: Base receives RightMessage from Buttons64
        rightExpander.producerMessage = new P64::RightMessage;
        rightExpander.consumerMessage = new P64::RightMessage;
        memset(rightExpander.producerMessage, 0, sizeof(P64::RightMessage));
        memset(rightExpander.consumerMessage, 0, sizeof(P64::RightMessage));
    }

    ~Base() {
        delete (P64::RightMessage*) rightExpander.producerMessage;
        delete (P64::RightMessage*) rightExpander.consumerMessage;
    }

    void onReset() override {
        midiInput.reset();
        midiOutput.reset();
        currentPage    = 0;
        pageSelectMode = false;
        ledsDirty      = true;
        repaintNeeded  = false;
        memset(sentLeds, 0, sizeof(sentLeds));
    }

    // ── helpers ──────────────────────────────────────────────────────────────

    bool isPageModule(Module* m) const {
        return m && (m->model == modelButtons64);
    }

    void sendLed(int note, uint8_t velocity) {
        midi::Message msg;
        msg.setStatus(0x9);    // note-on
        msg.setChannel(0);
        msg.setNote((uint8_t) note);
        msg.setValue(velocity);
        midiOutput.sendMessage(msg);
    }

    void setGridLed(int gridIdx, uint8_t velocity) {
        sendLed(P64::gridIndexToNote(gridIdx), velocity);
    }

    void setTopLed(int col, uint8_t velocity) {
        // Top round buttons addressed via CC on channel 0; light them with Note-On on channel 0
        // On the MkII the top buttons respond to Note-On on the "Automap" channel (ch 0 / MIDI ch 1)
        // using notes 104+col (same numbering as their CC)
        sendLed(104 + col, velocity);
    }

    void pushPageSelectLeds() {
        for (int i = 0; i < 64; i++) {
            uint8_t color = (i < pageCount) ? P64::LED_YELLOW : P64::LED_OFF;
            setGridLed(i, color);
        }
        if (currentPage < pageCount)
            setGridLed(currentPage, P64::LED_GREEN);
    }

    void clearGrid() {
        for (int i = 0; i < 64; i++)
            setGridLed(i, P64::LED_OFF);
        memset(sentLeds, P64::LED_OFF, sizeof(sentLeds));
    }

    void pushActiveLeds(const uint8_t leds[64]) {
        for (int i = 0; i < 64; i++) {
            if (leds[i] != sentLeds[i]) {
                setGridLed(i, leds[i]);
                sentLeds[i] = leds[i];
            }
        }
    }

    // ── MIDI processing ───────────────────────────────────────────────────────

    void processMidiMessage(const midi::Message& msg,
                            P64::LeftMessage* out) {
        uint8_t status = msg.getStatus();
        uint8_t note   = msg.getNote();
        uint8_t value  = msg.getValue();

        if (status == 0x9) {   // note-on (includes velocity-0 note-off)
            int row = note / 16;
            int col = note % 16;
            if (col == 8 && row <= 7) {
                // Right column scene button (A–H)
                if (out) {
                    out->sceneEvent[row]    = true;
                    out->sceneVelocity[row] = value;
                }
            } else {
                int idx = P64::gridNoteToIndex(note);
                if (idx >= 0) {
                    if (pageSelectMode && value > 0) {
                        // Grid press while in page-select mode → switch page
                        if (idx < pageCount) {
                            int prev = currentPage;
                            currentPage = idx;
                            if (currentPage != prev) {
                                pageTrigger.trigger(1e-3f);
                                ledsDirty = true;
                            }
                            pushPageSelectLeds();  // refresh overlay: move yellow to new page
                        }
                    } else if (out) {
                        // Normal pass-through to active page
                        out->noteEvent[note]    = true;
                        out->noteVelocity[note] = value;
                    }
                }
            }
        } else if (status == 0x8) {   // explicit note-off
            int row = note / 16;
            int col = note % 16;
            if (col == 8 && row <= 7) {
                if (out) {
                    out->sceneEvent[row]    = true;
                    out->sceneVelocity[row] = 0;
                }
            } else {
                int idx = P64::gridNoteToIndex(note);
                if (out && idx >= 0 && !pageSelectMode) {
                    out->noteEvent[note]    = true;
                    out->noteVelocity[note] = 0;
                }
            }
        } else if (status == 0xb) {   // CC
            if (note == P64::CC_PAGE_SELECT) {
                bool wasHeld = pageSelectMode;
                pageSelectMode = (value > 0);
                if (pageSelectMode && !wasHeld) {
                    pushPageSelectLeds();
                } else if (!pageSelectMode && wasHeld) {
                    ledsDirty = true;
                    repaintNeeded = true;  // ask active page to resend its LED state
                }
            } else if (out) {
                out->ccEvent[note]  = true;
                out->ccValue[note]  = value;
            }
        }
    }

    // ── process ──────────────────────────────────────────────────────────────

    void process(const ProcessArgs& args) override {
        // --- build outgoing LeftMessage ---
        bool hasPageExpander = isPageModule(rightExpander.module);

        P64::LeftMessage* leftMsg = nullptr;
        if (hasPageExpander) {
            leftMsg = reinterpret_cast<P64::LeftMessage*>(
                rightExpander.module->leftExpander.producerMessage);
            if (leftMsg) {
                memset(leftMsg, 0, sizeof(P64::LeftMessage));
                leftMsg->activePage       = currentPage;
                leftMsg->pageCounter      = 0;
                leftMsg->repaintRequested = repaintNeeded;
                repaintNeeded = false;
            }
        }

        // --- drain MIDI input ---
        midi::Message msg;
        while (midiInput.tryPop(&msg, args.frame)) {
            if (leftMsg)
                processMidiMessage(msg, leftMsg);
            else
                processMidiMessage(msg, nullptr);  // handle page-select even without expander
        }

        // --- request expander flip ---
        if (hasPageExpander && leftMsg) {
            rightExpander.module->leftExpander.messageFlipRequested = true;
        }
        rightExpander.messageFlipRequested = true;

        // --- read RightMessage (LED data) from active page ---
        if (!pageSelectMode) {
            auto* rightMsg = reinterpret_cast<P64::RightMessage*>(
                rightExpander.consumerMessage);
            if (rightMsg && rightMsg->dirty) {
                pushActiveLeds(rightMsg->gridLeds);
                rightMsg->dirty = false;
            } else if (ledsDirty) {
                // Page just changed or fresh start: clear grid then let page repaint
                clearGrid();
                ledsDirty = false;
            }
        }

        // --- discover page count from chain length reported in RightMessage ---
        if (hasPageExpander) {
            auto* rm = reinterpret_cast<P64::RightMessage*>(rightExpander.consumerMessage);
            pageCount = rm ? rm->chainLength : 1;
        } else {
            pageCount = 0;
        }

        // --- CV / trigger outputs ---
        outputs[PAGE_CV_OUTPUT].setVoltage((float) currentPage);
        outputs[PAGE_TRIG_OUTPUT].setVoltage(pageTrigger.process(args.sampleTime) ? 10.f : 0.f);

        // --- page indicator lights: green = active, yellow = connected, off = none ---
        for (int i = 0; i < 8; i++) {
            bool connected = (i < pageCount);
            bool active    = connected && (i == currentPage);
            lights[PAGE_LIGHT + i * 2 + 0].setBrightness(connected ? 1.f : 0.f); // green
            lights[PAGE_LIGHT + i * 2 + 1].setBrightness((connected && !active) ? 1.f : 0.f); // red → yellow
        }
    }

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "midiInput",  midiInput.toJson());
        json_object_set_new(root, "midiOutput", midiOutput.toJson());
        json_object_set_new(root, "currentPage", json_integer(currentPage));
        return root;
    }

    void dataFromJson(json_t* root) override {
        json_t* j;
        if ((j = json_object_get(root, "midiInput")))  midiInput.fromJson(j);
        if ((j = json_object_get(root, "midiOutput"))) midiOutput.fromJson(j);
        if ((j = json_object_get(root, "currentPage")))
            currentPage = clamp((int) json_integer_value(j), 0, 63);
        ledsDirty = true;
    }
};

// ── Widget ────────────────────────────────────────────────────────────────────

struct BaseWidget : ModuleWidget {
    BaseWidget(Base* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Base.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Page indicator lights (8 LEDs across the panel)
        const float lightY   = mm2px(18.5f);
        const float lightStep = mm2px(8.45f);
        const float lightX0  = mm2px(6.0f);

        // MIDI input display
        app::MidiDisplay* inputDisplay = createWidget<app::MidiDisplay>(mm2px(Vec(3.41, 29.0)));
        inputDisplay->box.size = mm2px(Vec(64.3, 28.0));
        inputDisplay->setMidiPort(module ? &module->midiInput : nullptr);
        addChild(inputDisplay);

        // MIDI output display
        app::MidiDisplay* outputDisplay = createWidget<app::MidiDisplay>(mm2px(Vec(3.41, 61.0)));
        outputDisplay->box.size = mm2px(Vec(64.3, 28.0));
        outputDisplay->setMidiPort(module ? &module->midiOutput : nullptr);
        addChild(outputDisplay);
        for (int i = 0; i < 8; i++) {
            addChild(createLightCentered<SmallLight<GreenRedLight>>(
                Vec(lightX0 + i * lightStep, lightY),
                module, Base::PAGE_LIGHT + i * 2));
        }

        // CV and trigger outputs
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.0, 103.0)), module, Base::PAGE_CV_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(51.0, 103.0)), module, Base::PAGE_TRIG_OUTPUT));
    }
};

Model* modelBase = createModel<Base, BaseWidget>("Base64");
