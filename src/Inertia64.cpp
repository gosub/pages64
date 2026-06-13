#include "PageModule.hpp"

// ── Inertia64 ─────────────────────────────────────────────────────────────────
// Eight rising masses, one per column, played with pedals: rows 1-4 are
// throttles, rows 5-8 are brakes, intensity growing toward the grid edges
// (the middle rows are the gentle pedals). Pedals act only while held; the
// strongest held pedal per side applies, and gas and brake may overlap.
//
// Each mass accelerates upward and its position wraps modulo the grid: when
// the cursor leaves the top it reappears at the bottom, so the position is a
// rising sawtooth whose frequency is the mass's speed. Momentum persists
// (no passive friction — a moving mass keeps moving until braked).
//
// Outputs: POS (poly 8ch, 0-10 V rising ramp per column, frequency = speed)
// and VEL (poly 8ch, 0-10 V speed). No clock — physics is continuous; masses
// keep moving while another page is active. RESET stops and re-zeros.

// Seconds to reach full speed from rest (and to shed it under braking), per
// pedal intensity; index 0 = the outermost/hardest pad of each group. The
// acceleration is maxSpeed / time, so the pedal feel stays consistent across
// max-speed settings. Heavy values = a massive, slow-to-rev flywheel.
static constexpr float PEDAL_TIME[4] = {3.f, 6.f, 12.f, 24.f};

struct Inertia64 : PageModule {
    enum ParamIds { NUM_PARAMS };
    enum InputIds  { NUM_INPUTS };
    enum OutputIds {
        POS_OUTPUT,
        VEL_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        ENUMS(ACTIVE_LIGHT, 2),
        NUM_LIGHTS
    };

    float pos[8] = {};        // position, fraction of a full traversal, wraps 0-1
    float vel[8] = {};        // traversals per second, clamped [0, maxSpeed]
    bool  padHeld[64] = {};

    float maxSpeed = 2.f;     // traversals/s (= Hz of the POS sawtooth)
    bool  wasMoving[8] = {};

    uint8_t cursorColor = P64::LED_GREEN;
    uint8_t pedalColor  = P64::LED_AMBER;

