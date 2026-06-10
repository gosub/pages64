#include "PageModule.hpp"

static constexpr int GOME_CLOCK_DIVS[] = {1,2,3,4,6,8,12,16,24,32,48,64};

static constexpr int NUM_PATTERNS = 8;
static constexpr int MAX_STEPS    = 16;

// A pattern is an ordered 2D sequence of (dRow,dCol) cell offsets relative to a
// root. Step 0 is always (0,0) — the root itself. dRow increases downward,
// dCol increases rightward (pitch rises toward bottom-right on the companion grid).
struct GomePattern {
    int    len;
    int8_t dRow[MAX_STEPS];
    int8_t dCol[MAX_STEPS];
};

// Built-in default arp shapes (a few classic moves; users record their own).
static const GomePattern GOME_DEFAULTS[NUM_PATTERNS] = {
    {1, {0},          {0}},                          // single note
    {4, {0,0,0,0},    {0,1,2,3}},                    // run right (up a scale)
    {4, {0,1,2,3},    {0,0,0,0}},                    // run down (up by fourths)
    {4, {0,0,0,0},    {0,1,2,1}},                    // up-down along the row
    {4, {0,1,2,3},    {0,1,2,3}},                    // diagonal climb
    {3, {0,1,2},      {0,0,0}},                       // triad in fourths
    {4, {0,0,1,1},    {0,2,0,2}},                     // box
    {6, {0,0,0,1,1,1},{0,1,2,0,1,2}},                 // two-row sweep
};

struct Gome64 : PageModule {
    enum ParamIds  { NUM_PARAMS };
    enum InputIds  { NUM_INPUTS };
    enum OutputIds {
        ENUMS(CELL_OUTPUT, 4),   // 4 polyphonic outputs, 16 channels each = 64 cell gates
        NUM_OUTPUTS
    };
    enum LightIds {
        ENUMS(ACTIVE_LIGHT, 2),
        NUM_LIGHTS
    };

    GomePattern patterns[NUM_PATTERNS];
    int  currentPattern = 0;            // selected by the top row; global to all walkers

    // Per-cell walker state: every held/latched cell is a root running the current pattern.
    bool held[64]     = {};
    int  rootStep[64];                  // step index per active root (-1 = inactive)
    dsp::PulseGenerator cellPulse[64];  // 5 ms gate on each *target* cell

    bool loopMode  = false;             // scene A: latched + sustained (else momentary hold)
    bool recording = false;             // scene B: record arm
    int  recRoot   = -1;                // first tapped cell during record (offset origin)

    int  offGridMode = 0;               // 0 = skip (default), 1 = wrap, 2 = clamp

    int  clockDiv = 1, clockDivCount = 0;

    // Colors
    uint8_t rootColor         = P64::LED_GREEN_DIM;
    uint8_t fireColor         = P64::LED_GREEN;
    uint8_t recColor          = P64::LED_RED;
    uint8_t activePageColor   = P64::LED_GREEN;
    uint8_t inactivePageColor = P64::LED_AMBER_DIM;

