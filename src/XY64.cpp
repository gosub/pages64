#include "PageModule.hpp"

// Slew rates: fraction of full range (0→1) per second, index 0=A(top/fast)…7=H(bottom/slow)
// Same table as Sliders64 so the scene buttons feel identical across pages.
static constexpr float XY_SLEW_RATES[8] = {
    1000.f,       // A (index 0, top): instant
    8.f,          // B: 0.125 s full range
    2.f,          // C: 0.5 s  ← default
    1.f,          // D: 1 s
    1.f / 2.f,    // E: 2 s
    1.f / 4.f,    // F: 4 s
    1.f / 8.f,    // G: 8 s
    1.f / 16.f,   // H (index 7, bottom): 16 s
};

struct XY64 : PageModule {
    enum ParamIds { NUM_PARAMS };
    enum InputIds  { NUM_INPUTS };
    enum OutputIds {
        X_OUTPUT,
        Y_OUTPUT,
        TRIG_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        ENUMS(ACTIVE_LIGHT, 2),  // lights[0,1] used by PageModule::process()
        NUM_LIGHTS
    };

    // Normalized coordinates: x 0=left…1=right, y 0=bottom…1=top.
    float   curX = 0.f, curY = 0.f;    // continuous cursor (the output)
    float   tgtX = 0.f, tgtY = 0.f;    // target set by pad press
    bool    traveling        = false;  // a landing still owes a TRIG
    int     selectedVelocity = 2;      // default: C (0.5 s), Sliders64 idiom
    uint8_t cursorColor      = P64::LED_GREEN;
    uint8_t targetColor      = P64::LED_GREEN_DIM;

    dsp::PulseGenerator arriveTrig;

    XY64() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configOutput(X_OUTPUT,    "Cursor X (0–10V, left→right)");
        configOutput(Y_OUTPUT,    "Cursor Y (0–10V, bottom→top)");
        configOutput(TRIG_OUTPUT, "Arrival trigger");
    }

    void onReset() override {
        PageModule::onReset();
        curX = curY = tgtX = tgtY = 0.f;
        traveling        = false;
        selectedVelocity = 2;
        cursorColor      = P64::LED_GREEN;
        targetColor      = P64::LED_GREEN_DIM;
    }

    // ── virtual hooks ─────────────────────────────────────────────────────────

    void pagePreProcess() override {
        // Straight-line glide at constant speed in normalized space; the
        // display quantizes to pads but the cursor (and the CV) never does.
        float dx = tgtX - curX;
        float dy = tgtY - curY;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist > 0.f) {
            float step = XY_SLEW_RATES[selectedVelocity] * sampleTime;
            if (step >= dist) {
                curX = tgtX;
                curY = tgtY;
            } else {
                curX += dx / dist * step;
                curY += dy / dist * step;
            }
            ledsDirty = true;   // cheap: rebuildLeds only runs when active
        }
        if (traveling && curX == tgtX && curY == tgtY) {
            traveling = false;
            arriveTrig.trigger(1e-3f);
        }
    }

    void pageActive(const P64::LeftMessage& msg) override {
        for (int e = 0; e < msg.eventCount; e++) {
            const P64::GridEvent& ev = msg.events[e];
            if (ev.value == 0) continue;
            if (ev.type == P64::GridEvent::SCENE) {
                selectedVelocity = ev.index;
                ledsDirty = true;
            } else if (ev.type == P64::GridEvent::PAD) {
                tgtX = (ev.index % 8) / 7.f;
                tgtY = (7 - ev.index / 8) / 7.f;
                traveling = true;   // fires on arrival — immediately at rate A
                ledsDirty = true;
            }
        }
    }

    void rebuildLeds() override {
        int curIdx = (7 - (int) std::round(curY * 7.f)) * 8
                   + (int) std::round(curX * 7.f);
        int tgtIdx = (7 - (int) std::round(tgtY * 7.f)) * 8
                   + (int) std::round(tgtX * 7.f);
        for (int i = 0; i < 64; i++) {
            uint8_t color = (i == curIdx) ? cursorColor
                          : (i == tgtIdx) ? targetColor
                          :                 P64::LED_OFF;
            if (color != ledState[i]) {
                ledState[i] = color;
                ledsDirty   = true;
            }
        }
    }

    void buildSceneLeds(uint8_t sceneLeds[8]) override {
        memset(sceneLeds, P64::LED_OFF, 8);
        sceneLeds[selectedVelocity] = cursorColor;
    }

    void updateOutputs() override {
        outputs[X_OUTPUT].setVoltage(curX * 10.f);
        outputs[Y_OUTPUT].setVoltage(curY * 10.f);
        outputs[TRIG_OUTPUT].setVoltage(arriveTrig.process(sampleTime) ? 10.f : 0.f);
    }

    // ── serialisation ─────────────────────────────────────────────────────────

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "curX", json_real(curX));
        json_object_set_new(root, "curY", json_real(curY));
        json_object_set_new(root, "tgtX", json_real(tgtX));
        json_object_set_new(root, "tgtY", json_real(tgtY));
        json_object_set_new(root, "selectedVelocity", json_integer(selectedVelocity));
        json_object_set_new(root, "cursorColor", json_integer(cursorColor));
        json_object_set_new(root, "targetColor", json_integer(targetColor));
        return root;
    }

    void dataFromJson(json_t* root) override {
        json_t* j;
        if ((j = json_object_get(root, "curX"))) curX = (float) json_real_value(j);
        if ((j = json_object_get(root, "curY"))) curY = (float) json_real_value(j);
        if ((j = json_object_get(root, "tgtX"))) tgtX = (float) json_real_value(j);
        if ((j = json_object_get(root, "tgtY"))) tgtY = (float) json_real_value(j);
        if ((j = json_object_get(root, "selectedVelocity")))
            selectedVelocity = clamp((int) json_integer_value(j), 0, 7);
        if ((j = json_object_get(root, "cursorColor")))
            cursorColor = (uint8_t) json_integer_value(j);
        if ((j = json_object_get(root, "targetColor")))
            targetColor = (uint8_t) json_integer_value(j);
        traveling = false;   // never owe a trigger across a reload
        ledsDirty = true;
    }
};

// ── Widget ────────────────────────────────────────────────────────────────────

struct XY64Widget : ModuleWidget {
    XY64Widget(XY64* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/XY64.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addChild(createLightCentered<SmallLight<GreenRedLight>>(
            mm2px(Vec(6.0f, 18.0f)), module, XY64::ACTIVE_LIGHT));

        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.f, 45.f)), module, XY64::X_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.f, 60.f)), module, XY64::Y_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.f, 75.f)), module, XY64::TRIG_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        XY64* m = getModule<XY64>();
        menu->addChild(new MenuSeparator);
        P64::appendColorMenu(menu, m, "Cursor color", &m->cursorColor);
        P64::appendColorMenu(menu, m, "Target color", &m->targetColor);
    }
};

Model* modelXY64 = createModel<XY64, XY64Widget>("XY64");
