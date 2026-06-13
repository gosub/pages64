#include "PageModule.hpp"

// ── Inertia64 ─────────────────────────────────────────────────────────────────
// Eight masses, one per column, played with pedals: rows 1-4 push up, rows 5-8
// push down, intensity growing toward the grid edges (the middle rows are the
// gentle pedals). Pedals act only while held; the strongest held pedal per
// side applies, and the two sides may overlap (net acceleration is their
// difference).
//
// Each mass's position wraps modulo the grid: when the cursor leaves the top
// it reappears at the bottom (and vice versa going down), so the position is a
// sawtooth whose frequency is the mass's speed. A lane is monodirectional
// (the down pads brake, velocity clamped at 0) or bidirectional (the down pads
// drive it into reverse) — set per lane on the Direction page (top button 2).
// Per-lane viscous friction (the Friction page, top button 3) damps the motion
// so a held pedal cruises at a terminal speed and an unpedaled mass coasts to a
// stop; friction 0 is the default eternal flywheel.
//
// Top buttons select a page: 1 = Play (pedals), 2 = Direction (mono/bi per
// lane), 3 = Friction (per-lane damping 0-8).
//
// Outputs: POS (poly 8ch, 0-10 V sawtooth per column, frequency = speed) and
// VEL (poly 8ch, signed ±10 V speed; monodirectional lanes stay 0-10 V). No
// clock — physics is continuous; masses keep moving while another page is
// active. RESET stops and re-zeros.

// Seconds to reach full speed from rest (and to shed it under braking), per
// pedal intensity; index 0 = the outermost/hardest pad of each group. The
// acceleration is maxSpeed / time, so the pedal feel stays consistent across
// max-speed settings. Heavy values = a massive, slow-to-rev flywheel.
static constexpr float PEDAL_TIME[4] = {3.f, 6.f, 12.f, 24.f};

// POS declick: slew the output so the sawtooth's wrap edge (and a RESET jump)
// take ~1 ms instead of one sample. At 10000 V/s the full 0-10 V swing takes
// 1 ms — far steeper than the fastest rising ramp (80 V/s at max speed), so
// only the discontinuities are rounded; the ramp itself is untouched.
static constexpr float POS_SLEW_RATE = 10000.f;   // V/s

