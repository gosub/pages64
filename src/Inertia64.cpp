#include "PageModule.hpp"

// ── Inertia64 ─────────────────────────────────────────────────────────────────
// Eight zero-friction flywheels, one per column, played with pedals: rows 1-4
// are throttles, rows 5-8 are brakes, intensity growing toward the grid edges
// (the middle rows are the gentle pedals). Pedals act only while held; the
// strongest held pedal per side applies, and gas and brake may overlap.
//
// The column cursor is the rim point seen edge-on (x = sin of the phase), so
// it rides sinusoidally — fast through the middle, slowing into the
// turnarounds — which is the disc itself, readable as speed at a glance.
//
// Outputs: X (poly 8ch, ±5 V sine per column, frequency = disc speed) and
// VEL (poly 8ch, 0-10 V angular velocity). No clock — physics is continuous;
// discs keep spinning while another page is active. RESET stops and re-zeros.

// Pedal intensity in rev/s²; index 0 = the outermost row of each pedal group.
static constexpr float PEDAL_RATES[4] = {8.f, 4.f, 2.f, 1.f};

struct Inertia64 : PageModule {
    enum ParamIds { NUM_PARAMS };
    enum InputIds  { NUM_INPUTS };
    enum OutputIds {
        X_OUTPUT,
        VEL_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        ENUMS(ACTIVE_LIGHT, 2),
        NUM_LIGHTS
    };

    float phase[8] = {};      // revolutions, wraps 0-1
    float omega[8] = {};      // revolutions per second, clamped [0, maxSpeed]
    bool  padHeld[64] = {};

    float maxSpeed = 16.f;    // rev/s

    uint8_t cursorColor = P64::LED_GREEN;
    uint8_t pedalColor  = P64::LED_AMBER;

    Inertia64() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configOutput(X_OUTPUT,   "Rim point X (poly 8ch, ±5V)");
        configOutput(VEL_OUTPUT, "Angular velocity (poly 8ch, 0-10V)");
    }

    void onReset() override {
        PageModule::onReset();
        memset(phase, 0, sizeof(phase));
        memset(omega, 0, sizeof(omega));
        memset(padHeld, 0, sizeof(padHeld));
        cursorColor = P64::LED_GREEN;
        pedalColor  = P64::LED_AMBER;
    }

    // ── virtual hooks ─────────────────────────────────────────────────────────

    void pagePreProcess() override {
        auto* msg = reinterpret_cast<P64::LeftMessage*>(leftExpander.consumerMessage);
        if (msg && msg->resetTick) {
            memset(phase, 0, sizeof(phase));
            memset(omega, 0, sizeof(omega));
            ledsDirty = true;
        }

        for (int col = 0; col < 8; col++) {
            float accel = 0.f, brake = 0.f;
            for (int r = 0; r < 4; r++) {
                if (padHeld[r * 8 + col])
                    accel = std::max(accel, PEDAL_RATES[r]);
                if (padHeld[(7 - r) * 8 + col])
                    brake = std::max(brake, PEDAL_RATES[r]);
            }
            omega[col] = clamp(omega[col] + (accel - brake) * sampleTime,
                               0.f, maxSpeed);
            phase[col] += omega[col] * sampleTime;
            phase[col] -= std::floor(phase[col]);
        }
    }

    void pageActive(const P64::LeftMessage& msg) override {
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                int note = row * 16 + col;
                if (!msg.noteEvent[note]) continue;
                padHeld[row * 8 + col] = msg.noteVelocity[note] > 0;
                ledsDirty = true;
            }
        }
    }

    void pageInactive() override {
        for (int i = 0; i < 64; i++) {
            if (padHeld[i]) {
                padHeld[i] = false;
                ledsDirty  = true;
            }
        }
    }

    void rebuildLeds() override {
        for (int col = 0; col < 8; col++) {
            float x         = std::sin(2.f * M_PI * phase[col]);
            int   cursorRow = clamp((int)std::round((1.f - x) * 0.5f * 7.f), 0, 7);
            for (int row = 0; row < 8; row++) {
                int     idx = row * 8 + col;
                uint8_t c   = padHeld[idx]      ? pedalColor
                            : (row == cursorRow) ? cursorColor
                            :                      P64::LED_OFF;
                if (c != ledState[idx]) {
                    ledState[idx] = c;
                    ledsDirty     = true;
                }
            }
        }
    }

    void updateOutputs() override {
        outputs[X_OUTPUT].setChannels(8);
        outputs[VEL_OUTPUT].setChannels(8);
        for (int col = 0; col < 8; col++) {
            outputs[X_OUTPUT].setVoltage(5.f * std::sin(2.f * M_PI * phase[col]), col);
            outputs[VEL_OUTPUT].setVoltage(omega[col] / maxSpeed * 10.f, col);
        }
    }

    // ── serialisation ─────────────────────────────────────────────────────────

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_t* jp = json_array();
        json_t* jo = json_array();
        for (int i = 0; i < 8; i++) {
            json_array_append_new(jp, json_real(phase[i]));
            json_array_append_new(jo, json_real(omega[i]));
        }
        json_object_set_new(root, "phase",       jp);
        json_object_set_new(root, "omega",       jo);
        json_object_set_new(root, "cursorColor", json_integer(cursorColor));
        json_object_set_new(root, "pedalColor",  json_integer(pedalColor));
        return root;
    }

    void dataFromJson(json_t* root) override {
        json_t* j;
        if ((j = json_object_get(root, "phase")))
            for (int i = 0; i < 8; i++) {
                json_t* v = json_array_get(j, i);
                if (v) phase[i] = clamp((float)json_real_value(v), 0.f, 1.f);
            }
        if ((j = json_object_get(root, "omega")))
            for (int i = 0; i < 8; i++) {
                json_t* v = json_array_get(j, i);
                if (v) omega[i] = clamp((float)json_real_value(v), 0.f, maxSpeed);
            }
        if ((j = json_object_get(root, "cursorColor")))
            cursorColor = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "pedalColor")))
            pedalColor = (uint8_t)json_integer_value(j);
        ledsDirty = true;
    }
};

// ── Widget ────────────────────────────────────────────────────────────────────

struct Inertia64Widget : ModuleWidget {
    Inertia64Widget(Inertia64* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Inertia64.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addChild(createLightCentered<SmallLight<GreenRedLight>>(
            mm2px(Vec(6.0f, 18.0f)), module, Inertia64::ACTIVE_LIGHT));

        addOutput(createOutputCentered<PJ301MPort>(
            mm2px(Vec(20.0f, 92.0f)), module, Inertia64::X_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(
            mm2px(Vec(20.0f, 104.0f)), module, Inertia64::VEL_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        Inertia64* m = getModule<Inertia64>();
        menu->addChild(new MenuSeparator);
        P64::appendColorMenu(menu, m, "Cursor color", &m->cursorColor);
        P64::appendColorMenu(menu, m, "Pedal color",  &m->pedalColor);
    }
};

Model* modelInertia64 = createModel<Inertia64, Inertia64Widget>("Inertia64");
