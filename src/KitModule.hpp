#pragma once
#include "plugin.hpp"
#include "Scales.hpp"

// ── KitModule ─────────────────────────────────────────────────────────────────
// Shared shell for kit companions (64Drums, 64Objects, 64Grains): 64-cell
// gate format in (4 × 16ch poly), stereo mix out, one seeded sound recipe per
// cell. The shell owns the seed contract (serialize, reroll on request,
// Initialize = factory kit), the Layout / Quantize / key-follow options and
// their serialization, gate edge detection, and the mix output. process() is
// sealed; a kit implements:
//
//   regenKit()                 rebuild all 64 recipes from the seed + options
//   cellTriggered(cell)        a cell's gate rose this frame — start a voice
//   renderMix(l, r, dt)        add one sample of every active voice
//   kitReset()                 clear voices on Initialize
//   kitToJson()/kitFromJson()  kit-specific extras (regenKit runs after load)
//
// Variety convention (gate-don't-skip): every optional-feature parameter is
// always drawn from the per-cell RNG stream and only *gated* by its toggle,
// so flipping a toggle A/Bs the identical kit and old seeds keep their exact
// sound. Each feature also rolls a per-cell gate so part of the kit stays
// clean. Toggles therefore never call regenKit().

namespace P64 {

// The per-cell deterministic RNG stream (seed × cell index, warmed up).
struct KitRng {
    uint32_t s;
    KitRng(uint32_t seed, int cell) : s(seed ^ (uint32_t)(cell * 2654435761u)) {
        for (int k = 0; k < 4; k++) next();
    }
    uint32_t next() {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return s;
    }
    float uni() { return (next() >> 8) / 16777216.f; }
};

// Seed-deterministic Fisher–Yates permutation (the Shuffled layout).
template <typename T>
void kitShuffle(T* cells, int n, uint32_t seed) {
    uint32_t s = seed ^ 0x9e3779b9u;
    auto next = [&]() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; };
    for (int k = 0; k < 4; k++) next();
    for (int i = n - 1; i > 0; i--) {
        int j = next() % (uint32_t)(i + 1);
        std::swap(cells[i], cells[j]);
    }
}

} // namespace P64

struct KitModule : Module {
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

    enum Layout    { LAYOUT_FAMILY, LAYOUT_SHUFFLED, LAYOUT_RANDOM };
    enum QuantMode { QUANT_OFF, QUANT_NEAREST, QUANT_WALK };

    const uint32_t factorySeed;
    const int varietyAll;        // OR of the kit's variety bits
    const int numFamilies;       // family catalog size; may exceed the 8 grid
                                 // rows — extras are reachable via Row families
    uint32_t seed;
    int layout = LAYOUT_FAMILY;
    int rowFamily[8] = {0, 1, 2, 3, 4, 5, 6, 7};   // per-row family map
    int variety = 0;
    int quantMode = QUANT_OFF;
    int rootNote = 0;            // 0–11, C…B
    int scaleIndex = 0;          // index into P64::SCALES
    bool followKey = true;       // track Base64's global key (root + scale)
    uint32_t keySerial = 0;
    float mixGain = 7.f;
    bool prevGate[64] = {};

    KitModule(uint32_t factorySeed, int varietyAll, int numFamilies = 8)
        : factorySeed(factorySeed), varietyAll(varietyAll),
          numFamilies(numFamilies), seed(factorySeed) {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (int i = 0; i < 4; i++)
            configInput(CELL_INPUT + i,
                string::f("Rows %d-%d cell gates (poly 16ch)", i * 2 + 1, i * 2 + 2));
        configOutput(MIXL_OUTPUT, "Mix left");
        configOutput(MIXR_OUTPUT, "Mix right");
    }

    // ── kit hooks ─────────────────────────────────────────────────────────────

    virtual void regenKit() = 0;
    virtual void cellTriggered(int cell) = 0;
    virtual void renderMix(float& mixL, float& mixR, float dt) = 0;
    virtual void kitReset() {}
    virtual void kitToJson(json_t* root) {}
    virtual void kitFromJson(json_t* root) {}

    // ── shared recipe helpers ─────────────────────────────────────────────────

    // Family for a cell: its row's mapped family (identity by default, the
    // Row families menu can point every row at any family), unless the Fully
    // random layout draws one. (Draws from the stream only in that layout,
    // keeping other layouts' streams — and therefore sounds — identical
    // across layout switches.)
    int cellFamily(P64::KitRng& rng, int cell) {
        // (numFamilies - 0.001f is bit-identical to the historical 7.999f
        // when the catalog is 8, keeping old random-layout kits unchanged)
        return layout == LAYOUT_RANDOM ? (int)(rng.uni() * (numFamilies - 0.001f))
                                       : rowFamily[cell / 8];
    }

