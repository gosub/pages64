#include "PageModule.hpp"

struct Buttons64 : PageModule {
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
        ENUMS(ACTIVE_LIGHT, 2),   // lights[0,1] used by PageModule::process()
        NUM_LIGHTS
    };

    bool    toggleState[64]    = {};
    bool    momentaryState[64] = {};
    bool    prevMomentary[4]   = {};
    uint8_t activeColor[4]     = {P64::LED_GREEN, P64::LED_GREEN, P64::LED_GREEN, P64::LED_GREEN};
    uint8_t offColor[4]        = {P64::LED_OFF,   P64::LED_OFF,   P64::LED_OFF,   P64::LED_OFF};

    Buttons64() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (int i = 0; i < 4; i++)
            configSwitch(MODE_PARAM + i, 0.f, 1.f, 1.f,
                string::f("Rows %d-%d mode", i * 2 + 1, i * 2 + 2), {"Toggle", "Momentary"});
        for (int i = 0; i < 4; i++)
            configOutput(ROW_OUTPUT + i,
                string::f("Rows %d-%d gates (poly 16ch)", i * 2 + 1, i * 2 + 2));
    }

    void onReset() override {
        PageModule::onReset();
        memset(toggleState,    0, sizeof(toggleState));
        memset(momentaryState, 0, sizeof(momentaryState));
        for (int i = 0; i < 4; i++) {
            prevMomentary[i] = true;
            activeColor[i]   = P64::LED_GREEN;
            offColor[i]      = P64::LED_OFF;
        }
    }

    // ── virtual hooks ─────────────────────────────────────────────────────────

    void pagePreProcess() override {
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
    }

    void pageActive(const P64::LeftMessage& msg) override {
        for (int row = 0; row < 8; row++) {
            int  out      = row / 2;
            bool momentary = outputMomentary(out);
            for (int col = 0; col < 8; col++) {
                int note = row * 16 + col;
                if (!msg.noteEvent[note]) continue;
                bool on  = msg.noteVelocity[note] > 0;
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
    }

    void pageInactive() override {
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

    void rebuildLeds() override {
        for (int out = 0; out < 4; out++) {
            const bool* active    = outputMomentary(out) ? momentaryState : toggleState;
            bool        connected = outputs[ROW_OUTPUT + out].isConnected();
            for (int row = 0; row < 2; row++) {
                for (int col = 0; col < 8; col++) {
                    int idx = (out * 2 + row) * 8 + col;
                    uint8_t color = active[idx] ? activeColor[out]
                                  : connected   ? offColor[out]
                                  :               P64::LED_OFF;
                    if (color != ledState[idx]) {
                        ledState[idx] = color;
                        ledsDirty     = true;
                    }
                }
            }
        }
    }

    void updateOutputs() override {
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

    // ── helpers ───────────────────────────────────────────────────────────────

    bool outputMomentary(int out) {
        return params[MODE_PARAM + out].getValue() > 0.5f;
    }

    // ── serialisation ─────────────────────────────────────────────────────────

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
        json_t* offColors = json_array();
        for (int i = 0; i < 4; i++)
            json_array_append_new(offColors, json_integer(offColor[i]));
        json_object_set_new(root, "offColor", offColors);
        return root;
    }

    void dataFromJson(json_t* root) override {
        json_t* state = json_object_get(root, "toggleState");
        if (state)
            for (int i = 0; i < 64; i++) {
                json_t* v = json_array_get(state, i);
                if (v) toggleState[i] = json_boolean_value(v);
            }
        json_t* colors = json_object_get(root, "activeColor");
        if (colors)
            for (int i = 0; i < 4; i++) {
                json_t* v = json_array_get(colors, i);
                if (v) activeColor[i] = (uint8_t) json_integer_value(v);
            }
        json_t* offColors = json_object_get(root, "offColor");
        if (offColors)
            for (int i = 0; i < 4; i++) {
                json_t* v = json_array_get(offColors, i);
                if (v) offColor[i] = (uint8_t) json_integer_value(v);
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

        addChild(createLightCentered<SmallLight<GreenRedLight>>(
            mm2px(Vec(6.0f, 18.0f)), module, Buttons64::ACTIVE_LIGHT));

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
            menu->addChild(createSubmenuItem(
                    string::f("Rows %d-%d on color", out * 2 + 1, out * 2 + 2), "", [=](Menu* sub) {
                for (auto& c : P64::LED_COLOR_DEFS) {
                    if (c.velocity == P64::LED_OFF) continue;
                    uint8_t vel = c.velocity;
                    sub->addChild(createCheckMenuItem(c.name, "",
                        [=]() { return m->activeColor[out] == vel; },
                        [=]() { m->activeColor[out] = vel; m->ledsDirty = true; }
                    ));
                }
            }));
            menu->addChild(createSubmenuItem(
                    string::f("Rows %d-%d off color", out * 2 + 1, out * 2 + 2), "", [=](Menu* sub) {
                for (auto& c : P64::LED_COLOR_DEFS) {
                    uint8_t vel = c.velocity;
                    sub->addChild(createCheckMenuItem(c.name, "",
                        [=]() { return m->offColor[out] == vel; },
                        [=]() { m->offColor[out] = vel; m->ledsDirty = true; }
                    ));
                }
            }));
        }
    }
};

Model* modelButtons64 = createModel<Buttons64, Buttons64Widget>("Buttons64");
