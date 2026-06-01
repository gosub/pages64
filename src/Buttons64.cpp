#include "plugin.hpp"

struct Buttons64 : Module {
    enum ParamIds {
        ENUMS(MODE_PARAM, 4),   // one per output pair: 0 = toggle, 1 = momentary
        NUM_PARAMS
    };
    enum InputIds  { NUM_INPUTS };
    enum OutputIds {
        ENUMS(ROW_OUTPUT, 4),   // 4 polyphonic outputs, 16 channels each (2 rows × 8 cols)
        NUM_OUTPUTS
    };
    enum LightIds {
        ENUMS(ACTIVE_LIGHT, 2),   // GreenRedLight: green = active, yellow = connected
        NUM_LIGHTS
    };

    int  myPageIndex = 0;
    bool toggleState[64]    = {};
    bool momentaryState[64] = {};

    uint8_t ledState[64];
    bool    ledsDirty = true;
    bool    wasActive = false;
    bool    prevMomentary[4] = {};
    uint8_t activeColor[4]   = {P64::LED_GREEN, P64::LED_GREEN, P64::LED_GREEN, P64::LED_GREEN};

    Buttons64() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (int i = 0; i < 4; i++)
            configSwitch(MODE_PARAM + i, 0.f, 1.f, 1.f,   // default: momentary
                string::f("Rows %d-%d mode", i * 2 + 1, i * 2 + 2), {"Toggle", "Momentary"});
        memset(ledState, P64::LED_OFF, sizeof(ledState));

        for (int i = 0; i < 4; i++)
            configOutput(ROW_OUTPUT + i,
                string::f("Rows %d-%d gates (poly 16ch)", i * 2 + 1, i * 2 + 2));

        leftExpander.producerMessage = new P64::LeftMessage;
        leftExpander.consumerMessage = new P64::LeftMessage;
        memset(leftExpander.producerMessage, 0, sizeof(P64::LeftMessage));
        memset(leftExpander.consumerMessage, 0, sizeof(P64::LeftMessage));