    // Nearest note of the current root/scale to frequency f (Hz).
    float quantizeFreq(float f) {
        const P64::Scale& sc = P64::SCALES[scaleIndex];
        float st = 12.f * std::log2(f / 16.3516f);   // semitones above C0
        int   ref = (int) std::round(st);
        int   best = ref;
        float bestDist = 1e9f;
        for (int s = ref - 6; s <= ref + 6; s++) {
            int pc = ((s - rootNote) % 12 + 12) % 12;
            for (int d = 0; d < sc.size; d++)
                if (sc.deg[d] == pc) {
                    float dist = std::fabs(s - st);
                    if (dist < bestDist) { bestDist = dist; best = s; }
                    break;
                }
        }
        return 16.3516f * std::exp2(best / 12.f);
    }

    // Column-walks-the-scale: col consecutive scale degrees up from the root
    // octave nearest the family's register.
    float walkFreq(float baseFreq, int col) {
        const P64::Scale& sc = P64::SCALES[scaleIndex];
        float st  = 12.f * std::log2(baseFreq / 16.3516f);
        int   oct = (int) std::round((st - rootNote) / 12.f);
        int   semis = rootNote + 12 * oct + P64::degreeToSemitone(sc, col);
        return 16.3516f * std::exp2(semis / 12.f);
    }

    // ── sealed frame path ─────────────────────────────────────────────────────

    void process(const ProcessArgs& args) final {
        if (P64::followSharedKey(followKey, keySerial, rootNote, scaleIndex)
                && quantMode != QUANT_OFF)
            regenKit();

        for (int in = 0; in < 4; in++) {
            int chs = inputs[CELL_INPUT + in].getChannels();
            for (int ch = 0; ch < chs && ch < 16; ch++) {
                int  cell = in * 16 + ch;
                bool gate = inputs[CELL_INPUT + in].getVoltage(ch) >= 1.f;
                if (gate && !prevGate[cell])
                    cellTriggered(cell);
                prevGate[cell] = gate;
            }
        }

        float mixL = 0.f, mixR = 0.f;
        renderMix(mixL, mixR, args.sampleTime);
        outputs[MIXL_OUTPUT].setVoltage(clamp(mixL * mixGain, -11.f, 11.f));
        outputs[MIXR_OUTPUT].setVoltage(clamp(mixR * mixGain, -11.f, 11.f));
    }

    void onReset() override {
        seed = factorySeed;   // initialize = the factory kit
        layout = LAYOUT_FAMILY;
        for (int r = 0; r < 8; r++) rowFamily[r] = r;
        variety = 0;
        quantMode = QUANT_OFF;
        rootNote = 0;
        scaleIndex = 0;
        followKey = true;
        keySerial = 0;
        memset(prevGate, 0, sizeof(prevGate));
        kitReset();
        regenKit();
    }

    // ── serialisation ─────────────────────────────────────────────────────────

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "seed", json_integer((json_int_t) seed));
        json_object_set_new(root, "layout", json_integer(layout));
        json_t* jr = json_array();
        for (int r = 0; r < 8; r++)
            json_array_append_new(jr, json_integer(rowFamily[r]));
        json_object_set_new(root, "rowFamily", jr);
        json_object_set_new(root, "variety", json_integer(variety));
        json_object_set_new(root, "quantMode", json_integer(quantMode));
        json_object_set_new(root, "rootNote", json_integer(rootNote));
        json_object_set_new(root, "scaleIndex", json_integer(scaleIndex));
        json_object_set_new(root, "followKey", json_boolean(followKey));
        kitToJson(root);
        return root;
    }

    void dataFromJson(json_t* root) override {
        json_t* j;
        if ((j = json_object_get(root, "seed")))
            seed = (uint32_t) json_integer_value(j);
        if ((j = json_object_get(root, "layout")))
            layout = clamp((int) json_integer_value(j), 0, 2);
        if ((j = json_object_get(root, "rowFamily")))
            for (int r = 0; r < 8; r++) {
                json_t* je = json_array_get(j, r);
                if (je) rowFamily[r] =
                    clamp((int) json_integer_value(je), 0, numFamilies - 1);
            }
        if ((j = json_object_get(root, "variety")))
            variety = (int) json_integer_value(j) & varietyAll;
        if ((j = json_object_get(root, "quantMode")))
            quantMode = clamp((int) json_integer_value(j), 0, 2);
        if ((j = json_object_get(root, "rootNote")))
            rootNote = clamp((int) json_integer_value(j), 0, 11);
        if ((j = json_object_get(root, "scaleIndex")))
            scaleIndex = clamp((int) json_integer_value(j), 0, P64::NUM_SCALES - 1);
        followKey = false;   // patches from before the global key stay local
        if ((j = json_object_get(root, "followKey")))
            followKey = json_boolean_value(j);
        keySerial = 0;       // re-sync on the first frame if following
        kitFromJson(root);
        regenKit();
    }
};

