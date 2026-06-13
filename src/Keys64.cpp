#include "PageModule.hpp"
#include "Scales.hpp"

// ── Keys64 ────────────────────────────────────────────────────────────────────
// The grid as a playable isomorphic / scale keyboard. Each cell is a note
// (pitch rises up and to the right); pressing it plays it. A voice allocator
// collapses the held cells onto a polyphonic V/OCT + GATE + RTRG output, so it
// drives a poly voice directly — no 64Notes needed in the patch. Scale math is
// shared from Scales.hpp.
//
// Notes are momentary by default. Scene A is a latch switch: tap it (press and
// release with no note in between) to toggle a global latch mode where every
// press toggles a sustained note; or hold A and play to latch just those notes
// (they stay on after release, while other notes remain momentary). Turning the
// global latch mode off again clears all sustained notes.
//
// Held cells are lit bright; latched cells in the latch color; tonic cells
// (pitch class == root) dim for orientation.
//
// Scene B toggles an arpeggiator: while on, the held/latched notes are played
// one at a time on each (divided) clock tick, monophonically, in the selected
// pattern.

static const char* ARP_MODE_NAMES[] = {
    "Up", "Down", "Up-down", "Down-up", "Converge", "Diverge",
    "As played", "Alternating root", "Random", "Random (no repeat)"
};
static constexpr int NUM_ARP_MODES = sizeof(ARP_MODE_NAMES) / sizeof(ARP_MODE_NAMES[0]);

// Piano root selector on the Scale page: white keys (C D E F G A B) on one
// row, black keys (C# D# _ F# G# A#) on the row above, in their grid columns.
static const int WHITE_NOTE[7] = {0, 2, 4, 5, 7, 9, 11};
static const int BLACK_COL[5]  = {0, 1, 3, 4, 5};
static const int BLACK_NOTE[5] = {1, 3, 6, 8, 10};

struct Keys64 : PageModule {
    enum ParamIds { NUM_PARAMS };
    enum InputIds  { NUM_INPUTS };
    enum OutputIds {
        PITCH_OUTPUT,
        GATE_OUTPUT,
        RTRG_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        ENUMS(ACTIVE_LIGHT, 2),
        NUM_LIGHTS
    };

    static constexpr int SCENE_LATCH = 0;   // A
    static constexpr int SCENE_ARP   = 1;   // B

    // Layout
    int arrangement = 0;   // 0 = scale grid (in-key), 1 = isomorphic (chromatic)
    int scaleIndex  = 0;   // Major
    int rootNote    = 0;   // C
    int octave      = 3;   // base MIDI = 12*(octave+1) + root
    int rowDegrees  = 3;   // scale grid: degrees per row upward
    int colSemis    = 2;   // isomorphic: semitones per column
    int rowSemis    = 5;   // isomorphic: semitones per row upward

    // Voices
    int maxPoly   = 8;     // 1-16
    int stealMode = 0;     // oldest/newest/lowest/highest/round-robin/off

    uint8_t noteMap[64]    = {};
    bool    held[64]       = {};   // physically held (momentary)
    bool    latched[64]    = {};   // sustained (toggled on)
    bool    wasSounding[64] = {};  // for voice edge detection

    // Latch (scene A)
    bool latchMode    = false;     // global: every press is a toggle
    bool aHeld        = false;     // scene A physically down
    bool aPlayedNote  = false;     // a note was toggled while A was held

    struct Voice {
        bool    active = false;
        int     cell   = -1;
        uint8_t note   = 0;
        float   pitch  = 0.f;
        int64_t age    = 0;
        dsp::PulseGenerator retrig;
    } voices[16];
    int64_t ageCounter = 0;
    int     rrIndex    = 0;

    int     pressStamp[64] = {};   // order a cell started sounding (for "as-played")
    int64_t orderCounter   = 0;

    int  subPage = 0;     // 0 = play, 1 = scale options, 2 = arp options