    Inertia64() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configOutput(POS_OUTPUT, "Position ramp (poly 8ch, 0-10V)");
        configOutput(VEL_OUTPUT, "Speed (poly 8ch, 0-10V)");
    }

    void onReset() override {
        PageModule::onReset();
        memset(pos, 0, sizeof(pos));
        memset(vel, 0, sizeof(vel));
        memset(padHeld, 0, sizeof(padHeld));
        memset(wasMoving, 0, sizeof(wasMoving));
        maxSpeed    = 2.f;
        cursorColor = P64::LED_GREEN;
        pedalColor  = P64::LED_AMBER;
    }

    // ── virtual hooks ─────────────────────────────────────────────────────────

    void pagePreProcess() override {
        auto* msg = reinterpret_cast<P64::LeftMessage*>(leftExpander.consumerMessage);
        if (msg && msg->resetTick) {
            memset(pos, 0, sizeof(pos));
            memset(vel, 0, sizeof(vel));
            ledsDirty = true;
        }

        for (int col = 0; col < 8; col++) {
            float accel = 0.f, brake = 0.f;
            for (int r = 0; r < 4; r++) {
                float rate = maxSpeed / PEDAL_TIME[r];
                if (padHeld[r * 8 + col])        // rows 0-3: throttle (top = hardest)
                    accel = std::max(accel, rate);
                if (padHeld[(7 - r) * 8 + col])  // rows 7-4: brake  (bottom = hardest)
                    brake = std::max(brake, rate);
            }
            vel[col] = clamp(vel[col] + (accel - brake) * sampleTime, 0.f, maxSpeed);
            pos[col] += vel[col] * sampleTime;
            pos[col] -= std::floor(pos[col]);

            // The scene LEDs mirror moving state; push the transitions.
            bool moving = vel[col] > 0.f;
            if (moving != wasMoving[col]) {
                wasMoving[col] = moving;
                ledsDirty = true;
            }
        }
    }

    void pageActive(const P64::LeftMessage& msg) override {
        // Scene A-H: handbrake — instant stop of column 1-8, position frozen.
        for (int i = 0; i < 8; i++) {
            if (msg.sceneEvent[i] && msg.sceneVelocity[i] > 0 && vel[i] > 0.f) {
                vel[i]    = 0.f;
                ledsDirty = true;
            }
        }

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
            int rowFromBottom = clamp((int)std::floor(pos[col] * 8.f), 0, 7);
            int cursorRow     = 7 - rowFromBottom;   // rises up the column, wraps
            for (int row = 0; row < 8; row++) {
                int     idx = row * 8 + col;
                uint8_t c   = padHeld[idx]       ? pedalColor
                            : (row == cursorRow) ? cursorColor
                            :                      P64::LED_OFF;
                if (c != ledState[idx]) {
                    ledState[idx] = c;
                    ledsDirty     = true;
                }
            }
        }
    }

    void buildSceneLeds(uint8_t sceneLeds[8]) override {
        for (int i = 0; i < 8; i++)
            sceneLeds[i] = vel[i] > 0.f ? cursorColor : P64::LED_OFF;
    }

    void updateOutputs() override {
        outputs[POS_OUTPUT].setChannels(8);
        outputs[VEL_OUTPUT].setChannels(8);
        for (int col = 0; col < 8; col++) {
            outputs[POS_OUTPUT].setVoltage(pos[col] * 10.f, col);
            outputs[VEL_OUTPUT].setVoltage(vel[col] / maxSpeed * 10.f, col);
        }
    }

    // ── serialisation ─────────────────────────────────────────────────────────

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_t* jp = json_array();
        json_t* jv = json_array();
        for (int i = 0; i < 8; i++) {
            json_array_append_new(jp, json_real(pos[i]));
            json_array_append_new(jv, json_real(vel[i]));
        }
        json_object_set_new(root, "pos",         jp);
        json_object_set_new(root, "vel",         jv);
        json_object_set_new(root, "maxSpeed",    json_real(maxSpeed));
        json_object_set_new(root, "cursorColor", json_integer(cursorColor));
        json_object_set_new(root, "pedalColor",  json_integer(pedalColor));
        return root;
    }

    void dataFromJson(json_t* root) override {
        json_t* j;
        if ((j = json_object_get(root, "maxSpeed")))
            maxSpeed = clamp((float)json_real_value(j), 1.f, 8.f);
        if ((j = json_object_get(root, "pos")))
            for (int i = 0; i < 8; i++) {
                json_t* v = json_array_get(j, i);
                if (v) pos[i] = clamp((float)json_real_value(v), 0.f, 1.f);
            }
        if ((j = json_object_get(root, "vel")))
            for (int i = 0; i < 8; i++) {
                json_t* v = json_array_get(j, i);
                if (v) vel[i] = clamp((float)json_real_value(v), 0.f, maxSpeed);
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
            mm2px(Vec(20.0f, 92.0f)), module, Inertia64::POS_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(
            mm2px(Vec(20.0f, 104.0f)), module, Inertia64::VEL_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        Inertia64* m = getModule<Inertia64>();
        menu->addChild(new MenuSeparator);
        menu->addChild(createSubmenuItem("Max speed", "", [=](Menu* sub) {
            for (int s : {1, 2, 4, 8}) {
                sub->addChild(createCheckMenuItem(string::f("%d /s", s), "",
                    [=]() { return m->maxSpeed == (float)s; },
                    [=]() {
                        m->maxSpeed = (float)s;
                        for (int i = 0; i < 8; i++)
                            m->vel[i] = std::min(m->vel[i], m->maxSpeed);
                    }
                ));
            }
        }));
        P64::appendColorMenu(menu, m, "Cursor color", &m->cursorColor);
        P64::appendColorMenu(menu, m, "Pedal color",  &m->pedalColor);
    }
};

Model* modelInertia64 = createModel<Inertia64, Inertia64Widget>("Inertia64");
