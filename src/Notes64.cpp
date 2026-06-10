#include "plugin.hpp"
#include "Scales.hpp"

// ── 64Notes ───────────────────────────────────────────────────────────────────
// Companion (non-page) module: maps the 64-cell gate format emitted by Gome64 /
// Buttons64 (4 polyphonic cables, 16 channels each) to pitched polyphony.
// A voice allocator with stealing collapses the 64 cells into 1-16 voices on
// poly V/OCT + GATE + RTRG outputs. Pure CV in → CV out; no Launchpad chain.

static const float NOTES_FIXED_LENS[]      = {0.05f, 0.1f, 0.2f, 0.5f, 1.f, 2.f};
static const char* NOTES_FIXED_LEN_NAMES[] = {"50 ms", "100 ms", "200 ms", "500 ms", "1 s", "2 s"};
static const int   NOTES_CLOCK_TICKS[]     = {1, 2, 4, 8};

struct Notes64 : Module {
    enum ParamIds  { NUM_PARAMS };
    enum InputIds  {
        ENUMS(CELL_INPUT, 4),   // 4 polyphonic inputs, 16 channels each = 64 cell gates
        TRANSPOSE_INPUT,
        CLOCK_INPUT,
        NUM_INPUTS
    };
    enum OutputIds {
        PITCH_OUTPUT,
        GATE_OUTPUT,
        RETRIG_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds  { NUM_LIGHTS };

    // ── configuration (all in the context menu) ───────────────────────────────
    int arrangement   = 2;   // 0=1D scale, 1=2D isomorphic, 2=2D scale grid, 3=chord per cell
    int scaleIndex    = 0;   // Major
    int rootNote      = 0;   // C
    int octave        = 3;   // base MIDI = 12*(octave+1) + rootNote → C3 = 48
    int colInterval   = 2;   // isomorphic: semitones per column (rightward)
    int rowInterval   = 5;   // isomorphic: semitones per row (downward)
    int rowDegrees    = 3;   // scale grid / chords: degrees per row (3 = diatonic fourth)
    int chordType     = 0;   // 0 = triad, 1 = seventh
    int maxPoly       = 8;   // output channels (1-16)
    int stealMode     = 0;   // 0=oldest 1=newest 2=lowest 3=highest 4=round-robin 5=off
    int lenMode       = 1;   // 0=gate follow, 1=fixed time, 2=clock-synced
    int fixedLenIndex = 2;   // 200 ms
    int clockTicksIndex = 0; // 1 tick

    // ── note map (rebuilt on any config change) ───────────────────────────────
    uint8_t noteMap[64][4];
    int8_t  noteCount[64];

    // ── voice allocator ────────────────────────────────────────────────────────
    struct Voice {
        bool    active = false;
        int     cell   = -1;
        uint8_t note   = 0;
        float   pitch  = 0.f;   // volts, pre-transpose
        int64_t age    = 0;
        float   timer  = 0.f;   // fixed mode: seconds remaining
        int     ticks  = 0;     // clock mode: ticks remaining
        dsp::PulseGenerator retrig;
    };
    Voice   voices[16];
    int64_t ageCounter = 0;
    int     rrIndex    = 0;
    bool    prevCell[64] = {};
    bool    prevConnected[4] = {};
    bool    prevClock    = false;

    Notes64() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (int i = 0; i < 4; i++)
            configInput(CELL_INPUT + i,
                string::f("Rows %d-%d cell gates (poly 16ch)", i * 2 + 1, i * 2 + 2));
        configInput(TRANSPOSE_INPUT, "Transpose (1V/oct)");
        configInput(CLOCK_INPUT, "Clock (for clock-synced note length)");
        configOutput(PITCH_OUTPUT,  "Pitch (1V/oct, poly)");
        configOutput(GATE_OUTPUT,   "Gate (poly)");
        configOutput(RETRIG_OUTPUT, "Retrigger (poly)");
        rebuildNoteMap();
    }

