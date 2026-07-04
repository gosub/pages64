#include "PageModule.hpp"

// Default patterns from Press Cafe by stretta (first 8 steps of patterns 0-7)
static constexpr bool CAFE_DEFAULT_RHYTHMS[8][8] = {
    {1,0,0,1,0,0,1,1},
    {1,0,0,1,1,0,0,0},
    {1,1,0,1,0,0,1,1},
    {1,0,0,0,0,1,0,0},
    {1,1,0,0,0,1,0,0},
    {1,0,1,0,0,1,0,1},
    {1,0,0,1,1,0,1,1},
    {1,1,0,1,1,0,0,0},
};

struct Cafe64 : PageModule {
    enum ParamIds  { NUM_PARAMS };
    enum InputIds  { NUM_INPUTS };
    enum OutputIds {
        ENUMS(TRIG_OUTPUT, 8),
        POLY_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        ENUMS(ACTIVE_LIGHT, 2),
        NUM_LIGHTS
    };

    // Sub-page: 0=play, 1=rhythm editor, 2=length editor
    int subPage = 0;

    // 8 rhythm patterns × 8 steps each; rhythms[r][s] = true if step s is active in rhythm r
    bool rhythms[8][8] = {};
    // Length per rhythm (1–8)
    int lengths[8];

    // Per-voice play state
    int  activeRow[8];       // which rhythm voice c plays (-1 = off)
    int  stepPos[8];         // current step position within the active rhythm
    bool waitingSync[8];     // true: armed, waiting for next clock before starting
    int  pendingRow[8];      // rhythm to start on the next sync tick

    // Track which play-page buttons are held (note-on received, note-off not yet)
    bool playHeld[64] = {};

    P64::ClockDivider clockDiv;

    // 5 ms trigger pulses per output column
    dsp::PulseGenerator trigPulse[8];

    // Toggle/latch mode: scene A arms columns permanently instead of momentary hold
    bool toggleMode = false;

    // Colors
    uint8_t activePageColor   = P64::LED_GREEN;
    uint8_t inactivePageColor = P64::LED_AMBER_DIM;
    uint8_t stepColor         = P64::LED_GREEN;
    uint8_t latchOnColor      = P64::LED_AMBER;      // latch indicator: step is on
    uint8_t latchOffColor     = P64::LED_AMBER_DIM;  // latch indicator: step is off

