#include "KitModule.hpp"

// ── 64Drums ───────────────────────────────────────────────────────────────────
// Kit companion drum synth on the KitModule shell: 64-cell gate format in
// (Rhythm64, Buttons64, Gome64, Life64), stereo mix out. One drum voice per
// cell, generated from a seed — row picks the family (top→bottom: click, open
// hat, closed hat, blip, clap, snare, tom, kick), column and per-cell jitter
// vary the character. Layout / Quantize / Variety options and the seed
// contract live in the shell (KitModule.hpp); this file is the recipe and the
// voices: sine with pitch-drop envelope + filtered noise, plus five per-cell
// gated variety extras (fold, FM, ring mod, resonant noise, rising pitch).
// Full design: Drums64.md.

static constexpr int DRUM_VOICES = 16;

struct Drums64 : KitModule {
    enum Variety {
        VAR_FOLD = 1 << 0,
        VAR_FM   = 1 << 1,
        VAR_RING = 1 << 2,
        VAR_RESO = 1 << 3,
        VAR_RISE = 1 << 4,
        VAR_ALL  = (1 << 5) - 1,
    };

    // Per-cell voice recipe, fixed at kit generation
    struct Cell {
        float f0;         // oscillator base frequency (Hz)
        float decay;      // amplitude decay time (s)
        float pitchAmt;   // pitch-drop depth (multiples of f0)
        float pitchRate;  // pitch-envelope decay (1/s)
        float sineAmt;
        float noiseAmt;
        float lpFc;       // noise lowpass cutoff (Hz)
        float hpFc;       // noise highpass cutoff (Hz)
        float pan;        // 0 = left … 1 = right
        // Variety recipe — always drawn at generation, applied only when the
        // matching toggle is on, so flipping a toggle A/Bs the identical kit.
        float shape;      // fold brightness 0–1 (VAR_FOLD)
        float fmRatio;    // FM modulator ratio (VAR_FM)
        float fmAmt;      // FM index, decays with the amp env (VAR_FM)
        float rmRatio;    // ring-mod oscillator ratio (VAR_RING)
        float rmAmt;      // ring-mod depth, decays with the amp env (VAR_RING)
        float reso;       // resonant noise-band mix 0–1 (VAR_RESO)
        float bpFc;       // resonant band center (Hz)
        bool  rise;       // pitch envelope rises instead of drops (VAR_RISE)
    };
    Cell cells[64];

    struct Voice {
        bool  active = false;
        int   cell   = -1;
        float phase = 0.f, pitchEnv = 0.f, env = 0.f;
        float lp = 0.f, hpLp = 0.f;   // one-pole states (LP, and LP behind the HP)
        float fmPhase = 0.f, rmPhase = 0.f;
        float svfLow = 0.f, svfBand = 0.f;   // resonant noise-band states
    };
    Voice voices[DRUM_VOICES];

    Drums64() : KitModule(0x64726d73, VAR_ALL) {
        regenKit();
    }

    void kitReset() override {
        for (auto& v : voices) v = Voice{};
    }

    // ── kit generation ────────────────────────────────────────────────────────

