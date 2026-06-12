#include "PageModule.hpp"

// ── Life64 ────────────────────────────────────────────────────────────────────
// Conway's Game of Life on the grid. The clock steps the generations (B3/S23);
// pressing a pad toggles a cell at any time, frozen or running. Edges are
// bounded by default; a context-menu option wraps them into a torus.
//
// Scene buttons: A freeze, B clear, C randomize, D loop, E recall, F save,
// G pattern library.
//
// Outputs: 64 cell gates in the Gome64/Buttons64 4 × 16-channel poly format
// (live cell = sustained 10 V), ROWS/COLS binary CVs and a density CV.

static constexpr int SCENE_FREEZE    = 0;   // A
static constexpr int SCENE_CLEAR     = 1;   // B
static constexpr int SCENE_RANDOMIZE = 2;   // C
static constexpr int SCENE_LOOP      = 3;   // D
static constexpr int SCENE_RECALL    = 4;   // E
static constexpr int SCENE_SAVE      = 5;   // F
static constexpr int SCENE_LIBRARY   = 6;   // G

// ── Pattern library ───────────────────────────────────────────────────────────
// Famous patterns whose full-cycle bounding box fits the 8×8 grid, one family
// per browser row: still lifes, oscillators, spaceships, methuselahs.
// Row bits: MSB = leftmost column. All verified by simulation.

struct LifePattern {
    uint8_t slot;      // browser pad (row-major grid index)
    uint8_t w, h;      // bounding box; loaded centered
    uint8_t rows[8];
};

static const LifePattern LIB_PATTERNS[] = {
    // row 1: still lifes
    { 0, 2, 2, {0b11, 0b11}},                                          // block
    { 1, 4, 3, {0b0110, 0b1001, 0b0110}},                              // beehive
    { 2, 4, 4, {0b0110, 0b1001, 0b0101, 0b0010}},                      // loaf
    { 3, 4, 4, {0b0110, 0b1001, 0b1001, 0b0110}},                      // pond
    { 4, 3, 3, {0b110, 0b101, 0b010}},                                 // boat
    { 5, 3, 3, {0b110, 0b101, 0b011}},                                 // ship
    { 6, 3, 3, {0b010, 0b101, 0b010}},                                 // tub
    { 7, 4, 4, {0b0100, 0b1010, 0b0101, 0b0010}},                      // barge
    // row 2: oscillators (p2 p2 p2 p2 p3 p4 p5 p5)
    { 8, 3, 1, {0b111}},                                               // blinker
    { 9, 4, 2, {0b0111, 0b1110}},                                      // toad
    {10, 4, 4, {0b1100, 0b1100, 0b0011, 0b0011}},                      // beacon
    {11, 4, 4, {0b0010, 0b1010, 0b0101, 0b0100}},                      // clock
    {12, 6, 7, {0b000110, 0b001001, 0b100101, 0b100010,
                0b100000, 0b000100, 0b011000}},                        // jam
    {13, 7, 7, {0b0001100, 0b0101000, 0b1000001, 0b0100011,
                0b0000000, 0b0001010, 0b0000100}},                     // mazing
    {14, 8, 8, {0b00011000, 0b00100100, 0b01000010, 0b10000001,
                0b10000001, 0b01000010, 0b00100100, 0b00011000}},      // octagon 2
    {15, 8, 7, {0b00011000, 0b01000010, 0b01000010, 0b01000010,
                0b00100100, 0b10100101, 0b11000011}},                  // fumarole
    // row 3: spaceships (orbit when wrap is on)
    {16, 3, 3, {0b010, 0b001, 0b111}},                                 // glider
    {17, 5, 4, {0b01001, 0b10000, 0b10001, 0b11110}},                  // LWSS
    {18, 6, 5, {0b000100, 0b010001, 0b100000, 0b100001, 0b111110}},    // MWSS
    {19, 7, 5, {0b0001100, 0b0100001, 0b1000000, 0b1000001,
                0b1111110}},                                           // HWSS
    // row 4: methuselahs (chaotic seeds at this size)
    {24, 3, 3, {0b011, 0b110, 0b010}},                                 // R-pentomino
    {25, 3, 3, {0b111, 0b101, 0b101}},                                 // pi-heptomino
    {26, 7, 3, {0b0100000, 0b0001000, 0b1100111}},                     // acorn
    {27, 8, 3, {0b00000010, 0b11000000, 0b01000111}},                  // diehard
};