    Cafe64() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (int i = 0; i < 8; i++)
            configOutput(TRIG_OUTPUT + i, string::f("Trigger %d", i + 1));
        configOutput(POLY_OUTPUT, "Poly triggers (8-channel)");
        memcpy(rhythms, CAFE_DEFAULT_RHYTHMS, sizeof(rhythms));
        for (int i = 0; i < 8; i++) {
            lengths[i]     = 8;
            activeRow[i]   = -1;
            stepPos[i]     = 0;
            waitingSync[i] = false;
            pendingRow[i]  = 0;
        }
    }

    void onReset() override {
        PageModule::onReset();
        subPage = 0;
        memcpy(rhythms, CAFE_DEFAULT_RHYTHMS, sizeof(rhythms));
        for (int i = 0; i < 8; i++) {
            lengths[i]     = 8;
            activeRow[i]   = -1;
            stepPos[i]     = 0;
            waitingSync[i] = false;
            pendingRow[i]  = 0;
        }
        memset(playHeld, 0, sizeof(playHeld));
        clockDiv.set(1);
        toggleMode        = false;
        activePageColor   = P64::LED_GREEN;
        inactivePageColor = P64::LED_AMBER_DIM;
        stepColor         = P64::LED_GREEN;
        latchOnColor      = P64::LED_AMBER;
        latchOffColor     = P64::LED_AMBER_DIM;
    }

    // ── virtual hooks ─────────────────────────────────────────────────────────

    void pagePreProcess() override {
        auto* msg = reinterpret_cast<P64::LeftMessage*>(leftExpander.consumerMessage);
        if (!msg) return;

        bool tick = clockDiv.process(msg->clockTick);

        if (tick) {
            for (int c = 0; c < 8; c++) {
                // Sync-arm: activate on this clock edge
                if (waitingSync[c]) {
                    activeRow[c]   = pendingRow[c];
                    waitingSync[c] = false;
                    stepPos[c]     = 0;
                }
                if (activeRow[c] < 0) continue;

                int rhythm = activeRow[c];
                int len    = lengths[rhythm];
                if (rhythms[rhythm][stepPos[c]])
                    trigPulse[c].trigger(0.005f);
                stepPos[c] = (stepPos[c] + 1) % len;
                ledsDirty = true;
            }
        }
    }

    void pageActive(const P64::LeftMessage& msg) override {
        // Sub-page switching: top buttons CC 104 (play), 105 (rhythm), 106 (length)
        for (int e = 0; e < msg.eventCount; e++) {
            const P64::GridEvent& ev = msg.events[e];
            if (ev.type == P64::GridEvent::CC && ev.value > 0
                    && ev.index >= 104 && ev.index <= 106) {
                subPage = ev.index - 104;
                ledsDirty = true;
            }
        }

        // Scene A: toggle latch mode on/off
        if (P64::sceneOn(msg, 0)) {
            toggleMode = !toggleMode;
            if (!toggleMode) {
                // Turning latch off: stop all voices
                for (int c = 0; c < 8; c++) {
                    activeRow[c]   = -1;
                    waitingSync[c] = false;
                    stepPos[c]     = 0;
                }
            }
            // Always clear held tracking (irrelevant in latch mode)
            memset(playHeld, 0, sizeof(playHeld));
            ledsDirty = true;
        }

        for (int e = 0; e < msg.eventCount; e++) {
            const P64::GridEvent& ev = msg.events[e];
            if (ev.type == P64::GridEvent::PAD) {
                bool pressed = ev.value > 0;
                int  idx     = ev.index;
                int  row     = idx / 8;
                int  col     = idx % 8;

                // Momentary mode: note-offs stop the column when all buttons released
                if (!toggleMode && !pressed && playHeld[idx]) {
                    playHeld[idx] = false;
                    bool anyHeld = false;
                    for (int r = 0; r < 8; r++)
                        if (playHeld[r * 8 + col]) { anyHeld = true; break; }
                    if (!anyHeld) {
                        activeRow[col]   = -1;
                        waitingSync[col] = false;
                        stepPos[col]     = 0;
                        ledsDirty = true;
                    }
                    continue;
                }

                if (!pressed) continue;  // toggle mode: ignore all note-offs

                if (subPage == 0) {
                    if (toggleMode) {
                        // Tap to arm/stop/change; no hold tracking
                        if (activeRow[col] < 0 && !waitingSync[col]) {
                            pendingRow[col]  = row;
                            waitingSync[col] = true;
                        } else if (waitingSync[col]) {
                            if (pendingRow[col] == row)
                                waitingSync[col] = false;  // cancel
                            else
                                pendingRow[col] = row;     // change pending
                        } else {
                            if (activeRow[col] == row) {
                                activeRow[col] = -1;       // stop
                                stepPos[col]   = 0;
                            } else {
                                activeRow[col] = row;      // immediate change
                                stepPos[col]   = 0;
                            }
                        }
                    } else {
                        // Momentary: hold to play, release to stop
                        playHeld[idx]   = true;
                        pendingRow[col] = row;
                        if (activeRow[col] < 0) {
                            waitingSync[col] = true;
                        } else {
                            activeRow[col] = row;
                            stepPos[col]   = 0;
                        }
                    }
                    ledsDirty = true;
                } else if (subPage == 1) {
                    // Rhythm editor: toggle step (bottom row = step 0)
                    rhythms[col][7 - row] = !rhythms[col][7 - row];
                    ledsDirty = true;
                } else if (subPage == 2) {
                    // Length editor: bar height from bottom
                    lengths[col] = 8 - row;
                    if (activeRow[col] >= 0 && stepPos[col] >= lengths[col])
                        stepPos[col] = 0;
                    ledsDirty = true;
                }
            }
        }
    }

    void pageInactive() override {
        memset(playHeld, 0, sizeof(playHeld));
        if (!toggleMode) {
            for (int c = 0; c < 8; c++) {
                activeRow[c]   = -1;
                waitingSync[c] = false;
                stepPos[c]     = 0;
            }
            ledsDirty = true;
        }
    }

    bool rhythmIsActive(int r) const {
        for (int c = 0; c < 8; c++)
            if (activeRow[c] == r || (waitingSync[c] && pendingRow[c] == r))
                return true;
        return false;
    }

    void rebuildLeds() override {
        memset(ledState, P64::LED_OFF, sizeof(ledState));

        if (subPage == 0) {
            // Play page: scrolling pattern display per column.
            // Row 7 (bottom) = last-fired step; content scrolls down each clock tick.
            // Columns waiting for sync show the static pattern (step 0 at bottom) dimmed.
            for (int c = 0; c < 8; c++) {
                if (activeRow[c] >= 0) {
                    int rhythm = activeRow[c];
                    int len    = lengths[rhythm];
                    // stepPos was already advanced after firing, so last fired = stepPos-1
                    int lastFired = (stepPos[c] - 1 + len) % len;
                    for (int row = 0; row < 8; row++) {
                        // row 7 → offset 0 (lastFired), row 6 → offset 1 (next), …
                        int step = (lastFired + (7 - row)) % len;
                        ledState[row * 8 + c] = rhythms[rhythm][step] ? stepColor : P64::LED_OFF;
                    }
                }
            }
            // Latch mode: overlay a fixed indicator at the active/pending row.
            // Uses latchOnColor/latchOffColor so it's always distinct from stepColor.
            if (toggleMode) {
                for (int c = 0; c < 8; c++) {
                    int indicatorRow = -1;
                    if (activeRow[c] >= 0)   indicatorRow = activeRow[c];
                    else if (waitingSync[c]) indicatorRow = pendingRow[c];
                    if (indicatorRow >= 0) {
                        bool stepOn = ledState[indicatorRow * 8 + c] != P64::LED_OFF;
                        ledState[indicatorRow * 8 + c] = stepOn ? latchOnColor : latchOffColor;
                    }
                }
            }
        } else if (subPage == 1) {
            // Rhythm editor: column c shows rhythm c, bottom = step 0
            for (int c = 0; c < 8; c++) {
                uint8_t color = (toggleMode && rhythmIsActive(c)) ? latchOnColor : stepColor;
                for (int s = 0; s < 8; s++)
                    if (rhythms[c][s])
                        ledState[(7 - s) * 8 + c] = color;
            }
        } else if (subPage == 2) {
            // Length editor: filled bar from bottom up
            for (int c = 0; c < 8; c++) {
                uint8_t color = (toggleMode && rhythmIsActive(c)) ? latchOnColor : stepColor;
                for (int i = 0; i < lengths[c]; i++)
                    ledState[(7 - i) * 8 + c] = color;
            }
        }
    }

    void buildTopLeds(uint8_t topLeds[8]) override {
        memset(topLeds, P64::LED_OFF, 8);
        for (int b = 0; b < 3; b++)
            topLeds[b] = (b == subPage) ? activePageColor : inactivePageColor;
    }

    void buildSceneLeds(uint8_t sceneLeds[8]) override {
        memset(sceneLeds, P64::LED_OFF, 8);
        if (toggleMode)
            sceneLeds[0] = latchOnColor;
    }

    void updateOutputs() override {
        outputs[POLY_OUTPUT].setChannels(8);
        for (int c = 0; c < 8; c++) {
            float v = trigPulse[c].process(sampleTime) ? 10.f : 0.f;
            outputs[TRIG_OUTPUT + c].setVoltage(v);
            outputs[POLY_OUTPUT].setVoltage(v, c);
        }
    }

    // ── serialisation ─────────────────────────────────────────────────────────

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "subPage",           json_integer(subPage));
        json_object_set_new(root, "toggleMode",        json_boolean(toggleMode));
        json_object_set_new(root, "clockDiv",          json_integer(clockDiv.div));
        json_object_set_new(root, "activePageColor",   json_integer(activePageColor));
        json_object_set_new(root, "inactivePageColor", json_integer(inactivePageColor));
        json_object_set_new(root, "stepColor",         json_integer(stepColor));
        json_object_set_new(root, "latchOnColor",      json_integer(latchOnColor));
        json_object_set_new(root, "latchOffColor",     json_integer(latchOffColor));
        json_t* rh = json_array();
        for (int r = 0; r < 8; r++) {
            json_t* row = json_array();
            for (int s = 0; s < 8; s++)
                json_array_append_new(row, json_boolean(rhythms[r][s]));
            json_array_append_new(rh, row);
        }
        json_object_set_new(root, "rhythms", rh);
        json_t* ln = json_array();
        for (int i = 0; i < 8; i++)
            json_array_append_new(ln, json_integer(lengths[i]));
        json_object_set_new(root, "lengths", ln);
        return root;
    }

    void dataFromJson(json_t* root) override {
        json_t* j;
        if ((j = json_object_get(root, "subPage")))
            subPage = clamp((int)json_integer_value(j), 0, 2);
        if ((j = json_object_get(root, "toggleMode")))
            toggleMode = json_boolean_value(j);
        if ((j = json_object_get(root, "clockDiv")))
            clockDiv.set(clamp((int)json_integer_value(j), 1, 64));
        if ((j = json_object_get(root, "activePageColor")))
            activePageColor = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "inactivePageColor")))
            inactivePageColor = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "stepColor")))
            stepColor = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "latchOnColor")))
            latchOnColor = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "latchOffColor")))
            latchOffColor = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "rhythms")))
            for (int r = 0; r < 8; r++) {
                json_t* row = json_array_get(j, r);
                if (!row) continue;
                for (int s = 0; s < 8; s++) {
                    json_t* v = json_array_get(row, s);
                    if (v) rhythms[r][s] = json_boolean_value(v);
                }
            }
        if ((j = json_object_get(root, "lengths")))
            for (int i = 0; i < 8; i++) {
                json_t* v = json_array_get(j, i);
                if (v) lengths[i] = clamp((int)json_integer_value(v), 1, 8);
            }
        ledsDirty = true;
    }
};

