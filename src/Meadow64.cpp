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
    float flashT[8]   = {};       // fire-flash countdown per row

    P64::ClockDivider clockDiv;
    dsp::PulseGenerator trigPulse[8];

    uint8_t cursorColor = P64::LED_GREEN;
    uint8_t homeColor   = P64::LED_GREEN_DIM;
    uint8_t flashColor  = P64::LED_YELLOW;
    uint8_t muteColor   = P64::LED_RED;

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
        memset(muted,  0, sizeof(muted));
        memset(flashT, 0, sizeof(flashT));
        clockDiv.set(1);
        cursorColor = P64::LED_GREEN;
        homeColor   = P64::LED_GREEN_DIM;
        flashColor  = P64::LED_YELLOW;
        muteColor   = P64::LED_RED;
    }

    // ── counters ────────────────────────────────────────────────────────────────

    void reloadAll() {
        for (int r = 0; r < 8; r++) pos[r] = len[r] - 1;
    }

    void tick() {
        for (int r = 0; r < 8; r++) {
            if (pos[r] == 0) {              // at the fire edge → fire and reload
                if (!muted[r]) trigPulse[r].trigger(0.005f);
                flashT[r] = FLASH;
                pos[r] = len[r] - 1;
            } else {
                pos[r]--;
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
        // Scene buttons A-H: mute rows.
        for (int i = 0; i < 8; i++)
            if (msg.sceneEvent[i] && msg.sceneVelocity[i] > 0) {
                muted[i] = !muted[i];
                ledsDirty = true;
            }

        // Grid: tap a pad to set that row's length (col 1 = 1 … col 8 = 8) and
        // reload the counter to its home column.
        for (int row = 0; row < 8; row++)
            for (int col = 0; col < 8; col++) {
                int note = row * 16 + col;
                if (msg.noteEvent[note] && msg.noteVelocity[note] > 0) {
                    len[row] = col + 1;
                    pos[row] = col;          // home = len-1 = col
                    ledsDirty = true;
                }
            }
    }

    void rebuildLeds() override {
        uint8_t next[64];
        memset(next, P64::LED_OFF, sizeof(next));
        for (int r = 0; r < 8; r++) {
            next[r * 8 + (len[r] - 1)] = homeColor;             // home marker
            uint8_t cur = flashT[r] > 0.f ? flashColor
                        : muted[r]        ? muteColor
                        :                   cursorColor;
            next[r * 8 + pos[r]] = cur;                         // cursor
        }
        for (int i = 0; i < 64; i++)
            if (next[i] != ledState[i]) { ledState[i] = next[i]; ledsDirty = true; }
    }

    void buildSceneLeds(uint8_t sceneLeds[8]) override {
        memset(sceneLeds, P64::LED_OFF, 8);
        for (int i = 0; i < 8; i++)
            if (muted[i]) sceneLeds[i] = muteColor;
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
        json_object_set_new(root, "len",         jl);
        json_object_set_new(root, "muted",       jm);
        json_object_set_new(root, "clockDiv",    json_integer(clockDiv.div));
        json_object_set_new(root, "cursorColor", json_integer(cursorColor));
        json_object_set_new(root, "homeColor",   json_integer(homeColor));
        json_object_set_new(root, "flashColor",  json_integer(flashColor));
        json_object_set_new(root, "muteColor",   json_integer(muteColor));
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
        if ((j = json_object_get(root, "clockDiv")))
            clockDiv.set(clamp((int)json_integer_value(j), 1, 64));
        if ((j = json_object_get(root, "cursorColor"))) cursorColor = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "homeColor")))   homeColor   = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "flashColor")))  flashColor  = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "muteColor")))   muteColor   = (uint8_t)json_integer_value(j);
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
    }
};

Model* modelMeadow64 = createModel<Meadow64, Meadow64Widget>("Meadow64");
