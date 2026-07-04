#include "KitModule.hpp"

// ── 64Micro ───────────────────────────────────────────────────────────────────
// Kit companion: single, designed micro-sounds (0.2–20 ms) on the KitModule
// shell — the deterministic temperament (Raster-Noton lineage: clicks, test
// tones, data buzzes, sub thumps). Charter: every trigger of a cell is
// bit-identical — voice rendering never calls random::uniform(); noise comes
// from a per-cell LFSR re-seeded identically at every strike, and the
// Variety extras are sequenced cycles (A-B-A-B), never probability rolls.
// 64Grains is stochastic clouds; 64Micro is single deterministic events.
// Nine-family catalog: eight default rows + Fold off-grid via Row families.
// Full design: docs/design/Micro64.md.

static constexpr int MIC_VOICES = 24;

struct Micro64 : KitModule {
    enum Variety {
        VAR_ALT  = 1 << 0,   // cycle 2–4 pitch variants, A-B-A-B
        VAR_PONG = 1 << 1,   // hits alternate hard L/R
        VAR_DROP = 1 << 2,   // every Nth hit is silence
        VAR_DBL  = 1 << 3,   // sample-exact second hit a few ms later
        VAR_ALL  = (1 << 4) - 1,
    };
    enum Family {
        FAM_CLICK, FAM_TICK, FAM_CRUSH, FAM_DATA,
        FAM_BLIP, FAM_ZAP, FAM_PING, FAM_THUMP,
        FAM_FOLD,            // catalog extra, off the default grid
        NUM_FAMILIES,
    };
    enum Window { WIN_RECT, WIN_EXP, WIN_HANN };

    // Per-cell recipe, fixed at kit generation
    struct Cell {
        int   fam;
        float f0;         // Hz: tone / bit rate / resonator (0 = unpitched)
        float f1;         // zap target, thump drop target, blip second tone
        float dur;        // event length (s)
        int   win;
        float winK;       // exp window steepness
        float lpFc;       // tick color (Hz)
        float t60;        // ping ring time (s)
        float crushHz;    // crush sample-hold rate
        float levels;     // crush quantize levels
        float foldK;      // fold depth
        bool  dual;       // blip: dual tone
        float pan;
        uint32_t nseed;   // LFSR seed, re-applied at every trigger
        // Variety recipe — always drawn, applied only when the toggle is on.
        int   altN;       // pitch variants in the cycle (VAR_ALT)
        float altOff[4];  // semitone offsets, [0] = 0
        bool  pong;       // (VAR_PONG)
        int   dropN;      // every Nth hit silent, 0 = clean (VAR_DROP)
        float dblS;       // doubler delay (s) (VAR_DBL)
        float dblGain;    // doubler level, 0 = clean (VAR_DBL)
        // runtime
        int   hits;       // free-running hit counter driving the cycles
    };
    Cell cells[64];

    struct Voice {
        bool  active = false;
        int   cell   = -1;
        int   delay  = 0;             // samples before the event starts
        float gain   = 1.f;
        float t = 0.f, dur = 1.f;
        float phase = 0.f, freq = 0.f, fMul = 1.f;
        float phase2 = 0.f, freq2 = 0.f;
        float re = 0.f, im = 0.f, rotRe = 0.f, rotIm = 0.f;   // ping phasor
        uint32_t lfsr = 1;
        float lp = 0.f, lpC = 0.f;
        float hold = 0.f; int holdN = 0, holdDiv = 1;
        int   bitN = 0, bitDiv = 1;   // data stream clock
        float ampL = 0.f, ampR = 0.f;
    };
    Voice voices[MIC_VOICES];

    Micro64() : KitModule(0x6d696372, VAR_ALL, NUM_FAMILIES) {
        mixGain = 6.f;
        regenKit();
    }

    void kitReset() override {
        for (auto& v : voices) v = Voice{};
        for (auto& c : cells) c.hits = 0;
    }

    // ── kit generation ────────────────────────────────────────────────────────

