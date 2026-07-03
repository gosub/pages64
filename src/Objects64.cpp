#include "KitModule.hpp"

// ── 64Objects ─────────────────────────────────────────────────────────────────
// Kit companion: modal percussion on the KitModule shell — struck objects of
// different materials and shapes, one per cell. Row picks the object
// (top→bottom: woodblock, tine, glass, marimba, vibraphone, harp, membrane,
// bell), column the size, per-cell jitter the strike. A voice is a bank of
// damped resonators (complex phasor per mode; body = mode ratios, material =
// damping law, strike hardness/position folded into the initial amplitudes);
// the harp row is Karplus-Strong. Variety extras (beating, rattle, flam,
// mute) follow the shell's gate-don't-skip convention. Full design:
// docs/design/Objects64.md.

static constexpr int OBJ_VOICES = 24;
static constexpr int OBJ_MODES  = 8;
static constexpr int KS_BUF     = 4096;   // power of two; f0 ≥ fs/4094
static constexpr int KS_MASK    = KS_BUF - 1;

struct Objects64 : KitModule {
    enum Variety {
        VAR_BEAT   = 1 << 0,
        VAR_RATTLE = 1 << 1,
        VAR_FLAM   = 1 << 2,
        VAR_MUTE   = 1 << 3,
        VAR_ALL    = (1 << 4) - 1,
    };
    enum Ring { RING_CHOKE, RING_DAMPED, RING_NATURAL };

    // Per-cell recipe, fixed at kit generation
    struct Cell {
        bool  string;                 // harp row: Karplus-Strong, not modal
        float f0;                     // fundamental (Hz); quantize target
        int   nModes;
        float modeRatio[OBJ_MODES];   // mode freq / f0
        float modeT60[OBJ_MODES];     // s (material damping law, per mode)
        float modeGain[OBJ_MODES];    // strike hardness + position, normalized
        float ksT60;                  // string decay (s)
        float ksBright;               // loop lowpass coefficient 0–1
        float pluckPos;               // comb position 0–0.5
        float pluckBright;            // excitation lowpass coefficient 0–1
        float pan;                    // 0 = left … 1 = right
        // Variety recipe — always drawn, applied only when the toggle is on.
        float beatHz;                 // detune of duplicated modes (VAR_BEAT)
        float rattleAmt;              // buzz mix (VAR_RATTLE)
        float flamGain;               // second-strike level (VAR_FLAM)
        float flamS;                  // second-strike delay (s)
        bool  mute;                   // felt-damped cell (VAR_MUTE)
    };
    Cell cells[64];

    struct Voice {
        bool  active = false;
        int   cell   = -1;
        bool  string = false;
        int   life   = 0;             // samples until -60 dB
        // modal: complex phasor per mode, damping folded into the rotation
        int   n = 0;
        float rotRe[OBJ_MODES], rotIm[OBJ_MODES];
        float re[OBJ_MODES], im[OBJ_MODES];
        float exRe[OBJ_MODES];        // strike amplitudes (kept for the flam)
        // Karplus-Strong (buf deliberately NOT cleared on reset: the fill at
        // trigger covers every slot the read head visits)
        float buf[KS_BUF];
        int   len = 0, wr = 0;
        float frac = 0.f, loopLp = 0.f, loss = 0.f, bright = 0.f;
        int   feed = 0; float feedAmp = 0.f;   // string re-excitation (flam)
        int   flamWait = 0; float flamAmt = 0.f;

        void reset() {
            active = string = false;
            cell = -1;
            life = n = len = wr = feed = flamWait = 0;
            frac = loopLp = loss = bright = feedAmp = flamAmt = 0.f;
            memset(re, 0, sizeof(re));
            memset(im, 0, sizeof(im));
            memset(exRe, 0, sizeof(exRe));
        }
    };
    Voice voices[OBJ_VOICES];

    int ring = RING_NATURAL;

    Objects64() : KitModule(0x6f626a73, VAR_ALL) {
        mixGain = 5.f;
        regenKit();
    }

