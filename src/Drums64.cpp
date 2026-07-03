#include "plugin.hpp"

// ── 64Drums ───────────────────────────────────────────────────────────────────
// Companion drum synth: 64-cell gate format in (Rhythm64, Buttons64, Gome64,
// Life64), stereo mix out. One drum voice per cell, generated from a seed —
// row picks the family (top→bottom: click, open hat, closed hat, blip, clap,
// snare, tom, kick), column and per-cell jitter vary the character. The seed
// is serialized: a patch always reloads its kit. Full design: Drums64.md.

static constexpr int DRUM_VOICES = 16;

struct Drums64 : Module {
    enum ParamIds  { NUM_PARAMS };
    enum InputIds  {
        ENUMS(CELL_INPUT, 4),   // rows 1-2 / 3-4 / 5-6 / 7-8, 16ch poly each
        NUM_INPUTS
    };
    enum OutputIds {
        MIXL_OUTPUT,
        MIXR_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds  { NUM_LIGHTS };

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

    enum Layout { LAYOUT_FAMILY, LAYOUT_SHUFFLED, LAYOUT_RANDOM };

    enum Variety {
        VAR_FOLD = 1 << 0,
        VAR_FM   = 1 << 1,
        VAR_RING = 1 << 2,
        VAR_RESO = 1 << 3,
        VAR_RISE = 1 << 4,
        VAR_ALL  = (1 << 5) - 1,
    };

    uint32_t seed = 0x64726d73;
    int layout = LAYOUT_FAMILY;
    int variety = 0;
    bool prevGate[64] = {};

    Drums64() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (int i = 0; i < 4; i++)
            configInput(CELL_INPUT + i,
                string::f("Rows %d-%d cell gates (poly 16ch)", i * 2 + 1, i * 2 + 2));
        configOutput(MIXL_OUTPUT, "Mix left");
        configOutput(MIXR_OUTPUT, "Mix right");
        regenKit();
    }

    void onReset() override {
        seed = 0x64726d73;   // initialize = the factory kit
        layout = LAYOUT_FAMILY;
        variety = 0;
        regenKit();
        for (auto& v : voices) v = Voice{};
        memset(prevGate, 0, sizeof(prevGate));
    }

    // ── kit generation ────────────────────────────────────────────────────────

    static uint32_t xorshift(uint32_t& s) {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return s;
    }

    void regenKit() {
        for (int i = 0; i < 64; i++) {
            uint32_t rng = seed ^ (uint32_t)(i * 2654435761u);
            for (int k = 0; k < 4; k++) xorshift(rng);
            auto rnd = [&]() { return (xorshift(rng) >> 8) / 16777216.f; };
            // col 0 → 1: pitch rises across the row, decay tightens slightly
            float spread = (i % 8) / 7.f;
            // Fully random layout draws the family per cell; the other layouts
            // keep the row = family stream so the same seed makes the same sounds.
            int family = (layout == LAYOUT_RANDOM) ? (int)(rnd() * 7.999f) : i / 8;
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
            c.pan = 0.5f + (rnd() - 0.5f) * 0.6f;   // gentle stereo spread

            // Variety draws come last so pre-variety seeds keep their sounds.
            c.shape   = rnd() * rnd();              // biased toward subtle
            c.fmRatio = 0.5f + 5.5f * rnd();
            c.fmAmt   = 8.f * rnd() * rnd();
            c.rmRatio = 1.25f + 6.f * rnd();
            c.rmAmt   = 0.3f + 0.7f * rnd();
            c.reso    = rnd();
            c.rise    = rnd() < 0.35f;
            c.bpFc    = std::sqrt(std::max(c.hpFc, 150.f) * std::max(c.lpFc, 300.f));
        }

        // Shuffled layout: the exact same 64 sounds, permuted by the seed.
        if (layout == LAYOUT_SHUFFLED) {
            uint32_t rng = seed ^ 0x9e3779b9u;
            for (int k = 0; k < 4; k++) xorshift(rng);
            for (int i = 63; i > 0; i--) {
                int j = xorshift(rng) % (uint32_t)(i + 1);
                std::swap(cells[i], cells[j]);
            }
        }
    }

