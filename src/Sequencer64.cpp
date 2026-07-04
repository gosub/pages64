#include "PageModule.hpp"

// ── Sequencer64 ───────────────────────────────────────────────────────────────
// A cross between Step64 and Sliders64: each column holds a slider-style value
// (tap a row, bottom = 0 V, top = 10 V, all 8 rows are resolution) and the
// divided clock walks a playhead through the loop range, sending the playing
// column's value to the CV output. TRIG pulses on every step advance; POLY
// carries all eight column values continuously.
//
// Scene A (momentary) borrows the bottom row as a Step64-style control strip:
// tap = jump (immediate — the CV is what you hear), hold + press = loop range.

// Slew ladder: index 0 = off (stepped); the rest is Sliders64's rate ladder,
// as fraction of full range (0→1) per second, labeled by full-range time.
static constexpr float SEQ_SLEW_RATES[8] =
    {0.f, 8.f, 2.f, 1.f, 0.5f, 0.25f, 0.125f, 0.0625f};
static const char* SEQ_SLEW_NAMES[8] =
    {"Off", "0.125 s", "0.5 s", "1 s", "2 s", "4 s", "8 s", "16 s"};

struct Sequencer64 : PageModule {
    enum ParamIds { NUM_PARAMS };
    enum InputIds  { NUM_INPUTS };
    enum OutputIds {
        CV_OUTPUT,
        TRIG_OUTPUT,
        POLY_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        ENUMS(ACTIVE_LIGHT, 2),
        NUM_LIGHTS
    };

    int value[8];                 // per-column value 0–7 (0 = bottom row = 0 V)
    int pos       = 0;            // playhead column
    int loopStart = 0;
    int loopEnd   = 7;            // inclusive

    // Scene A held: the bottom row is the control strip (jump / loop range).
    bool stripHeld       = false;
    bool ctrlHeld[8]     = {};
    bool ctrlTwoBtnFired = false;

    // Slewed output positions (normalized 0–1); display always shows targets.
    int   slewIdx   = 0;
    float cvOut     = 0.f;
    float colOut[8] = {};

    P64::ClockDivider clockDiv;
    dsp::PulseGenerator trigPulse;

    bool    fullBar        = true;
    uint8_t valueColor     = P64::LED_GREEN;
    uint8_t indicatorColor = P64::LED_AMBER;
    uint8_t controlColor   = P64::LED_YELLOW;