    void regenKit() override {
        for (int i = 0; i < 64; i++) {
            P64::KitRng rng(seed, i);
            auto rnd = [&]() { return rng.uni(); };
            float spread = (i % 8) / 7.f;
            int family = cellFamily(rng, i);
            float jitter = 0.85f + 0.3f * rnd();

            Cell& c = cells[i];
            int hits = c.hits;   // survives regen: the cycles keep phase
            c = Cell{};
            c.hits = hits;
            c.fam  = family;

            switch (family) {
                case FAM_CLICK:   // shaped micro-transient
                    c.f0   = (2000.f + 6000.f * spread) * jitter;
                    c.dur  = 0.0003f + 0.0022f * rnd();
                    c.win  = rnd() < 0.4f ? WIN_RECT : WIN_EXP;
                    c.winK = 5.f + 4.f * rnd();
                    break;
                case FAM_TICK:    // filtered noise impulse (unpitched)
                    c.lpFc = (2500.f + 9000.f * spread) * jitter;
                    c.dur  = 0.0004f + 0.002f * rnd();
                    c.win  = WIN_EXP;
                    c.winK = 6.f + 3.f * rnd();
                    break;
                case FAM_CRUSH:   // bit/rate artifact fragment
                    c.f0      = (250.f + 1400.f * spread) * jitter;
                    c.crushHz = 900.f + 3500.f * rnd();
                    c.levels  = 3.f + 10.f * rnd();
                    c.dur     = 0.003f + 0.007f * rnd();
                    c.win     = WIN_RECT;
                    break;
                case FAM_DATA:    // 1-bit LFSR bitstream burst; f0 = bit rate
                    c.f0  = (300.f + 2700.f * spread) * jitter;
                    c.dur = 0.006f + 0.04f * rnd();
                    c.win = WIN_RECT;
                    break;
                case FAM_BLIP:    // rect-gated sine burst, optional dual tone
                    c.f0   = (300.f + 2700.f * spread) * jitter;
                    c.dual = rnd() < 0.35f;
                    c.f1   = c.f0 * (1.2f + 1.3f * rnd());
                    c.dur  = 0.004f + 0.016f * rnd();
                    c.win  = WIN_RECT;
                    break;
                case FAM_ZAP: {   // ms-scale pitch sweep
                    c.f0 = (1200.f + 4000.f * spread) * jitter;
                    float rise = rnd();
                    float amt  = rnd();
                    c.f1  = rise < 0.25f ? c.f0 * (3.f + 4.f * amt)
                                         : c.f0 * (0.05f + 0.1f * amt);
                    c.dur = 0.004f + 0.012f * rnd();
                    c.win = WIN_EXP;
                    c.winK = 4.f + 3.f * rnd();
                    break;
                }
                case FAM_PING:    // impulse through a high-Q resonator
                    c.f0  = (400.f + 3000.f * spread) * jitter;
                    c.t60 = 0.01f + 0.05f * rnd();
                    c.dur = std::min(c.t60 * 2.f, 0.06f);
                    c.win = WIN_RECT;   // the phasor decays by itself
                    break;
                case FAM_THUMP:   // gated sub-sine pulse
                    c.f0  = (40.f + 50.f * spread) * jitter;
                    c.f1  = c.f0 * 0.8f;
                    c.dur = 0.02f + 0.04f * rnd();
                    c.win = WIN_RECT;
                    break;
                default:          // FAM_FOLD: aliased/waveshaped transient
                    c.f0    = (300.f + 2200.f * spread) * jitter;
                    c.foldK = 2.f + 6.f * rnd();
                    c.dur   = 0.002f + 0.008f * rnd();
                    c.win   = WIN_EXP;
                    c.winK  = 4.f + 4.f * rnd();
                    break;
            }
            c.nseed = rng.next() | 1;

            if (quantMode != QUANT_OFF && c.f0 > 0.f) {
                if (quantMode == QUANT_WALK && layout == LAYOUT_FAMILY) {
                    static const float baseF[NUM_FAMILIES] =
                        {2000.f, 0.f, 250.f, 300.f, 300.f,
                         1200.f, 400.f, 40.f, 300.f};
                    c.f0 = walkFreq(baseF[family], i % 8);
                }
                else
                    c.f0 = quantizeFreq(c.f0);
            }

            c.pan = 0.5f + (rnd() - 0.5f) * 0.6f;

            // Variety draws come last (stable stream); per-cell gates keep
            // part of the kit clean. Cycles, not probability: which cells
            // carry an ingredient is rolled once here, deterministically.
            float g, a;
            g = rnd(); a = rnd();
            c.altN = (g < 0.4f) ? 2 + (int)(a * 2.999f) : 1;
            c.altOff[0] = 0.f;
            for (int k = 1; k < 4; k++)
                c.altOff[k] = (rnd() < 0.5f ? -1.f : 1.f) * (1.f + 11.f * rnd());
            c.pong = rnd() < 0.4f;
            g = rnd(); a = rnd();
            c.dropN = (g < 0.35f) ? 2 + (int)(a * 2.999f) : 0;
            g = rnd(); a = rnd();
            c.dblGain = (g < 0.35f) ? 0.4f + 0.4f * a : 0.f;
            c.dblS    = 0.008f + 0.03f * rnd();
        }

        if (layout == LAYOUT_SHUFFLED)
            P64::kitShuffle(cells, 64, seed);
    }

