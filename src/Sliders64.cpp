#include "PageModule.hpp"

struct Sliders64 : PageModule {
    enum ParamIds {
        ENUMS(RANGE_PARAM, 8),  // per-column: 0 = 5V, 1 = 10V
        NUM_PARAMS
    };
    enum InputIds  { NUM_INPUTS };
    enum OutputIds {
        ENUMS(SLIDER_OUTPUT, 8),
        NUM_OUTPUTS
    };
    enum LightIds {
        ENUMS(ACTIVE_LIGHT, 2),  // lights[0,1] used by PageModule::process()
        NUM_LIGHTS
    };

    // Slew rates: fraction of full range (0→1) per second, indexed 0=H(slow)…7=A(fast)
    static constexpr float SLEW_RATES[8] = {
        1.f / 16.f,   // H: 16 s full range
        1.f / 8.f,    // G: 8 s
        1.f / 4.f,    // F: 4 s
        1.f / 2.f,    // E: 2 s
        1.f,          // D: 1 s
        2.f,          // C: 0.5 s
        8.f,          // B: 0.125 s
        1000.f,       // A: instant
    };

    float   sliderValue[8]    = {};   // current output (normalised 0–1, slewed)
    float   sliderTarget[8]   = {};   // target set by pad press
    int     selectedVelocity  = 3;    // default: E (index 3, 2s), mid-range slew
    uint8_t sliderColor       = P64::LED_GREEN;
    bool    fullBar            = true;

    Sliders64() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (int i = 0; i < 8; i++) {
            configSwitch(RANGE_PARAM + i, 0.f, 1.f, 0.f,
                string::f("Column %d range", i + 1), {"5V", "10V"});
            configOutput(SLIDER_OUTPUT + i, string::f("Column %d CV", i + 1));
        }
    }

    void onReset() override {
        PageModule::onReset();
        for (int i = 0; i < 8; i++) {
            sliderValue[i]  = 0.f;
            sliderTarget[i] = 0.f;
        }
        selectedVelocity = 3;
        sliderColor      = P64::LED_GREEN;
        fullBar          = true;
    }

    // ── virtual hooks ─────────────────────────────────────────────────────────

    void pagePreProcess() override {
        bool changed = false;
        for (int i = 0; i < 8; i++) {
            if (sliderValue[i] == sliderTarget[i]) continue;
            float delta = SLEW_RATES[selectedVelocity] * sampleTime;
            float diff  = sliderTarget[i] - sliderValue[i];
            if (std::abs(diff) <= delta)
                sliderValue[i] = sliderTarget[i];
            else
                sliderValue[i] += (diff > 0.f ? delta : -delta);
            changed = true;
        }
        if (changed) ledsDirty = true;
    }

    void pageActive(const P64::LeftMessage& msg) override {
        // Scene buttons A–H: select slew velocity
        for (int i = 0; i < 8; i++) {
            if (msg.sceneEvent[i] && msg.sceneVelocity[i] > 0) {
                selectedVelocity = i;
                ledsDirty = true;
            }
        }
        // Grid pads: set slider target for that column
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                int note = row * 16 + col;
                if (msg.noteEvent[note] && msg.noteVelocity[note] > 0) {
                    sliderTarget[col] = (7.f - row) / 7.f;
                    ledsDirty = true;
                }
            }
        }
    }

    void rebuildLeds() override {
        for (int col = 0; col < 8; col++) {
            int topRow = (int) std::round((1.f - sliderValue[col]) * 7.f);
            topRow = clamp(topRow, 0, 7);
            for (int row = 0; row < 8; row++) {
                int     idx = row * 8 + col;
                bool    lit = fullBar ? (row >= topRow) : (row == topRow);
                uint8_t color = lit ? sliderColor : P64::LED_OFF;
                if (color != ledState[idx]) {
                    ledState[idx] = color;
                    ledsDirty     = true;
                }
            }
        }
    }

    void buildSceneLeds(uint8_t sceneLeds[8]) override {
        memset(sceneLeds, P64::LED_OFF, 8);
        sceneLeds[selectedVelocity] = sliderColor;
    }

    void updateOutputs() override {
        for (int i = 0; i < 8; i++) {
            float maxV = (params[RANGE_PARAM + i].getValue() > 0.5f) ? 10.f : 5.f;
            outputs[SLIDER_OUTPUT + i].setVoltage(sliderValue[i] * maxV);
        }
    }

    // ── serialisation ─────────────────────────────────────────────────────────

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "selectedVelocity", json_integer(selectedVelocity));
        json_object_set_new(root, "sliderColor",      json_integer(sliderColor));
        json_object_set_new(root, "fullBar",          json_boolean(fullBar));
        json_t* vals = json_array();
        json_t* tgts = json_array();
        for (int i = 0; i < 8; i++) {
            json_array_append_new(vals, json_real(sliderValue[i]));
            json_array_append_new(tgts, json_real(sliderTarget[i]));
        }
        json_object_set_new(root, "sliderValue",  vals);
        json_object_set_new(root, "sliderTarget", tgts);
        return root;
    }

    void dataFromJson(json_t* root) override {
        json_t* j;
        if ((j = json_object_get(root, "selectedVelocity")))
            selectedVelocity = clamp((int) json_integer_value(j), 0, 7);
        if ((j = json_object_get(root, "sliderColor")))
            sliderColor = (uint8_t) json_integer_value(j);
        if ((j = json_object_get(root, "fullBar")))
            fullBar = json_boolean_value(j);
        if ((j = json_object_get(root, "sliderValue")))
            for (int i = 0; i < 8; i++) {
                json_t* v = json_array_get(j, i);
                if (v) sliderValue[i] = (float) json_real_value(v);
            }
        if ((j = json_object_get(root, "sliderTarget")))
            for (int i = 0; i < 8; i++) {
                json_t* v = json_array_get(j, i);
                if (v) sliderTarget[i] = (float) json_real_value(v);
            }
        ledsDirty = true;
    }
};

