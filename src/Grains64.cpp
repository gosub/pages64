#include "KitModule.hpp"

// ── 64Grains ──────────────────────────────────────────────────────────────────
// Kit companion: synthetic microsound on the KitModule shell. Every cell
// trigger spawns a seeded micro-event *cloud* — a handful of windowed sine or
// noise grains scheduled over tens to hundreds of milliseconds. Row picks the
// texture (top→bottom: dust, crackle, glitch, chirp, trainlet, bubble, hiss,
// rumble), column the register/density. No sample buffer, no granular
// sampling — grains are synthesized. Per-hit micro-timing is free-running;
// the *recipes* are what the seed guarantees. Variety extras (reverse,
// accelerando, sweep, glide) follow the shell's gate-don't-skip convention.
// Full design: docs/design/Grains64.md.

static constexpr int G_CLOUDS = 16;
static constexpr int G_GRAINS = 96;

struct Grains64 : KitModule {
    enum Variety {
        VAR_REV   = 1 << 0,
        VAR_ACCEL = 1 << 1,
        VAR_SWEEP = 1 << 2,
        VAR_GLIDE = 1 << 3,
        VAR_ALL   = (1 << 4) - 1,
    };

    // Per-cell cloud recipe, fixed at kit generation
    struct Cell {
        bool  noise;        // grain source: filtered noise vs. sine
        bool  regular;      // trainlet: grain spacing = 1/f0, exactly
        float f0;           // pitch center (Hz; 0 = unpitched, quantize skips)
        int   count;        // grains per cloud
        float cloudLen;     // s
        float grainLen;     // s
        float pitchScat;    // per-grain scatter (semitones)
        float glide;        // per-grain glide (octaves over the grain)
        float lpFc, hpFc;   // noise color
        float crush;        // 0–1: sample-hold + level quantize (glitch)
        float attack;       // window attack fraction 0–0.9
        float scatter;      // per-grain stereo scatter 0–1
        float pan;          // cloud center
        // Variety recipe — always drawn, applied only when the toggle is on.
        bool  rev;          // cloud swells into the end (VAR_REV)
        float accel;        // last/first interval ratio (VAR_ACCEL; 1 = clean)
        float sweep;        // pan travel across the cloud (VAR_SWEEP)
        float glideX;       // extra per-grain glide (VAR_GLIDE)
    };
    Cell cells[64];

    struct Cloud {
        bool     active = false;
        int      cell = -1;
        float    t = 0.f, next = 0.f;
        int      spawned = 0;
        uint32_t rs = 1;    // free-running per-hit micro-jitter stream
    };
    Cloud clouds[G_CLOUDS];

    struct Grain {
        bool  active = false;
        bool  noise = false;
        float t = 0.f, dur = 1.f, attack = 0.5f;
        float phase = 0.f, freq = 0.f, fMul = 1.f;
        float amp = 0.f, gl = 0.f, gr = 0.f;
        float lp = 0.f, hp = 0.f, lpC = 0.f, hpC = 0.f;
        float crush = 0.f, hold = 0.f;
        int   holdN = 0, holdDiv = 1;
    };
    Grain grains[G_GRAINS];

    Grains64() : KitModule(0x67726e73, VAR_ALL) {
        regenKit();
    }