    // ── voices ────────────────────────────────────────────────────────────────

    void launch(int cell, int delaySamples, float gain, float pan, float pitchMul) {
        Voice* v = nullptr;
        for (auto& cand : voices)
            if (!cand.active) { v = &cand; break; }
        if (!v) {   // steal the voice closest to its end
            v = &voices[0];
            for (auto& cand : voices)
                if (cand.dur - cand.t < v->dur - v->t) v = &cand;
        }
        const Cell& c = cells[cell];
        float fs = APP->engine->getSampleRate();

        *v = Voice{};
        v->active = true;
        v->cell   = cell;
        v->delay  = delaySamples;
        v->gain   = gain;
        v->dur    = c.dur;
        v->lfsr   = c.nseed;   // identical every strike: the charter
        v->ampL   = (1.f - pan);
        v->ampR   = pan;

        float f0 = c.f0 * pitchMul;
        switch (c.fam) {
            case FAM_CLICK:
            case FAM_BLIP:
            case FAM_CRUSH:
            case FAM_FOLD:
                v->freq  = f0;
                v->freq2 = c.f1 * pitchMul;
                break;
            case FAM_ZAP:
            case FAM_THUMP:
                v->freq = f0;
                v->fMul = std::pow(std::max(c.f1 * pitchMul, 1.f) / std::max(f0, 1.f),
                                   1.f / std::max(c.dur * fs, 1.f));
                break;
            case FAM_PING: {
                float r = std::pow(10.f, -3.f / (c.t60 * fs));
                float w = 2.f * (float)M_PI * std::min(f0, 0.45f * fs) / fs;
                v->rotRe = r * std::cos(w);
                v->rotIm = r * std::sin(w);
                v->re    = 1.f;
                break;
            }
            default:   // FAM_TICK, FAM_DATA
                break;
        }
        v->lpC = c.lpFc > 0.f
               ? clamp(2.f * (float)M_PI * c.lpFc / fs, 0.f, 1.f) : 0.f;
        if (c.fam == FAM_CRUSH)
            v->holdDiv = std::max(1, (int)(fs / c.crushHz));
        if (c.fam == FAM_DATA)
            v->bitDiv = std::max(1, (int)(fs / std::max(f0, 30.f)));
    }

    void cellTriggered(int cell) override {
        Cell& c = cells[cell];
        int hit = c.hits++;

        if ((variety & VAR_DROP) && c.dropN > 0 && hit % c.dropN == c.dropN - 1)
            return;   // the anti-hit

        float pan = c.pan;
        if ((variety & VAR_PONG) && c.pong)
            pan = (hit & 1) ? 0.88f : 0.12f;

        float pitchMul = 1.f;
        if ((variety & VAR_ALT) && c.altN > 1)
            pitchMul = std::exp2(c.altOff[hit % c.altN] / 12.f);

        launch(cell, 0, 1.f, pan, pitchMul);
        if ((variety & VAR_DBL) && c.dblGain > 0.f) {
            float fs = APP->engine->getSampleRate();
            launch(cell, (int)(c.dblS * fs), c.dblGain, pan, pitchMul);
        }
    }