    Sequencer64() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configOutput(CV_OUTPUT,   "Step CV (0–10V)");
        configOutput(TRIG_OUTPUT, "Step advance trigger");
        configOutput(POLY_OUTPUT, "Column CVs (poly 8ch)");
        for (int i = 0; i < 8; i++)
            value[i] = 0;
    }

    void onReset() override {
        PageModule::onReset();
        for (int i = 0; i < 8; i++)
            value[i] = 0;
        pos       = 0;
        loopStart = 0;
        loopEnd   = 7;
        slewIdx   = 0;
        cvOut     = 0.f;
        memset(colOut, 0, sizeof(colOut));
        clockDiv.set(1);
        fullBar        = true;
        valueColor     = P64::LED_GREEN;
        indicatorColor = P64::LED_AMBER;
        controlColor   = P64::LED_YELLOW;
    }

    // ── virtual hooks ─────────────────────────────────────────────────────────

    void pagePreProcess() override {
        auto* msg = reinterpret_cast<P64::LeftMessage*>(leftExpander.consumerMessage);
        if (!msg) return;

        if (msg->resetTick) {
            pos = loopStart;
            clockDiv.reset();
            ledsDirty = true;
        }

        if (clockDiv.process(msg->clockTick)) {
            pos = (pos < loopStart || pos >= loopEnd) ? loopStart : pos + 1;
            trigPulse.trigger(0.005f);
            ledsDirty = true;
        }

        slewToward(cvOut, value[pos] / 7.f);
        for (int i = 0; i < 8; i++)
            slewToward(colOut[i], value[i] / 7.f);
    }

    void slewToward(float& cur, float target) {
        if (slewIdx == 0) {
            cur = target;
            return;
        }
        float delta = SEQ_SLEW_RATES[slewIdx] * sampleTime;
        float diff  = target - cur;
        if (std::abs(diff) <= delta)
            cur = target;
        else
            cur += diff > 0.f ? delta : -delta;
    }

    void pageActive(const P64::LeftMessage& msg) override {
        // Scene A: momentary control strip on the bottom row.
        for (int e = 0; e < msg.eventCount; e++) {
            const P64::GridEvent& ev = msg.events[e];
            if (ev.type != P64::GridEvent::SCENE || ev.index != 0) continue;
            stripHeld = ev.value > 0;
            if (!stripHeld) {
                memset(ctrlHeld, 0, sizeof(ctrlHeld));
                ctrlTwoBtnFired = false;
            }
            ledsDirty = true;
        }

        for (int e = 0; e < msg.eventCount; e++) {
            const P64::GridEvent& ev = msg.events[e];
            if (ev.type == P64::GridEvent::PAD) {
                bool pressed = ev.value > 0;
                int  row     = ev.index / 8;
                int  col     = ev.index % 8;

                if (stripHeld && row == 7) {
                    // Control strip (the Step64 control-row idiom: two-button
                    // on second press, single tap resolved on release).
                    if (pressed) {
                        int heldCol = -1;
                        for (int c2 = 0; c2 < 8; c2++)
                            if (c2 != col && ctrlHeld[c2]) { heldCol = c2; break; }
                        ctrlHeld[col] = true;

                        if (heldCol >= 0) {
                            loopStart = std::min(col, heldCol);
                            loopEnd   = std::max(col, heldCol);
                            if (pos < loopStart || pos > loopEnd)
                                pos = loopStart;
                            ctrlTwoBtnFired = true;
                            ledsDirty = true;
                        }
                    } else {
                        ctrlHeld[col] = false;
                        bool anyHeld = false;
                        for (int c2 = 0; c2 < 8; c2++)
                            if (ctrlHeld[c2]) { anyHeld = true; break; }

                        // Tap inside the loop: jump immediately — the CV is a
                        // continuous output, so the move is what you hear.
                        if (!ctrlTwoBtnFired && !anyHeld
                                && col >= loopStart && col <= loopEnd) {
                            pos = col;
                            ledsDirty = true;
                        }
                        if (!anyHeld)
                            ctrlTwoBtnFired = false;
                    }
                } else if (pressed) {
                    // Value editing (acting on press); rows 1-7 keep editing
                    // even while the strip is held.
                    value[col] = 7 - row;
                    ledsDirty  = true;
                }
            }
        }
    }

    void pageInactive() override {
        if (stripHeld) {
            stripHeld = false;
            memset(ctrlHeld, 0, sizeof(ctrlHeld));
            ctrlTwoBtnFired = false;
            ledsDirty = true;
        }
    }

    void buildSceneLeds(uint8_t sceneLeds[8]) override {
        memset(sceneLeds, P64::LED_OFF, 8);
        if (stripHeld)
            sceneLeds[0] = controlColor;
    }

    void rebuildLeds() override {
        int lastRow = stripHeld ? 7 : 8;   // strip borrows the bottom row
        for (int col = 0; col < 8; col++) {
            uint8_t color  = (col == pos) ? indicatorColor : valueColor;
            int     topRow = 7 - value[col];
            for (int row = 0; row < lastRow; row++) {
                int     idx = row * 8 + col;
                bool    lit = fullBar ? (row >= topRow) : (row == topRow);
                uint8_t c   = lit ? color : P64::LED_OFF;
                if (c != ledState[idx]) {
                    ledState[idx] = c;
                    ledsDirty     = true;
                }
            }
            if (stripHeld) {
                bool    inLoop = col >= loopStart && col <= loopEnd;
                uint8_t c      = (col == pos) ? indicatorColor
                               : inLoop       ? controlColor
                               :                P64::LED_OFF;
                if (c != ledState[7 * 8 + col]) {
                    ledState[7 * 8 + col] = c;
                    ledsDirty = true;
                }
            }
        }
    }

    void updateOutputs() override {
        outputs[CV_OUTPUT].setVoltage(cvOut * 10.f);
        outputs[TRIG_OUTPUT].setVoltage(trigPulse.process(sampleTime) ? 10.f : 0.f);
        outputs[POLY_OUTPUT].setChannels(8);
        for (int i = 0; i < 8; i++)
            outputs[POLY_OUTPUT].setVoltage(colOut[i] * 10.f, i);
    }

    // ── serialisation ─────────────────────────────────────────────────────────

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_t* jv = json_array();
        for (int i = 0; i < 8; i++)
            json_array_append_new(jv, json_integer(value[i]));
        json_object_set_new(root, "value",          jv);
        json_object_set_new(root, "pos",            json_integer(pos));
        json_object_set_new(root, "loopStart",      json_integer(loopStart));
        json_object_set_new(root, "loopEnd",        json_integer(loopEnd));
        json_object_set_new(root, "clockDiv",       json_integer(clockDiv.div));
        json_object_set_new(root, "slew",           json_integer(slewIdx));
        json_object_set_new(root, "fullBar",        json_boolean(fullBar));
        json_object_set_new(root, "valueColor",     json_integer(valueColor));
        json_object_set_new(root, "indicatorColor", json_integer(indicatorColor));
        json_object_set_new(root, "controlColor",   json_integer(controlColor));
        return root;
    }

    void dataFromJson(json_t* root) override {
        json_t* j;
        if ((j = json_object_get(root, "value")))
            for (int i = 0; i < 8; i++) {
                json_t* v = json_array_get(j, i);
                if (v) value[i] = clamp((int)json_integer_value(v), 0, 7);
            }
        if ((j = json_object_get(root, "pos")))
            pos = clamp((int)json_integer_value(j), 0, 7);
        if ((j = json_object_get(root, "loopStart")))
            loopStart = clamp((int)json_integer_value(j), 0, 7);
        if ((j = json_object_get(root, "loopEnd")))
            loopEnd = clamp((int)json_integer_value(j), 0, 7);
        if (loopEnd < loopStart)
            std::swap(loopStart, loopEnd);
        if ((j = json_object_get(root, "clockDiv")))
            clockDiv.set(clamp((int)json_integer_value(j), 1, 64));
        if ((j = json_object_get(root, "slew")))
            slewIdx = clamp((int)json_integer_value(j), 0, 7);
        if ((j = json_object_get(root, "fullBar")))
            fullBar = json_boolean_value(j);
        if ((j = json_object_get(root, "valueColor")))
            valueColor = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "indicatorColor")))
            indicatorColor = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "controlColor")))
            controlColor = (uint8_t)json_integer_value(j);
        ledsDirty = true;
    }
};