    // Arpeggiator (scene B)
    bool arpOn   = false;
    int  arpMode = 0;     // see ARP_MODE_NAMES
    P64::ClockDivider clockDiv;
    int64_t arpSeqPos      = 0;
    int     arpLastIdx     = 0;
    float   arpPitch       = 0.f;
    bool    arpHasNote     = false;
    bool    arpWasSounding = false;
    dsp::PulseGenerator arpRetrig;

    uint8_t playColor  = P64::LED_GREEN;
    uint8_t latchColor = P64::LED_AMBER;
    uint8_t rootColor  = P64::LED_RED_DIM;
    uint8_t arpColor   = P64::LED_LIME;
    uint8_t pageColor  = P64::LED_AMBER;       // config-page selections (dim = available)

    Keys64() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configOutput(PITCH_OUTPUT, "Pitch (1V/oct, poly)");
        configOutput(GATE_OUTPUT,  "Gate (poly)");
        configOutput(RTRG_OUTPUT,  "Retrigger (poly)");
        rebuildNoteMap();
    }

    void onReset() override {
        PageModule::onReset();
        arrangement = 0;
        scaleIndex  = 0;
        rootNote    = 0;
        octave      = 3;
        rowDegrees  = 3;
        colSemis    = 2;
        rowSemis    = 5;
        maxPoly     = 8;
        stealMode   = 0;
        memset(held, 0, sizeof(held));
        memset(latched, 0, sizeof(latched));
        memset(wasSounding, 0, sizeof(wasSounding));
        latchMode = aHeld = aPlayedNote = false;
        for (auto& v : voices) v.active = false;
        subPage = 0;
        arpOn = false;
        arpMode = 0;
        clockDiv.set(1);
        arpSeqPos = 0;
        arpLastIdx = 0;
        arpHasNote = arpWasSounding = false;
        playColor  = P64::LED_GREEN;
        latchColor = P64::LED_AMBER;
        rootColor  = P64::LED_RED_DIM;
        arpColor   = P64::LED_LIME;
        pageColor  = P64::LED_AMBER;
        rebuildNoteMap();
    }

    // ── note layout ─────────────────────────────────────────────────────────────

    void rebuildNoteMap() {
        const P64::Scale& sc = P64::SCALES[scaleIndex];
        int base = 12 * (octave + 1) + rootNote;
        for (int row = 0; row < 8; row++) {
            int up = 7 - row;   // rows from the bottom; pitch rises upward
            for (int col = 0; col < 8; col++) {
                int midi = (arrangement == 0)
                    ? base + P64::degreeToSemitone(sc, col + rowDegrees * up)
                    : base + col * colSemis + up * rowSemis;
                noteMap[row * 8 + col] = (uint8_t) clamp(midi, 0, 127);
            }
        }
        ledsDirty = true;
    }

    // ── voice allocation ─────────────────────────────────────────────────────────

    void startVoice(int v, int cell, uint8_t note) {
        voices[v].active = true;
        voices[v].cell   = cell;
        voices[v].note   = note;
        voices[v].pitch  = (note - 60) / 12.f;
        voices[v].age    = ++ageCounter;
        voices[v].retrig.trigger(1e-3f);
    }

    int chooseSteal() {
        int best = -1;
        switch (stealMode) {
            case 0: { int64_t b = INT64_MAX; for (int v = 0; v < maxPoly; v++) if (voices[v].age < b) { b = voices[v].age; best = v; } } break;
            case 1: { int64_t b = -1;        for (int v = 0; v < maxPoly; v++) if (voices[v].age > b) { b = voices[v].age; best = v; } } break;
            case 2: { float b = INFINITY;    for (int v = 0; v < maxPoly; v++) if (voices[v].pitch < b) { b = voices[v].pitch; best = v; } } break;
            case 3: { float b = -INFINITY;   for (int v = 0; v < maxPoly; v++) if (voices[v].pitch > b) { b = voices[v].pitch; best = v; } } break;
            case 4: best = rrIndex++ % maxPoly; break;
            default: break;   // off: drop
        }
        return best;
    }

    void voiceOn(int cell) {
        uint8_t note = noteMap[cell];
        int target = -1;
        for (int v = 0; v < maxPoly; v++)
            if (voices[v].active && voices[v].cell == cell) { target = v; break; }
        if (target < 0)
            for (int v = 0; v < maxPoly; v++)
                if (!voices[v].active) { target = v; break; }
        if (target < 0)
            target = chooseSteal();
        if (target >= 0)
            startVoice(target, cell, note);
    }

    void voiceOff(int cell) {
        for (int v = 0; v < maxPoly; v++)
            if (voices[v].active && voices[v].cell == cell)
                voices[v].active = false;
    }

    // Drive the voices from the current sounding set (held | latched).
    void syncVoices() {
        for (int cell = 0; cell < 64; cell++) {
            bool sounding = held[cell] || latched[cell];
            if (sounding && !wasSounding[cell]) {
                pressStamp[cell] = ++orderCounter;
                voiceOn(cell);
            } else if (!sounding && wasSounding[cell]) {
                voiceOff(cell);
            }
            wasSounding[cell] = sounding;
        }
    }

    // ── arpeggiator ─────────────────────────────────────────────────────────────

    // Note index to play for the mode, given m sounding notes and step s.
    int arpIndex(int m, int64_t s) {
        if (m <= 1) return 0;
        switch (arpMode) {
            case 0: return s % m;                                      // up
            case 1: return (m - 1) - (s % m);                          // down
            case 2: { int per = 2*m-2, p = s % per; return p < m ? p : per - p; }          // up-down
            case 3: { int per = 2*m-2, p = s % per; return p < m ? (m-1-p) : (p-(m-1)); }  // down-up
            case 4: { int k = s % m; return (k%2==0) ? k/2 : m-1-(k/2); }                  // converge
            case 5: { int k = m-1-(int)(s % m); return (k%2==0) ? k/2 : m-1-(k/2); }       // diverge
            case 6: return s % m;                                      // as-played (pool pre-sorted)
            case 7: { int per = 2*(m-1), p = s % per; return (p%2==0) ? 0 : p/2+1; }       // alternating root
            case 8: return random::u32() % m;                          // random
            case 9: { int r = random::u32() % m; if (r == arpLastIdx) r = (r + 1) % m; return r; }  // random no-repeat
        }
        return 0;
    }

    void arpStep() {
        int pool[64], m = 0;
        for (int cell = 0; cell < 64; cell++)
            if (held[cell] || latched[cell]) pool[m++] = cell;
        if (m == 0) { arpHasNote = false; return; }

        if (arpMode == 6)   // as-played: order by when each note started sounding
            std::sort(pool, pool + m, [&](int a, int b) { return pressStamp[a] < pressStamp[b]; });
        else                // otherwise by pitch, low → high
            std::sort(pool, pool + m, [&](int a, int b) { return noteMap[a] < noteMap[b]; });

        int idx = arpIndex(m, arpSeqPos);
        arpLastIdx = idx;
        arpPitch   = (noteMap[pool[idx]] - 60) / 12.f;
        arpHasNote = true;
        arpRetrig.trigger(1e-3f);
        arpSeqPos++;
    }

    void pagePreProcess() override {
        auto* msg = reinterpret_cast<P64::LeftMessage*>(leftExpander.consumerMessage);
        if (!msg) return;
        if (msg->resetTick) {
            clockDiv.reset();
            arpSeqPos = 0;
        }
        if (arpOn && clockDiv.process(msg->clockTick))
            arpStep();
    }

    void clearLatched() {
        memset(latched, 0, sizeof(latched));
        ledsDirty = true;
    }

    // ── virtual hooks ─────────────────────────────────────────────────────────

    void pageActive(const P64::LeftMessage& msg) override {
        // Top buttons 1-3: page select (play / scale options / arp options).
        // Leaving the play page releases held momentary notes.
        for (int b = 0; b < 3; b++) {
            int cc = 104 + b;
            if (msg.ccEvent[cc] && msg.ccValue[cc] > 0 && subPage != b) {
                subPage = b;
                if (subPage != 0) {
                    for (int c = 0; c < 64; c++) held[c] = false;
                    aHeld = false;
                }
                ledsDirty = true;
            }
        }

        if (subPage == 0)      playPage(msg);
        else if (subPage == 1) scalePage(msg);
        else                   arpPage(msg);
    }

    void playPage(const P64::LeftMessage& msg) {
        // Scene A — latch switch.
        if (msg.sceneEvent[SCENE_LATCH]) {
            if (msg.sceneVelocity[SCENE_LATCH] > 0) {
                aHeld       = true;
                aPlayedNote = false;
            } else {
                aHeld = false;
                if (!aPlayedNote) {            // a tap: toggle global latch mode
                    latchMode = !latchMode;
                    if (!latchMode)
                        clearLatched();         // leaving latch mode clears sustains
                }
            }
            ledsDirty = true;
        }

        // Scene B — arpeggiator on/off.
        if (msg.sceneEvent[SCENE_ARP] && msg.sceneVelocity[SCENE_ARP] > 0) {
            arpOn = !arpOn;
            arpSeqPos = 0;
            ledsDirty = true;
        }

        // Grid: play notes. A press toggles a sustained note when the global
        // latch mode is on or scene A is held; otherwise it is momentary.
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                int note = row * 16 + col;
                if (!msg.noteEvent[note]) continue;
                int  cell    = row * 8 + col;
                bool pressed = msg.noteVelocity[note] > 0;
                if (pressed) {
                    if (latchMode || aHeld) {
                        latched[cell] = !latched[cell];
                        if (aHeld) aPlayedNote = true;
                    } else {
                        held[cell] = true;
                    }
                } else {
                    held[cell] = false;
                }
                ledsDirty = true;
            }
        }
    }

    // Scale page: scale, layout, root (piano), octave — set on the grid.
    void scalePage(const P64::LeftMessage& msg) {
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                if (!msg.noteEvent[row * 16 + col] || msg.noteVelocity[row * 16 + col] == 0)
                    continue;
                if (row == 0) {                              // scales 1-8
                    scaleIndex = col; rebuildNoteMap();
                } else if (row == 1) {                       // scales 9-12, layout
                    if (col < P64::NUM_SCALES - 8) { scaleIndex = 8 + col; rebuildNoteMap(); }
                    else if (col == 6)             { arrangement = 0; rebuildNoteMap(); }
                    else if (col == 7)             { arrangement = 1; rebuildNoteMap(); }
                } else if (row == 3) {                       // root: black keys
                    for (int k = 0; k < 5; k++)
                        if (BLACK_COL[k] == col) { rootNote = BLACK_NOTE[k]; rebuildNoteMap(); }
                } else if (row == 4 && col < 7) {            // root: white keys
                    rootNote = WHITE_NOTE[col]; rebuildNoteMap();
                } else if (row == 6) {                       // octave 0-7
                    octave = col; rebuildNoteMap();
                }
            }
        }
    }

    // Arp page: pick the arpeggiator mode (one cell each).
    void arpPage(const P64::LeftMessage& msg) {
        for (int row = 0; row < 2; row++) {
            for (int col = 0; col < 8; col++) {
                if (!msg.noteEvent[row * 16 + col] || msg.noteVelocity[row * 16 + col] == 0)
                    continue;
                int idx = row * 8 + col;
                if (idx < NUM_ARP_MODES) { arpMode = idx; ledsDirty = true; }
            }
        }
    }

    void pageInactive() override {
        // Release momentary notes (no note-off arrives once you leave the
        // page); latched notes keep sounding.
        bool any = false;
        for (int c = 0; c < 64; c++) if (held[c]) { held[c] = false; any = true; }
        aHeld = false;
        if (any) ledsDirty = true;
    }

    void rebuildLeds() override {
        uint8_t next[64];
        if (subPage == 1)      buildScaleLeds(next);
        else if (subPage == 2) buildArpLeds(next);
        else                   buildPlayLeds(next);
        for (int i = 0; i < 64; i++) {
            if (next[i] != ledState[i]) {
                ledState[i] = next[i];
                ledsDirty   = true;
            }
        }
    }

    void buildPlayLeds(uint8_t next[64]) {
        for (int cell = 0; cell < 64; cell++)
            next[cell] = latched[cell]                  ? latchColor
                       : held[cell]                     ? playColor
                       : (noteMap[cell] % 12 == rootNote) ? rootColor
                       :                                  P64::LED_OFF;
    }

    void buildScaleLeds(uint8_t next[64]) {
        memset(next, P64::LED_OFF, 64);
        uint8_t dim = P64::LED_AMBER_DIM;
        for (int s = 0; s < P64::NUM_SCALES; s++) {     // scale selector (rows 0-1)
            int r = s < 8 ? 0 : 1, c = s < 8 ? s : s - 8;
            next[r * 8 + c] = (s == scaleIndex) ? pageColor : dim;
        }
        next[1 * 8 + 6] = (arrangement == 0) ? pageColor : dim;   // layout
        next[1 * 8 + 7] = (arrangement == 1) ? pageColor : dim;
        for (int k = 0; k < 5; k++)                     // root: black keys (row 3)
            next[3 * 8 + BLACK_COL[k]] = (rootNote == BLACK_NOTE[k]) ? pageColor : dim;
        for (int c = 0; c < 7; c++)                     // root: white keys (row 4)
            next[4 * 8 + c] = (rootNote == WHITE_NOTE[c]) ? pageColor : dim;
        for (int c = 0; c < 8; c++)                     // octave (row 6)
            next[6 * 8 + c] = (octave == c) ? pageColor : dim;
    }

    void buildArpLeds(uint8_t next[64]) {
        memset(next, P64::LED_OFF, 64);
        for (int i = 0; i < NUM_ARP_MODES; i++) {
            int r = i < 8 ? 0 : 1, c = i < 8 ? i : i - 8;
            next[r * 8 + c] = (i == arpMode) ? pageColor : P64::LED_AMBER_DIM;
        }
    }

    void buildTopLeds(uint8_t topLeds[8]) override {
        memset(topLeds, P64::LED_OFF, 8);
        for (int b = 0; b < 3; b++)
            topLeds[b] = (b == subPage) ? pageColor : P64::LED_AMBER_DIM;
    }

    void buildSceneLeds(uint8_t sceneLeds[8]) override {
        memset(sceneLeds, P64::LED_OFF, 8);
        sceneLeds[SCENE_LATCH] = latchMode ? latchColor
                              : aHeld      ? P64::LED_AMBER_DIM
                              :              P64::LED_OFF;
        sceneLeds[SCENE_ARP] = arpOn ? arpColor : P64::LED_OFF;
    }

    void updateOutputs() override {
        syncVoices();

        if (arpOn) {
            // Monophonic: the arp steps through the held/latched notes.
            bool anySounding = false;
            for (int c = 0; c < 64 && !anySounding; c++) anySounding = held[c] || latched[c];
            if (anySounding && !arpWasSounding) {   // first note of a chord plays at once
                arpSeqPos = 0;
                arpStep();
            }
            arpWasSounding = anySounding;
            if (!anySounding) arpHasNote = false;

            outputs[PITCH_OUTPUT].setChannels(1);
            outputs[GATE_OUTPUT].setChannels(1);
            outputs[RTRG_OUTPUT].setChannels(1);
            outputs[PITCH_OUTPUT].setVoltage(arpPitch);
            outputs[GATE_OUTPUT].setVoltage(arpHasNote ? 10.f : 0.f);
            outputs[RTRG_OUTPUT].setVoltage(arpRetrig.process(sampleTime) ? 10.f : 0.f);
            return;
        }

        arpWasSounding = false;
        outputs[PITCH_OUTPUT].setChannels(maxPoly);
        outputs[GATE_OUTPUT].setChannels(maxPoly);
        outputs[RTRG_OUTPUT].setChannels(maxPoly);
        for (int v = 0; v < maxPoly; v++) {
            outputs[PITCH_OUTPUT].setVoltage(voices[v].pitch, v);
            outputs[GATE_OUTPUT].setVoltage(voices[v].active ? 10.f : 0.f, v);
            outputs[RTRG_OUTPUT].setVoltage(voices[v].retrig.process(sampleTime) ? 10.f : 0.f, v);
        }
    }

    // ── serialisation ─────────────────────────────────────────────────────────

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "arrangement", json_integer(arrangement));
        json_object_set_new(root, "scale",       json_integer(scaleIndex));
        json_object_set_new(root, "root",        json_integer(rootNote));
        json_object_set_new(root, "octave",      json_integer(octave));
        json_object_set_new(root, "rowDegrees",  json_integer(rowDegrees));
        json_object_set_new(root, "colSemis",    json_integer(colSemis));
        json_object_set_new(root, "rowSemis",    json_integer(rowSemis));
        json_object_set_new(root, "maxPoly",     json_integer(maxPoly));
        json_object_set_new(root, "stealMode",   json_integer(stealMode));
        json_object_set_new(root, "latchMode",   json_boolean(latchMode));
        json_object_set_new(root, "subPage",     json_integer(subPage));
        json_object_set_new(root, "arpOn",       json_boolean(arpOn));
        json_object_set_new(root, "arpMode",     json_integer(arpMode));
        json_object_set_new(root, "clockDiv",    json_integer(clockDiv.div));
        json_t* jl = json_array();
        for (int i = 0; i < 64; i++) json_array_append_new(jl, json_boolean(latched[i]));
        json_object_set_new(root, "latched",     jl);
        json_object_set_new(root, "playColor",   json_integer(playColor));
        json_object_set_new(root, "latchColor",  json_integer(latchColor));
        json_object_set_new(root, "rootColor",   json_integer(rootColor));
        return root;
    }

    void dataFromJson(json_t* root) override {
        json_t* j;
        if ((j = json_object_get(root, "arrangement"))) arrangement = clamp((int)json_integer_value(j), 0, 1);
        if ((j = json_object_get(root, "scale")))       scaleIndex  = clamp((int)json_integer_value(j), 0, P64::NUM_SCALES - 1);
        if ((j = json_object_get(root, "root")))        rootNote    = clamp((int)json_integer_value(j), 0, 11);
        if ((j = json_object_get(root, "octave")))      octave      = clamp((int)json_integer_value(j), 0, 7);
        if ((j = json_object_get(root, "rowDegrees")))  rowDegrees  = clamp((int)json_integer_value(j), 1, 7);
        if ((j = json_object_get(root, "colSemis")))    colSemis    = clamp((int)json_integer_value(j), 1, 7);
        if ((j = json_object_get(root, "rowSemis")))    rowSemis    = clamp((int)json_integer_value(j), 1, 12);
        if ((j = json_object_get(root, "maxPoly")))     maxPoly     = clamp((int)json_integer_value(j), 1, 16);
        if ((j = json_object_get(root, "stealMode")))   stealMode   = clamp((int)json_integer_value(j), 0, 5);
        if ((j = json_object_get(root, "latchMode")))   latchMode   = json_boolean_value(j);
        if ((j = json_object_get(root, "arpOn")))       arpOn       = json_boolean_value(j);
        if ((j = json_object_get(root, "subPage")))     subPage     = clamp((int)json_integer_value(j), 0, 2);
        if ((j = json_object_get(root, "arpMode")))     arpMode     = clamp((int)json_integer_value(j), 0, NUM_ARP_MODES - 1);
        if ((j = json_object_get(root, "clockDiv")))    clockDiv.set(clamp((int)json_integer_value(j), 1, 64));
        if ((j = json_object_get(root, "latched")))
            for (int i = 0; i < 64; i++) {
                json_t* v = json_array_get(j, i);
                if (v) latched[i] = json_boolean_value(v);
            }
        if ((j = json_object_get(root, "playColor")))   playColor   = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "latchColor")))  latchColor  = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "rootColor")))   rootColor   = (uint8_t)json_integer_value(j);
        rebuildNoteMap();
    }
};