    void kitReset() override {
        for (auto& c : clouds) c = Cloud{};
        for (auto& g : grains) g = Grain{};
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
            c.scatter = 0.5f;

            switch (family) {
                case 0:   // dust — sparse single clicks
                    c.noise    = true;
                    c.count    = 2 + (int)(4.f * rnd());
                    c.cloudLen = 0.12f + 0.25f * rnd();
                    c.grainLen = 0.0008f + 0.0015f * rnd();
                    c.lpFc     = 6000.f + 7000.f * rnd();
                    c.hpFc     = 1000.f + 1500.f * spread;
                    c.attack   = 0.15f;
                    c.scatter  = 0.7f;
                    break;
                case 1:   // crackle — dense chaotic micro-pops
                    c.noise    = true;
                    c.count    = 16 + (int)(30.f * rnd());
                    c.cloudLen = 0.06f + 0.15f * rnd();
                    c.grainLen = 0.0004f + 0.0009f * rnd();
                    c.lpFc     = 9000.f + 5000.f * rnd();
                    c.hpFc     = 2500.f + 2000.f * spread;
                    c.attack   = 0.1f;
                    c.scatter  = 0.9f;
                    break;
                case 2:   // glitch — bit-reduced chirp fragments
                    c.f0        = (300.f + 1300.f * spread) * jitter;
                    c.count     = 4 + (int)(7.f * rnd());
                    c.cloudLen  = 0.08f + 0.18f * rnd();
                    c.grainLen  = 0.004f + 0.008f * rnd();
                    c.pitchScat = 14.f + 10.f * rnd();
                    c.crush     = 0.4f + 0.6f * rnd();
                    c.attack    = 0.05f;
                    break;
                case 3:   // chirp / glisson — pitch-swept sine grains
                    c.f0        = (350.f + 1050.f * spread) * jitter;
                    c.count     = 3 + (int)(6.f * rnd());
                    c.cloudLen  = 0.1f + 0.25f * rnd();
                    c.grainLen  = 0.008f + 0.017f * rnd();
                    c.pitchScat = 4.f + 6.f * rnd();
                    c.glide     = (rnd() < 0.5f ? -1.f : 1.f) * (0.8f + 1.5f * rnd());
                    c.attack    = 0.3f;
                    break;
                case 4:   // trainlet — pitched click train (rate = f0)
                    c.noise    = true;
                    c.regular  = true;
                    c.f0       = (55.f + 165.f * spread) * jitter;
                    c.cloudLen = 0.12f + 0.2f * rnd();
                    c.grainLen = 0.0012f + 0.0015f * rnd();
                    c.lpFc     = 3000.f + 5000.f * rnd();
                    c.attack   = 0.1f;
                    break;
                case 5:   // bubble — upward blips
                    c.f0        = (280.f + 620.f * spread) * jitter;
                    c.count     = 2 + (int)(4.f * rnd());
                    c.cloudLen  = 0.15f + 0.2f * rnd();
                    c.grainLen  = 0.015f + 0.03f * rnd();
                    c.pitchScat = 3.f + 4.f * rnd();
                    c.glide     = 0.7f + 1.3f * rnd();
                    c.attack    = 0.5f;
                    break;
                case 6:   // hiss — shaped noise bursts
                    c.noise    = true;
                    c.count    = 3 + (int)(6.f * rnd());
                    c.cloudLen = 0.1f + 0.25f * rnd();
                    c.grainLen = 0.012f + 0.025f * rnd();
                    c.lpFc     = 9000.f + 5000.f * rnd();
                    c.hpFc     = 2500.f + 3500.f * spread + 1000.f * rnd();
                    c.attack   = 0.4f;
                    break;
                default:  // rumble — low granular rolls
                    c.noise    = true;
                    c.count    = 5 + (int)(10.f * rnd());
                    c.cloudLen = 0.2f + 0.3f * rnd();
                    c.grainLen = 0.025f + 0.045f * rnd();
                    c.lpFc     = 80.f + 170.f * spread + 50.f * rnd();
                    c.attack   = 0.4f;
                    break;
            }

            // Quantize the pitch center (chirps, glitches, bubbles) or the
            // train rate; unpitched textures (f0 = 0) are untouched.
            if (quantMode != QUANT_OFF && c.f0 > 0.f) {
                if (quantMode == QUANT_WALK && layout == LAYOUT_FAMILY) {
                    static const float baseF[8] =
                        {0.f, 0.f, 300.f, 350.f, 55.f, 280.f, 0.f, 0.f};
                    c.f0 = walkFreq(baseF[family], i % 8);
                }
                else
                    c.f0 = quantizeFreq(c.f0);
            }
            if (c.regular)   // trainlet: grain count follows the (possibly
                             // quantized) rate so the cloud length holds
                c.count = clamp((int)(c.cloudLen * c.f0), 4, 48);

            c.pan = 0.5f + (rnd() - 0.5f) * 0.6f;

            // Variety draws come last; per-cell gates keep part of the kit
            // clean, gate + amount always drawn (stable stream).
            float g, a;
            c.rev = rnd() < 0.45f;
            g = rnd(); a = rnd();
            c.accel  = (g < 0.45f) ? std::exp2((a - 0.5f) * 3.f) : 1.f;
            g = rnd(); a = rnd();
            c.sweep  = (g < 0.45f) ? (a - 0.5f) * 2.2f : 0.f;
            g = rnd(); a = rnd();
            c.glideX = (g < 0.4f)  ? 0.5f + 1.2f * a : 0.f;
        }