    void renderMix(float& mixL, float& mixR, float dt) override {
        for (auto& v : voices) {
            if (!v.active) continue;
            if (v.delay > 0) { v.delay--; continue; }
            const Cell& c = cells[v.cell];

            float x = v.t / v.dur;
            float w;
            switch (c.win) {
                case WIN_EXP:  w = std::exp(-x * c.winK); break;
                case WIN_HANN: w = 0.5f - 0.5f * std::cos(2.f * (float)M_PI * x); break;
                default: {   // rect with ~0.3 ms cosine edges
                    float e = std::min(v.t, v.dur - v.t) / 0.0003f;
                    w = e >= 1.f ? 1.f : 0.5f - 0.5f * std::cos((float)M_PI * clamp(e, 0.f, 1.f));
                    break;
                }
            }

            float s = 0.f;
            switch (c.fam) {
                case FAM_CLICK:
                case FAM_BLIP:
                    s = std::sin(2.f * (float)M_PI * v.phase);
                    v.phase += v.freq * dt;
                    if (c.dual) {
                        s = 0.6f * s + 0.6f * std::sin(2.f * (float)M_PI * v.phase2);
                        v.phase2 += v.freq2 * dt;
                    }
                    break;
                case FAM_TICK: {
                    v.lfsr ^= v.lfsr << 13; v.lfsr ^= v.lfsr >> 17; v.lfsr ^= v.lfsr << 5;
                    float n = ((v.lfsr >> 8) / 8388608.f) - 1.f;
                    v.lp += v.lpC * (n - v.lp);
                    s = v.lp * 2.f;
                    break;
                }
                case FAM_CRUSH: {
                    float raw = std::sin(2.f * (float)M_PI * v.phase);
                    v.phase += v.freq * dt;
                    if (--v.holdN <= 0) { v.hold = raw; v.holdN = v.holdDiv; }
                    s = std::round(v.hold * c.levels) / c.levels;
                    break;
                }
                case FAM_DATA:
                    if (--v.bitN <= 0) {
                        v.lfsr ^= v.lfsr << 13; v.lfsr ^= v.lfsr >> 17; v.lfsr ^= v.lfsr << 5;
                        v.bitN = v.bitDiv;
                    }
                    s = (v.lfsr & 1) ? 0.8f : -0.8f;
                    break;
                case FAM_ZAP:
                case FAM_THUMP:
                    s = std::sin(2.f * (float)M_PI * v.phase);
                    v.phase += v.freq * dt;
                    v.freq  *= v.fMul;
                    break;
                case FAM_PING: {
                    float nr = v.rotRe * v.re - v.rotIm * v.im;
                    float ni = v.rotRe * v.im + v.rotIm * v.re;
                    v.re = nr; v.im = ni;
                    s = nr;
                    break;
                }
                default:   // FAM_FOLD
                    s = std::sin(c.foldK * std::sin(2.f * (float)M_PI * v.phase));
                    v.phase += v.freq * dt;
                    break;
            }
            s *= w * v.gain;

            v.t += dt;
            if (v.t >= v.dur)
                v.active = false;

            mixL += s * v.ampL;
            mixR += s * v.ampR;
        }
    }
};

// ── Widget ────────────────────────────────────────────────────────────────────

struct Micro64Widget : ModuleWidget {
    Micro64Widget(Micro64* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Micro64.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        for (int i = 0; i < 4; i++)
            addInput(createInputCentered<PJ301MPort>(
                mm2px(Vec(15.24f, 30.f + i * 14.f)), module, Micro64::CELL_INPUT + i));

        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(38.f, 94.f)),  module, Micro64::MIXL_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(38.f, 106.f)), module, Micro64::MIXR_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        Micro64* m = getModule<Micro64>();
        P64::appendKitMenu(menu, m,
            {"Click", "Tick", "Crush", "Data", "Blip",
             "Zap", "Ping", "Thump", "Fold"},
            {
                {"Alternate — A-B pitch cycles",  Micro64::VAR_ALT},
                {"Ping-pong — hard L/R",          Micro64::VAR_PONG},
                {"Dropout — every Nth hit rests", Micro64::VAR_DROP},
                {"Doubler — exact echo hit",      Micro64::VAR_DBL},
            });
    }
};

Model* modelMicro64 = createModel<Micro64, Micro64Widget>("64Micro");