// ── Widget ────────────────────────────────────────────────────────────────────

struct Keys64Widget : ModuleWidget {
    Keys64Widget(Keys64* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Keys64.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addChild(createLightCentered<SmallLight<GreenRedLight>>(
            mm2px(Vec(6.0f, 18.0f)), module, Keys64::ACTIVE_LIGHT));

        addOutput(createOutputCentered<PJ301MPort>(
            mm2px(Vec(20.0f, 80.0f)), module, Keys64::PITCH_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(
            mm2px(Vec(20.0f, 92.0f)), module, Keys64::GATE_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(
            mm2px(Vec(20.0f, 104.0f)), module, Keys64::RTRG_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        Keys64* m = getModule<Keys64>();
        menu->addChild(new MenuSeparator);

        menu->addChild(createIndexSubmenuItem("Arrangement",
            {"Scale grid", "Isomorphic"},
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
        for (int i = 0; i <= 7; i++) octaveNames.push_back(string::f("%d", i));
        menu->addChild(createIndexSubmenuItem("Base octave", octaveNames,
            [=]() { return m->octave; },
            [=](int v) { m->octave = v; m->rebuildNoteMap(); }));

        if (m->arrangement == 0) {
            menu->addChild(createIndexSubmenuItem("Row degrees",
                {"1", "2", "3", "4", "5", "6", "7"},
                [=]() { return m->rowDegrees - 1; },
                [=](int v) { m->rowDegrees = v + 1; m->rebuildNoteMap(); }));
        } else {
            menu->addChild(createIndexSubmenuItem("Column semitones",
                {"1", "2", "3", "4", "5", "6", "7"},
                [=]() { return m->colSemis - 1; },
                [=](int v) { m->colSemis = v + 1; m->rebuildNoteMap(); }));
            std::vector<std::string> rs;
            for (int i = 1; i <= 12; i++) rs.push_back(string::f("%d", i));
            menu->addChild(createIndexSubmenuItem("Row semitones", rs,
                [=]() { return m->rowSemis - 1; },
                [=](int v) { m->rowSemis = v + 1; m->rebuildNoteMap(); }));
        }

        std::vector<std::string> polyNames;
        for (int i = 1; i <= 16; i++) polyNames.push_back(string::f("%d", i));
        menu->addChild(createIndexSubmenuItem("Polyphony", polyNames,
            [=]() { return m->maxPoly - 1; },
            [=](int v) { m->maxPoly = v + 1; }));

        menu->addChild(createIndexSubmenuItem("Voice stealing",
            {"Oldest", "Newest", "Lowest", "Highest", "Round-robin", "Off (drop)"},
            [=]() { return m->stealMode; },
            [=](int v) { m->stealMode = v; }));

        menu->addChild(new MenuSeparator);
        menu->addChild(createIndexSubmenuItem("Arpeggiator mode",
            {ARP_MODE_NAMES, ARP_MODE_NAMES + NUM_ARP_MODES},
            [=]() { return m->arpMode; },
            [=](int v) { m->arpMode = v; }));
        P64::appendClockDivMenu(menu, &m->clockDiv);

        menu->addChild(new MenuSeparator);
        P64::appendColorMenu(menu, m, "Play color",  &m->playColor);
        P64::appendColorMenu(menu, m, "Latch color", &m->latchColor);
        P64::appendColorMenu(menu, m, "Root color",  &m->rootColor, true);
        P64::appendColorMenu(menu, m, "Arp color",   &m->arpColor);
        P64::appendColorMenu(menu, m, "Page color",  &m->pageColor);
    }
};

Model* modelKeys64 = createModel<Keys64, Keys64Widget>("Keys64");
