#include "PageModule.hpp"

// Slew rates: fraction of full range (0→1) per second, index 0=A(top/fast)…7=H(bottom/slow)
static constexpr float SLEW_RATES[8] = {
    1000.f,       // A (index 0, top): instant
    8.f,          // B: 0.125 s full range
    2.f,          // C: 0.5 s  ← default
    1.f,          // D: 1 s
    1.f / 2.f,    // E: 2 s
    1.f / 4.f,    // F: 4 s
    1.f / 8.f,    // G: 8 s
    1.f / 16.f,   // H (index 7, bottom): 16 s
};

struct Sliders64 : PageModule {
    enum ParamIds { NUM_PARAMS };
    enum InputIds  { NUM_INPUTS };
    enum OutputIds {
        ENUMS(SLIDER_OUTPUT, 8),
        POLY_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        ENUMS(ACTIVE_LIGHT, 2),  // lights[0,1] used by PageModule::process()
        NUM_LIGHTS
    };

    float   sliderValue[8]    = {};   // current output (normalised 0–1, slewed)
    float   sliderTarget[8]   = {};   // target set by pad press
    int     curve[8]          = {};   // per-column response curve (P64::ResponseCurve)
    int     selectedVelocity  = 2;    // default: C (index 2, 0.5s), third fastest
    int     voltRange         = 0;    // index into P64::VOLT_RANGES (default 0 – 10 V)
    uint8_t sliderColor       = P64::LED_GREEN;
    bool    fullBar            = true;

    Sliders64() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (int i = 0; i < 8; i++)
            configOutput(SLIDER_OUTPUT + i, string::f("Column %d CV", i + 1));
        configOutput(POLY_OUTPUT, "Poly CV (8-channel)");
    }

    void onReset() override {
        PageModule::onReset();
        for (int i = 0; i < 8; i++) {
            sliderValue[i]  = 0.f;
            sliderTarget[i] = 0.f;
            curve[i]        = P64::CURVE_LINEAR;
        }
        selectedVelocity = 2;
        voltRange        = 0;
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
        for (int e = 0; e < msg.eventCount; e++) {
            const P64::GridEvent& ev = msg.events[e];
            if (ev.value == 0) continue;
            if (ev.type == P64::GridEvent::SCENE) {
                // Scene buttons A–H: select slew velocity
                selectedVelocity = ev.index;
                ledsDirty = true;
            } else if (ev.type == P64::GridEvent::PAD) {
                // Grid pads: set slider target for that column
                sliderTarget[ev.index % 8] = (7.f - ev.index / 8) / 7.f;
                ledsDirty = true;
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
        const P64::VoltRange& r = P64::VOLT_RANGES[voltRange];
        outputs[POLY_OUTPUT].setChannels(8);
        for (int i = 0; i < 8; i++) {
            float p = P64::applyCurve(curve[i], sliderValue[i]);
            float v = r.lo + p * (r.hi - r.lo);
            outputs[SLIDER_OUTPUT + i].setVoltage(v);
            outputs[POLY_OUTPUT].setVoltage(v, i);
        }
    }

    // ── serialisation ─────────────────────────────────────────────────────────

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "selectedVelocity", json_integer(selectedVelocity));
        json_object_set_new(root, "voltRange",        json_integer(voltRange));
        json_object_set_new(root, "sliderColor",      json_integer(sliderColor));
        json_object_set_new(root, "fullBar",          json_boolean(fullBar));
        json_t* vals = json_array();
        json_t* tgts = json_array();
        json_t* crv  = json_array();
        for (int i = 0; i < 8; i++) {
            json_array_append_new(vals, json_real(sliderValue[i]));
            json_array_append_new(tgts, json_real(sliderTarget[i]));
            json_array_append_new(crv,  json_integer(curve[i]));
        }
        json_object_set_new(root, "sliderValue",  vals);
        json_object_set_new(root, "sliderTarget", tgts);
        json_object_set_new(root, "curve",        crv);
        return root;
    }

    void dataFromJson(json_t* root) override {
        json_t* j;
        if ((j = json_object_get(root, "selectedVelocity")))
            selectedVelocity = clamp((int) json_integer_value(j), 0, 7);
        if ((j = json_object_get(root, "voltRange")))
            voltRange = clamp((int) json_integer_value(j), 0, P64::NUM_VOLT_RANGES - 1);
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
        if ((j = json_object_get(root, "curve")))
            for (int i = 0; i < 8; i++) {
                json_t* v = json_array_get(j, i);
                if (v) curve[i] = clamp((int) json_integer_value(v), 0, P64::NUM_CURVES - 1);
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

        // 8 col outputs, 10mm apart; then POLY below separator
        const float rowY[8] = { 25.f, 35.f, 45.f, 55.f, 65.f, 75.f, 85.f, 95.f };
        for (int i = 0; i < 8; i++) {
            addOutput(createOutputCentered<PJ301MPort>(
                mm2px(Vec(20.0f, rowY[i])), module, Sliders64::SLIDER_OUTPUT + i));
        }
        addOutput(createOutputCentered<PJ301MPort>(
            mm2px(Vec(20.0f, 108.0f)), module, Sliders64::POLY_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        Sliders64* m = getModule<Sliders64>();
        menu->addChild(new MenuSeparator);
        P64::appendVoltRangeMenu(menu, &m->voltRange);
        menu->addChild(createSubmenuItem("Response curve", "", [=](Menu* sub) {
            for (int i = 0; i < 8; i++)
                P64::appendResponseMenu(sub, string::f("Column %d", i + 1), &m->curve[i]);
        }));
        menu->addChild(createSubmenuItem("Colors", "", [=](Menu* sub) {
            P64::appendColorMenu(sub, m, "Slider", &m->sliderColor);
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