    Gome64() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (int i = 0; i < 4; i++)
            configOutput(CELL_OUTPUT + i,
                string::f("Rows %d-%d gates (poly 16ch)", i * 2 + 1, i * 2 + 2));
        loadDefaults();
        for (int i = 0; i < 64; i++) rootStep[i] = -1;
    }

    void loadDefaults() {
        for (int p = 0; p < NUM_PATTERNS; p++)
            patterns[p] = GOME_DEFAULTS[p];
    }

    void onReset() override {
        PageModule::onReset();
        loadDefaults();
        currentPattern = 0;
        memset(held, 0, sizeof(held));
        for (int i = 0; i < 64; i++) rootStep[i] = -1;
        loopMode      = false;
        recording     = false;
        recRoot       = -1;
        offGridMode   = 0;
        clockDiv      = 1;
        clockDivCount = 0;
        rootColor         = P64::LED_GREEN_DIM;
        fireColor         = P64::LED_GREEN;
        recColor          = P64::LED_RED;
        activePageColor   = P64::LED_GREEN;
        inactivePageColor = P64::LED_AMBER_DIM;
    }

    // Resolve a root + offset to a target cell index, or -1 if it falls off-grid
    // under "skip" mode.
    int resolveTarget(int rootIdx, int dRow, int dCol) const {
        int r = rootIdx / 8 + dRow;
        int c = rootIdx % 8 + dCol;
        switch (offGridMode) {
            case 1:  r = ((r % 8) + 8) % 8; c = ((c % 8) + 8) % 8; break;  // wrap
            case 2:  r = clamp(r, 0, 7);    c = clamp(c, 0, 7);    break;  // clamp
            default: if (r < 0 || r > 7 || c < 0 || c > 7) return -1;      // skip
        }
        return r * 8 + c;
    }

    // ── virtual hooks ─────────────────────────────────────────────────────────

    void pagePreProcess() override {
        auto* msg = reinterpret_cast<P64::LeftMessage*>(leftExpander.consumerMessage);
        if (!msg) return;

        // Reset input: restart all walkers and the divider.
        if (msg->resetTick) {
            for (int i = 0; i < 64; i++)
                if (held[i]) rootStep[i] = 0;
            clockDivCount = 0;
        }

        bool tick = false;
        if (msg->clockTick) {
            if (++clockDivCount >= clockDiv) {
                clockDivCount = 0;
                tick = true;
            }
        }

        if (tick && !recording) {
            const GomePattern& pat = patterns[currentPattern];
            int len = pat.len < 1 ? 1 : pat.len;
            for (int idx = 0; idx < 64; idx++) {
                if (!held[idx] || rootStep[idx] < 0) continue;
                int s = rootStep[idx] % len;
                int target = resolveTarget(idx, pat.dRow[s], pat.dCol[s]);
                if (target >= 0)
                    cellPulse[target].trigger(0.005f);
                rootStep[idx] = (s + 1) % len;
            }
            ledsDirty = true;
        }
    }

    void pageActive(const P64::LeftMessage& msg) override {
        // Scene A: loop mode (latched sustain). Turning it off stops all roots.
        if (msg.sceneEvent[0] && msg.sceneVelocity[0] > 0) {
            loopMode = !loopMode;
            if (!loopMode) {
                memset(held, 0, sizeof(held));
                for (int i = 0; i < 64; i++) rootStep[i] = -1;
            }
            ledsDirty = true;
        }

        // Scene B: record arm. Entering clears the selected pattern; exiting finalizes.
        if (msg.sceneEvent[1] && msg.sceneVelocity[1] > 0) {
            recording = !recording;
            if (recording) {
                patterns[currentPattern].len = 0;
                recRoot = -1;
                memset(held, 0, sizeof(held));
                for (int i = 0; i < 64; i++) rootStep[i] = -1;
            } else if (patterns[currentPattern].len == 0) {
                patterns[currentPattern].len     = 1;   // never leave a zero-length pattern
                patterns[currentPattern].dRow[0] = 0;
                patterns[currentPattern].dCol[0] = 0;
            }
            ledsDirty = true;
        }

        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                int note = row * 16 + col;
                if (!msg.noteEvent[note]) continue;
                bool pressed = msg.noteVelocity[note] > 0;
                int  idx     = row * 8 + col;

                if (recording) {
                    if (!pressed) continue;
                    GomePattern& pat = patterns[currentPattern];
                    if (recRoot < 0) {
                        recRoot     = idx;
                        pat.dRow[0] = 0;
                        pat.dCol[0] = 0;
                        pat.len     = 1;
                    } else if (pat.len < MAX_STEPS) {
                        pat.dRow[pat.len] = (int8_t)(row - recRoot / 8);
                        pat.dCol[pat.len] = (int8_t)(col - recRoot % 8);
                        pat.len++;
                    }
                    ledsDirty = true;
                    continue;
                }

                if (row == 0) {
                    // Top grid row is the pattern selector strip (rows 1-7 play).
                    if (!pressed) continue;
                    currentPattern = col;
                    int len = patterns[currentPattern].len < 1 ? 1 : patterns[currentPattern].len;
                    for (int i = 0; i < 64; i++)
                        if (rootStep[i] >= 0) rootStep[i] %= len;
                    ledsDirty = true;
                    continue;
                }

                if (loopMode) {
                    // Latch: tap to start, tap again to stop.
                    if (!pressed) continue;
                    if (held[idx]) {
                        held[idx]     = false;
                        rootStep[idx] = -1;
                    } else {
                        held[idx]     = true;
                        rootStep[idx] = 0;
                    }
                    ledsDirty = true;
                } else {
                    // Momentary: hold to play, release to stop.
                    held[idx]     = pressed;
                    rootStep[idx] = pressed ? 0 : -1;
                    ledsDirty = true;
                }
            }
        }
    }

    void pageInactive() override {
        if (recording) return;     // keep an in-progress recording across page switches
        if (loopMode)   return;    // latched arpeggios keep running in the background
        // Momentary roots can't receive their note-off from another page; clear them.
        bool changed = false;
        for (int i = 0; i < 64; i++) {
            if (held[i]) {
                held[i]     = false;
                rootStep[i] = -1;
                changed = true;
            }
        }
        if (changed) ledsDirty = true;
    }

    void rebuildLeds() override {
        memset(ledState, P64::LED_OFF, sizeof(ledState));

        if (recording) {
            if (recRoot >= 0) {
                ledState[recRoot] = fireColor;
                const GomePattern& pat = patterns[currentPattern];
                for (int s = 1; s < pat.len; s++) {
                    int target = resolveTarget(recRoot, pat.dRow[s], pat.dCol[s]);
                    if (target >= 0) ledState[target] = recColor;
                }
            }
            return;
        }

        for (int i = 8; i < 64; i++) {   // rows 1-7: roots and firing cells
            if (cellPulse[i].remaining > 0.f)
                ledState[i] = fireColor;
            else if (held[i])
                ledState[i] = rootColor;
        }
        // Row 0: pattern selector strip (active pattern highlighted).
        for (int col = 0; col < NUM_PATTERNS; col++)
            ledState[col] = (col == currentPattern) ? activePageColor : inactivePageColor;
    }

    void buildSceneLeds(uint8_t sceneLeds[8]) override {
        memset(sceneLeds, P64::LED_OFF, 8);
        if (loopMode)  sceneLeds[0] = P64::LED_AMBER;
        if (recording) sceneLeds[1] = P64::LED_RED;
    }

    void updateOutputs() override {
        for (int out = 0; out < 4; out++) {
            outputs[CELL_OUTPUT + out].setChannels(16);
            for (int localRow = 0; localRow < 2; localRow++) {
                for (int col = 0; col < 8; col++) {
                    int idx = (out * 2 + localRow) * 8 + col;
                    float v = cellPulse[idx].process(sampleTime) ? 10.f : 0.f;
                    outputs[CELL_OUTPUT + out].setVoltage(v, localRow * 8 + col);
                }
            }
        }
    }

    // ── serialisation ─────────────────────────────────────────────────────────

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "currentPattern",    json_integer(currentPattern));
        json_object_set_new(root, "loopMode",          json_boolean(loopMode));
        json_object_set_new(root, "offGridMode",       json_integer(offGridMode));
        json_object_set_new(root, "clockDiv",          json_integer(clockDiv));
        json_object_set_new(root, "rootColor",         json_integer(rootColor));
        json_object_set_new(root, "fireColor",         json_integer(fireColor));
        json_object_set_new(root, "recColor",          json_integer(recColor));
        json_object_set_new(root, "activePageColor",   json_integer(activePageColor));
        json_object_set_new(root, "inactivePageColor", json_integer(inactivePageColor));
        json_t* pats = json_array();
        for (int p = 0; p < NUM_PATTERNS; p++) {
            json_t* jp = json_object();
            json_object_set_new(jp, "len", json_integer(patterns[p].len));
            json_t* dr = json_array();
            json_t* dc = json_array();
            for (int s = 0; s < patterns[p].len; s++) {
                json_array_append_new(dr, json_integer(patterns[p].dRow[s]));
                json_array_append_new(dc, json_integer(patterns[p].dCol[s]));
            }
            json_object_set_new(jp, "dRow", dr);
            json_object_set_new(jp, "dCol", dc);
            json_array_append_new(pats, jp);
        }
        json_object_set_new(root, "patterns", pats);
        return root;
    }

    void dataFromJson(json_t* root) override {
        json_t* j;
        if ((j = json_object_get(root, "currentPattern")))
            currentPattern = clamp((int)json_integer_value(j), 0, NUM_PATTERNS - 1);
        if ((j = json_object_get(root, "loopMode")))
            loopMode = json_boolean_value(j);
        if ((j = json_object_get(root, "offGridMode")))
            offGridMode = clamp((int)json_integer_value(j), 0, 2);
        if ((j = json_object_get(root, "clockDiv")))
            clockDiv = clamp((int)json_integer_value(j), 1, 64);
        if ((j = json_object_get(root, "rootColor")))
            rootColor = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "fireColor")))
            fireColor = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "recColor")))
            recColor = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "activePageColor")))
            activePageColor = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "inactivePageColor")))
            inactivePageColor = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "patterns"))) {
            for (int p = 0; p < NUM_PATTERNS; p++) {
                json_t* jp = json_array_get(j, p);
                if (!jp) continue;
                json_t* jl = json_object_get(jp, "len");
                json_t* dr = json_object_get(jp, "dRow");
                json_t* dc = json_object_get(jp, "dCol");
                int len = jl ? clamp((int)json_integer_value(jl), 1, MAX_STEPS) : 1;
                patterns[p].len = len;
                for (int s = 0; s < len; s++) {
                    json_t* vr = dr ? json_array_get(dr, s) : nullptr;
                    json_t* vc = dc ? json_array_get(dc, s) : nullptr;
                    patterns[p].dRow[s] = vr ? (int8_t)json_integer_value(vr) : 0;
                    patterns[p].dCol[s] = vc ? (int8_t)json_integer_value(vc) : 0;
                }
            }
        }
        // Live state is transient; don't restore held/recording.
        recording = false;
        recRoot   = -1;
        memset(held, 0, sizeof(held));
        for (int i = 0; i < 64; i++) rootStep[i] = -1;
        ledsDirty = true;
    }
};