    void startVoice(int cell) {
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

    // ── process ───────────────────────────────────────────────────────────────

    void process(const ProcessArgs& args) override {
        for (int in = 0; in < 4; in++) {
            int chs = inputs[CELL_INPUT + in].getChannels();
            for (int ch = 0; ch < chs && ch < 16; ch++) {
                int  cell = in * 16 + ch;
                bool gate = inputs[CELL_INPUT + in].getVoltage(ch) >= 1.f;
                if (gate && !prevGate[cell])
                    startVoice(cell);
                prevGate[cell] = gate;
            }
        }

        float mixL = 0.f, mixR = 0.f;
        float dt = args.sampleTime;
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
                if (variety & VAR_FM) {
                    v.fmPhase += freq * c.fmRatio * dt;
                    v.fmPhase -= (int) v.fmPhase;
                    ph += c.fmAmt * v.env * std::sin(2.f * (float)M_PI * v.fmPhase);
                }
                float s = std::sin(ph);
                if ((variety & VAR_FOLD) && c.shape > 0.01f)
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
                if ((variety & VAR_RESO) && c.reso > 0.05f) {
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

        outputs[MIXL_OUTPUT].setVoltage(clamp(mixL * 7.f, -11.f, 11.f));
        outputs[MIXR_OUTPUT].setVoltage(clamp(mixR * 7.f, -11.f, 11.f));
    }

    // ── serialisation ─────────────────────────────────────────────────────────

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "seed", json_integer((json_int_t) seed));
        json_object_set_new(root, "layout", json_integer(layout));
        json_object_set_new(root, "variety", json_integer(variety));
        return root;
    }

    void dataFromJson(json_t* root) override {
        json_t* j;
        if ((j = json_object_get(root, "seed")))
            seed = (uint32_t) json_integer_value(j);
        if ((j = json_object_get(root, "layout")))
            layout = clamp((int) json_integer_value(j), 0, 2);
        if ((j = json_object_get(root, "variety")))
            variety = (int) json_integer_value(j) & VAR_ALL;
        regenKit();
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
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuItem("Reroll kit", "",
            [=]() { m->seed = random::u32(); m->regenKit(); }));

        menu->addChild(createIndexSubmenuItem("Layout",
            {"Families by row", "Shuffled", "Fully random"},
            [=]() { return m->layout; },
            [=](int v) { m->layout = v; m->regenKit(); }));

        // Toggles gate the already-drawn recipe, so no regen: flipping one
        // audits the same kit with/without that ingredient.
        int on = __builtin_popcount((unsigned) m->variety);
        menu->addChild(createSubmenuItem("Variety",
            on ? string::f("%d on", on) : "off",
            [=](Menu* sub) {
                sub->addChild(createMenuItem("All on", "",
                    [=]() { m->variety = Drums64::VAR_ALL; }));
                sub->addChild(createMenuItem("All off", "",
                    [=]() { m->variety = 0; }));
                sub->addChild(new MenuSeparator);
                auto item = [&](const char* name, int bit) {
                    sub->addChild(createCheckMenuItem(name, "",
                        [=]() { return (m->variety & bit) != 0; },
                        [=]() { m->variety ^= bit; }));
                };
                item("Fold — brighter, driven sines", Drums64::VAR_FOLD);
                item("FM — metallic, clangy", Drums64::VAR_FM);
                item("Ring mod — growl, sidebands", Drums64::VAR_RING);
                item("Resonant noise — zaps, lasers", Drums64::VAR_RESO);
                item("Rising pitch — whoops", Drums64::VAR_RISE);
            }));
    }
};

Model* modelDrums64 = createModel<Drums64, Drums64Widget>("64Drums");