    void onReset() override {
        arrangement     = 2;
        scaleIndex      = 0;
        rootNote        = 0;
        octave          = 3;
        colInterval     = 2;
        rowInterval     = 5;
        rowDegrees      = 3;
        chordType       = 0;
        maxPoly         = 8;
        stealMode       = 0;
        lenMode         = 1;
        fixedLenIndex   = 2;
        clockTicksIndex = 0;
        clearVoices();
        rebuildNoteMap();
    }

    void clearVoices() {
        for (auto& v : voices)
            v = Voice{};
        memset(prevCell, 0, sizeof(prevCell));
        ageCounter = 0;
        rrIndex    = 0;
    }

    // ── cell → pitch map ───────────────────────────────────────────────────────
    // Pitch rises toward the bottom-right of the grid, matching Gome64's
    // pattern conventions (dRow down / dCol right = up in pitch).

    void rebuildNoteMap() {
        const P64::Scale& sc = P64::SCALES[scaleIndex];
        int base = 12 * (octave + 1) + rootNote;
        for (int cell = 0; cell < 64; cell++) {
            int row = cell / 8;
            int col = cell % 8;
            int midi[4] = {};
            int count   = 1;
            switch (arrangement) {
                case 0:   // 1D scale: degrees run left→right, top→bottom
                    midi[0] = base + P64::degreeToSemitone(sc, row * 8 + col);
                    break;
                case 1:   // 2D isomorphic: fixed semitone intervals
                    midi[0] = base + col * colInterval + row * rowInterval;
                    break;
                case 2:   // 2D scale grid (gome-native): in-key everywhere
                    midi[0] = base + P64::degreeToSemitone(sc, col + rowDegrees * row);
                    break;
                case 3: { // chord per cell: stacked thirds through the scale
                    int d = col + rowDegrees * row;
                    count = (chordType == 1) ? 4 : 3;
                    for (int n = 0; n < count; n++)
                        midi[n] = base + P64::degreeToSemitone(sc, d + 2 * n);
                } break;
            }
            noteCount[cell] = (int8_t) count;
            for (int n = 0; n < count; n++)
                noteMap[cell][n] = (uint8_t) clamp(midi[n], 0, 127);
        }
    }

    // ── voice allocation ───────────────────────────────────────────────────────

    void startVoice(int v, int cell, uint8_t note) {
        voices[v].active = true;
        voices[v].cell   = cell;
        voices[v].note   = note;
        voices[v].pitch  = (note - 60) / 12.f;
        voices[v].age    = ++ageCounter;
        voices[v].timer  = NOTES_FIXED_LENS[fixedLenIndex];
        voices[v].ticks  = NOTES_CLOCK_TICKS[clockTicksIndex];
        voices[v].retrig.trigger(1e-3f);
    }

    int chooseSteal() {
        int   best = -1;
        switch (stealMode) {
            case 0: {  // oldest
                int64_t bestAge = INT64_MAX;
                for (int v = 0; v < maxPoly; v++)
                    if (voices[v].age < bestAge) { bestAge = voices[v].age; best = v; }
            } break;
            case 1: {  // newest
                int64_t bestAge = -1;
                for (int v = 0; v < maxPoly; v++)
                    if (voices[v].age > bestAge) { bestAge = voices[v].age; best = v; }
            } break;
            case 2: {  // lowest pitch
                float bestPitch = INFINITY;
                for (int v = 0; v < maxPoly; v++)
                    if (voices[v].pitch < bestPitch) { bestPitch = voices[v].pitch; best = v; }
            } break;
            case 3: {  // highest pitch
                float bestPitch = -INFINITY;
                for (int v = 0; v < maxPoly; v++)
                    if (voices[v].pitch > bestPitch) { bestPitch = voices[v].pitch; best = v; }
            } break;
            case 4:    // round-robin
                best = rrIndex++ % maxPoly;
                break;
            default:   // off: drop the new note
                break;
        }
        return best;
    }

