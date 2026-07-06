#include "PageModule.hpp"

// The whole 8×8 grid is a single high-resolution fader. Cells fill in reading
// order — top-left (index 0) toward bottom-right (index 63) — so the grid reads
// like a flood rising across it. Pressing cell i sets the level to i/63, giving
// 64 evenly-spaced steps with both endpoints exact: top-left = 0 V, bottom-right
// = 10 V. Four independent faders live in one module, selected by top buttons
// 1–4 (CC 104–107). Scene buttons A–H pick the slew rate exactly as in Sliders64.

// Slew rates: fraction of full range (0→1) per second, index 0=A(top/fast)…7=H(bottom/slow)
static constexpr float FLOOD_SLEW_RATES[8] = {
    1000.f,       // A (index 0, top): instant
    8.f,          // B: 0.125 s full range
    2.f,          // C: 0.5 s  ← default
    1.f,          // D: 1 s
    1.f / 2.f,    // E: 2 s
    1.f / 4.f,    // F: 4 s
    1.f / 8.f,    // G: 8 s
    1.f / 16.f,   // H (index 7, bottom): 16 s
};

// Output voltage ranges, selectable from the context menu (index 0 = default).
struct FloodRange { float lo, hi; const char* label; };
static constexpr FloodRange FLOOD_RANGES[] = {
    { 0.f, 10.f, "0 – 10 V" },
    { 0.f,  5.f, "0 – 5 V"  },
    { 0.f,  2.f, "0 – 2 V"  },
    { 0.f,  1.f, "0 – 1 V"  },
    {-1.f,  1.f, "-1 – +1 V" },
    {-2.f,  2.f, "-2 – +2 V" },
    {-5.f,  5.f, "-5 – +5 V" },
};
static constexpr int NUM_RANGES = sizeof(FLOOD_RANGES) / sizeof(FLOOD_RANGES[0]);

static constexpr int NUM_FADERS = 4;

struct Flood64 : PageModule {
    enum ParamIds { NUM_PARAMS };
    enum InputIds  { NUM_INPUTS };
    enum OutputIds {
        ENUMS(FLOOD_OUTPUT, NUM_FADERS),
        POLY_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        ENUMS(ACTIVE_LIGHT, 2),  // lights[0,1] used by PageModule::process()
        NUM_LIGHTS
    };

    float   faderValue[NUM_FADERS]  = {};   // current output (normalised 0–1, slewed)
    float   faderTarget[NUM_FADERS] = {};   // target set by pad press
    int     subPage          = 0;    // which fader (0–3) is shown on the grid
    int     selectedVelocity = 2;    // default: C (index 2, 0.5s), third fastest
    int     voltRange        = 0;    // index into FLOOD_RANGES (default 0 – 10 V)
    uint8_t fillColor        = P64::LED_GREEN;
    uint8_t selectorColor    = P64::LED_AMBER_DIM;  // dim selector for the other faders
    bool    waterLineOnly    = false;               // false: full flood, true: single line

