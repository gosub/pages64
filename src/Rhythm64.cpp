#include "PageModule.hpp"

// ── Rhythm64 ──────────────────────────────────────────────────────────────────
// Generative rhythm engine: every pad owns a fixed random rhythm derived from
// (seed, pad). Hold a pad and its rhythm plays on the divided clock; scene A
// switches to latch mode. Row sets density and beat-bias (top busy, bottom
// sparse and on the beat); columns are siblings. Full design: Drums64.md.

static const int RHY_LEN_CHOICES[3] = {8, 16, 32};

struct Rhythm64 : PageModule {
    enum ParamIds { NUM_PARAMS };
    enum InputIds  { NUM_INPUTS };
    enum OutputIds {
        ENUMS(CELL_OUTPUT, 4),   // rows 1-2 / 3-4 / 5-6 / 7-8, 16ch poly each
        NUM_OUTPUTS
    };
    enum LightIds {
        ENUMS(ACTIVE_LIGHT, 2),
        NUM_LIGHTS
    };

    uint32_t seed      = 0x64726d73;  // kit identity; reroll draws a new one
    uint32_t pattern[64] = {};        // bit s = hit at step s
    bool     held[64]    = {};
    bool     latched[64] = {};
    bool     latchMode   = false;
    int      lenIndex    = 1;         // 16 steps
    int      stepPos     = -1;        // pre-first-tick; first tick plays step 0
    float    flash[64]   = {};        // hit flash timers (LED only)
    uint8_t  armColor    = P64::LED_GREEN_DIM;
    uint8_t  hitColor    = P64::LED_GREEN;
    P64::ClockDivider clockDiv;
    dsp::PulseGenerator pulse[64];

