#include "PageModule.hpp"

static constexpr int STEP_CLOCK_DIVS[] = {1,2,3,4,6,8,12,16,24,32,48,64};

struct Step64 : PageModule {
    enum ParamIds { NUM_PARAMS };
    enum InputIds  { NUM_INPUTS };
    enum OutputIds {
        ENUMS(TRIG_OUTPUT, 7),
        STEP_OUTPUT,
        POLY_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        ENUMS(ACTIVE_LIGHT, 2),
        NUM_LIGHTS
    };

    bool steps[7][8]  = {};   // steps[trigRow=0..6][stepCol=0..7]
    int  loopStart    = 0;    // first active column (0..7)
    int  activeLen    = 8;    // loop length 1..8
    int  currentStep  = 0;    // next step to fire (0..7)
    int  indicatorStep = 0;   // step that last fired (for LED + CV)

    // Control-row pad tracking
    bool ctrlHeld[8]     = {};
    bool ctrlTwoBtnFired = false;

    // Clock
    int  clockDiv      = 1;
    int  clockDivCount = 0;

    // Trigger pulse generators
    dsp::PulseGenerator trigPulse[7];

    // Step indicator flash (50 ms)
    dsp::PulseGenerator stepPulse;
    bool indicatorOn = false;

    // Colors
    uint8_t controlColor   = P64::LED_YELLOW;
    uint8_t activeColor    = P64::LED_GREEN;
    uint8_t indicatorColor = P64::LED_GREEN_DIM;