// Per-lane friction is viscous: each sample v -= k·v·dt, so a held pedal
// settles at a terminal speed (pedal accel / k) instead of running to the
// clamp — the pedals become speed setpoints. The coefficient per level 0-8
// (0 = no friction = an eternal flywheel, the default) is geometric, so the
// low settings are very gentle and it ramps to a firm stop at the top.
static constexpr float FRICTION_K[9] = {
    0.f, 0.125f, 0.186f, 0.276f, 0.410f, 0.610f, 0.906f, 1.346f, 2.f
};

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
    float vel[8] = {};        // traversals per second, clamped to the lane range
    bool  padHeld[64] = {};
    bool  bidir[8] = {};      // false = monodirectional (clamp at 0), true = reverse OK
    int   friction[8] = {};   // viscous friction level 0-8 per lane (0 = none)

    int   subPage  = 0;       // 0 = play, 1 = direction, 2 = friction
    float maxSpeed = 2.f;     // traversals/s (= Hz of the POS sawtooth)
    bool  wasMoving[8] = {};

    bool  declick    = true;  // slew POS to soften the sawtooth wrap edge
    float posOut[8]  = {};    // slewed POS output, volts
    bool  absVel     = false; // VEL emits |speed| (0-10V) instead of signed

    uint8_t cursorColor       = P64::LED_GREEN;
    uint8_t pedalColor        = P64::LED_AMBER;
    uint8_t activePageColor   = P64::LED_GREEN;
    uint8_t inactivePageColor = P64::LED_AMBER_DIM;

    Inertia64() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configOutput(POS_OUTPUT, "Position ramp (poly 8ch, 0-10V)");
        configOutput(VEL_OUTPUT, "Signed speed (poly 8ch, ±10V)");
    }

    void onReset() override {
        PageModule::onReset();
        memset(pos, 0, sizeof(pos));
        memset(vel, 0, sizeof(vel));
        memset(padHeld, 0, sizeof(padHeld));
        memset(bidir, 0, sizeof(bidir));
        memset(friction, 0, sizeof(friction));
        memset(wasMoving, 0, sizeof(wasMoving));
        memset(posOut, 0, sizeof(posOut));
        subPage     = 0;
        declick     = true;
        absVel      = false;
        maxSpeed    = 2.f;
        cursorColor       = P64::LED_GREEN;
        pedalColor        = P64::LED_AMBER;
        activePageColor   = P64::LED_GREEN;
        inactivePageColor = P64::LED_AMBER_DIM;
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
            float up = 0.f, down = 0.f;
            for (int r = 0; r < 4; r++) {
                float rate = maxSpeed / PEDAL_TIME[r];
                if (padHeld[r * 8 + col])        // rows 0-3: push up (top = hardest)
                    up = std::max(up, rate);
                if (padHeld[(7 - r) * 8 + col])  // rows 7-4: push down (bottom = hardest)
                    down = std::max(down, rate);
            }
            float v = vel[col] + (up - down) * sampleTime;
            // Viscous friction: decay toward 0, so a held pedal cruises at a
            // terminal speed rather than running to the clamp.
            v -= FRICTION_K[friction[col]] * v * sampleTime;
            // Monodirectional lanes brake at 0; bidirectional lanes reverse.
            float lo = bidir[col] ? -maxSpeed : 0.f;
            v = clamp(v, lo, maxSpeed);
            // Let a damped, unpedaled lane actually come to rest.
            if (friction[col] > 0 && up == 0.f && down == 0.f
                    && std::abs(v) < 1e-3f * maxSpeed)
                v = 0.f;
            vel[col] = v;
            pos[col] += vel[col] * sampleTime;
            pos[col] -= std::floor(pos[col]);   // wraps either direction

            // The scene LEDs mirror moving state; push the transitions.
            bool moving = vel[col] != 0.f;
            if (moving != wasMoving[col]) {
                wasMoving[col] = moving;
                ledsDirty = true;
            }
        }
    }

    void pageActive(const P64::LeftMessage& msg) override {
        // Sub-page switching (top buttons 1-3: play, direction, friction).
        // Leaving the play page releases all held pedals so none stick.
        for (int b = 0; b < 3; b++) {
            int cc = 104 + b;
            if (msg.ccEvent[cc] && msg.ccValue[cc] > 0 && subPage != b) {
                subPage = b;
                memset(padHeld, 0, sizeof(padHeld));
                ledsDirty = true;
            }
        }

        if (subPage == 0)
            playPage(msg);
        else if (subPage == 1)
            directionPage(msg);
        else
            frictionPage(msg);
    }

    void playPage(const P64::LeftMessage& msg) {
        // Scene A-H, two-stage per column: tap a moving column to handbrake
        // it (stop, position frozen); tap it again while stopped to send it
        // home to the bottom (position 0).
        for (int i = 0; i < 8; i++) {
            if (msg.sceneEvent[i] && msg.sceneVelocity[i] > 0) {
                if (vel[i] != 0.f)
                    vel[i] = 0.f;
                else
                    pos[i] = 0.f;
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

    void directionPage(const P64::LeftMessage& msg) {
        // Tap anywhere in a column to toggle mono/bidirectional.
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                int note = row * 16 + col;
                if (msg.noteEvent[note] && msg.noteVelocity[note] > 0) {
                    bidir[col] = !bidir[col];
                    ledsDirty  = true;
                }
            }
        }
    }

    void frictionPage(const P64::LeftMessage& msg) {
        // Bar per column: tap a row to set friction to that height (bottom = 1,
        // top = 8); tap the current top of the bar again to clear to 0.
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                int note = row * 16 + col;
                if (msg.noteEvent[note] && msg.noteVelocity[note] > 0) {
                    int h = 8 - row;
                    friction[col] = (friction[col] == h) ? 0 : h;
                    ledsDirty = true;
                }
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
        uint8_t next[64];
        if (subPage == 1)
            buildDirectionLeds(next);
        else if (subPage == 2)
            buildFrictionLeds(next);
        else
            buildPlayLeds(next);
        for (int i = 0; i < 64; i++) {
            if (next[i] != ledState[i]) {
                ledState[i] = next[i];
                ledsDirty   = true;
            }
        }
    }

    void buildPlayLeds(uint8_t next[64]) {
        for (int col = 0; col < 8; col++) {
            int rowFromBottom = clamp((int)std::floor(pos[col] * 8.f), 0, 7);
            int cursorRow     = 7 - rowFromBottom;   // climbs the column, wraps
            for (int row = 0; row < 8; row++) {
                int idx = row * 8 + col;
                next[idx] = padHeld[idx]       ? pedalColor
                          : (row == cursorRow) ? cursorColor
                          :                      P64::LED_OFF;
            }
        }
    }

    // Direction page: top cell lit = goes up; top + bottom cells lit = goes
    // both ways (bidirectional).
    void buildDirectionLeds(uint8_t next[64]) {
        memset(next, P64::LED_OFF, 64);
        for (int col = 0; col < 8; col++) {
            next[0 * 8 + col] = cursorColor;
            if (bidir[col])
                next[7 * 8 + col] = cursorColor;
        }
    }

    // Friction page: a bar from the bottom, height = the friction level.
    void buildFrictionLeds(uint8_t next[64]) {
        memset(next, P64::LED_OFF, 64);
        for (int col = 0; col < 8; col++)
            for (int i = 0; i < friction[col]; i++)
                next[(7 - i) * 8 + col] = cursorColor;
    }

    void buildSceneLeds(uint8_t sceneLeds[8]) override {
        for (int i = 0; i < 8; i++)
            sceneLeds[i] = vel[i] != 0.f ? cursorColor : P64::LED_OFF;
    }

    void buildTopLeds(uint8_t topLeds[8]) override {
        memset(topLeds, P64::LED_OFF, 8);
        for (int b = 0; b < 3; b++)
            topLeds[b] = (b == subPage) ? activePageColor : inactivePageColor;
    }

    void updateOutputs() override {
        outputs[POS_OUTPUT].setChannels(8);
        outputs[VEL_OUTPUT].setChannels(8);
        float maxStep = POS_SLEW_RATE * sampleTime;
        for (int col = 0; col < 8; col++) {
            float target = pos[col] * 10.f;
            if (declick) {
                float diff = target - posOut[col];
                posOut[col] += std::abs(diff) <= maxStep ? diff
                             : (diff > 0.f ? maxStep : -maxStep);
            } else {
                posOut[col] = target;
            }
            outputs[POS_OUTPUT].setVoltage(posOut[col], col);
            float v = absVel ? std::abs(vel[col]) : vel[col];
            outputs[VEL_OUTPUT].setVoltage(v / maxSpeed * 10.f, col);
        }
    }

    // ── serialisation ─────────────────────────────────────────────────────────

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_t* jp = json_array();
        json_t* jv = json_array();
        json_t* jb = json_array();
        json_t* jf = json_array();
        for (int i = 0; i < 8; i++) {
            json_array_append_new(jp, json_real(pos[i]));
            json_array_append_new(jv, json_real(vel[i]));
            json_array_append_new(jb, json_boolean(bidir[i]));
            json_array_append_new(jf, json_integer(friction[i]));
        }
        json_object_set_new(root, "pos",               jp);
        json_object_set_new(root, "vel",               jv);
        json_object_set_new(root, "bidir",             jb);
        json_object_set_new(root, "friction",          jf);
        json_object_set_new(root, "subPage",           json_integer(subPage));
        json_object_set_new(root, "maxSpeed",          json_real(maxSpeed));
        json_object_set_new(root, "declick",           json_boolean(declick));
        json_object_set_new(root, "absVel",            json_boolean(absVel));
        json_object_set_new(root, "cursorColor",       json_integer(cursorColor));
        json_object_set_new(root, "pedalColor",        json_integer(pedalColor));
        json_object_set_new(root, "activePageColor",   json_integer(activePageColor));
        json_object_set_new(root, "inactivePageColor", json_integer(inactivePageColor));
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
                if (v) vel[i] = clamp((float)json_real_value(v), -maxSpeed, maxSpeed);
            }
        if ((j = json_object_get(root, "bidir")))
            for (int i = 0; i < 8; i++) {
                json_t* v = json_array_get(j, i);
                if (v) bidir[i] = json_boolean_value(v);
            }
        if ((j = json_object_get(root, "friction")))
            for (int i = 0; i < 8; i++) {
                json_t* v = json_array_get(j, i);
                if (v) friction[i] = clamp((int)json_integer_value(v), 0, 8);
            }
        if ((j = json_object_get(root, "subPage")))
            subPage = clamp((int)json_integer_value(j), 0, 2);
        if ((j = json_object_get(root, "declick")))
            declick = json_boolean_value(j);
        if ((j = json_object_get(root, "absVel")))
            absVel = json_boolean_value(j);
        for (int i = 0; i < 8; i++) {  // start the slewed output at the restored position
            posOut[i] = pos[i] * 10.f;
            if (!bidir[i]) vel[i] = std::max(vel[i], 0.f);
        }
        if ((j = json_object_get(root, "cursorColor")))
            cursorColor = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "pedalColor")))
            pedalColor = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "activePageColor")))
            activePageColor = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "inactivePageColor")))
            inactivePageColor = (uint8_t)json_integer_value(j);
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
        menu->addChild(createBoolPtrMenuItem("Declick POS output (1 ms slew)", "",
                                             &m->declick));
        menu->addChild(createBoolPtrMenuItem("Absolute VEL (0-10V both directions)", "",
                                             &m->absVel));
        P64::appendColorMenu(menu, m, "Cursor color",        &m->cursorColor);
        P64::appendColorMenu(menu, m, "Pedal color",         &m->pedalColor);
        P64::appendColorMenu(menu, m, "Active page color",   &m->activePageColor);
        P64::appendColorMenu(menu, m, "Inactive page color", &m->inactivePageColor);
    }
};

Model* modelInertia64 = createModel<Inertia64, Inertia64Widget>("Inertia64");