    Rhythm64() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (int i = 0; i < 4; i++)
            configOutput(CELL_OUTPUT + i,
                string::f("Rows %d-%d triggers (poly 16ch)", i * 2 + 1, i * 2 + 2));
        regenPatterns();
    }

    void onReset() override {
        PageModule::onReset();
        memset(held,    0, sizeof(held));
        memset(latched, 0, sizeof(latched));
        memset(flash,   0, sizeof(flash));
        latchMode = false;
        lenIndex  = 1;
        stepPos   = -1;
        clockDiv.set(1);
        armColor  = P64::LED_GREEN_DIM;
        hitColor  = P64::LED_GREEN;
        seed      = 0x64726d73;   // initialize = the factory rhythms
        regenPatterns();
    }

    // ── pattern generation ────────────────────────────────────────────────────

    static uint32_t xorshift(uint32_t& s) {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return s;
    }

    void regenPatterns() {
        for (int pad = 0; pad < 64; pad++) {
            uint32_t rng = seed ^ (uint32_t)(pad * 2654435761u);
            for (int i = 0; i < 4; i++) xorshift(rng);   // decorrelate

            int row = pad / 8;
            // Density: top row ~12/16, bottom row ~2/16.
            float density = (12.f - row * 10.f / 7.f) / 16.f;
            // Beat bias: bottom rows pull their hits onto every 4th step.
            float bias = row / 7.f;

            uint32_t bits = 0;
            for (int s = 0; s < 32; s++) {
                float p = density;
                p *= (s % 4 == 0) ? (1.f + 3.f * bias) : (1.f - bias);
                if ((xorshift(rng) >> 8) / 16777216.f < p)
                    bits |= (1u << s);
            }
            if (!bits)
                bits = 1u << (pad % 16);   // no silent pads
            pattern[pad] = bits;
        }
        memset(flash, 0, sizeof(flash));
    }

    bool armed(int pad) const { return latchMode ? latched[pad] : held[pad]; }

    // ── virtual hooks ─────────────────────────────────────────────────────────

    void pagePreProcess() override {
        auto* msg = reinterpret_cast<P64::LeftMessage*>(leftExpander.consumerMessage);
        if (msg) {
            if (msg->resetTick) {
                stepPos = -1;
                clockDiv.reset();
            }
            if (clockDiv.process(msg->clockTick)) {
                stepPos = (stepPos + 1) % RHY_LEN_CHOICES[lenIndex];
                for (int pad = 0; pad < 64; pad++) {
                    if (armed(pad) && (pattern[pad] >> stepPos & 1)) {
                        pulse[pad].trigger(5e-3f);
                        flash[pad] = 0.06f;
                        ledsDirty  = true;
                    }
                }
            }
        }
        for (int pad = 0; pad < 64; pad++) {
            if (flash[pad] > 0.f) {
                flash[pad] -= sampleTime;
                if (flash[pad] <= 0.f) ledsDirty = true;
            }
        }
    }

    void pageActive(const P64::LeftMessage& msg) override {
        for (int e = 0; e < msg.eventCount; e++) {
            const P64::GridEvent& ev = msg.events[e];
            if (ev.type == P64::GridEvent::SCENE && ev.index == 0 && ev.value > 0) {
                latchMode = !latchMode;
                memset(latched, 0, sizeof(latched));   // both directions start silent
                memset(held,    0, sizeof(held));
                ledsDirty = true;
            } else if (ev.type == P64::GridEvent::PAD) {
                if (latchMode) {
                    if (ev.value > 0) {
                        latched[ev.index] = !latched[ev.index];
                        ledsDirty = true;
                    }
                } else {
                    held[ev.index] = ev.value > 0;
                    ledsDirty = true;
                }
            }
        }
    }

    void pageInactive() override {
        // Held pads release when you leave the page; latched ones keep playing.
        for (int i = 0; i < 64; i++) {
            if (held[i]) {
                held[i]   = false;
                ledsDirty = true;
            }
        }
    }

    void rebuildLeds() override {
        for (int i = 0; i < 64; i++) {
            uint8_t color = (flash[i] > 0.f) ? hitColor
                          : armed(i)         ? armColor
                          :                    P64::LED_OFF;
            if (color != ledState[i]) {
                ledState[i] = color;
                ledsDirty   = true;
            }
        }
    }

    void buildSceneLeds(uint8_t sceneLeds[8]) override {
        memset(sceneLeds, P64::LED_OFF, 8);
        if (latchMode)
            sceneLeds[0] = hitColor;
    }

    void updateOutputs() override {
        for (int out = 0; out < 4; out++) {
            outputs[CELL_OUTPUT + out].setChannels(16);
            for (int ch = 0; ch < 16; ch++) {
                int pad = out * 16 + ch;
                outputs[CELL_OUTPUT + out].setVoltage(
                    pulse[pad].process(sampleTime) ? 10.f : 0.f, ch);
            }
        }
    }

    // ── serialisation ─────────────────────────────────────────────────────────

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "seed",      json_integer((json_int_t) seed));
        json_object_set_new(root, "latchMode", json_boolean(latchMode));
        json_object_set_new(root, "lenIndex",  json_integer(lenIndex));
        json_object_set_new(root, "clockDiv",  json_integer(clockDiv.div));
        json_object_set_new(root, "armColor",  json_integer(armColor));
        json_object_set_new(root, "hitColor",  json_integer(hitColor));
        json_t* jl = json_array();
        for (int i = 0; i < 64; i++)
            json_array_append_new(jl, json_boolean(latched[i]));
        json_object_set_new(root, "latched", jl);
        return root;
    }

    void dataFromJson(json_t* root) override {
        json_t* j;
        if ((j = json_object_get(root, "seed"))) {
            seed = (uint32_t) json_integer_value(j);
            regenPatterns();
        }
        if ((j = json_object_get(root, "latchMode")))
            latchMode = json_boolean_value(j);
        if ((j = json_object_get(root, "lenIndex")))
            lenIndex = clamp((int) json_integer_value(j), 0, 2);
        if ((j = json_object_get(root, "clockDiv")))
            clockDiv.set(clamp((int) json_integer_value(j), 1, 64));
        if ((j = json_object_get(root, "armColor")))
            armColor = (uint8_t) json_integer_value(j);
        if ((j = json_object_get(root, "hitColor")))
            hitColor = (uint8_t) json_integer_value(j);
        if ((j = json_object_get(root, "latched")))
            for (int i = 0; i < 64; i++) {
                json_t* v = json_array_get(j, i);
                if (v) latched[i] = json_boolean_value(v);
            }
        ledsDirty = true;
    }
};

// ── Widget ────────────────────────────────────────────────────────────────────

struct Rhythm64Widget : ModuleWidget {
    Rhythm64Widget(Rhythm64* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Rhythm64.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addChild(createLightCentered<SmallLight<GreenRedLight>>(
            mm2px(Vec(6.0f, 18.0f)), module, Rhythm64::ACTIVE_LIGHT));

        for (int i = 0; i < 4; i++)
            addOutput(createOutputCentered<PJ301MPort>(
                mm2px(Vec(20.f, 45.f + i * 15.f)), module, Rhythm64::CELL_OUTPUT + i));
    }

    void appendContextMenu(Menu* menu) override {
        Rhythm64* m = getModule<Rhythm64>();
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuItem("Reroll rhythms", "",
            [=]() { m->seed = random::u32(); m->regenPatterns(); m->ledsDirty = true; }));
        menu->addChild(createIndexSubmenuItem("Pattern length",
            {"8 steps", "16 steps", "32 steps"},
            [=]() { return m->lenIndex; },
            [=](int v) { m->lenIndex = v; m->stepPos = -1; }));
        P64::appendClockDivMenu(menu, &m->clockDiv);
        P64::appendColorMenu(menu, m, "Armed color", &m->armColor);
        P64::appendColorMenu(menu, m, "Hit color",   &m->hitColor);
    }
};

Model* modelRhythm64 = createModel<Rhythm64, Rhythm64Widget>("Rhythm64");
