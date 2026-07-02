#include "PageModule.hpp"

// ── Bounce64 ──────────────────────────────────────────────────────────────────
// Bouncing-ball rhythm machine (Tenori-on bounce mode / boiingg). Each column
// holds one ball: press a pad in rows A-G to drop it from that height (row G =
// 1, row A = 7). The ball falls one row per clock tick, fires a trigger when it
// hits the floor, and climbs back to its apex — so the period is 2 × height
// ticks. Tapping a new height re-drops the ball immediately (dribbling); the
// bottom row (H) removes the ball. Scene buttons A-H mute columns 1-8: a muted
// ball keeps bouncing in its mute color so it re-enters in phase.

struct Bounce64 : PageModule {
    enum ParamIds { NUM_PARAMS };
    enum InputIds  { NUM_INPUTS };
    enum OutputIds {
        ENUMS(TRIG_OUTPUT, 8),
        POLY_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        ENUMS(ACTIVE_LIGHT, 2),
        NUM_LIGHTS
    };

    // Per-column ball state
    int  height[8] = {};   // apex height in rows, 1-7 (0 = no ball)
    int  phase[8]  = {};   // 0 .. 2*height-1; 0 = apex, height = floor hit
    bool muted[8]  = {};

    P64::ClockDivider clockDiv;
    dsp::PulseGenerator trigPulse[8];

    // Colors
    uint8_t ballColor = P64::LED_GREEN;
    uint8_t apexColor = P64::LED_GREEN_DIM;
    uint8_t hitColor  = P64::LED_AMBER;
    uint8_t muteColor = P64::LED_RED;