    void regenKit() override {
        for (int i = 0; i < 64; i++) {
            P64::KitRng rng(seed, i);
            auto rnd = [&]() { return rng.uni(); };
            // col 0 → 1: pitch rises across the row, decay tightens slightly
            float spread = (i % 8) / 7.f;
            int family = cellFamily(rng, i);
            float jitter = 0.85f + 0.3f * rnd();

            Cell& c = cells[i];
            auto base = [&](float f0, float decay, float pitchAmt, float pitchRate,
                            float sineAmt, float noiseAmt, float lpFc, float hpFc) {
                c.f0 = f0; c.decay = decay; c.pitchAmt = pitchAmt; c.pitchRate = pitchRate;
                c.sineAmt = sineAmt; c.noiseAmt = noiseAmt; c.lpFc = lpFc; c.hpFc = hpFc;
            };
            switch (family) {
                case 7:   // kick
                    base((40.f + 18.f * spread) * jitter, 0.25f + 0.15f * rnd(),
                         3.f, 25.f, 1.f, 0.04f, 400.f, 0.f);
                    break;
                case 6:   // tom
                    base((85.f + 80.f * spread) * jitter, 0.18f + 0.12f * rnd(),
                         1.f, 18.f, 1.f, 0.08f, 800.f, 0.f);
                    break;
                case 5: { // snare — draws hoisted: arg order is unspecified
                    float dec = 0.12f + 0.08f * rnd();
                    float lp  = 5000.f + 3000.f * rnd();
                    base((165.f + 60.f * spread) * jitter, dec,
                         0.5f, 30.f, 0.5f, 0.8f, lp, 300.f);
                    break;
                }
                case 4: { // clap
                    float dec = 0.08f + 0.07f * rnd();
                    float lp  = 3500.f + 2500.f * rnd();
                    base(0.f, dec,
                         0.f, 0.f, 0.f, 1.f, lp, 600.f + 400.f * spread);
                    break;
                }
                case 3:   // perc blip
                    base((380.f + 520.f * spread) * jitter, 0.05f + 0.05f * rnd(),
                         0.5f, 40.f, 1.f, 0.05f, 2000.f, 0.f);
                    break;
                case 2:   // closed hat
                    base(0.f, 0.03f + 0.03f * rnd(),
                         0.f, 0.f, 0.f, 1.f, 12000.f, 6000.f + 2500.f * spread);
                    break;
                case 1:   // open hat
                    base(0.f, 0.2f + 0.2f * rnd(),
                         0.f, 0.f, 0.f, 1.f, 12000.f, 6000.f + 2500.f * spread);
                    break;
                default:  // click
                    base((2000.f + 1800.f * spread) * jitter, 0.015f + 0.015f * rnd(),
                         0.f, 0.f, 0.3f, 0.7f, 14000.f, 8000.f);
                    break;
            }

            // Quantize the landing pitch: the sweep transposes with f0, so the
            // note the ear hears is the one the envelope settles on.
            if (quantMode != QUANT_OFF && c.f0 > 0.f) {
                if (quantMode == QUANT_WALK && layout == LAYOUT_FAMILY) {
                    // Column walks the scale from the family's register
                    // (jitter-free: the row becomes playable as a melody).
                    static const float baseF[8] =
                        {2000.f, 0.f, 0.f, 380.f, 0.f, 165.f, 85.f, 40.f};
                    c.f0 = walkFreq(baseF[family], i % 8);
                }
                else {
                    // Nearest note; outside the family layout, "walk" means
                    // this too — the generation column isn't visible there.
                    c.f0 = quantizeFreq(c.f0);
                }
            }

            c.pan = 0.5f + (rnd() - 0.5f) * 0.6f;   // gentle stereo spread

            // Variety draws come last so pre-variety seeds keep their sounds.
            // Each feature rolls a per-cell gate, so with a toggle on a share
            // of cells still stays clean; gate and amount are both always
            // drawn to keep the stream stable.
            float g, a;
            g = rnd(); a = rnd();
            c.shape   = (g < 0.5f)  ? 0.15f + 0.85f * a : 0.f;
            c.fmRatio = 0.5f + 5.5f * rnd();
            g = rnd(); a = rnd();
            c.fmAmt   = (g < 0.4f)  ? 1.f + 7.f * a * a : 0.f;
            c.rmRatio = 1.25f + 6.f * rnd();
            g = rnd(); a = rnd();
            c.rmAmt   = (g < 0.35f) ? 0.4f + 0.6f * a : 0.f;
            g = rnd(); a = rnd();
            c.reso    = (g < 0.4f)  ? 0.3f + 0.7f * a : 0.f;
            c.rise    = rnd() < 0.35f;
            c.bpFc    = std::sqrt(std::max(c.hpFc, 150.f) * std::max(c.lpFc, 300.f));
        }

        // Shuffled layout: the exact same 64 sounds, permuted by the seed.
        if (layout == LAYOUT_SHUFFLED)
            P64::kitShuffle(cells, 64, seed);
    }

    // ── voices ────────────────────────────────────────────────────────────────

    void cellTriggered(int cell) override {
        Voice* v = nullptr;
        for (auto& cand : voices)
            if (!cand.active) { v = &cand; break; }
        if (!v) {   // steal the quietest
            v = &voices[0];
            for (auto& cand : voices)
                if (cand.env < v->env) v = &cand;
        }
        *v = Voice{};
        v->active   = true;
        v->cell     = cell;
        v->pitchEnv = 1.f;
        v->env      = 1.f;
    }