namespace P64 {

// The shared kit context menu: Reroll, Layout, Row families, Quantize, key
// follow/override, and the Variety submenu built from the kit's ingredient
// list. `families` names the kit's generator types in family-index order;
// the first 8 are the default top→bottom rows, any beyond that are
// reachable only through the Row families menu.
struct KitVarietyItem { const char* name; int bit; };

inline void appendKitMenu(Menu* menu, KitModule* m,
                          const std::vector<std::string>& families,
                          const std::vector<KitVarietyItem>& vars) {
    menu->addChild(new MenuSeparator);
    menu->addChild(createMenuItem("Reroll kit", "",
        [=]() { m->seed = random::u32(); m->regenKit(); }));

    menu->addChild(createIndexSubmenuItem("Layout",
        {"Families by row", "Shuffled", "Fully random"},
        [=]() { return m->layout; },
        [=](int v) { m->layout = v; m->regenKit(); }));

    // Point any row at any family (e.g. a full grid of one generator type).
    bool custom = false;
    for (int r = 0; r < 8; r++)
        if (m->rowFamily[r] != r) custom = true;
    menu->addChild(createSubmenuItem("Row families",
        custom ? "custom" : "",
        [=](Menu* sub) {
            for (int r = 0; r < 8; r++)
                sub->addChild(createIndexSubmenuItem(string::f("Row %d", r + 1),
                    families,
                    [=]() { return m->rowFamily[r]; },
                    [=](int v) { m->rowFamily[r] = v; m->regenKit(); }));
            sub->addChild(new MenuSeparator);
            sub->addChild(createMenuItem("Reset to one per row", "",
                [=]() {
                    for (int r = 0; r < 8; r++) m->rowFamily[r] = r;
                    m->regenKit();
                }));
        }));

    menu->addChild(createIndexSubmenuItem("Quantize",
        {"Off", "Nearest scale note", "Columns walk the scale"},
        [=]() { return m->quantMode; },
        [=](int v) { m->quantMode = v; m->regenKit(); }));

    menu->addChild(createCheckMenuItem("Follow Base64 global key", "",
        [=]() { return m->followKey; },
        [=]() {
            m->followKey = !m->followKey;
            m->keySerial = 0;   // adopt the global key on the next frame
        }));

    // Picking a local scale or root is the override gesture: follow turns off.
    std::vector<std::string> scaleNames;
    for (int i = 0; i < P64::NUM_SCALES; i++)
        scaleNames.push_back(P64::SCALES[i].name);
    menu->addChild(createIndexSubmenuItem("Scale", scaleNames,
        [=]() { return m->scaleIndex; },
        [=](int v) { m->followKey = false; m->scaleIndex = v; m->regenKit(); }));

    menu->addChild(createIndexSubmenuItem("Root note",
        {P64::NOTE_NAMES, P64::NOTE_NAMES + 12},
        [=]() { return m->rootNote; },
        [=](int v) { m->followKey = false; m->rootNote = v; m->regenKit(); }));

    // Toggles gate the already-drawn recipe, so no regen: flipping one
    // audits the same kit with/without that ingredient.
    int on = __builtin_popcount((unsigned) m->variety);
    menu->addChild(createSubmenuItem("Variety",
        on ? string::f("%d on", on) : "off",
        [=](Menu* sub) {
            sub->addChild(createMenuItem("All on", "",
                [=]() { m->variety = m->varietyAll; }));
            sub->addChild(createMenuItem("All off", "",
                [=]() { m->variety = 0; }));
            sub->addChild(new MenuSeparator);
            for (const auto& v : vars) {
                int bit = v.bit;
                sub->addChild(createCheckMenuItem(v.name, "",
                    [=]() { return (m->variety & bit) != 0; },
                    [=]() { m->variety ^= bit; }));
            }
        }));
}

} // namespace P64
