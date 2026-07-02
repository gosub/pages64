#include "PageModule.hpp"

// ── Meadow64 ──────────────────────────────────────────────────────────────────
// A meadowphysics-style cascading-counter sequencer. Each of the eight grid
// rows is a countdown counter: on each (divided) clock tick it steps toward the
// left edge and, on reaching it, fires its trigger and reloads. The length
// (period) is set by tapping a column. On firing a counter can also act on
// other counters through cross-rules (the rules page) — reset, hurry, stop, …
// — which cascade into evolving cross-rhythms.
//
// The ball runs leftward: the home/reload column is the right end of its run,
// the fire edge is column 0. Scene buttons A-H mute rows; a muted counter keeps
// running (and keeps applying its rules), only its trigger output is gated.
//
// Top button 2 opens the rules page: scene buttons pick the source row, the
// leftmost grid column picks the destination, and the rest of the grid picks
// the rule (by row), shown as a 6×6 glyph over a dim destination line.

enum RuleType { R_NONE, R_INC, R_DEC, R_MAX, R_MIN, R_RANDOM, R_POLE, R_STOP, R_COUNT };

// 6×6 glyphs, one per rule (col 0 = bit 5). Drawn in the grid's centre 6×6.
static const uint8_t GLYPH[R_COUNT][6] = {
    {0b000000, 0b010000, 0b001000, 0b000100, 0b000010, 0b000000},  // none  (slash)
    {0b001100, 0b001100, 0b111111, 0b111111, 0b001100, 0b001100},  // inc   (plus)
    {0b000000, 0b000000, 0b111111, 0b111111, 0b000000, 0b000000},  // dec   (minus)
    {0b110000, 0b110000, 0b111111, 0b111111, 0b110000, 0b110000},  // max   (├)
    {0b000011, 0b000011, 0b111111, 0b111111, 0b000011, 0b000011},  // min   (┤)
    {0b110011, 0b110011, 0b001100, 0b001100, 0b110011, 0b110011},  // random(bowtie)
    {0b001111, 0b001111, 0b110011, 0b110011, 0b111100, 0b111100},  // pole  (split)
    {0b111111, 0b111111, 0b110011, 0b110011, 0b111111, 0b111111},  // stop  (box)
};

struct Meadow64 : PageModule {
    enum ParamIds { NUM_PARAMS };
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

    static constexpr float FLASH = 0.06f;   // fire-flash duration (s)

    int   len[8];                 // length / period, 1-8
    int   pos[8];                 // current count, 0 … len-1 (0 = fire edge)
    bool  muted[8]    = {};
    bool  stopped[8]  = {};       // frozen by a stop rule
    float flashT[8]   = {};       // fire-flash countdown per row
    int   rule[8][8]  = {};       // rule[source][dest] = RuleType

    int   subPage   = 0;          // 0 = play, 1 = rules
    int   selSource = 0;          // rules page: source row being edited
    int   selDest   = 0;          // rules page: destination row being edited

    P64::ClockDivider clockDiv;
    dsp::PulseGenerator trigPulse[8];

    uint8_t cursorColor = P64::LED_GREEN;
    uint8_t homeColor   = P64::LED_GREEN_DIM;
    uint8_t flashColor  = P64::LED_YELLOW;
    uint8_t muteColor   = P64::LED_RED;
    uint8_t uiColor     = P64::LED_GREEN;       // rules-page selectors + glyph
    uint8_t lineColor   = P64::LED_AMBER_DIM;   // rules-page destination line

