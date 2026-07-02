#include "PageModule.hpp"
#include "Scales.hpp"

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
    dsp::SchmittTrigger clockTrigger;
    dsp::SchmittTrigger resetTrigger;

    // Temp save / reload (button 6, CC 109): hold to save, tap to reload
    static constexpr float SNAP_HOLD_SEC  = 0.75f;
    static constexpr float SNAP_FLASH_SEC = 0.4f;
    bool    snapHeld       = false;
    float   snapHoldTime   = 0.f;
    bool    snapSaved      = false;   // save already fired during this hold
    float   snapFlash      = 0.f;     // top LED 6 feedback timer
    uint8_t pendingCommand = P64::CMD_NONE;
    int     snapPage       = -1;      // Base64's own snapshot: the active page

    // Global key (root + scale): Base64 owns the setting and its persistence;
    // the live value all followers read is P64::sharedKey.
    int keyRoot  = 0;
    int keyScale = 0;

    void setGlobalKey(int root, int scale) {
        keyRoot  = root;
        keyScale = scale;
        P64::sharedKey.set(root, scale);
    }

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
        // ...and ClickMessage from a 64Pads mirror on its left
        leftExpander.producerMessage = new P64::ClickMessage;
        leftExpander.consumerMessage = new P64::ClickMessage;
        memset(leftExpander.producerMessage, 0, sizeof(P64::ClickMessage));
        memset(leftExpander.consumerMessage, 0, sizeof(P64::ClickMessage));
    }

    ~Base() {
        delete (P64::RightMessage*) rightExpander.producerMessage;
        delete (P64::RightMessage*) rightExpander.consumerMessage;
        delete (P64::ClickMessage*) leftExpander.producerMessage;
        delete (P64::ClickMessage*) leftExpander.consumerMessage;
    }

    void onReset() override {
        midiInput.reset();
        midiOutput.reset();
        currentPage    = 0;
        pageSelectMode = false;
        ledsDirty      = true;
        repaintNeeded  = false;
        clockTrigger.reset();
        resetTrigger.reset();
        snapHeld       = false;
        snapHoldTime   = 0.f;
        snapSaved      = false;
        snapFlash      = 0.f;
        pendingCommand = P64::CMD_NONE;
        snapPage       = -1;
        memset(sentLeds,      0, sizeof(sentLeds));
        memset(sentSceneLeds, 0, sizeof(sentSceneLeds));
        memset(sentTopLeds,   0, sizeof(sentTopLeds));
    }

    // ── helpers ──────────────────────────────────────────────────────────────

    // Cached neighbors, refreshed by onExpanderChange()
    PageModule* rightPage = nullptr;
    Module*     leftPads  = nullptr;   // 64Pads mirror, if attached on the left

    // On-screen mirror of everything sent to the device (fed to 64Pads)
    uint8_t mirrorGrid[64] = {};
    uint8_t mirrorScene[8] = {};
    uint8_t mirrorTop[8]   = {};

    void onExpanderChange(const ExpanderChangeEvent& e) override {
        if (e.side == 1) {
            rightPage = dynamic_cast<PageModule*>(rightExpander.module);
        } else {
            Module* m = leftExpander.module;
            leftPads = (m && m->model == modelPads64) ? m : nullptr;
            if (leftPads) {
                // Ask for a full repaint so the fresh mirror sees every LED
                ledsDirty     = true;
                repaintNeeded = true;
                memset(sentLeds,      0xFF, sizeof(sentLeds));
                memset(sentSceneLeds, 0xFF, sizeof(sentSceneLeds));
                memset(sentTopLeds,   0xFF, sizeof(sentTopLeds));
            }
        }
    }

    void sendLed(int note, uint8_t velocity) {
        int row = note / 16;
        int col = note % 16;
        if (col == 8 && row <= 7)
            mirrorScene[row] = velocity;
        else if (col <= 7 && row <= 7)
            mirrorGrid[row * 8 + col] = velocity;

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
        mirrorTop[col] = velocity;
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
                if (out)
                    out->pushEvent(P64::GridEvent::SCENE, (uint8_t) row, value);
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
                        out->pushEvent(P64::GridEvent::PAD, (uint8_t) idx, value);
                    }
                }
            }
        } else if (status == 0x8) {   // explicit note-off
            int row = note / 16;
            int col = note % 16;
            if (col == 8 && row <= 7) {
                if (out)
                    out->pushEvent(P64::GridEvent::SCENE, (uint8_t) row, 0);
            } else {
                int idx = P64::gridNoteToIndex(note);
                if (out && idx >= 0 && !pageSelectMode)
                    out->pushEvent(P64::GridEvent::PAD, (uint8_t) idx, 0);
            }
        } else if (status == 0xb) {   // CC
            if (note == 109) {
                // Button 6, reserved: hold = temp save, tap = temp reload.
                // The save itself fires from process() when the hold matures.
                if (value > 0) {
                    snapHeld     = true;
                    snapHoldTime = 0.f;
                    snapSaved    = false;
                } else {
                    snapHeld = false;
                    if (!snapSaved)
                        pendingCommand = P64::CMD_RESTORE;
                }
            } else if (note == P64::CC_PAGE_SELECT) {
                bool wasHeld = pageSelectMode;
                pageSelectMode = (value > 0);
                if (pageSelectMode && !wasHeld) {
                    pushPageSelectLeds();
                } else if (!pageSelectMode && wasHeld) {
                    ledsDirty = true;
                    repaintNeeded = true;  // ask active page to resend its LED state
                    // The overlay wrote LEDs without updating the diff caches, so
                    // they no longer reflect the device; poison them (0xFF is not a
                    // valid velocity) so the next push re-sends every pad.
                    memset(sentLeds,      0xFF, sizeof(sentLeds));
                    memset(sentSceneLeds, 0xFF, sizeof(sentSceneLeds));
                    memset(sentTopLeds,   0xFF, sizeof(sentTopLeds));
                }
            } else if (out) {
                out->pushEvent(P64::GridEvent::CC, note, value);
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
        // Schmitt hysteresis (0.1 V / 1 V) so slow or noisy edges tick once
        float clockVoltage = inputs[CLOCK_INPUT].getVoltage();
        float resetVoltage = inputs[RESET_INPUT].getVoltage();
        bool  clockTick    = clockTrigger.process(clockVoltage, 0.1f, 1.f);
        bool  resetTick    = resetTrigger.process(resetVoltage, 0.1f, 1.f);

        // --- temp save / reload gesture timing (button 6) ---
        if (snapHeld && !snapSaved) {
            snapHoldTime += args.sampleTime;
            if (snapHoldTime >= SNAP_HOLD_SEC) {
                pendingCommand = P64::CMD_SAVE;
                snapSaved      = true;
                snapFlash      = SNAP_FLASH_SEC;
                setTopLed(5, P64::LED_GREEN);   // button 6 acknowledges the save
                sentTopLeds[5] = P64::LED_GREEN;
            }
        }
        if (snapFlash > 0.f) {
            snapFlash -= args.sampleTime;
            if (snapFlash <= 0.f) {
                sentTopLeds[5] = 0xFF;    // let the page's own value repaint it
                repaintNeeded  = true;
                ledsDirty      = true;
            }
        }

        // Base64's own share of the snapshot: the active page index
        if (pendingCommand == P64::CMD_SAVE) {
            snapPage = currentPage;
        } else if (pendingCommand == P64::CMD_RESTORE && snapPage >= 0) {
            if (currentPage != snapPage) {
                currentPage = snapPage;
                pageTrigger.trigger(1e-3f);
            }
            ledsDirty = true;
        }

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
                leftMsg->command          = pendingCommand;
                leftMsg->clockVoltage     = clockVoltage;
                leftMsg->resetVoltage     = resetVoltage;
                leftMsg->clockTick        = clockTick;
                leftMsg->resetTick        = resetTick;
                repaintNeeded = false;
            }
        }
        pendingCommand = P64::CMD_NONE;

        // --- drain MIDI input ---
        midi::Message msg;
        while (midiInput.tryPop(&msg, args.frame)) {
            if (leftMsg)
                processMidiMessage(msg, leftMsg);
            else
                processMidiMessage(msg, nullptr);  // handle page-select even without expander
        }

        // --- drain 64Pads clicks through the same path as hardware MIDI ---
        if (leftPads) {
            auto* clicks = reinterpret_cast<P64::ClickMessage*>(leftExpander.consumerMessage);
            if (clicks) {
                for (int i = 0; i < clicks->count && i < P64::ClickMessage::MAX; i++) {
                    const P64::GridEvent& ev = clicks->events[i];
                    midi::Message m2;
                    m2.setChannel(0);
                    if (ev.type == P64::GridEvent::CC) {
                        m2.setStatus(0xb);
                        m2.setNote(ev.index);
                    } else {
                        m2.setStatus(0x9);
                        m2.setNote((uint8_t)(ev.type == P64::GridEvent::SCENE
                            ? ev.index * 16 + 8
                            : P64::gridIndexToNote(ev.index)));
                    }
                    m2.setValue(ev.value);
                    processMidiMessage(m2, leftMsg);
                }
                clicks->count = 0;   // never reprocess if no flip arrives
            }
            leftExpander.messageFlipRequested = true;

            // --- push the LED mirror to 64Pads ---
            auto* mirror = reinterpret_cast<P64::MirrorMessage*>(
                leftPads->rightExpander.producerMessage);
            if (mirror) {
                memcpy(mirror->grid,  mirrorGrid,  sizeof(mirrorGrid));
                memcpy(mirror->scene, mirrorScene, sizeof(mirrorScene));
                memcpy(mirror->top,   mirrorTop,   sizeof(mirrorTop));
            }
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
        json_object_set_new(root, "keyRoot",  json_integer(keyRoot));
        json_object_set_new(root, "keyScale", json_integer(keyScale));
        return root;
    }

    void dataFromJson(json_t* root) override {
        json_t* j;
        if ((j = json_object_get(root, "midiInput")))  midiInput.fromJson(j);
        if ((j = json_object_get(root, "midiOutput"))) midiOutput.fromJson(j);
        if ((j = json_object_get(root, "currentPage")))
            currentPage = clamp((int) json_integer_value(j), 0, 63);
        int kr = keyRoot, ks = keyScale;
        if ((j = json_object_get(root, "keyRoot")))
            kr = clamp((int) json_integer_value(j), 0, 11);
        if ((j = json_object_get(root, "keyScale")))
            ks = clamp((int) json_integer_value(j), 0, P64::NUM_SCALES - 1);
        setGlobalKey(kr, ks);
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

    void appendContextMenu(Menu* menu) override {
        Base* m = getModule<Base>();
        menu->addChild(new MenuSeparator);
        menu->addChild(createSubmenuItem("Global key",
            string::f("%s %s", P64::NOTE_NAMES[m->keyRoot], P64::SCALES[m->keyScale].name),
            [=](Menu* sub) {
                sub->addChild(createIndexSubmenuItem("Root note",
                    {P64::NOTE_NAMES, P64::NOTE_NAMES + 12},
                    [=]() { return m->keyRoot; },
                    [=](int v) { m->setGlobalKey(v, m->keyScale); }));
                std::vector<std::string> scaleNames;
                for (int i = 0; i < P64::NUM_SCALES; i++)
                    scaleNames.push_back(P64::SCALES[i].name);
                sub->addChild(createIndexSubmenuItem("Scale", scaleNames,
                    [=]() { return m->keyScale; },
                    [=](int v) { m->setGlobalKey(m->keyRoot, v); }));
            }));
    }
};

Model* modelBase = createModel<Base, BaseWidget>("Base64");