        if (layout == LAYOUT_SHUFFLED)
            P64::kitShuffle(cells, 64, seed);
    }

    // ── clouds and grains ─────────────────────────────────────────────────────

    static float cu(Cloud& cl) {   // per-hit micro-jitter (free-running)
        cl.rs ^= cl.rs << 13; cl.rs ^= cl.rs >> 17; cl.rs ^= cl.rs << 5;
        return (cl.rs >> 8) / 16777216.f;
    }

    void cellTriggered(int cell) override {
        Cloud* cl = nullptr;
        for (auto& cand : clouds)
            if (!cand.active) { cl = &cand; break; }
        if (!cl) {   // steal the furthest-along cloud
            cl = &clouds[0];
            for (auto& cand : clouds)
                if (cand.t > cl->t) cl = &cand;
        }
        *cl = Cloud{};
        cl->active = true;
        cl->cell   = cell;
        cl->rs     = random::u32() | 1;
    }

    void spawnGrain(Cloud& cl, const Cell& c, float dt) {
        Grain* g = nullptr;
        for (auto& cand : grains)
            if (!cand.active) { g = &cand; break; }
        if (!g) {   // steal the grain closest to its end
            g = &grains[0];
            for (auto& cand : grains)
                if (cand.t / cand.dur > g->t / g->dur) g = &cand;
        }

        float pos = clamp(cl.t / c.cloudLen, 0.f, 1.f);
        *g = Grain{};
        g->active = true;
        g->noise  = c.noise;
        g->dur    = c.grainLen * (0.6f + 0.8f * cu(cl));
        g->attack = c.attack;
        g->amp    = 0.75f / std::pow((float) c.count, 0.35f);
        // cloud envelope: gently decaying, or swelling when reversed
        if ((variety & VAR_REV) && c.rev)
            g->amp *= 0.25f + 0.75f * pos * pos;
        else
            g->amp *= 1.f - 0.35f * pos;

        if (!c.noise) {
            g->freq = c.f0 * std::exp2(c.pitchScat * (cu(cl) - 0.5f) / 12.f);
            float gld = c.glide;
            if ((variety & VAR_GLIDE) && c.glideX > 0.f)
                gld += c.glideX * (cu(cl) - 0.5f) * 2.f;
            g->fMul = std::exp2(gld * dt / g->dur);
            if (c.crush > 0.f) {
                g->crush   = c.crush;
                g->holdDiv = 1 + (int)(c.crush * 0.004f / dt);   // ~4 ms · crush
                g->holdN   = 1;
            }
        }
        else {
            g->lpC = clamp(2.f * (float)M_PI * c.lpFc * dt, 0.f, 1.f);
            g->hpC = c.hpFc > 0.f
                   ? clamp(2.f * (float)M_PI * c.hpFc * dt, 0.f, 1.f) : 0.f;
        }

        float p = c.pan + c.scatter * (cu(cl) - 0.5f) * 0.8f;
        if ((variety & VAR_SWEEP) && c.sweep != 0.f)
            p += c.sweep * (pos - 0.5f);
        p = clamp(p, 0.f, 1.f);
        g->gl = 1.f - p;
        g->gr = p;

        // schedule the next grain
        float iv;
        if (c.regular)
            iv = 1.f / std::max(c.f0, 1.f);
        else
            iv = (c.cloudLen / c.count) * (0.4f + 1.2f * cu(cl));
        if ((variety & VAR_ACCEL) && c.accel != 1.f)
            iv *= std::pow(c.accel, (float) cl.spawned / c.count);
        cl.next = cl.t + iv;
        cl.spawned++;
    }

    void renderMix(float& mixL, float& mixR, float dt) override {
        for (auto& cl : clouds) {
            if (!cl.active) continue;
            const Cell& c = cells[cl.cell];
            if (cl.t >= cl.next && cl.spawned < c.count)
                spawnGrain(cl, c, dt);
            cl.t += dt;
            if (cl.spawned >= c.count)
                cl.active = false;   // remaining grains finish on their own
        }

        for (auto& g : grains) {
            if (!g.active) continue;
            float x = g.t / g.dur;
            float env = x < g.attack ? x / g.attack
                                     : (1.f - x) / (1.f - g.attack);
            float s;
            if (g.noise) {
                float n = random::uniform() * 2.f - 1.f;
                g.lp += g.lpC * (n - g.lp);
                s = g.lp;
                if (g.hpC > 0.f) {
                    g.hp += g.hpC * (s - g.hp);
                    s -= g.hp;
                }
            }
            else {
                s = std::sin(2.f * (float)M_PI * g.phase);
                g.phase += g.freq * dt;
                g.phase -= (int) g.phase;
                g.freq  *= g.fMul;
                if (g.crush > 0.f) {
                    if (--g.holdN <= 0) {
                        g.hold  = s;
                        g.holdN = g.holdDiv;
                    }
                    float q = 20.f - 14.f * g.crush;
                    s = std::round(g.hold * q) / q;
                }
            }
            s *= env * g.amp;
            mixL += s * g.gl;
            mixR += s * g.gr;

            g.t += dt;
            if (g.t >= g.dur)
                g.active = false;
        }
    }
};

// ── Widget ────────────────────────────────────────────────────────────────────

struct Grains64Widget : ModuleWidget {
    Grains64Widget(Grains64* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Grains64.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        for (int i = 0; i < 4; i++)
            addInput(createInputCentered<PJ301MPort>(
                mm2px(Vec(15.24f, 30.f + i * 14.f)), module, Grains64::CELL_INPUT + i));

        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(38.f, 94.f)),  module, Grains64::MIXL_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(38.f, 106.f)), module, Grains64::MIXR_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        Grains64* m = getModule<Grains64>();
        P64::appendKitMenu(menu, m,
            {"Dust", "Crackle", "Glitch", "Chirp",
             "Trainlet", "Bubble", "Hiss", "Rumble"},
            {
            {"Reverse — swell into the end",    Grains64::VAR_REV},
            {"Accelerando — trains speed/slow", Grains64::VAR_ACCEL},
            {"Sweep — pan across the cloud",    Grains64::VAR_SWEEP},
            {"Glide — pitch trajectories",      Grains64::VAR_GLIDE},
        });
    }
};

Model* modelGrains64 = createModel<Grains64, Grains64Widget>("64Grains");