    Bounce64() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (int i = 0; i < 8; i++)
            configOutput(TRIG_OUTPUT + i, string::f("Trigger %d", i + 1));
        configOutput(POLY_OUTPUT, "Poly triggers (8-channel)");
    }

    void onReset() override {
        PageModule::onReset();
        memset(height, 0, sizeof(height));
        memset(phase,  0, sizeof(phase));
        memset(muted,  0, sizeof(muted));
        clockDiv.set(1);
        ballColor = P64::LED_GREEN;
        apexColor = P64::LED_GREEN_DIM;
        hitColor  = P64::LED_AMBER;
        muteColor = P64::LED_RED;
    }

    // Ball altitude in rows above the floor at the current phase.
    int ballPos(int c) const {
        return (phase[c] <= height[c]) ? height[c] - phase[c]
                                       : phase[c] - height[c];
    }

    // ── virtual hooks ─────────────────────────────────────────────────────────

    void pagePreProcess() override {
        auto* msg = reinterpret_cast<P64::LeftMessage*>(leftExpander.consumerMessage);
        if (!msg) return;

        if (msg->resetTick) {
            memset(phase, 0, sizeof(phase));   // re-drop every ball from its apex
            clockDiv.reset();
            ledsDirty = true;
        }

        if (clockDiv.process(msg->clockTick)) {
            for (int c = 0; c < 8; c++) {
                if (height[c] <= 0) continue;
                phase[c] = (phase[c] + 1) % (2 * height[c]);
                if (phase[c] == height[c] && !muted[c])
                    trigPulse[c].trigger(0.005f);
            }
            ledsDirty = true;
        }
    }

    void pageActive(const P64::LeftMessage& msg) override {
        // Scene buttons A-H: mute toggles for columns 1-8
        for (int e = 0; e < msg.eventCount; e++) {
            const P64::GridEvent& ev = msg.events[e];
            if (ev.type == P64::GridEvent::SCENE && ev.value > 0) {
                muted[ev.index] = !muted[ev.index];
                ledsDirty = true;
            }
        }

        for (int e = 0; e < msg.eventCount; e++) {
            const P64::GridEvent& ev = msg.events[e];
            if (ev.type == P64::GridEvent::PAD && ev.value > 0) {
                int row = ev.index / 8;
                int col = ev.index % 8;

                if (row < 7) {
                    height[col] = 7 - row;   // row A = 7 … row G = 1
                    phase[col]  = 0;         // drop from the apex
                } else {
                    height[col] = 0;         // row H: remove the ball
                    phase[col]  = 0;
                }
                ledsDirty = true;
            }
        }
    }

    void rebuildLeds() override {
        memset(ledState, P64::LED_OFF, sizeof(ledState));
        for (int c = 0; c < 8; c++) {
            if (height[c] <= 0) continue;
            ledState[(7 - height[c]) * 8 + c] = apexColor;   // apex marker
            int pos = ballPos(c);
            uint8_t color = muted[c] ? muteColor
                          : (pos == 0 ? hitColor : ballColor);
            ledState[(7 - pos) * 8 + c] = color;
        }
    }

    void buildSceneLeds(uint8_t sceneLeds[8]) override {
        memset(sceneLeds, P64::LED_OFF, 8);
        for (int i = 0; i < 8; i++) {
            if (muted[i])
                sceneLeds[i] = muteColor;
            else if (height[i] > 0)
                sceneLeds[i] = ballColor;
        }
    }

    void updateOutputs() override {
        outputs[POLY_OUTPUT].setChannels(8);
        for (int c = 0; c < 8; c++) {
            float v = trigPulse[c].process(sampleTime) ? 10.f : 0.f;
            outputs[TRIG_OUTPUT + c].setVoltage(v);
            outputs[POLY_OUTPUT].setVoltage(v, c);
        }
    }

    // ── serialisation ─────────────────────────────────────────────────────────

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "clockDiv",  json_integer(clockDiv.div));
        json_object_set_new(root, "ballColor", json_integer(ballColor));
        json_object_set_new(root, "apexColor", json_integer(apexColor));
        json_object_set_new(root, "hitColor",  json_integer(hitColor));
        json_object_set_new(root, "muteColor", json_integer(muteColor));
        json_t* jh = json_array();
        json_t* jm = json_array();
        for (int i = 0; i < 8; i++) {
            json_array_append_new(jh, json_integer(height[i]));
            json_array_append_new(jm, json_boolean(muted[i]));
        }
        json_object_set_new(root, "height", jh);
        json_object_set_new(root, "muted",  jm);
        return root;
    }

    void dataFromJson(json_t* root) override {
        json_t* j;
        if ((j = json_object_get(root, "clockDiv")))
            clockDiv.set(clamp((int)json_integer_value(j), 1, 64));
        if ((j = json_object_get(root, "ballColor")))
            ballColor = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "apexColor")))
            apexColor = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "hitColor")))
            hitColor = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "muteColor")))
            muteColor = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "height")))
            for (int i = 0; i < 8; i++) {
                json_t* v = json_array_get(j, i);
                if (v) height[i] = clamp((int)json_integer_value(v), 0, 7);
            }
        if ((j = json_object_get(root, "muted")))
            for (int i = 0; i < 8; i++) {
                json_t* v = json_array_get(j, i);
                if (v) muted[i] = json_boolean_value(v);
            }
        memset(phase, 0, sizeof(phase));
        ledsDirty = true;
    }
};

// ── Widget ────────────────────────────────────────────────────────────────────

struct Bounce64Widget : ModuleWidget {
    Bounce64Widget(Bounce64* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Bounce64.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addChild(createLightCentered<SmallLight<GreenRedLight>>(
            mm2px(Vec(6.0f, 18.0f)), module, Bounce64::ACTIVE_LIGHT));

        const float trigY[8] = {24.f, 34.f, 44.f, 54.f, 64.f, 74.f, 84.f, 94.f};
        for (int i = 0; i < 8; i++) {
            addOutput(createOutputCentered<PJ301MPort>(
                mm2px(Vec(20.0f, trigY[i])), module, Bounce64::TRIG_OUTPUT + i));
        }

        addOutput(createOutputCentered<PJ301MPort>(
            mm2px(Vec(20.0f, 108.0f)), module, Bounce64::POLY_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        Bounce64* m = getModule<Bounce64>();
        menu->addChild(new MenuSeparator);
        P64::appendClockDivMenu(menu, &m->clockDiv);
        P64::appendColorMenu(menu, m, "Ball color",       &m->ballColor);
        P64::appendColorMenu(menu, m, "Apex color",       &m->apexColor, true);
        P64::appendColorMenu(menu, m, "Floor hit color",  &m->hitColor);
        P64::appendColorMenu(menu, m, "Mute color",       &m->muteColor);
    }
};

Model* modelBounce64 = createModel<Bounce64, Bounce64Widget>("Bounce64");