    Flood64() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (int i = 0; i < NUM_FADERS; i++)
            configOutput(FLOOD_OUTPUT + i, string::f("Fader %d CV", i + 1));
        configOutput(POLY_OUTPUT, "Poly CV (4-channel)");
    }

    void onReset() override {
        PageModule::onReset();
        for (int i = 0; i < NUM_FADERS; i++) {
            faderValue[i]  = 0.f;
            faderTarget[i] = 0.f;
        }
        subPage          = 0;
        selectedVelocity = 2;
        voltRange        = 0;
        fillColor        = P64::LED_GREEN;
        selectorColor    = P64::LED_AMBER_DIM;
        waterLineOnly    = false;
    }

    // ── virtual hooks ─────────────────────────────────────────────────────────

    void pagePreProcess() override {
        // Slew every fader toward its target — background faders keep moving even
        // while their sub-page is not the one on the grid.
        bool changed = false;
        for (int i = 0; i < NUM_FADERS; i++) {
            if (faderValue[i] == faderTarget[i]) continue;
            float delta = FLOOD_SLEW_RATES[selectedVelocity] * sampleTime;
            float diff  = faderTarget[i] - faderValue[i];
            if (std::abs(diff) <= delta)
                faderValue[i] = faderTarget[i];
            else
                faderValue[i] += (diff > 0.f ? delta : -delta);
            changed = true;
        }
        if (changed) ledsDirty = true;
    }

    void pageActive(const P64::LeftMessage& msg) override {
        for (int e = 0; e < msg.eventCount; e++) {
            const P64::GridEvent& ev = msg.events[e];
            if (ev.value == 0) continue;
            if (ev.type == P64::GridEvent::CC
                    && ev.index >= 104 && ev.index < 104 + NUM_FADERS) {
                // Top buttons 1–4: select which fader is on the grid
                subPage = ev.index - 104;
                ledsDirty = true;
            } else if (ev.type == P64::GridEvent::SCENE) {
                // Scene buttons A–H: select slew velocity
                selectedVelocity = ev.index;
                ledsDirty = true;
            } else if (ev.type == P64::GridEvent::PAD) {
                // Grid pads: each cell is one 1/64th quantum. Pressing cell i
                // floods up to it (i+1 cells → (i+1)/64). The top-left cell
                // toggles the last quantum on/off, so the fader reaches exactly
                // 0 (empty grid) as well as full (1.0, all 64 cells lit).
                int filled;
                if (ev.index == 0) {
                    int cur = (int) std::round(faderTarget[subPage] * 64.f);
                    filled = (cur == 1) ? 0 : 1;
                } else {
                    filled = ev.index + 1;
                }
                faderTarget[subPage] = filled / 64.f;
                ledsDirty = true;
            }
        }
    }

    // Number of flooded cells for the current fader (0…64).
    int floodFilled() const {
        int f = (int) std::round(faderValue[subPage] * 64.f);
        return clamp(f, 0, 64);
    }

    void rebuildLeds() override {
        int filled = floodFilled();
        for (int i = 0; i < 64; i++) {
            bool    lit   = waterLineOnly ? (i == filled - 1) : (i < filled);
            uint8_t color = lit ? fillColor : P64::LED_OFF;
            if (color != ledState[i]) {
                ledState[i] = color;
                ledsDirty   = true;
            }
        }
    }

    void buildTopLeds(uint8_t topLeds[8]) override {
        memset(topLeds, P64::LED_OFF, 8);
        for (int b = 0; b < NUM_FADERS; b++)
            topLeds[b] = (b == subPage) ? fillColor : selectorColor;
    }

    void buildSceneLeds(uint8_t sceneLeds[8]) override {
        memset(sceneLeds, P64::LED_OFF, 8);
        sceneLeds[selectedVelocity] = fillColor;
    }

    void updateOutputs() override {
        const FloodRange& r = FLOOD_RANGES[voltRange];
        outputs[POLY_OUTPUT].setChannels(NUM_FADERS);
        for (int i = 0; i < NUM_FADERS; i++) {
            float v = r.lo + faderValue[i] * (r.hi - r.lo);
            outputs[FLOOD_OUTPUT + i].setVoltage(v);
            outputs[POLY_OUTPUT].setVoltage(v, i);
        }
    }

    // ── serialisation ─────────────────────────────────────────────────────────

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "subPage",          json_integer(subPage));
        json_object_set_new(root, "selectedVelocity", json_integer(selectedVelocity));
        json_object_set_new(root, "voltRange",        json_integer(voltRange));
        json_object_set_new(root, "fillColor",        json_integer(fillColor));
        json_object_set_new(root, "selectorColor",    json_integer(selectorColor));
        json_object_set_new(root, "waterLineOnly",    json_boolean(waterLineOnly));
        json_t* vals = json_array();
        json_t* tgts = json_array();
        for (int i = 0; i < NUM_FADERS; i++) {
            json_array_append_new(vals, json_real(faderValue[i]));
            json_array_append_new(tgts, json_real(faderTarget[i]));
        }
        json_object_set_new(root, "faderValue",  vals);
        json_object_set_new(root, "faderTarget", tgts);
        return root;
    }

    void dataFromJson(json_t* root) override {
        json_t* j;
        if ((j = json_object_get(root, "subPage")))
            subPage = clamp((int) json_integer_value(j), 0, NUM_FADERS - 1);
        if ((j = json_object_get(root, "selectedVelocity")))
            selectedVelocity = clamp((int) json_integer_value(j), 0, 7);
        if ((j = json_object_get(root, "voltRange")))
            voltRange = clamp((int) json_integer_value(j), 0, NUM_RANGES - 1);
        if ((j = json_object_get(root, "fillColor")))
            fillColor = (uint8_t) json_integer_value(j);
        if ((j = json_object_get(root, "selectorColor")))
            selectorColor = (uint8_t) json_integer_value(j);
        if ((j = json_object_get(root, "waterLineOnly")))
            waterLineOnly = json_boolean_value(j);
        if ((j = json_object_get(root, "faderValue")))
            for (int i = 0; i < NUM_FADERS; i++) {
                json_t* v = json_array_get(j, i);
                if (v) faderValue[i] = (float) json_real_value(v);
            }
        if ((j = json_object_get(root, "faderTarget")))
            for (int i = 0; i < NUM_FADERS; i++) {
                json_t* v = json_array_get(j, i);
                if (v) faderTarget[i] = (float) json_real_value(v);
            }
        ledsDirty = true;
    }
};