struct Life64 : PageModule {
    enum ParamIds { NUM_PARAMS };
    enum InputIds  { NUM_INPUTS };
    enum OutputIds {
        ENUMS(CELL_OUTPUT, 4),   // 4 polyphonic outputs, 16 channels each = 64 cell gates
        ROWS_OUTPUT,
        COLS_OUTPUT,
        DENS_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        ENUMS(ACTIVE_LIGHT, 2),
        NUM_LIGHTS
    };

    bool cells[64]  = {};
    bool memory[64] = {};
    bool wrap       = false;
    bool frozen     = false;
    int  densityPct = 20;       // randomize: live-cell probability in percent
    float saveFlash = 0.f;      // scene F confirmation flash countdown

    // Frame loop (scene D): restore the start frame every loopLen ticks.
    bool loopOn         = false;
    int  loopLen        = 16;
    int  loopTick       = 0;
    bool loopFrame[64]  = {};
    bool loopArmPending = false;  // cleared frame: drawing re-captures the start until the next tick
    bool loopHeld       = false;  // scene D held: grid shows/edits the loop length
    bool loopPadUsed    = false;  // a length pad was tapped during the hold (suppresses the toggle)

    bool browserOpen = false;     // scene G: grid shows the pattern library

    bool grayDecode = false;      // ROWS/COLS: decode bit patterns as Gray code

    P64::ClockDivider clockDiv;

    uint8_t cellColor = P64::LED_GREEN;
    uint8_t uiColor   = P64::LED_AMBER;

