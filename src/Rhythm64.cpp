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

    // Punch-in FX (hold scene B → grid = effect selector; rows = effects,
    // columns = amount; momentary, readout-only, nothing serialized).
    // Full design: docs/design/PunchIn64.md.
    bool     fxHeld     = false;   // scene B held
    int      fxCell     = -1;      // active effect pad (-1 = none)
    int      fxAnchor   = 0;       // stepPos when the effect pad was pressed
    int      fxTicks    = 0;       // divided ticks since the pad press
    int      fxSuppress = -1;      // drag: step already pre-fired early
    float    fxPeriod   = 0.5f;    // seconds per divided tick (from clockPeriod)
    uint32_t fxRng      = 0x9d2c5680u;   // free-running (thin, shuffle)

    // Sub-step scheduler: pending fires in samples (ratchet, ×n, push/drag)
    static constexpr int FX_QUEUE = 32;   // ≥ the ×24 ratchet's 23 sub-hits
    struct FxPending { int samples; uint8_t step; };
    FxPending fxQueue[FX_QUEUE];
    int fxQueueN = 0;

    float frnd() {
        fxRng ^= fxRng << 13; fxRng ^= fxRng >> 17; fxRng ^= fxRng << 5;
        return (fxRng >> 8) / 16777216.f;
    }

    void fxEnqueue(float sec, int step) {
        if (fxQueueN >= FX_QUEUE) return;
        fxQueue[fxQueueN].samples = std::max(1, (int)(sec / sampleTime));
        fxQueue[fxQueueN].step    = (uint8_t) step;
        fxQueueN++;
    }

    void fxClear() {
        fxQueueN   = 0;
        fxSuppress = -1;
    }

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
        fxHeld    = false;
        fxCell    = -1;
        fxClear();
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

    // ── punch-in engine ───────────────────────────────────────────────────────
    // The step counter runs untouched in global time; effects only transform
    // what gets read and when, so releasing lands back exactly on the grid.

    // Fire one readout step, through the per-hit effects (density, mask).
    void fireStep(int step) {
        int len = RHY_LEN_CHOICES[lenIndex];
        int row = fxCell >= 0 ? fxCell / 8 : -1;
        int col = fxCell >= 0 ? fxCell % 8 : 0;
        static const float KEEP[4] = {0.15f, 0.33f, 0.55f, 0.75f};

        for (int pad = 0; pad < 64; pad++) {
            if (!armed(pad)) continue;
            int prow = pad / 8;
            if (row == 4) {   // mask: which row bands pass
                bool ok = col == 0 ? prow == 7
                        : col == 1 ? prow >= 6
                        : col == 2 ? prow >= 5
                        : col == 3 ? prow >= 4
                        : col == 4 ? prow <= 3
                        : col == 5 ? prow <= 2
                        : col == 6 ? prow <= 1
                        :            prow == 0;
                if (!ok) continue;
            }
            bool hit = pattern[pad] >> step & 1;
            if (row == 3) {   // density: thin ← → fill
                if (col < 4) {
                    if (hit && frnd() > KEEP[col]) hit = false;
                } else {
                    int copies = col - 3;   // 1..4 shifted copies OR-ed in
                    const int offs[4] = {len / 2, len / 4, 3 * len / 4, 1};
                    for (int c = 0; c < copies && !hit; c++)
                        hit = pattern[pad] >> ((step + offs[c]) % len) & 1;
                }
            }
            if (hit) {
                pulse[pad].trigger(5e-3f);
                flash[pad] = 0.06f;
                ledsDirty  = true;
            }
        }
    }

    // One divided clock tick, routed through the time-domain effects.
    void onTick() {
        int len = RHY_LEN_CHOICES[lenIndex];
        int row = fxCell >= 0 ? fxCell / 8 : -1;
        int col = fxCell >= 0 ? fxCell % 8 : 0;
        if (fxCell >= 0) fxTicks++;
        float P = fxPeriod;

        switch (row) {
            case 0: {   // loop: roll the last n steps, window ending at the anchor
                static const int LOOP[8] = {1, 2, 3, 4, 6, 8, 12, 16};
                int n = LOOP[col];
                fireStep(((fxAnchor - n + 1 + (fxTicks - 1) % n) % len + len) % len);
                break;
            }
            case 1: {   // ratchet: the step's hits become k sub-hits
                static const int RATCH[8] = {2, 3, 4, 6, 8, 12, 16, 24};
                int k = RATCH[col];
                fireStep(stepPos);
                for (int i = 1; i < k; i++)
                    fxEnqueue(P * i / k, stepPos);
                break;
            }
            case 2: {   // time: ÷4 ÷3 ÷2 · reverse · ×2 ×3 ×4 ×8
                if (col <= 2) {
                    int n = 4 - col;   // ÷4, ÷3, ÷2
                    if ((fxTicks - 1) % n == 0)
                        fireStep((fxAnchor + (fxTicks - 1) / n + 1) % len);
                }
                else if (col == 3) {   // reverse
                    fireStep(((fxAnchor - fxTicks) % len + len) % len);
                }
                else {   // ×n: readout races ahead with sub-tick steps
                    static const int MUL[4] = {2, 3, 4, 8};
                    int n = MUL[col - 4];
                    int base = fxAnchor + (fxTicks - 1) * n + 1;
                    fireStep(base % len);
                    for (int i = 1; i < n; i++)
                        fxEnqueue(P * i / n, (base + i) % len);
                }
                break;
            }
            case 5: {   // shuffle: shared slice index — verticality survives
                static const int WIN[8] = {2, 3, 4, 6, 8, 12, 16, 32};
                int w   = std::min(WIN[col], len);
                int off = (int)(frnd() * w);
                fireStep(((stepPos - off) % len + len) % len);
                break;
            }
            case 6: {   // push/drag: hits late (right) or early (left)
                static const float AMT[8] =
                    {0.45f, 0.33f, 0.2f, 0.1f, 0.1f, 0.2f, 0.33f, 0.45f};
                float f = AMT[col];
                if (col >= 4) {   // push: whole step fires late
                    fxEnqueue(f * P, stepPos);
                }
                else {   // drag: pre-fire the next step early, skip its tick
                    if (stepPos != fxSuppress)
                        fireStep(stepPos);
                    fxSuppress = (stepPos + 1) % len;
                    fxEnqueue((1.f - f) * P, fxSuppress);
                }
                break;
            }
            default:    // no effect, density (3), mask (4), spare row (7)
                fireStep(stepPos);
                break;
        }
    }

    // ── virtual hooks ─────────────────────────────────────────────────────────

    void pagePreProcess() override {
        auto* msg = reinterpret_cast<P64::LeftMessage*>(leftExpander.consumerMessage);
        if (msg) {
            if (msg->clockPeriod > 0.f)
                fxPeriod = msg->clockPeriod * clockDiv.div;
            if (msg->resetTick) {
                stepPos = -1;
                clockDiv.reset();
                fxClear();
            }
            if (clockDiv.process(msg->clockTick)) {
                stepPos = (stepPos + 1) % RHY_LEN_CHOICES[lenIndex];
                onTick();
            }
        }
        // drain the sub-step scheduler
        for (int q = 0; q < fxQueueN; ) {
            if (--fxQueue[q].samples <= 0) {
                fireStep(fxQueue[q].step);
                fxQueue[q] = fxQueue[--fxQueueN];
            } else
                q++;
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
            } else if (ev.type == P64::GridEvent::SCENE && ev.index == 1) {
                fxHeld = ev.value > 0;   // punch-in selector while held
                if (!fxHeld) {
                    fxCell = -1;
                    fxClear();
                }
                ledsDirty = true;
            } else if (ev.type == P64::GridEvent::PAD) {
                if (fxHeld) {
                    // grid is the effect selector; pads don't arm/disarm
                    if (ev.value > 0) {
                        if (ev.index / 8 != 7) {   // row 8 = spare, inert
                            fxCell   = ev.index;
                            fxAnchor = stepPos < 0 ? 0 : stepPos;
                            fxTicks  = 0;
                            fxClear();
                            ledsDirty = true;
                        }
                    } else if (ev.index == fxCell) {
                        fxCell = -1;
                        fxClear();
                        ledsDirty = true;
                    }
                } else if (latchMode) {
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
        if (fxHeld || fxCell >= 0) {
            fxHeld = false;
            fxCell = -1;
            fxClear();
            ledsDirty = true;
        }
    }

    void rebuildLeds() override {
        for (int i = 0; i < 64; i++) {
            uint8_t color;
            if (fxHeld)   // effect selector overlay: amber map, bright pick
                color = (i == fxCell)  ? P64::LED_AMBER
                      : (i / 8 == 7)   ? P64::LED_OFF        // spare row
                      :                  P64::LED_AMBER_DIM;
            else
                color = (flash[i] > 0.f) ? hitColor
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
        if (fxHeld)
            sceneLeds[1] = P64::LED_AMBER;
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
        menu->addChild(createSubmenuItem("Colors", "", [=](Menu* sub) {
            P64::appendColorMenu(sub, m, "Armed", &m->armColor);
            P64::appendColorMenu(sub, m, "Hit",   &m->hitColor);
        }));
    }
};

Model* modelRhythm64 = createModel<Rhythm64, Rhythm64Widget>("Rhythm64");
