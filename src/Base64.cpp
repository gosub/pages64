#include "PageModule.hpp"

struct Base : Module {
    enum ParamIds { NUM_PARAMS };
    enum InputIds  {
        CLOCK_INPUT,
        RESET_INPUT,
        NUM_INPUTS
    };
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
    uint8_t sentLeds[64]      = {};
    uint8_t sentSceneLeds[8]  = {};
    uint8_t sentTopLeds[8]    = {};
    bool    ledsDirty         = true;  // send full refresh on first frame
    bool    repaintNeeded     = false; // set when exiting page-select; cleared after signalling page modules
    int     prevMidiDeviceId  = -1;    // detect MIDI output connect/disconnect

    // Clock/reset edge detection (ticks are broadcast to page modules)
    bool prevClockHigh = false;
    bool prevResetHigh = false;

    Base() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configInput(CLOCK_INPUT, "Clock");
        configInput(RESET_INPUT, "Reset");
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
        prevClockHigh  = false;
        prevResetHigh  = false;
        memset(sentLeds,      0, sizeof(sentLeds));
        memset(sentSceneLeds, 0, sizeof(sentSceneLeds));
        memset(sentTopLeds,   0, sizeof(sentTopLeds));
    }

    // ── helpers ──────────────────────────────────────────────────────────────

    // Cached right neighbor, refreshed by onExpanderChange()
    PageModule* rightPage = nullptr;

    void onExpanderChange(const ExpanderChangeEvent& e) override {
        if (e.side == 1)
            rightPage = dynamic_cast<PageModule*>(rightExpander.module);
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
        // Top round buttons are lit via CC (same CC number as they send: 104+col)
        midi::Message msg;
        msg.setStatus(0xb);   // CC
        msg.setChannel(0);
        msg.setNote((uint8_t)(104 + col));
        msg.setValue(velocity);
        midiOutput.sendMessage(msg);
    }

    void pushPageSelectLeds() {
        for (int i = 0; i < 64; i++) {
            uint8_t color = (i < pageCount) ? P64::LED_YELLOW : P64::LED_OFF;
            setGridLed(i, color);
        }
        if (currentPage < pageCount)
            setGridLed(currentPage, P64::LED_GREEN);
        for (int i = 0; i < 8; i++) {
            setTopLed(i, P64::LED_OFF);
            sentTopLeds[i] = P64::LED_OFF;
        }
    }

    void clearGrid() {
        for (int i = 0; i < 64; i++)
            setGridLed(i, P64::LED_OFF);
        memset(sentLeds, P64::LED_OFF, sizeof(sentLeds));
        for (int i = 0; i < 8; i++)
            sendLed(i * 16 + 8, P64::LED_OFF);
        memset(sentSceneLeds, P64::LED_OFF, sizeof(sentSceneLeds));
        for (int i = 0; i < 8; i++)
            setTopLed(i, P64::LED_OFF);
        memset(sentTopLeds, P64::LED_OFF, sizeof(sentTopLeds));
    }

    void pushActiveLeds(const uint8_t leds[64], const uint8_t sceneLeds[8],
                        const uint8_t topLeds[8]) {
        for (int i = 0; i < 64; i++) {
            if (leds[i] != sentLeds[i]) {
                setGridLed(i, leds[i]);
                sentLeds[i] = leds[i];
            }
        }
        for (int i = 0; i < 8; i++) {
            if (sceneLeds[i] != sentSceneLeds[i]) {
                sendLed(i * 16 + 8, sceneLeds[i]);
                sentSceneLeds[i] = sceneLeds[i];
            }
        }
        for (int i = 0; i < 8; i++) {
            if (topLeds[i] != sentTopLeds[i]) {
                setTopLed(i, topLeds[i]);
                sentTopLeds[i] = topLeds[i];
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
        // --- detect MIDI output connect: trigger full repaint ---
        int curDeviceId = midiOutput.deviceId;
        if (curDeviceId != prevMidiDeviceId && curDeviceId >= 0) {
            ledsDirty     = true;
            repaintNeeded = true;
            memset(sentLeds,      P64::LED_OFF, sizeof(sentLeds));
            memset(sentSceneLeds, P64::LED_OFF, sizeof(sentSceneLeds));
        }
        prevMidiDeviceId = curDeviceId;

        // --- clock/reset edge detection (computed once for all page modules) ---
        float clockVoltage = inputs[CLOCK_INPUT].getVoltage();
        float resetVoltage = inputs[RESET_INPUT].getVoltage();
        bool  clockHigh    = clockVoltage >= 1.0f;
        bool  resetHigh    = resetVoltage >= 1.0f;
        bool  clockTick    = clockHigh && !prevClockHigh;
        bool  resetTick    = resetHigh && !prevResetHigh;
        prevClockHigh = clockHigh;
        prevResetHigh = resetHigh;

        // --- build outgoing LeftMessage ---
        bool hasPageExpander = (rightPage != nullptr);

        P64::LeftMessage* leftMsg = nullptr;
        if (hasPageExpander) {
            leftMsg = reinterpret_cast<P64::LeftMessage*>(
                rightPage->leftExpander.producerMessage);
            if (leftMsg) {
                memset(leftMsg, 0, sizeof(P64::LeftMessage));
                leftMsg->activePage       = currentPage;
                leftMsg->pageCounter      = 0;
                leftMsg->repaintRequested = repaintNeeded;
                leftMsg->clockVoltage     = clockVoltage;
                leftMsg->resetVoltage     = resetVoltage;
                leftMsg->clockTick        = clockTick;
                leftMsg->resetTick        = resetTick;
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
            rightPage->leftExpander.messageFlipRequested = true;
        }
        rightExpander.messageFlipRequested = true;

        // --- read RightMessage (LED data) from active page ---
        if (!pageSelectMode) {
            auto* rightMsg = reinterpret_cast<P64::RightMessage*>(
                rightExpander.consumerMessage);
            if (rightMsg && rightMsg->dirty) {
                pushActiveLeds(rightMsg->gridLeds, rightMsg->sceneLeds, rightMsg->topLeds);
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

        // Clamp active page if the chain shrank
        if (pageCount > 0 && currentPage >= pageCount) {
            currentPage = pageCount - 1;
            ledsDirty   = true;
        }

        // --- CV / trigger outputs ---
        outputs[PAGE_CV_OUTPUT].setVoltage((float) currentPage);
        outputs[PAGE_TRIG_OUTPUT].setVoltage(pageTrigger.process(args.sampleTime) ? 10.f : 0.f);

        // --- page indicator lights: green = active, yellow = connected, off = none ---
        for (int i = 0; i < 8; i++) {
            bool connected = (i < pageCount);
            bool active    = connected && (i == currentPage);
            lights[PAGE_LIGHT + i * 2 + 0].setBrightness(active ? 1.f : (connected ? 0.25f : 0.f));
            lights[PAGE_LIGHT + i * 2 + 1].setBrightness((connected && !active) ? 0.25f : 0.f);
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

        // CLK, RST, PAGE, TRIG — all on one row
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(14.0, 97.0)),  module, Base::CLOCK_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(28.0, 97.0)),  module, Base::RESET_INPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(43.0, 97.0)), module, Base::PAGE_CV_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(57.0, 97.0)), module, Base::PAGE_TRIG_OUTPUT));
    }
};

Model* modelBase = createModel<Base, BaseWidget>("Base64");