    Life64() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (int i = 0; i < 4; i++)
            configOutput(CELL_OUTPUT + i,
                string::f("Rows %d-%d cell gates (poly 16ch)", i * 2 + 1, i * 2 + 2));
        configOutput(ROWS_OUTPUT, "Row binary CVs (poly 8ch)");
        configOutput(COLS_OUTPUT, "Column binary CVs (poly 8ch)");
        configOutput(DENS_OUTPUT, "Density CV");
        initMemory();
    }

    // The memory slot ships with a glider, so Recall does something
    // delightful on a fresh module.
    void initMemory() {
        memset(memory, 0, sizeof(memory));
        static const uint8_t glider[3] = {0b010, 0b001, 0b111};
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 3; c++)
                memory[(r + 2) * 8 + (c + 2)] = (glider[r] >> (2 - c)) & 1;
    }

    void onReset() override {
        PageModule::onReset();
        memset(cells, 0, sizeof(cells));
        initMemory();
        wrap       = false;
        frozen     = false;
        densityPct = 20;
        saveFlash  = 0.f;
        loopOn         = false;
        loopLen        = 16;
        loopTick       = 0;
        loopArmPending = false;
        loopHeld       = false;
        browserOpen    = false;
        memset(loopFrame, 0, sizeof(loopFrame));
        grayDecode = false;
        clockDiv.set(1);
        cellColor = P64::LED_GREEN;
        uiColor   = P64::LED_AMBER;
    }

    void randomizeFrame() {
        for (int i = 0; i < 64; i++)
            cells[i] = random::uniform() < densityPct / 100.f;
        ledsDirty = true;
    }

    // "This is the new material, loop from here."
    void captureLoopStart() {
        memcpy(loopFrame, cells, sizeof(loopFrame));
        loopTick       = 0;
        loopArmPending = false;
    }

    void loadPattern(const LifePattern& p) {
        memset(cells, 0, sizeof(cells));
        int r0 = (8 - p.h) / 2;
        int c0 = (8 - p.w) / 2;
        for (int r = 0; r < p.h; r++)
            for (int c = 0; c < p.w; c++)
                cells[(r0 + r) * 8 + (c0 + c)] = (p.rows[r] >> (p.w - 1 - c)) & 1;
        ledsDirty = true;
    }

    // ── simulation ────────────────────────────────────────────────────────────

    void stepGeneration() {
        bool next[64];
        for (int r = 0; r < 8; r++) {
            for (int c = 0; c < 8; c++) {
                int n = 0;
                for (int dr = -1; dr <= 1; dr++) {
                    for (int dc = -1; dc <= 1; dc++) {
                        if (!dr && !dc) continue;
                        int rr = r + dr, cc = c + dc;
                        if (wrap) {
                            rr = (rr + 8) % 8;
                            cc = (cc + 8) % 8;
                        } else if (rr < 0 || rr > 7 || cc < 0 || cc > 7) {
                            continue;
                        }
                        if (cells[rr * 8 + cc]) n++;
                    }
                }
                next[r * 8 + c] = n == 3 || (cells[r * 8 + c] && n == 2);
            }
        }
        memcpy(cells, next, sizeof(cells));
    }

    // ── virtual hooks ─────────────────────────────────────────────────────────

    void pagePreProcess() override {
        if (saveFlash > 0.f) {
            saveFlash -= sampleTime;
            if (saveFlash <= 0.f)
                ledsDirty = true;   // push the flash-off transition
        }

        auto* msg = reinterpret_cast<P64::LeftMessage*>(leftExpander.consumerMessage);
        if (!msg) return;

        if (msg->resetTick) {
            clockDiv.reset();
            // RESET restarts the loop; it never clears a hand-drawn colony.
            if (loopOn) {
                memcpy(cells, loopFrame, sizeof(cells));
                loopTick  = 0;
                ledsDirty = true;
            }
        }

        // Frozen: ticks are ignored, the frame holds still and stays editable.
        if (!frozen && clockDiv.process(msg->clockTick)) {
            if (loopOn && ++loopTick >= loopLen) {
                memcpy(cells, loopFrame, sizeof(cells));
                loopTick = 0;
            } else {
                stepGeneration();
            }
            loopArmPending = false;
            ledsDirty = true;
        }
    }

    void pageActive(const P64::LeftMessage& msg) override {
        auto scenePressed = [&](int i) {
            return msg.sceneEvent[i] && msg.sceneVelocity[i] > 0;
        };

        if (scenePressed(SCENE_FREEZE)) {
            frozen = !frozen;
            if (!frozen && loopOn)
                captureLoopStart();
            ledsDirty = true;   // scene LEDs are only pushed on dirty frames
        }
        if (scenePressed(SCENE_CLEAR)) {
            memset(cells, 0, sizeof(cells));
            if (loopOn) {
                captureLoopStart();        // silence loops as silence...
                loopArmPending = true;     // ...until something is drawn before the next tick
            }
            ledsDirty = true;
        }
        if (scenePressed(SCENE_RANDOMIZE)) {
            randomizeFrame();
            if (loopOn)
                captureLoopStart();
        }
        if (scenePressed(SCENE_RECALL)) {
            memcpy(cells, memory, sizeof(cells));
            if (loopOn)
                captureLoopStart();
            ledsDirty = true;
        }
        if (scenePressed(SCENE_SAVE)) {
            memcpy(memory, cells, sizeof(memory));
            saveFlash = 0.3f;
            ledsDirty = true;   // scene LEDs are only pushed on dirty frames
        }
        if (scenePressed(SCENE_LIBRARY)) {
            browserOpen = !browserOpen;
            ledsDirty   = true;
        }

        // Scene D: tap toggles the loop, hold turns the grid into the
        // length selector (resolved on release, like Flin64's gestures).
        if (msg.sceneEvent[SCENE_LOOP]) {
            if (msg.sceneVelocity[SCENE_LOOP] > 0) {
                loopHeld    = true;
                loopPadUsed = false;
            } else {
                loopHeld = false;
                if (!loopPadUsed) {
                    loopOn = !loopOn;
                    if (loopOn)
                        captureLoopStart();
                }
            }
            ledsDirty = true;
        }

        // Grid presses, routed by mode.
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                int note = row * 16 + col;
                if (!msg.noteEvent[note] || msg.noteVelocity[note] == 0) continue;
                int idx = row * 8 + col;

                if (browserOpen) {
                    for (auto& p : LIB_PATTERNS) {
                        if (p.slot == idx) {
                            loadPattern(p);
                            browserOpen = false;
                            if (loopOn)
                                captureLoopStart();
                            break;
                        }
                    }
                } else if (loopHeld) {
                    loopLen     = idx + 1;   // row-major: 1 (top-left) … 64
                    loopPadUsed = true;
                    ledsDirty   = true;
                } else {
                    // Toggle cells on press (no two-button gestures; edit
                    // latency matters when punching cells into a running colony).
                    cells[idx] = !cells[idx];
                    // Drawing onto a cleared looping frame re-captures the
                    // start, so the loop replays the whole drawing.
                    if (loopOn && loopArmPending) {
                        memcpy(loopFrame, cells, sizeof(loopFrame));
                        loopTick = 0;
                    }
                    ledsDirty = true;
                }
            }
        }
    }

    void pageInactive() override {
        if (loopHeld || browserOpen) {
            loopHeld    = false;
            browserOpen = false;
            ledsDirty   = true;
        }
    }

    void buildSceneLeds(uint8_t sceneLeds[8]) override {
        memset(sceneLeds, P64::LED_OFF, 8);
        if (frozen)
            sceneLeds[SCENE_FREEZE] = uiColor;
        if (loopOn || loopHeld)
            sceneLeds[SCENE_LOOP] = uiColor;
        if (saveFlash > 0.f)
            sceneLeds[SCENE_SAVE] = uiColor;
        if (browserOpen)
            sceneLeds[SCENE_LIBRARY] = uiColor;
    }

    void rebuildLeds() override {
        uint8_t next[64];
        if (browserOpen) {
            memset(next, P64::LED_OFF, sizeof(next));
            for (auto& p : LIB_PATTERNS)
                next[p.slot] = uiColor;
        } else if (loopHeld) {
            for (int i = 0; i < 64; i++)
                next[i] = i < loopLen ? uiColor : P64::LED_OFF;
        } else {
            for (int i = 0; i < 64; i++)
                next[i] = cells[i] ? cellColor : P64::LED_OFF;
        }
        for (int i = 0; i < 64; i++) {
            if (next[i] != ledState[i]) {
                ledState[i] = next[i];
                ledsDirty   = true;
            }
        }
    }

    float byteToVoltage(uint8_t b) {
        if (grayDecode) {
            b ^= b >> 4;
            b ^= b >> 2;
            b ^= b >> 1;
        }
        return b / 255.f * 10.f;
    }

    void updateOutputs() override {
        int live = 0;
        for (int out = 0; out < 4; out++) {
            outputs[CELL_OUTPUT + out].setChannels(16);
            for (int localRow = 0; localRow < 2; localRow++) {
                for (int col = 0; col < 8; col++) {
                    int idx = (out * 2 + localRow) * 8 + col;
                    if (cells[idx]) live++;
                    outputs[CELL_OUTPUT + out].setVoltage(cells[idx] ? 10.f : 0.f,
                                                          localRow * 8 + col);
                }
            }
        }

        // Each row/column read as an 8-bit number (MSB = left / top).
        outputs[ROWS_OUTPUT].setChannels(8);
        outputs[COLS_OUTPUT].setChannels(8);
        for (int i = 0; i < 8; i++) {
            uint8_t rowByte = 0, colByte = 0;
            for (int k = 0; k < 8; k++) {
                if (cells[i * 8 + k]) rowByte |= 1 << (7 - k);
                if (cells[k * 8 + i]) colByte |= 1 << (7 - k);
            }
            outputs[ROWS_OUTPUT].setVoltage(byteToVoltage(rowByte), i);
            outputs[COLS_OUTPUT].setVoltage(byteToVoltage(colByte), i);
        }

        outputs[DENS_OUTPUT].setVoltage(live / 64.f * 10.f);
    }

    // ── serialisation ─────────────────────────────────────────────────────────

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_t* jc = json_array();
        json_t* jm = json_array();
        for (int i = 0; i < 64; i++) {
            json_array_append_new(jc, json_boolean(cells[i]));
            json_array_append_new(jm, json_boolean(memory[i]));
        }
        json_object_set_new(root, "cells",     jc);
        json_object_set_new(root, "memory",    jm);
        json_object_set_new(root, "density",   json_integer(densityPct));
        json_object_set_new(root, "loopOn",    json_boolean(loopOn));
        json_object_set_new(root, "loopLen",   json_integer(loopLen));
        json_object_set_new(root, "gray",      json_boolean(grayDecode));
        json_object_set_new(root, "wrap",      json_boolean(wrap));
        json_object_set_new(root, "clockDiv",  json_integer(clockDiv.div));
        json_object_set_new(root, "cellColor", json_integer(cellColor));
        json_object_set_new(root, "uiColor",   json_integer(uiColor));
        return root;
    }

    void dataFromJson(json_t* root) override {
        json_t* j;
        if ((j = json_object_get(root, "cells")))
            for (int i = 0; i < 64; i++) {
                json_t* v = json_array_get(j, i);
                if (v) cells[i] = json_boolean_value(v);
            }
        if ((j = json_object_get(root, "memory")))
            for (int i = 0; i < 64; i++) {
                json_t* v = json_array_get(j, i);
                if (v) memory[i] = json_boolean_value(v);
            }
        if ((j = json_object_get(root, "density")))
            densityPct = clamp((int)json_integer_value(j), 1, 100);
        if ((j = json_object_get(root, "loopOn")))
            loopOn = json_boolean_value(j);
        if ((j = json_object_get(root, "loopLen")))
            loopLen = clamp((int)json_integer_value(j), 1, 64);
        if ((j = json_object_get(root, "gray")))
            grayDecode = json_boolean_value(j);
        if ((j = json_object_get(root, "wrap")))
            wrap = json_boolean_value(j);
        // The loop start frame is transient; a reloaded patch loops from
        // its saved frame.
        if (loopOn)
            captureLoopStart();
        if ((j = json_object_get(root, "clockDiv")))
            clockDiv.set(clamp((int)json_integer_value(j), 1, 64));
        if ((j = json_object_get(root, "cellColor")))
            cellColor = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "uiColor")))
            uiColor = (uint8_t)json_integer_value(j);
        ledsDirty = true;
    }
};

