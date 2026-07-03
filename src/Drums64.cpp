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
    };
    Cell cells[64];

    struct Voice {
        bool  active = false;
        int   cell   = -1;
        float phase = 0.f, pitchEnv = 0.f, env = 0.f;
        float lp = 0.f, hpLp = 0.f;   // one-pole states (LP, and LP behind the HP)
    };
    Voice voices[DRUM_VOICES];

    enum Layout { LAYOUT_FAMILY, LAYOUT_SHUFFLED, LAYOUT_RANDOM };

    uint32_t seed = 0x64726d73;
    int layout = LAYOUT_FAMILY;
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
            switch (family) {
                case 7:   // kick
                    c = {(40.f + 18.f * spread) * jitter, 0.25f + 0.15f * rnd(),
                         3.f, 25.f, 1.f, 0.04f, 400.f, 0.f, 0.f};
                    break;
                case 6:   // tom
                    c = {(85.f + 80.f * spread) * jitter, 0.18f + 0.12f * rnd(),
                         1.f, 18.f, 1.f, 0.08f, 800.f, 0.f, 0.f};
                    break;
                case 5:   // snare
                    c = {(165.f + 60.f * spread) * jitter, 0.12f + 0.08f * rnd(),
                         0.5f, 30.f, 0.5f, 0.8f, 5000.f + 3000.f * rnd(), 300.f, 0.f};
                    break;
                case 4:   // clap
                    c = {0.f, 0.08f + 0.07f * rnd(),
                         0.f, 0.f, 0.f, 1.f, 3500.f + 2500.f * rnd(), 600.f + 400.f * spread, 0.f};
                    break;
                case 3:   // perc blip
                    c = {(380.f + 520.f * spread) * jitter, 0.05f + 0.05f * rnd(),
                         0.5f, 40.f, 1.f, 0.05f, 2000.f, 0.f, 0.f};
                    break;
                case 2:   // closed hat
                    c = {0.f, 0.03f + 0.03f * rnd(),
                         0.f, 0.f, 0.f, 1.f, 12000.f, 6000.f + 2500.f * spread, 0.f};
                    break;
                case 1:   // open hat
                    c = {0.f, 0.2f + 0.2f * rnd(),
                         0.f, 0.f, 0.f, 1.f, 12000.f, 6000.f + 2500.f * spread, 0.f};
                    break;
                default:  // click
                    c = {(2000.f + 1800.f * spread) * jitter, 0.015f + 0.015f * rnd(),
                         0.f, 0.f, 0.3f, 0.7f, 14000.f, 8000.f, 0.f};
                    break;
            }
            c.pan = 0.5f + (rnd() - 0.5f) * 0.6f;   // gentle stereo spread
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
                float freq = c.f0 * (1.f + c.pitchAmt * v.pitchEnv * v.pitchEnv);
                v.phase += freq * dt;
                v.phase -= (int) v.phase;
                out += c.sineAmt * std::sin(2.f * M_PI * v.phase);
            }
            if (c.noiseAmt > 0.f) {
                float n = random::uniform() * 2.f - 1.f;
                if (c.hpFc > 0.f) {
                    v.hpLp += clamp(2.f * M_PI * c.hpFc * dt, 0.f, 1.f) * (n - v.hpLp);
                    n -= v.hpLp;   // one-pole highpass
                }
                v.lp += clamp(2.f * M_PI * c.lpFc * dt, 0.f, 1.f) * (n - v.lp);
                out += c.noiseAmt * v.lp;
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
        return root;
    }

    void dataFromJson(json_t* root) override {
        json_t* j;
        if ((j = json_object_get(root, "seed")))
            seed = (uint32_t) json_integer_value(j);
        if ((j = json_object_get(root, "layout")))
            layout = clamp((int) json_integer_value(j), 0, 2);
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
    }
};

Model* modelDrums64 = createModel<Drums64, Drums64Widget>("64Drums");