    void kitReset() override {
        ring = RING_NATURAL;
        clearVoices();
    }

    void clearVoices() {
        for (auto& v : voices) v.reset();
    }

    void onSampleRateChange() override {
        clearVoices();   // rotation coefficients are computed at trigger time
    }

    void kitToJson(json_t* root) override {
        json_object_set_new(root, "ring", json_integer(ring));
    }

    void kitFromJson(json_t* root) override {
        json_t* j;
        if ((j = json_object_get(root, "ring")))
            ring = clamp((int) json_integer_value(j), 0, 2);
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
            c = Cell{};

            // Modal recipe: body = ratios, material = damping law
            // T(f) = T0 · ratio^-gamma, strike = hardness/position → gains.
            auto modal = [&](std::initializer_list<float> ratios,
                             float f0, float t60, float gamma) {
                c.f0 = f0;
                c.nModes = (int) ratios.size();
                float hard = rnd();
                float pos  = 0.08f + 0.42f * rnd();
                int k = 0;
                for (float r : ratios)
                    c.modeRatio[k++] = r * (0.99f + 0.02f * rnd());
                float sum = 0.f;
                for (k = 0; k < c.nModes; k++) {
                    float r = c.modeRatio[k];
                    c.modeT60[k] = t60 * std::pow(std::max(r, 0.5f), -gamma);
                    float g = std::fabs(std::sin((k + 1) * (float)M_PI * pos));
                    g /= 1.f + std::max(r - 1.f, 0.f) * 1.5f * (1.f - hard);
                    c.modeGain[k] = g;
                    sum += g;
                }
                for (k = 0; k < c.nModes; k++)
                    c.modeGain[k] /= std::max(sum, 0.1f);
            };

            switch (family) {
                case 0:   // woodblock — dry, few modes, treble dies fast
                    modal({1.f, 2.42f, 3.93f},
                          (750.f + 650.f * spread) * jitter, 0.05f + 0.05f * rnd(), 1.3f);
                    break;
                case 1:   // music-box tine — clamped bar
                    modal({1.f, 6.267f, 17.547f},
                          (900.f + 900.f * spread) * jitter, 0.6f + 0.8f * rnd(), 0.6f);
                    break;
                case 2:   // glass / bowl — sparse, pure, long
                    modal({1.f, 2.32f, 4.25f, 6.63f},
                          (500.f + 700.f * spread) * jitter, 1.2f + 1.8f * rnd(), 0.35f);
                    break;
                case 3:   // marimba bar — wood free bar
                    modal({1.f, 2.756f, 5.404f, 8.933f},
                          (130.f + 260.f * spread) * jitter, 0.2f + 0.15f * rnd(), 1.1f);
                    break;
                case 4:   // vibraphone bar — metal free bar, long even ring
                    modal({1.f, 2.756f, 5.404f, 8.933f},
                          (175.f + 350.f * spread) * jitter, 2.0f + 3.0f * rnd(), 0.45f);
                    break;
                case 5:   // harp / pluck — Karplus-Strong
                    c.string      = true;
                    c.f0          = (110.f + 330.f * spread) * jitter;
                    c.ksT60       = 1.0f + 1.8f * rnd();
                    c.ksBright    = 0.25f + 0.6f * rnd();
                    c.pluckPos    = 0.12f + 0.3f * rnd();
                    c.pluckBright = 0.15f + 0.55f * rnd();
                    break;
                case 6:   // membrane — tom/tabla, Bessel modes
                    modal({1.f, 1.594f, 2.136f, 2.296f, 2.653f, 2.918f},
                          (75.f + 105.f * spread) * jitter, 0.2f + 0.25f * rnd(), 0.9f);
                    break;
                default:  // bell — hum below the prime, longest ring
                    modal({0.5f, 1.f, 1.2f, 1.5f, 2.f, 2.5f, 2.67f},
                          (210.f + 210.f * spread) * jitter, 2.5f + 4.0f * rnd(), 0.5f);
                    break;
            }

            // Quantize the fundamental; the mode stack transposes with it.
            if (quantMode != QUANT_OFF && c.f0 > 0.f) {
                if (quantMode == QUANT_WALK && layout == LAYOUT_FAMILY) {
                    static const float baseF[8] =
                        {750.f, 900.f, 500.f, 130.f, 175.f, 110.f, 75.f, 210.f};
                    c.f0 = walkFreq(baseF[family], i % 8);
                }
                else
                    c.f0 = quantizeFreq(c.f0);
            }

            c.pan = 0.5f + (rnd() - 0.5f) * 0.6f;

            // Variety draws come last; per-cell gates keep part of the kit
            // clean, gate + amount always drawn (stable stream).
            float g, a;
            g = rnd(); a = rnd();
            c.beatHz    = (g < 0.4f)  ? 0.6f + 2.4f * a : 0.f;
            g = rnd(); a = rnd();
            c.rattleAmt = (g < 0.35f) ? 0.35f + 0.45f * a : 0.f;
            g = rnd(); a = rnd();
            c.flamGain  = (g < 0.35f) ? 0.35f + 0.4f * a : 0.f;
            c.flamS     = 0.012f + 0.03f * rnd();
            c.mute      = rnd() < 0.3f;
        }