    void noteOn(int cell) {
        for (int n = 0; n < noteCount[cell]; n++) {
            uint8_t note = noteMap[cell][n];
            // Same cell+note already sounding → retrigger in place
            int target = -1;
            for (int v = 0; v < maxPoly; v++)
                if (voices[v].active && voices[v].cell == cell && voices[v].note == note) {
                    target = v;
                    break;
                }
            if (target < 0)
                for (int v = 0; v < maxPoly; v++)
                    if (!voices[v].active) { target = v; break; }
            if (target < 0)
                target = chooseSteal();
            if (target >= 0)
                startVoice(target, cell, note);
        }
    }

    void releaseCell(int cell) {
        for (int v = 0; v < maxPoly; v++)
            if (voices[v].active && voices[v].cell == cell)
                voices[v].active = false;
    }

    // ── process ───────────────────────────────────────────────────────────────

    void process(const ProcessArgs& args) override {
        // Clock-synced note length: count down ticks on each rising edge
        bool clockHigh = inputs[CLOCK_INPUT].getVoltage() >= 1.0f;
        bool clockTick = clockHigh && !prevClock;
        prevClock = clockHigh;

        for (int v = 0; v < maxPoly; v++) {
            if (!voices[v].active) continue;
            if (lenMode == 1) {
                voices[v].timer -= args.sampleTime;
                if (voices[v].timer <= 0.f)
                    voices[v].active = false;
            } else if (lenMode == 2 && clockTick) {
                if (--voices[v].ticks <= 0)
                    voices[v].active = false;
            }
        }

        // Scan the 64 cell gates and edge-detect (disconnected inputs are free)
        for (int in = 0; in < 4; in++) {
            if (!inputs[CELL_INPUT + in].isConnected()) {
                if (prevConnected[in]) {
                    // cable pulled: falling edge on every held cell of this input
                    for (int ch = 0; ch < 16; ch++) {
                        int cell = (in * 2 + ch / 8) * 8 + ch % 8;
                        if (prevCell[cell] && lenMode == 0)
                            releaseCell(cell);
                        prevCell[cell] = false;
                    }
                    prevConnected[in] = false;
                }
                continue;
            }
            prevConnected[in] = true;
            for (int ch = 0; ch < 16; ch++) {
                int  cell = (in * 2 + ch / 8) * 8 + ch % 8;
                bool high = inputs[CELL_INPUT + in].getVoltage(ch) >= 1.0f;
                if (high && !prevCell[cell])
                    noteOn(cell);
                else if (!high && prevCell[cell] && lenMode == 0)
                    releaseCell(cell);
                prevCell[cell] = high;
            }
        }

        // Outputs
        float transpose = inputs[TRANSPOSE_INPUT].getVoltage();
        outputs[PITCH_OUTPUT].setChannels(maxPoly);
        outputs[GATE_OUTPUT].setChannels(maxPoly);
        outputs[RETRIG_OUTPUT].setChannels(maxPoly);
        for (int v = 0; v < maxPoly; v++) {
            outputs[PITCH_OUTPUT].setVoltage(voices[v].pitch + transpose, v);
            outputs[GATE_OUTPUT].setVoltage(voices[v].active ? 10.f : 0.f, v);
            outputs[RETRIG_OUTPUT].setVoltage(voices[v].retrig.process(args.sampleTime) ? 10.f : 0.f, v);
        }
    }