    Meadow64() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (int i = 0; i < 8; i++)
            configOutput(TRIG_OUTPUT + i, string::f("Row %d trigger", i + 1));
        configOutput(POLY_OUTPUT, "Poly triggers (8-channel)");
        for (int r = 0; r < 8; r++) { len[r] = r + 1; pos[r] = len[r] - 1; }
    }

    void onReset() override {
        PageModule::onReset();
        for (int r = 0; r < 8; r++) { len[r] = r + 1; pos[r] = len[r] - 1; }
        memset(muted,   0, sizeof(muted));
        memset(stopped, 0, sizeof(stopped));
        memset(flashT,  0, sizeof(flashT));
        memset(rule,    0, sizeof(rule));
        subPage = selSource = selDest = 0;
        clockDiv.set(1);
        cursorColor = P64::LED_GREEN;
        homeColor   = P64::LED_GREEN_DIM;
        flashColor  = P64::LED_YELLOW;
        muteColor   = P64::LED_RED;
        uiColor     = P64::LED_GREEN;
        lineColor   = P64::LED_AMBER_DIM;
    }

    // ── counters ────────────────────────────────────────────────────────────────

    void reloadAll() {
        for (int r = 0; r < 8; r++) { pos[r] = len[r] - 1; stopped[r] = false; }
    }

    void tick() {
        int  q[128], qh = 0, qt = 0;        // fire queue (cascade)
        bool fired[8] = {};

        // Advance every running counter; queue the ones at the fire edge.
        for (int r = 0; r < 8; r++) {
            if (stopped[r]) continue;
            if (pos[r] == 0) q[qt++] = r;
            else pos[r]--;
        }

        // Resolve firings, cascading through the rules; each row fires once.
        while (qh < qt) {
            int r = q[qh++];
            if (fired[r]) continue;
            fired[r] = true;
            if (!muted[r]) trigPulse[r].trigger(0.005f);
            flashT[r] = FLASH;
            pos[r] = len[r] - 1;            // reload

            for (int c = 0; c < 8; c++) {
                switch (rule[r][c]) {
                    case R_INC: if (!stopped[c]) pos[c] = std::min(pos[c] + 1, len[c] - 1); break;
                    case R_DEC: if (!stopped[c]) { if (pos[c] > 0) pos[c]--; if (pos[c] == 0) q[qt++] = c; } break;
                    case R_MAX: pos[c] = len[c] - 1; stopped[c] = false; break;
                    case R_MIN: pos[c] = 0;          stopped[c] = false; break;
                    case R_RANDOM: pos[c] = len[c] > 1 ? (int)(random::u32() % (uint32_t)len[c]) : 0;
                                   stopped[c] = false; break;
                    case R_POLE: { int hi = len[c] - 1; pos[c] = pos[c] * 2 <= hi ? 0 : hi;
                                   stopped[c] = false; } break;
                    case R_STOP: stopped[c] = true; break;
                    default: break;          // R_NONE
                }
            }
        }
    }

    // ── virtual hooks ─────────────────────────────────────────────────────────

    void pagePreProcess() override {
        auto* msg = reinterpret_cast<P64::LeftMessage*>(leftExpander.consumerMessage);

        for (int r = 0; r < 8; r++)
            if (flashT[r] > 0.f) {
                flashT[r] -= sampleTime;
                if (flashT[r] <= 0.f) ledsDirty = true;
            }

        if (!msg) return;
        if (msg->resetTick) {
            reloadAll();
            clockDiv.reset();
            ledsDirty = true;
        }
        if (clockDiv.process(msg->clockTick)) {
            tick();
            ledsDirty = true;
        }
    }

    void pageActive(const P64::LeftMessage& msg) override {
        // Top buttons 1-2: page select (play / rules).
        for (int b = 0; b < 2; b++) {
            if (P64::ccOn(msg, 104 + b) && subPage != b) {
                subPage = b;
                ledsDirty = true;
            }
        }
        if (subPage == 0) playPage(msg);
        else              rulesPage(msg);
    }

    void playPage(const P64::LeftMessage& msg) {
        for (int e = 0; e < msg.eventCount; e++) {
            const P64::GridEvent& ev = msg.events[e];
            if (ev.value == 0) continue;
            if (ev.type == P64::GridEvent::SCENE) {
                // Scene buttons A-H: mute rows.
                muted[ev.index] = !muted[ev.index];
                ledsDirty = true;
            } else if (ev.type == P64::GridEvent::PAD) {
                // Grid: tap a pad to set that row's length (col 1 = 1 … col 8 = 8),
                // reload the counter to its home column and revive it if stopped.
                int row = ev.index / 8;
                int col = ev.index % 8;
                len[row]     = col + 1;
                pos[row]     = col;      // home = len-1 = col
                stopped[row] = false;
                ledsDirty    = true;
            }
        }
    }

    void rulesPage(const P64::LeftMessage& msg) {
        for (int e = 0; e < msg.eventCount; e++) {
            const P64::GridEvent& ev = msg.events[e];
            if (ev.value == 0) continue;
            if (ev.type == P64::GridEvent::SCENE) {
                // Scene buttons A-H: select the source row.
                selSource = ev.index;
                ledsDirty = true;
            } else if (ev.type == P64::GridEvent::PAD) {
                int row = ev.index / 8;
                int col = ev.index % 8;
                if (col == 0)
                    selDest = row;                       // left column: destination
                else
                    rule[selSource][selDest] = row;      // rest: rule = pressed row
                ledsDirty = true;
            }
        }
    }

    void rebuildLeds() override {
        uint8_t next[64];
        if (subPage == 0) buildPlayLeds(next);
        else              buildRulesLeds(next);
        for (int i = 0; i < 64; i++)
            if (next[i] != ledState[i]) { ledState[i] = next[i]; ledsDirty = true; }
    }

    void buildPlayLeds(uint8_t next[64]) {
        memset(next, P64::LED_OFF, 64);
        for (int r = 0; r < 8; r++) {
            next[r * 8 + (len[r] - 1)] = homeColor;             // home marker
            uint8_t cur = flashT[r] > 0.f ? flashColor
                        : muted[r]        ? muteColor
                        :                   cursorColor;
            next[r * 8 + pos[r]] = cur;                         // cursor
        }
    }

    void buildRulesLeds(uint8_t next[64]) {
        memset(next, P64::LED_OFF, 64);
        // dim destination line (cols 1-7), behind the glyph
        for (int c = 1; c < 8; c++) next[selDest * 8 + c] = lineColor;
        // left column: destination selector (selected bright, ruled dim)
        for (int d = 0; d < 8; d++)
            next[d * 8] = (d == selDest)               ? uiColor
                        : rule[selSource][d] != R_NONE ? P64::LED_GREEN_DIM
                        :                                P64::LED_OFF;
        // 6×6 glyph of the selected pair's rule, centred (grid rows/cols 1-6)
        const uint8_t* g = GLYPH[rule[selSource][selDest]];
        for (int gr = 0; gr < 6; gr++)
            for (int gc = 0; gc < 6; gc++)
                if (g[gr] & (1 << (5 - gc)))
                    next[(gr + 1) * 8 + (gc + 1)] = uiColor;
    }

    void buildSceneLeds(uint8_t sceneLeds[8]) override {
        memset(sceneLeds, P64::LED_OFF, 8);
        if (subPage == 0) {
            for (int i = 0; i < 8; i++)
                if (muted[i]) sceneLeds[i] = muteColor;
        } else {
            sceneLeds[selSource] = uiColor;     // rules page: the source row
        }
    }

    void buildTopLeds(uint8_t topLeds[8]) override {
        memset(topLeds, P64::LED_OFF, 8);
        for (int b = 0; b < 2; b++)
            topLeds[b] = (b == subPage) ? uiColor : P64::LED_GREEN_DIM;
    }

    void updateOutputs() override {
        outputs[POLY_OUTPUT].setChannels(8);
        for (int r = 0; r < 8; r++) {
            float v = trigPulse[r].process(sampleTime) ? 10.f : 0.f;
            outputs[TRIG_OUTPUT + r].setVoltage(v);
            outputs[POLY_OUTPUT].setVoltage(v, r);
        }
    }

    // ── serialisation ─────────────────────────────────────────────────────────

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_t* jl = json_array();
        json_t* jm = json_array();
        for (int r = 0; r < 8; r++) {
            json_array_append_new(jl, json_integer(len[r]));
            json_array_append_new(jm, json_boolean(muted[r]));
        }
        json_t* jr = json_array();
        for (int r = 0; r < 8; r++)
            for (int c = 0; c < 8; c++)
                json_array_append_new(jr, json_integer(rule[r][c]));
        json_object_set_new(root, "len",         jl);
        json_object_set_new(root, "muted",       jm);
        json_object_set_new(root, "rule",        jr);
        json_object_set_new(root, "clockDiv",    json_integer(clockDiv.div));
        json_object_set_new(root, "cursorColor", json_integer(cursorColor));
        json_object_set_new(root, "homeColor",   json_integer(homeColor));
        json_object_set_new(root, "flashColor",  json_integer(flashColor));
        json_object_set_new(root, "muteColor",   json_integer(muteColor));
        json_object_set_new(root, "uiColor",     json_integer(uiColor));
        json_object_set_new(root, "lineColor",   json_integer(lineColor));
        return root;
    }

    void dataFromJson(json_t* root) override {
        json_t* j;
        if ((j = json_object_get(root, "len")))
            for (int r = 0; r < 8; r++) {
                json_t* v = json_array_get(j, r);
                if (v) len[r] = clamp((int)json_integer_value(v), 1, 8);
            }
        if ((j = json_object_get(root, "muted")))
            for (int r = 0; r < 8; r++) {
                json_t* v = json_array_get(j, r);
                if (v) muted[r] = json_boolean_value(v);
            }
        if ((j = json_object_get(root, "rule")))
            for (int r = 0; r < 8; r++)
                for (int c = 0; c < 8; c++) {
                    json_t* v = json_array_get(j, r * 8 + c);
                    if (v) rule[r][c] = clamp((int)json_integer_value(v), 0, R_COUNT - 1);
                }
        if ((j = json_object_get(root, "clockDiv")))
            clockDiv.set(clamp((int)json_integer_value(j), 1, 64));
        if ((j = json_object_get(root, "cursorColor"))) cursorColor = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "homeColor")))   homeColor   = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "flashColor")))  flashColor  = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "muteColor")))   muteColor   = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "uiColor")))     uiColor     = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "lineColor")))   lineColor   = (uint8_t)json_integer_value(j);
        for (int r = 0; r < 8; r++) pos[r] = len[r] - 1;
        ledsDirty = true;
    }
};