    void renderMix(float& mixL, float& mixR, float dt) override {
        for (auto& v : voices) {
            if (!v.active) continue;
            const Cell& c = cells[v.cell];

            float out = 0.f;
            if (c.sineAmt > 0.f) {
                float pAmt = c.pitchAmt;
                if ((variety & VAR_RISE) && c.rise && c.pitchAmt > 0.f)
                    pAmt = -0.6f * std::min(c.pitchAmt, 1.2f);   // start low, rise to f0
                float freq = c.f0 * (1.f + pAmt * v.pitchEnv * v.pitchEnv);
                v.phase += freq * dt;
                v.phase -= (int) v.phase;
                float ph = 2.f * (float)M_PI * v.phase;
                if ((variety & VAR_FM) && c.fmAmt > 0.f) {
                    v.fmPhase += freq * c.fmRatio * dt;
                    v.fmPhase -= (int) v.fmPhase;
                    ph += c.fmAmt * v.env * std::sin(2.f * (float)M_PI * v.fmPhase);
                }
                float s = std::sin(ph);
                if ((variety & VAR_FOLD) && c.shape > 0.f)
                    s = std::sin(ph + 4.f * c.shape * (0.25f + 0.75f * v.env) * s);
                if ((variety & VAR_RING) && c.rmAmt > 0.f) {
                    v.rmPhase += c.f0 * c.rmRatio * dt;
                    v.rmPhase -= (int) v.rmPhase;
                    float d = c.rmAmt * v.env;
                    s *= 1.f - d + d * std::sin(2.f * (float)M_PI * v.rmPhase);
                }
                out += c.sineAmt * s;
            }
            if (c.noiseAmt > 0.f) {
                float n = random::uniform() * 2.f - 1.f;
                float raw = n;
                if (c.hpFc > 0.f) {
                    v.hpLp += clamp(2.f * M_PI * c.hpFc * dt, 0.f, 1.f) * (n - v.hpLp);
                    n -= v.hpLp;   // one-pole highpass
                }
                v.lp += clamp(2.f * M_PI * c.lpFc * dt, 0.f, 1.f) * (n - v.lp);
                float shaped = v.lp;
                if ((variety & VAR_RESO) && c.reso > 0.f) {
                    // Chamberlin SVF bandpass on the raw noise; q also
                    // roughly normalizes the resonant gain
                    float f = std::min(2.f * std::sin((float)M_PI * c.bpFc * dt), 0.7f);
                    float q = std::max(1.2f - c.reso, 0.2f);
                    v.svfLow  += f * v.svfBand;
                    float high = raw - v.svfLow - q * v.svfBand;
                    v.svfBand += f * high;
                    shaped = crossfade(shaped, v.svfBand * q, c.reso);
                }
                out += c.noiseAmt * shaped;
            }
            out *= v.env;

            v.env      -= v.env * dt / c.decay;
            v.pitchEnv -= v.pitchEnv * dt * c.pitchRate;
            if (v.env < 1e-3f)
                v.active = false;

            mixL += out * (1.f - c.pan);
            mixR += out * c.pan;
        }
    }
};

// ── Widget ────────────────────────────────────────────────────────────────────

struct Drums64Widget : ModuleWidget {
    Drums64Widget(Drums64* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Drums64.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        for (int i = 0; i < 4; i++)
            addInput(createInputCentered<PJ301MPort>(
                mm2px(Vec(15.24f, 30.f + i * 14.f)), module, Drums64::CELL_INPUT + i));

        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(38.f, 94.f)),  module, Drums64::MIXL_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(38.f, 106.f)), module, Drums64::MIXR_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        Drums64* m = getModule<Drums64>();
        P64::appendKitMenu(menu, m, {
            {"Fold — brighter, driven sines",   Drums64::VAR_FOLD},
            {"FM — metallic, clangy",           Drums64::VAR_FM},
            {"Ring mod — growl, sidebands",     Drums64::VAR_RING},
            {"Resonant noise — zaps, lasers",   Drums64::VAR_RESO},
            {"Rising pitch — whoops",           Drums64::VAR_RISE},
        });
    }
};

Model* modelDrums64 = createModel<Drums64, Drums64Widget>("64Drums");