        rightExpander.producerMessage = new P64::RightMessage;
        rightExpander.consumerMessage = new P64::RightMessage;
        memset(rightExpander.producerMessage, 0, sizeof(P64::RightMessage));
        memset(rightExpander.consumerMessage, 0, sizeof(P64::RightMessage));
    }

    ~Buttons64() {
        delete (P64::LeftMessage*)  leftExpander.producerMessage;
        delete (P64::LeftMessage*)  leftExpander.consumerMessage;
        delete (P64::RightMessage*) rightExpander.producerMessage;
        delete (P64::RightMessage*) rightExpander.consumerMessage;
    }

    void onReset() override {
        memset(toggleState,    0,            sizeof(toggleState));
        memset(momentaryState, 0,            sizeof(momentaryState));
        memset(ledState,       P64::LED_OFF, sizeof(ledState));
        ledsDirty = true;
        wasActive = false;
        for (int i = 0; i < 4; i++) {
            prevMomentary[i] = true;
            activeColor[i]   = P64::LED_GREEN;
        }
    }

    // ── helpers ──────────────────────────────────────────────────────────────

    bool isLeftNeighbour(Module* m) const {
        return m && (m->model == modelBase || m->model == modelButtons64);
    }

    bool isRightNeighbour(Module* m) const {
        return m && m->model == modelButtons64;
    }

    bool outputMomentary(int out) {
        return params[MODE_PARAM + out].getValue() > 0.5f;
    }

    void rebuildLeds() {
        for (int out = 0; out < 4; out++) {
            const bool* active = outputMomentary(out) ? momentaryState : toggleState;
            for (int row = 0; row < 2; row++) {
                for (int col = 0; col < 8; col++) {
                    int idx = (out * 2 + row) * 8 + col;
                    uint8_t color = active[idx] ? activeColor[out] : P64::LED_OFF;
                    if (color != ledState[idx]) {
                        ledState[idx] = color;
                        ledsDirty     = true;
                    }
                }
            }
        }
    }

    void setOutputs() {
        for (int out = 0; out < 4; out++) {
            const bool* active = outputMomentary(out) ? momentaryState : toggleState;
            outputs[ROW_OUTPUT + out].setChannels(16);
            for (int row = 0; row < 2; row++) {
                for (int col = 0; col < 8; col++) {
                    int idx = (out * 2 + row) * 8 + col;
                    outputs[ROW_OUTPUT + out].setVoltage(active[idx] ? 5.f : 0.f, row * 8 + col);
                }
            }
        }
    }

    // ── process ──────────────────────────────────────────────────────────────

    void process(const ProcessArgs& args) override {
        // Switching momentary→toggle clears toggle state so no phantom presses carry over
        for (int out = 0; out < 4; out++) {
            bool momentary = outputMomentary(out);
            if (prevMomentary[out] && !momentary) {
                for (int r = 0; r < 2; r++)
                    for (int col = 0; col < 8; col++)
                        toggleState[(out * 2 + r) * 8 + col] = false;
                ledsDirty = true;
            }
            prevMomentary[out] = momentary;
        }

        bool amActive = false;

        if (isLeftNeighbour(leftExpander.module)) {
            // 1. Read LeftMessage
            auto* fromLeft = reinterpret_cast<P64::LeftMessage*>(leftExpander.consumerMessage);
            myPageIndex = fromLeft ? fromLeft->pageCounter : 0;
            amActive    = fromLeft && (fromLeft->activePage == myPageIndex);

            // 2. Forward LeftMessage rightward
            if (isRightNeighbour(rightExpander.module)) {
                auto* toRight = reinterpret_cast<P64::LeftMessage*>(
                    rightExpander.module->leftExpander.producerMessage);
                if (toRight && fromLeft) {
                    *toRight = *fromLeft;
                    toRight->pageCounter = myPageIndex + 1;
                }
                rightExpander.module->leftExpander.messageFlipRequested = true;
            }

            // 3. Process MIDI events when active
            if (amActive && (!wasActive || (fromLeft && fromLeft->repaintRequested)))
                ledsDirty = true;

            if (amActive && fromLeft) {
                for (int row = 0; row < 8; row++) {
                    int out = row / 2;
                    bool momentary = outputMomentary(out);
                    for (int col = 0; col < 8; col++) {
                        int note = row * 16 + col;
                        if (!fromLeft->noteEvent[note]) continue;
                        bool on  = fromLeft->noteVelocity[note] > 0;
                        int  idx = row * 8 + col;
                        if (momentary) {
                            momentaryState[idx] = on;
                            ledsDirty = true;
                        } else if (on) {
                            toggleState[idx] = !toggleState[idx];
                            ledsDirty = true;
                        }
                    }
                }
            } else if (!amActive) {
                // Clear momentary states for any output in momentary mode
                for (int out = 0; out < 4; out++) {
                    if (!outputMomentary(out)) continue;
                    for (int row = 0; row < 2; row++) {
                        for (int col = 0; col < 8; col++) {
                            int idx = (out * 2 + row) * 8 + col;
                            if (momentaryState[idx]) {
                                momentaryState[idx] = false;
                                ledsDirty = true;
                            }
                        }
                    }
                }
            }

            // 4. Read RightMessage from next page in chain
            P64::RightMessage chainMsg = {};
            if (isRightNeighbour(rightExpander.module)) {
                auto* fromRight = reinterpret_cast<P64::RightMessage*>(rightExpander.consumerMessage);
                if (fromRight) chainMsg = *fromRight;
            }
            rightExpander.messageFlipRequested = true;

            // 5. Build and send RightMessage to left neighbour
            auto* toLeft = reinterpret_cast<P64::RightMessage*>(
                leftExpander.module->rightExpander.producerMessage);
            if (toLeft) {
                if (amActive) {
                    rebuildLeds();
                    memcpy(toLeft->gridLeds, ledState, 64);
                    toLeft->dirty = ledsDirty;
                    ledsDirty     = false;
                } else {
                    *toLeft = chainMsg;
                }
                toLeft->chainLength = 1 + chainMsg.chainLength;
            }

            leftExpander.messageFlipRequested = true;
        }

        wasActive = amActive;
        setOutputs();
        bool connected = isLeftNeighbour(leftExpander.module);
        lights[ACTIVE_LIGHT + 0].setBrightness(connected ? 1.f : 0.f);
        lights[ACTIVE_LIGHT + 1].setBrightness((connected && !amActive) ? 1.f : 0.f);
    }

    json_t* dataToJson() override {
        json_t* root  = json_object();
        json_t* state = json_array();
        for (int i = 0; i < 64; i++)
            json_array_append_new(state, json_boolean(toggleState[i]));
        json_object_set_new(root, "toggleState", state);
        json_t* colors = json_array();
        for (int i = 0; i < 4; i++)
            json_array_append_new(colors, json_integer(activeColor[i]));
        json_object_set_new(root, "activeColor", colors);
        return root;
    }

    void dataFromJson(json_t* root) override {
        json_t* state = json_object_get(root, "toggleState");
        if (state) {
            for (int i = 0; i < 64; i++) {
                json_t* v = json_array_get(state, i);
                if (v) toggleState[i] = json_boolean_value(v);
            }
        }
        json_t* colors = json_object_get(root, "activeColor");
        if (colors) {
            for (int i = 0; i < 4; i++) {
                json_t* v = json_array_get(colors, i);
                if (v) activeColor[i] = (uint8_t) json_integer_value(v);
            }
        }
        ledsDirty = true;
    }
};

// ── Widget ────────────────────────────────────────────────────────────────────

struct Buttons64Widget : ModuleWidget {
    Buttons64Widget(Buttons64* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Buttons64.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addChild(createLightCentered<MediumLight<GreenRedLight>>(
            mm2px(Vec(20.32f, 18.0f)), module, Buttons64::ACTIVE_LIGHT));

        // 4 output groups; switch left at x=12, jack right at x=28
        const float groupY[4] = { 32.0f, 56.0f, 80.0f, 104.0f };
        for (int i = 0; i < 4; i++) {
            addParam(createParamCentered<CKSS>(
                mm2px(Vec(12.0f, groupY[i])), module, Buttons64::MODE_PARAM + i));
            addOutput(createOutputCentered<PJ301MPort>(
                mm2px(Vec(28.0f, groupY[i])), module, Buttons64::ROW_OUTPUT + i));
        }
    }

    void appendContextMenu(Menu* menu) override {
        Buttons64* m = getModule<Buttons64>();
        menu->addChild(new MenuSeparator);
        for (int out = 0; out < 4; out++) {
            std::string label = string::f("Rows %d-%d color", out * 2 + 1, out * 2 + 2);
            menu->addChild(createSubmenuItem(label, "", [=](Menu* sub) {
                for (auto& c : P64::LED_COLOR_DEFS) {
                    if (c.velocity == P64::LED_OFF) continue;
                    uint8_t vel = c.velocity;
                    sub->addChild(createCheckMenuItem(c.name, "",
                        [=]() { return m->activeColor[out] == vel; },
                        [=]() { m->activeColor[out] = vel; m->ledsDirty = true; }
                    ));
                }
            }));
        }
    }
};

Model* modelButtons64 = createModel<Buttons64, Buttons64Widget>("Buttons64");