// ── Widget ────────────────────────────────────────────────────────────────────

struct Gome64Widget : ModuleWidget {
    Gome64Widget(Gome64* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Gome64.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addChild(createLightCentered<SmallLight<GreenRedLight>>(
            mm2px(Vec(6.0f, 18.0f)), module, Gome64::ACTIVE_LIGHT));

        const float jackY[4] = {32.0f, 56.0f, 80.0f, 104.0f};
        for (int i = 0; i < 4; i++) {
            addOutput(createOutputCentered<PJ301MPort>(
                mm2px(Vec(20.0f, jackY[i])), module, Gome64::CELL_OUTPUT + i));
        }
    }

    void colorMenu(Menu* menu, const char* label, uint8_t Gome64::* field) {
        Gome64* m = getModule<Gome64>();
        menu->addChild(createSubmenuItem(label, "", [=](Menu* sub) {
            for (auto& c : P64::LED_COLOR_DEFS) {
                if (c.velocity == P64::LED_OFF) continue;
                uint8_t vel = c.velocity;
                sub->addChild(createCheckMenuItem(c.name, "",
                    [=]() { return m->*field == vel; },
                    [=]() { m->*field = vel; m->ledsDirty = true; }
                ));
            }
        }));
    }

    void appendContextMenu(Menu* menu) override {
        Gome64* m = getModule<Gome64>();
        menu->addChild(new MenuSeparator);
        menu->addChild(createSubmenuItem("Clock divider", "", [=](Menu* sub) {
            for (int d : GOME_CLOCK_DIVS) {
                sub->addChild(createCheckMenuItem(string::f("÷%d", d), "",
                    [=]() { return m->clockDiv == d; },
                    [=]() { m->clockDiv = d; m->clockDivCount = 0; }
                ));
            }
        }));
        menu->addChild(createSubmenuItem("Off-grid behavior", "", [=](Menu* sub) {
            const char* names[3] = {"Skip", "Wrap", "Clamp"};
            for (int i = 0; i < 3; i++) {
                int mode = i;
                sub->addChild(createCheckMenuItem(names[i], "",
                    [=]() { return m->offGridMode == mode; },
                    [=]() { m->offGridMode = mode; m->ledsDirty = true; }
                ));
            }
        }));
        colorMenu(menu, "Root color",          &Gome64::rootColor);
        colorMenu(menu, "Fire color",          &Gome64::fireColor);
        colorMenu(menu, "Record color",        &Gome64::recColor);
        colorMenu(menu, "Active pattern color",   &Gome64::activePageColor);
        colorMenu(menu, "Inactive pattern color", &Gome64::inactivePageColor);
    }
};

Model* modelGome64 = createModel<Gome64, Gome64Widget>("Gome64");