    // ── serialisation ─────────────────────────────────────────────────────────

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "arrangement",     json_integer(arrangement));
        json_object_set_new(root, "scale",           json_integer(scaleIndex));
        json_object_set_new(root, "root",            json_integer(rootNote));
        json_object_set_new(root, "octave",          json_integer(octave));
        json_object_set_new(root, "colInterval",     json_integer(colInterval));
        json_object_set_new(root, "rowInterval",     json_integer(rowInterval));
        json_object_set_new(root, "rowDegrees",      json_integer(rowDegrees));
        json_object_set_new(root, "chordType",       json_integer(chordType));
        json_object_set_new(root, "maxPoly",         json_integer(maxPoly));
        json_object_set_new(root, "stealMode",       json_integer(stealMode));
        json_object_set_new(root, "lenMode",         json_integer(lenMode));
        json_object_set_new(root, "fixedLenIndex",   json_integer(fixedLenIndex));
        json_object_set_new(root, "clockTicksIndex", json_integer(clockTicksIndex));
        return root;
    }

    void dataFromJson(json_t* root) override {
        json_t* j;
        if ((j = json_object_get(root, "arrangement")))
            arrangement = clamp((int)json_integer_value(j), 0, 3);
        if ((j = json_object_get(root, "scale")))
            scaleIndex = clamp((int)json_integer_value(j), 0, P64::NUM_SCALES - 1);
        if ((j = json_object_get(root, "root")))
            rootNote = clamp((int)json_integer_value(j), 0, 11);
        if ((j = json_object_get(root, "octave")))
            octave = clamp((int)json_integer_value(j), 0, 7);
        if ((j = json_object_get(root, "colInterval")))
            colInterval = clamp((int)json_integer_value(j), 1, 7);
        if ((j = json_object_get(root, "rowInterval")))
            rowInterval = clamp((int)json_integer_value(j), 1, 12);
        if ((j = json_object_get(root, "rowDegrees")))
            rowDegrees = clamp((int)json_integer_value(j), 1, 7);
        if ((j = json_object_get(root, "chordType")))
            chordType = clamp((int)json_integer_value(j), 0, 1);
        if ((j = json_object_get(root, "maxPoly")))
            maxPoly = clamp((int)json_integer_value(j), 1, 16);
        if ((j = json_object_get(root, "stealMode")))
            stealMode = clamp((int)json_integer_value(j), 0, 5);
        if ((j = json_object_get(root, "lenMode")))
            lenMode = clamp((int)json_integer_value(j), 0, 2);
        if ((j = json_object_get(root, "fixedLenIndex")))
            fixedLenIndex = clamp((int)json_integer_value(j), 0, 5);
        if ((j = json_object_get(root, "clockTicksIndex")))
            clockTicksIndex = clamp((int)json_integer_value(j), 0, 3);
        rebuildNoteMap();
    }
};

// ── Widget ────────────────────────────────────────────────────────────────────