// ── Widget ────────────────────────────────────────────────────────────────────

struct Sliders64Widget : ModuleWidget {
    Sliders64Widget(Sliders64* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Sliders64.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addChild(createLightCentered<SmallLight<GreenRedLight>>(
            mm2px(Vec(6.0f, 18.0f)), module, Sliders64::ACTIVE_LIGHT));

        // 8 columns: x positions matching Grid64 (8.5mm pitch centered in 81.28mm)
        const float colX[8] = { 10.89f, 19.39f, 27.89f, 36.39f, 44.89f, 53.39f, 61.89f, 70.39f };

        for (int i = 0; i < 8; i++) {
            addParam(createParamCentered<CKSS>(
                mm2px(Vec(colX[i], 65.0f)), module, Sliders64::RANGE_PARAM + i));
            addOutput(createOutputCentered<PJ301MPort>(
                mm2px(Vec(colX[i], 82.0f)), module, Sliders64::SLIDER_OUTPUT + i));
        }
    }

    void appendContextMenu(Menu* menu) override {
        Sliders64* m = getModule<Sliders64>();
        menu->addChild(new MenuSeparator);
        menu->addChild(createSubmenuItem("Slider color", "", [=](Menu* sub) {
            for (auto& c : P64::LED_COLOR_DEFS) {
                if (c.velocity == P64::LED_OFF) continue;
                uint8_t vel = c.velocity;
                sub->addChild(createCheckMenuItem(c.name, "",
                    [=]() { return m->sliderColor == vel; },
                    [=]() { m->sliderColor = vel; m->ledsDirty = true; }
                ));
            }
        }));
        menu->addChild(createSubmenuItem("Slider style", "", [=](Menu* sub) {
            sub->addChild(createCheckMenuItem("Full bar", "",
                [=]() { return m->fullBar; },
                [=]() { m->fullBar = true; m->ledsDirty = true; }
            ));
            sub->addChild(createCheckMenuItem("Top LED only", "",
                [=]() { return !m->fullBar; },
                [=]() { m->fullBar = false; m->ledsDirty = true; }
            ));
        }));
    }
};

Model* modelSliders64 = createModel<Sliders64, Sliders64Widget>("Sliders64");