// ── Widget ────────────────────────────────────────────────────────────────────

struct Cafe64Widget : ModuleWidget {
    Cafe64Widget(Cafe64* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Cafe64.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addChild(createLightCentered<SmallLight<GreenRedLight>>(
            mm2px(Vec(6.0f, 18.0f)), module, Cafe64::ACTIVE_LIGHT));

        const float trigY[8] = {24.f, 34.f, 44.f, 54.f, 64.f, 74.f, 84.f, 94.f};
        for (int i = 0; i < 8; i++) {
            addOutput(createOutputCentered<PJ301MPort>(
                mm2px(Vec(20.0f, trigY[i])), module, Cafe64::TRIG_OUTPUT + i));
        }

        addOutput(createOutputCentered<PJ301MPort>(
            mm2px(Vec(20.0f, 108.0f)), module, Cafe64::POLY_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        Cafe64* m = getModule<Cafe64>();
        menu->addChild(new MenuSeparator);
        P64::appendClockDivMenu(menu, &m->clockDiv);
        menu->addChild(createSubmenuItem("Colors", "", [=](Menu* sub) {
            P64::appendColorMenu(sub, m, "Active page",              &m->activePageColor);
            P64::appendColorMenu(sub, m, "Inactive page",            &m->inactivePageColor);
            P64::appendColorMenu(sub, m, "Step",                     &m->stepColor);
            P64::appendColorMenu(sub, m, "Latch indicator (on step)",  &m->latchOnColor);
            P64::appendColorMenu(sub, m, "Latch indicator (off step)", &m->latchOffColor);
        }));
    }
};

Model* modelCafe64 = createModel<Cafe64, Cafe64Widget>("Cafe64");