// ── Widget ────────────────────────────────────────────────────────────────────

struct Meadow64Widget : ModuleWidget {
    Meadow64Widget(Meadow64* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Meadow64.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addChild(createLightCentered<SmallLight<GreenRedLight>>(
            mm2px(Vec(6.0f, 18.0f)), module, Meadow64::ACTIVE_LIGHT));

        const float trigY[8] = {24.f, 34.f, 44.f, 54.f, 64.f, 74.f, 84.f, 94.f};
        for (int i = 0; i < 8; i++)
            addOutput(createOutputCentered<PJ301MPort>(
                mm2px(Vec(20.0f, trigY[i])), module, Meadow64::TRIG_OUTPUT + i));
        addOutput(createOutputCentered<PJ301MPort>(
            mm2px(Vec(20.0f, 108.0f)), module, Meadow64::POLY_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        Meadow64* m = getModule<Meadow64>();
        menu->addChild(new MenuSeparator);
        P64::appendClockDivMenu(menu, &m->clockDiv);
        P64::appendColorMenu(menu, m, "Cursor color",      &m->cursorColor);
        P64::appendColorMenu(menu, m, "Home marker color", &m->homeColor);
        P64::appendColorMenu(menu, m, "Fire flash color",  &m->flashColor);
        P64::appendColorMenu(menu, m, "Mute color",        &m->muteColor);
        P64::appendColorMenu(menu, m, "Rules UI color",    &m->uiColor);
        P64::appendColorMenu(menu, m, "Destination line color", &m->lineColor);
    }
};

Model* modelMeadow64 = createModel<Meadow64, Meadow64Widget>("Meadow64");