struct Notes64Widget : ModuleWidget {
    Notes64Widget(Notes64* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Notes64.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Inputs (left column): 4 cell gates, transpose, clock
        const float cellY[4] = {30.f, 42.f, 54.f, 66.f};
        for (int i = 0; i < 4; i++)
            addInput(createInputCentered<PJ301MPort>(
                mm2px(Vec(14.f, cellY[i])), module, Notes64::CELL_INPUT + i));
        addInput(createInputCentered<PJ301MPort>(
            mm2px(Vec(14.f, 82.f)), module, Notes64::TRANSPOSE_INPUT));
        addInput(createInputCentered<PJ301MPort>(
            mm2px(Vec(14.f, 94.f)), module, Notes64::CLOCK_INPUT));

        // Outputs (right column): pitch, gate, retrigger
        addOutput(createOutputCentered<PJ301MPort>(
            mm2px(Vec(38.f, 70.f)), module, Notes64::PITCH_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(
            mm2px(Vec(38.f, 82.f)), module, Notes64::GATE_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(
            mm2px(Vec(38.f, 94.f)), module, Notes64::RETRIG_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        Notes64* m = getModule<Notes64>();
        menu->addChild(new MenuSeparator);

        menu->addChild(createIndexSubmenuItem("Arrangement",
            {"1D scale", "2D isomorphic", "2D scale grid", "Chord per cell"},
            [=]() { return m->arrangement; },
            [=](int v) { m->arrangement = v; m->rebuildNoteMap(); }));

        std::vector<std::string> scaleNames;
        for (int i = 0; i < P64::NUM_SCALES; i++)
            scaleNames.push_back(P64::SCALES[i].name);
        menu->addChild(createIndexSubmenuItem("Scale", scaleNames,
            [=]() { return m->scaleIndex; },
            [=](int v) { m->scaleIndex = v; m->rebuildNoteMap(); }));

        menu->addChild(createIndexSubmenuItem("Root note",
            {P64::NOTE_NAMES, P64::NOTE_NAMES + 12},
            [=]() { return m->rootNote; },
            [=](int v) { m->rootNote = v; m->rebuildNoteMap(); }));

        std::vector<std::string> octaveNames;
        for (int i = 0; i <= 7; i++)
            octaveNames.push_back(string::f("%d", i));
        menu->addChild(createIndexSubmenuItem("Base octave", octaveNames,
            [=]() { return m->octave; },
            [=](int v) { m->octave = v; m->rebuildNoteMap(); }));

        menu->addChild(createSubmenuItem("Isomorphic intervals", "", [=](Menu* sub) {
            sub->addChild(createIndexSubmenuItem("Column (semitones)",
                {"1", "2", "3", "4", "5", "6", "7"},
                [=]() { return m->colInterval - 1; },
                [=](int v) { m->colInterval = v + 1; m->rebuildNoteMap(); }));
            sub->addChild(createIndexSubmenuItem("Row (semitones)",
                {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12"},
                [=]() { return m->rowInterval - 1; },
                [=](int v) { m->rowInterval = v + 1; m->rebuildNoteMap(); }));
        }));

        menu->addChild(createIndexSubmenuItem("Row interval (scale degrees)",
            {"1", "2", "3", "4", "5", "6", "7"},
            [=]() { return m->rowDegrees - 1; },
            [=](int v) { m->rowDegrees = v + 1; m->rebuildNoteMap(); }));

        menu->addChild(createIndexSubmenuItem("Chord type",
            {"Triad", "Seventh"},
            [=]() { return m->chordType; },
            [=](int v) { m->chordType = v; m->rebuildNoteMap(); }));

        menu->addChild(new MenuSeparator);

        std::vector<std::string> polyNames;
        for (int i = 1; i <= 16; i++)
            polyNames.push_back(string::f("%d", i));
        menu->addChild(createIndexSubmenuItem("Polyphony", polyNames,
            [=]() { return m->maxPoly - 1; },
            [=](int v) {
                m->maxPoly = v + 1;
                // silence voices beyond the new channel count
                for (int i = m->maxPoly; i < 16; i++)
                    m->voices[i].active = false;
            }));

        menu->addChild(createIndexSubmenuItem("Voice stealing",
            {"Oldest", "Newest", "Lowest", "Highest", "Round-robin", "Off"},
            [=]() { return m->stealMode; },
            [=](int v) { m->stealMode = v; }));

        menu->addChild(createIndexSubmenuItem("Note length",
            {"Gate follow", "Fixed time", "Clock-synced"},
            [=]() { return m->lenMode; },
            [=](int v) { m->lenMode = v; }));

        menu->addChild(createIndexSubmenuItem("Fixed length",
            {NOTES_FIXED_LEN_NAMES, NOTES_FIXED_LEN_NAMES + 6},
            [=]() { return m->fixedLenIndex; },
            [=](int v) { m->fixedLenIndex = v; }));

        menu->addChild(createIndexSubmenuItem("Clock-synced length",
            {"1 tick", "2 ticks", "4 ticks", "8 ticks"},
            [=]() { return m->clockTicksIndex; },
            [=](int v) { m->clockTicksIndex = v; }));
    }
};

Model* modelNotes64 = createModel<Notes64, Notes64Widget>("64Notes");