// ── Widget ────────────────────────────────────────────────────────────────────

struct Sequencer64Widget : ModuleWidget {
    Sequencer64Widget(Sequencer64* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Sequencer64.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addChild(createLightCentered<SmallLight<GreenRedLight>>(
            mm2px(Vec(6.0f, 18.0f)), module, Sequencer64::ACTIVE_LIGHT));

        addOutput(createOutputCentered<PJ301MPort>(
            mm2px(Vec(20.0f, 80.0f)), module, Sequencer64::CV_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(
            mm2px(Vec(20.0f, 92.0f)), module, Sequencer64::TRIG_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(
            mm2px(Vec(20.0f, 104.0f)), module, Sequencer64::POLY_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        Sequencer64* m = getModule<Sequencer64>();
        menu->addChild(new MenuSeparator);
        P64::appendClockDivMenu(menu, &m->clockDiv);
        menu->addChild(createSubmenuItem("Slew", "", [=](Menu* sub) {
            for (int i = 0; i < 8; i++) {
                sub->addChild(createCheckMenuItem(SEQ_SLEW_NAMES[i], "",
                    [=]() { return m->slewIdx == i; },
                    [=]() { m->slewIdx = i; }
                ));
            }
        }));
        menu->addChild(createSubmenuItem("Display style", "", [=](Menu* sub) {
            sub->addChild(createCheckMenuItem("Full bar", "",
                [=]() { return m->fullBar; },
                [=]() { m->fullBar = true; m->ledsDirty = true; }
            ));
            sub->addChild(createCheckMenuItem("Top LED only", "",
                [=]() { return !m->fullBar; },
                [=]() { m->fullBar = false; m->ledsDirty = true; }
            ));
        }));
        menu->addChild(createSubmenuItem("Colors", "", [=](Menu* sub) {
            P64::appendColorMenu(sub, m, "Value",          &m->valueColor);
            P64::appendColorMenu(sub, m, "Step indicator", &m->indicatorColor);
            P64::appendColorMenu(sub, m, "Control bar",    &m->controlColor);
        }));
    }
};

Model* modelSequencer64 = createModel<Sequencer64, Sequencer64Widget>("Sequencer64");