    Step64() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (int i = 0; i < 7; i++)
            configOutput(TRIG_OUTPUT + i, string::f("Trigger %d", i + 1));
        configOutput(STEP_OUTPUT, "Step CV (0–10V)");
        configOutput(POLY_OUTPUT, "Poly triggers (7-channel)");
    }

    void onReset() override {
        PageModule::onReset();
        memset(steps, 0, sizeof(steps));
        loopStart     = 0;
        activeLen     = 8;
        currentStep   = 0;
        indicatorStep = 0;
        memset(ctrlHeld, 0, sizeof(ctrlHeld));
        ctrlTwoBtnFired = false;
        clockDiv      = 1;
        clockDivCount = 0;
        indicatorOn   = false;
        controlColor  = P64::LED_YELLOW;
        activeColor   = P64::LED_GREEN;
        indicatorColor = P64::LED_GREEN_DIM;
    }

    // ── virtual hooks ─────────────────────────────────────────────────────────

    void pagePreProcess() override {
        auto* msg = reinterpret_cast<P64::LeftMessage*>(leftExpander.consumerMessage);
        if (!msg) return;

        if (msg->resetTick) {
            currentStep = loopStart;
            ledsDirty   = true;
        }

        // Clock tick (with pre-divider)
        bool tick = false;
        if (msg->clockTick) {
            if (++clockDivCount >= clockDiv) {
                clockDivCount = 0;
                tick = true;
            }
        }

        if (tick) {
            // Fire triggers for the current (about-to-play) step
            indicatorStep = currentStep;
            for (int g = 0; g < 7; g++)
                if (steps[g][currentStep])
                    trigPulse[g].trigger(0.005f);
            stepPulse.trigger(0.05f);
            indicatorOn = true;
            ledsDirty   = true;
            // Advance within the active loop
            int relPos  = currentStep - loopStart;
            currentStep = loopStart + (relPos + 1) % activeLen;
        }

        // Detect when step indicator pulse expires
        bool newIndicator = stepPulse.process(sampleTime);
        if (newIndicator != indicatorOn) {
            indicatorOn = newIndicator;
            ledsDirty   = true;
        }
    }

    void pageActive(const P64::LeftMessage& msg) override {
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                int  note    = row * 16 + col;
                if (!msg.noteEvent[note]) continue;
                bool pressed = msg.noteVelocity[note] > 0;

                if (row == 0) {
                    // Control row
                    if (pressed) {
                        int heldCol = -1;
                        for (int c2 = 0; c2 < 8; c2++) {
                            if (c2 != col && ctrlHeld[c2]) { heldCol = c2; break; }
                        }
                        ctrlHeld[col] = true;

                        if (heldCol >= 0) {
                            // Two-button: set loop start and length
                            loopStart = std::min(col, heldCol);
                            activeLen = std::abs(col - heldCol) + 1;
                            // Keep currentStep inside loop
                            if (currentStep < loopStart || currentStep >= loopStart + activeLen)
                                currentStep = loopStart;
                            ctrlTwoBtnFired = true;
                            ledsDirty = true;
                        }
                        // else defer to note-off
                    } else {
                        ctrlHeld[col] = false;
                        bool anyHeld = false;
                        for (int c2 = 0; c2 < 8; c2++)
                            if (ctrlHeld[c2]) { anyHeld = true; break; }

                        // Single tap within active section: set next step to fire
                        if (!ctrlTwoBtnFired && !anyHeld
                                && col >= loopStart && col < loopStart + activeLen) {
                            currentStep = col;
                            ledsDirty   = true;
                        }
                        if (!anyHeld)
                            ctrlTwoBtnFired = false;
                    }
                } else {
                    // Trigger rows 1-7: toggle on note-on
                    if (pressed) {
                        steps[row - 1][col] = !steps[row - 1][col];
                        ledsDirty = true;
                    }
                }
            }
        }
    }

    void pageInactive() override {
        memset(ctrlHeld, 0, sizeof(ctrlHeld));
        ctrlTwoBtnFired = false;
    }

    void rebuildLeds() override {
        for (int col = 0; col < 8; col++) {
            for (int row = 0; row < 8; row++) {
                uint8_t color = P64::LED_OFF;

                // Step indicator: dim flash at the last-fired step
                if (col == indicatorStep && indicatorOn)
                    color = indicatorColor;

                if (row == 0) {
                    // Control row: solid bar for active section
                    bool inLoop = (col >= loopStart && col < loopStart + activeLen);
                    color = inLoop ? controlColor : P64::LED_OFF;
                } else {
                    // Trigger rows: active steps shown in activeColor (overrides indicator)
                    if (steps[row - 1][col])
                        color = activeColor;
                }

                ledState[row * 8 + col] = color;
            }
        }
    }

    void updateOutputs() override {
        outputs[POLY_OUTPUT].setChannels(7);
        for (int g = 0; g < 7; g++) {
            float v = trigPulse[g].process(sampleTime) ? 10.f : 0.f;
            outputs[TRIG_OUTPUT + g].setVoltage(v);
            outputs[POLY_OUTPUT].setVoltage(v, g);
        }

        float stepCv = (float)indicatorStep / 7.f * 10.f;
        outputs[STEP_OUTPUT].setVoltage(stepCv);
    }

    // ── serialisation ─────────────────────────────────────────────────────────

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "loopStart",      json_integer(loopStart));
        json_object_set_new(root, "activeLen",      json_integer(activeLen));
        json_object_set_new(root, "currentStep",    json_integer(currentStep));
        json_object_set_new(root, "clockDiv",       json_integer(clockDiv));
        json_object_set_new(root, "controlColor",   json_integer(controlColor));
        json_object_set_new(root, "activeColor",    json_integer(activeColor));
        json_object_set_new(root, "indicatorColor", json_integer(indicatorColor));
        json_t* rows = json_array();
        for (int g = 0; g < 7; g++) {
            json_t* row = json_array();
            for (int s = 0; s < 8; s++)
                json_array_append_new(row, json_boolean(steps[g][s]));
            json_array_append_new(rows, row);
        }
        json_object_set_new(root, "steps", rows);
        return root;
    }

    void dataFromJson(json_t* root) override {
        json_t* j;
        if ((j = json_object_get(root, "loopStart")))
            loopStart = clamp((int)json_integer_value(j), 0, 7);
        if ((j = json_object_get(root, "activeLen")))
            activeLen = clamp((int)json_integer_value(j), 1, 8);
        if ((j = json_object_get(root, "currentStep")))
            currentStep = clamp((int)json_integer_value(j), 0, 7);
        if ((j = json_object_get(root, "clockDiv")))
            clockDiv = clamp((int)json_integer_value(j), 1, 64);
        if ((j = json_object_get(root, "controlColor")))
            controlColor = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "activeColor")))
            activeColor = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "indicatorColor")))
            indicatorColor = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "steps")))
            for (int g = 0; g < 7; g++) {
                json_t* row = json_array_get(j, g);
                if (!row) continue;
                for (int s = 0; s < 8; s++) {
                    json_t* v = json_array_get(row, s);
                    if (v) steps[g][s] = json_boolean_value(v);
                }
            }
        ledsDirty = true;
    }
};