// ── Widget ────────────────────────────────────────────────────────────────────

struct Life64Widget : ModuleWidget {
    Life64Widget(Life64* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Life64.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addChild(createLightCentered<SmallLight<GreenRedLight>>(
            mm2px(Vec(6.0f, 18.0f)), module, Life64::ACTIVE_LIGHT));

        const float cellY[4] = {28.f, 40.f, 52.f, 64.f};
        for (int i = 0; i < 4; i++) {
            addOutput(createOutputCentered<PJ301MPort>(
                mm2px(Vec(20.0f, cellY[i])), module, Life64::CELL_OUTPUT + i));
        }
        addOutput(createOutputCentered<PJ301MPort>(
            mm2px(Vec(20.0f, 80.0f)), module, Life64::ROWS_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(
            mm2px(Vec(20.0f, 92.0f)), module, Life64::COLS_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(
            mm2px(Vec(20.0f, 104.0f)), module, Life64::DENS_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        Life64* m = getModule<Life64>();
        menu->addChild(new MenuSeparator);
        P64::appendClockDivMenu(menu, &m->clockDiv);
        menu->addChild(createBoolPtrMenuItem("Wrap edges (torus)", "", &m->wrap));
        menu->addChild(createSubmenuItem("Randomize density", "", [=](Menu* sub) {
            for (int d : {10, 20, 30}) {
                sub->addChild(createCheckMenuItem(string::f("%d%%", d), "",
                    [=]() { return m->densityPct == d; },
                    [=]() { m->densityPct = d; }
                ));
            }
        }));
        menu->addChild(createIndexPtrSubmenuItem("Binary decode",
            {"Classic", "Gray code"}, &m->grayDecode));
        P64::appendColorMenu(menu, m, "Cell color", &m->cellColor);
        P64::appendColorMenu(menu, m, "UI color",   &m->uiColor);
    }
};

Model* modelLife64 = createModel<Life64, Life64Widget>("Life64");
