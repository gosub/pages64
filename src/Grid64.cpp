#include "PageModule.hpp"

struct Grid64 : PageModule {
    enum ParamIds {
        MODE_PARAM,     // 0 = toggle, 1 = momentary (applies to all 64 buttons)
        NUM_PARAMS
    };
    enum InputIds  { NUM_INPUTS };
    enum OutputIds {
        ENUMS(GRID_OUTPUT, 64),  // one mono gate output per grid button
        NUM_OUTPUTS
    };
    enum LightIds {
        ENUMS(ACTIVE_LIGHT, 2),  // lights[0,1] used by PageModule::process()
        NUM_LIGHTS
    };

    bool    toggleState[64]    = {};
    bool    momentaryState[64] = {};
    bool    prevMomentary      = true;
    uint8_t activeColor        = P64::LED_GREEN;

    Grid64() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configSwitch(MODE_PARAM, 0.f, 1.f, 1.f, "Mode", {"Toggle", "Momentary"});
        for (int i = 0; i < 64; i++)
            configOutput(GRID_OUTPUT + i,
                string::f("Grid r%d c%d", i / 8 + 1, i % 8 + 1));
    }

    void onReset() override {
        PageModule::onReset();
        memset(toggleState,    0, sizeof(toggleState));
        memset(momentaryState, 0, sizeof(momentaryState));
        prevMomentary = true;
        activeColor   = P64::LED_GREEN;
    }

    // ── virtual hooks ─────────────────────────────────────────────────────────

    void pagePreProcess() override {
        bool momentary = isMomentary();
        if (prevMomentary && !momentary) {
            memset(toggleState, 0, sizeof(toggleState));
            ledsDirty = true;
        }
        prevMomentary = momentary;
    }

    void pageActive(const P64::LeftMessage& msg) override {
        bool momentary = isMomentary();
        for (int row = 0; row < 8; row++) {
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
        if (!isMomentary()) return;
        for (int i = 0; i < 64; i++) {
            if (momentaryState[i]) {
                momentaryState[i] = false;
                ledsDirty = true;
            }
        }
    }

    void rebuildLeds() override {
        bool momentary = isMomentary();
        for (int i = 0; i < 64; i++) {
            bool active = momentary ? momentaryState[i] : toggleState[i];
            uint8_t color = active ? activeColor : P64::LED_OFF;
            if (color != ledState[i]) {
                ledState[i] = color;
                ledsDirty   = true;
            }
        }
    }

    void updateOutputs() override {
        bool momentary = isMomentary();
        for (int i = 0; i < 64; i++) {
            bool active = momentary ? momentaryState[i] : toggleState[i];
            outputs[GRID_OUTPUT + i].setVoltage(active ? 5.f : 0.f);
        }
    }

    // ── helpers ───────────────────────────────────────────────────────────────

    bool isMomentary() { return params[MODE_PARAM].getValue() > 0.5f; }

    // ── serialisation ─────────────────────────────────────────────────────────

    json_t* dataToJson() override {
        json_t* root  = json_object();
        json_t* state = json_array();
        for (int i = 0; i < 64; i++)
            json_array_append_new(state, json_boolean(toggleState[i]));
        json_object_set_new(root, "toggleState", state);
        json_object_set_new(root, "activeColor", json_integer(activeColor));
        return root;
    }

    void dataFromJson(json_t* root) override {
        json_t* state = json_object_get(root, "toggleState");
        if (state)
            for (int i = 0; i < 64; i++) {
                json_t* v = json_array_get(state, i);
                if (v) toggleState[i] = json_boolean_value(v);
            }
        json_t* color = json_object_get(root, "activeColor");
        if (color) activeColor = (uint8_t) json_integer_value(color);
        ledsDirty = true;
    }
};

// ── Widget ────────────────────────────────────────────────────────────────────

struct Grid64Widget : ModuleWidget {
    Grid64Widget(Grid64* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Grid64.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addChild(createLightCentered<SmallLight<GreenRedLight>>(
            mm2px(Vec(6.0f, 18.0f)), module, Grid64::ACTIVE_LIGHT));

        addParam(createParamCentered<CKSS>(
            mm2px(Vec(40.64f, 108.0f)), module, Grid64::MODE_PARAM));

        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                float x = 10.89f + col * 8.5f;
                float y = 28.0f  + row * 9.0f;
                addOutput(createOutputCentered<PJ301MPort>(
                    mm2px(Vec(x, y)), module, Grid64::GRID_OUTPUT + row * 8 + col));
            }
        }
    }

    void appendContextMenu(Menu* menu) override {
        Grid64* m = getModule<Grid64>();
        menu->addChild(new MenuSeparator);
        menu->addChild(createSubmenuItem("Button color", "", [=](Menu* sub) {
            for (auto& c : P64::LED_COLOR_DEFS) {
                if (c.velocity == P64::LED_OFF) continue;
                uint8_t vel = c.velocity;
                sub->addChild(createCheckMenuItem(c.name, "",
                    [=]() { return m->activeColor == vel; },
                    [=]() { m->activeColor = vel; m->ledsDirty = true; }
                ));
            }
        }));
    }
};

Model* modelGrid64 = createModel<Grid64, Grid64Widget>("Grid64");