// ── Widget ────────────────────────────────────────────────────────────────────

struct Step64Widget : ModuleWidget {
    Step64Widget(Step64* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Step64.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addChild(createLightCentered<SmallLight<GreenRedLight>>(
            mm2px(Vec(6.0f, 18.0f)), module, Step64::ACTIVE_LIGHT));

        const float trigY[7] = {25.f, 35.f, 45.f, 55.f, 65.f, 75.f, 85.f};
        for (int i = 0; i < 7; i++) {
            addOutput(createOutputCentered<PJ301MPort>(
                mm2px(Vec(20.0f, trigY[i])), module, Step64::TRIG_OUTPUT + i));
        }

        addOutput(createOutputCentered<PJ301MPort>(
            mm2px(Vec(20.0f, 96.0f)), module, Step64::STEP_OUTPUT));

        addOutput(createOutputCentered<PJ301MPort>(
            mm2px(Vec(20.0f, 106.0f)), module, Step64::POLY_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        Step64* m = getModule<Step64>();
        menu->addChild(new MenuSeparator);
        menu->addChild(createSubmenuItem("Clock divider", "", [=](Menu* sub) {
            for (int d : STEP_CLOCK_DIVS) {
                sub->addChild(createCheckMenuItem(string::f("÷%d", d), "",
                    [=]() { return m->clockDiv == d; },
                    [=]() { m->clockDiv = d; m->clockDivCount = 0; }
                ));
            }
        }));
        menu->addChild(createSubmenuItem("Control bar color", "", [=](Menu* sub) {
            for (auto& c : P64::LED_COLOR_DEFS) {
                if (c.velocity == P64::LED_OFF) continue;
                uint8_t vel = c.velocity;
                sub->addChild(createCheckMenuItem(c.name, "",
                    [=]() { return m->controlColor == vel; },
                    [=]() { m->controlColor = vel; m->ledsDirty = true; }
                ));
            }
        }));
        menu->addChild(createSubmenuItem("Active step color", "", [=](Menu* sub) {
            for (auto& c : P64::LED_COLOR_DEFS) {
                if (c.velocity == P64::LED_OFF) continue;
                uint8_t vel = c.velocity;
                sub->addChild(createCheckMenuItem(c.name, "",
                    [=]() { return m->activeColor == vel; },
                    [=]() { m->activeColor = vel; m->ledsDirty = true; }
                ));
            }
        }));
        menu->addChild(createSubmenuItem("Step indicator color", "", [=](Menu* sub) {
            for (auto& c : P64::LED_COLOR_DEFS) {
                if (c.velocity == P64::LED_OFF) continue;
                uint8_t vel = c.velocity;
                sub->addChild(createCheckMenuItem(c.name, "",
                    [=]() { return m->indicatorColor == vel; },
                    [=]() { m->indicatorColor = vel; m->ledsDirty = true; }
                ));
            }
        }));
    }
};

Model* modelStep64 = createModel<Step64, Step64Widget>("Step64");