        if (layout == LAYOUT_SHUFFLED)
            P64::kitShuffle(cells, 64, seed);
    }

    // ── voices ────────────────────────────────────────────────────────────────

    void cellTriggered(int cell) override {
        // Self-choke first (re-striking an object steals its own ring),
        // then a free voice, then the one closest to silence.
        Voice* v = nullptr;
        for (auto& cand : voices)
            if (cand.active && cand.cell == cell) { v = &cand; break; }
        if (!v)
            for (auto& cand : voices)
                if (!cand.active) { v = &cand; break; }
        if (!v) {
            v = &voices[0];
            for (auto& cand : voices)
                if (cand.life < v->life) v = &cand;
        }

        const Cell& c = cells[cell];
        float fs = APP->engine->getSampleRate();
        static const float ringScale[3] = {0.22f, 0.55f, 1.f};
        bool  muted = (variety & VAR_MUTE) && c.mute;
        float decayScale = ringScale[ring] * (muted ? 0.18f : 1.f);

        v->reset();
        v->active = true;
        v->cell   = cell;
        v->string = c.string;

        if (!c.string) {
            bool beat = (variety & VAR_BEAT) && c.beatHz > 0.f;
            int  dup  = beat ? std::min(c.nModes, OBJ_MODES - c.nModes) : 0;
            v->n = c.nModes + dup;
            float maxT60 = 0.f;
            for (int k = 0; k < v->n; k++) {
                int   src  = k < c.nModes ? k : k - c.nModes;
                float freq = c.f0 * c.modeRatio[src] + (k >= c.nModes ? c.beatHz : 0.f);
                float gain = c.modeGain[src] * (k >= c.nModes ? 0.7f : 1.f);
                if (muted)   // felt: darker as well as shorter
                    gain /= 1.f + std::max(c.modeRatio[src] - 1.f, 0.f);
                if (freq > 0.45f * fs)
                    gain = 0.f;
                float t60 = std::max(c.modeT60[src] * decayScale, 0.005f);
                maxT60 = std::max(maxT60, gain > 0.f ? t60 : 0.f);
                float r = std::pow(10.f, -3.f / (t60 * fs));
                float w = 2.f * (float)M_PI * freq / fs;
                v->rotRe[k] = r * std::cos(w);
                v->rotIm[k] = r * std::sin(w);
                v->exRe[k]  = gain;
                v->re[k]    = gain;
            }
            v->life = (int)(fs * std::max(maxT60, 0.01f));
        }
        else {
            float f0 = clamp(c.f0, fs / (KS_BUF - 2.f), 0.25f * fs);
            float D  = fs / f0 - 0.5f;   // the loop lowpass adds ~1/2 sample
            v->len   = (int) D + 1;
            v->frac  = v->len - D;
            float t60 = std::max(c.ksT60 * decayScale, 0.02f);
            v->loss   = std::min(std::pow(10.f, -3.f / (f0 * t60)), 0.99995f);
            v->bright = c.ksBright;
            // pluck: lowpassed noise burst + position comb
            float lp = 0.f;
            for (int k = 0; k < v->len; k++) {
                float n = random::uniform() * 2.f - 1.f;
                lp += c.pluckBright * (n - lp);
                v->buf[k] = lp * 0.9f;
            }
            int off = std::max(1, (int)(c.pluckPos * v->len));
            for (int k = v->len - 1; k >= 0; k--) {
                int j = k - off;
                if (j < 0) j += v->len;
                v->buf[k] -= 0.8f * v->buf[j];
            }
            v->wr   = v->len;
            v->life = (int)(fs * t60 * 1.5f);
        }

        if ((variety & VAR_FLAM) && c.flamGain > 0.f) {
            v->flamWait = std::max(1, (int)(fs * c.flamS));
            v->flamAmt  = c.flamGain;
        }
    }

    void renderMix(float& mixL, float& mixR, float dt) override {
        for (auto& v : voices) {
            if (!v.active) continue;
            const Cell& c = cells[v.cell];

            if (v.flamWait > 0 && --v.flamWait == 0) {
                if (!v.string)
                    for (int k = 0; k < v.n; k++)
                        v.re[k] += v.flamAmt * v.exRe[k];
                else {
                    v.feed    = v.len;
                    v.feedAmp = v.flamAmt * 0.4f;
                }
            }

            float s = 0.f;
            if (!v.string) {
                for (int k = 0; k < v.n; k++) {
                    float nr = v.rotRe[k] * v.re[k] - v.rotIm[k] * v.im[k];
                    float ni = v.rotRe[k] * v.im[k] + v.rotIm[k] * v.re[k];
                    v.re[k] = nr;
                    v.im[k] = ni;
                    s += nr;
                }
            }
            else {
                int   i0 = (v.wr - v.len) & KS_MASK;
                float s0 = v.buf[i0];
                float s1 = v.buf[(i0 + 1) & KS_MASK];
                s = s0 + v.frac * (s1 - s0);
                v.loopLp += v.bright * (s - v.loopLp);
                float fb = v.loss * v.loopLp;
                if (v.feed > 0) {
                    fb += v.feedAmp * (random::uniform() * 2.f - 1.f);
                    v.feed--;
                }
                v.buf[v.wr] = fb;
                v.wr = (v.wr + 1) & KS_MASK;
            }

            if ((variety & VAR_RATTLE) && c.rattleAmt > 0.f)
                s = crossfade(s, std::tanh(5.f * s), c.rattleAmt);

            if (--v.life <= 0)
                v.active = false;

            mixL += s * (1.f - c.pan);
            mixR += s * c.pan;
        }
    }
};

// ── Widget ────────────────────────────────────────────────────────────────────

struct Objects64Widget : ModuleWidget {
    Objects64Widget(Objects64* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Objects64.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        for (int i = 0; i < 4; i++)
            addInput(createInputCentered<PJ301MPort>(
                mm2px(Vec(15.24f, 30.f + i * 14.f)), module, Objects64::CELL_INPUT + i));

        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(38.f, 94.f)),  module, Objects64::MIXL_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(38.f, 106.f)), module, Objects64::MIXR_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        Objects64* m = getModule<Objects64>();
        P64::appendKitMenu(menu, m, {
            {"Beating — detuned shimmer",     Objects64::VAR_BEAT},
            {"Rattle — buzz on loud hits",    Objects64::VAR_RATTLE},
            {"Flam — double strikes",         Objects64::VAR_FLAM},
            {"Mute — felt-damped cells",      Objects64::VAR_MUTE},
        });
        menu->addChild(createIndexSubmenuItem("Ring",
            {"Choke", "Damped", "Natural"},
            [=]() { return m->ring; },
            [=](int v) { m->ring = v; }));
    }
};

Model* modelObjects64 = createModel<Objects64, Objects64Widget>("64Objects");