// ── Widget ────────────────────────────────────────────────────────────────────

struct Flood64Widget : ModuleWidget {
    Flood64Widget(Flood64* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Flood64.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addChild(createLightCentered<SmallLight<GreenRedLight>>(
            mm2px(Vec(6.0f, 18.0f)), module, Flood64::ACTIVE_LIGHT));

        // 4 fader outputs, then POLY below the separator
        const float faderY[NUM_FADERS] = { 30.f, 46.f, 62.f, 78.f };
        for (int i = 0; i < NUM_FADERS; i++) {
            addOutput(createOutputCentered<PJ301MPort>(
                mm2px(Vec(20.0f, faderY[i])), module, Flood64::FLOOD_OUTPUT + i));
        }
        addOutput(createOutputCentered<PJ301MPort>(
            mm2px(Vec(20.0f, 100.0f)), module, Flood64::POLY_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        Flood64* m = getModule<Flood64>();
        menu->addChild(new MenuSeparator);
        menu->addChild(createSubmenuItem("Voltage range", "", [=](Menu* sub) {
            for (int i = 0; i < NUM_RANGES; i++) {
                int idx = i;
                sub->addChild(createCheckMenuItem(FLOOD_RANGES[i].label, "",
                    [=]() { return m->voltRange == idx; },
                    [=]() { m->voltRange = idx; }
                ));
            }
        }));
        menu->addChild(createSubmenuItem("Colors", "", [=](Menu* sub) {
            P64::appendColorMenu(sub, m, "Flood",    &m->fillColor);
            P64::appendColorMenu(sub, m, "Selector", &m->selectorColor, true);
        }));
        menu->addChild(createSubmenuItem("Fill style", "", [=](Menu* sub) {
            sub->addChild(createCheckMenuItem("Flood (full fill)", "",
                [=]() { return !m->waterLineOnly; },
                [=]() { m->waterLineOnly = false; m->ledsDirty = true; }
            ));
            sub->addChild(createCheckMenuItem("Water line (single cell)", "",
                [=]() { return m->waterLineOnly; },
                [=]() { m->waterLineOnly = true; m->ledsDirty = true; }
            ));
        }));
    }
};

Model* modelFlood64 = createModel<Flood64, Flood64Widget>("Flood64");
