#include "PageModule.hpp"

// Cycle periods in clock ticks, indexed by speed row (0 = top/fastest, 6 = slowest)
static constexpr int FLIN_PERIODS[7] = {1, 2, 3, 4, 6, 8, 16};

struct Flin64 : PageModule {
    enum ParamIds { NUM_PARAMS };
    enum InputIds  { NUM_INPUTS };
    enum OutputIds {
        ENUMS(GATE_OUTPUT, 8),
        POLY_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        ENUMS(ACTIVE_LIGHT, 2),
        NUM_LIGHTS
    };

    // Per-column state (-1 = inactive)
    int  activeRow[8]     = {-1,-1,-1,-1,-1,-1,-1,-1};
    int  snakeLenRows[8]  = { 1, 1, 1, 1, 1, 1, 1, 1};
    int  phase[8]         = {};
    bool gateHigh[8]      = {};

    // Pad tracking for two-button length gestures
    bool padHeld[64]      = {};

    // Rising-edge state
    bool prevClock        = false;
    bool prevReset        = false;

    // Appearance
    uint8_t snakeColor    = P64::LED_GREEN;
    uint8_t bgColor       = P64::LED_OFF;

    Flin64() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (int i = 0; i < 8; i++)
            configOutput(GATE_OUTPUT + i, string::f("Gate %d", i + 1));
        configOutput(POLY_OUTPUT, "Poly gates (8-channel)");
    }

    void onReset() override {
        PageModule::onReset();
        for (int i = 0; i < 8; i++) {
            activeRow[i]    = -1;
            snakeLenRows[i] = 1;
            phase[i]        = 0;
            gateHigh[i]     = false;
        }
        memset(padHeld, 0, sizeof(padHeld));
        prevClock   = false;
        prevReset   = false;
        snakeColor  = P64::LED_GREEN;
        bgColor     = P64::LED_OFF;
    }

    // ── virtual hooks ─────────────────────────────────────────────────────────

    void pagePreProcess() override {
        auto* msg = reinterpret_cast<P64::LeftMessage*>(leftExpander.consumerMessage);
        if (!msg) return;

        // Reset rising edge
        bool resetHigh = msg->resetVoltage >= 1.0f;
        if (resetHigh && !prevReset) {
            for (int c = 0; c < 8; c++) {
                phase[c]     = 0;
                gateHigh[c]  = false;
            }
            ledsDirty = true;
        }
        prevReset = resetHigh;

        // Clock rising edge
        bool clockHigh = msg->clockVoltage >= 1.0f;
        if (clockHigh && !prevClock) {
            for (int c = 0; c < 8; c++) {
                if (activeRow[c] < 0) continue;
                int period = FLIN_PERIODS[activeRow[c]];
                phase[c]++;
                if (phase[c] >= period) {
                    phase[c]    = 0;
                    gateHigh[c] = true;
                }
                // Close gate once tail clears the top border
                float vpos = phase[c] * 32.f / period;
                if (vpos >= snakeLenRows[c])
                    gateHigh[c] = false;
            }
            ledsDirty = true;
        }
        prevClock = clockHigh;
    }

    void pageActive(const P64::LeftMessage& msg) override {
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                int note = row * 16 + col;
                if (!msg.noteEvent[note]) continue;

                bool pressed = msg.noteVelocity[note] > 0;
                int  idx     = row * 8 + col;

                if (pressed) {
                    // Check if another row is already held in this column
                    int heldRow = -1;
                    for (int r2 = 0; r2 < 8; r2++) {
                        if (r2 != row && padHeld[r2 * 8 + col]) {
                            heldRow = r2;
                            break;
                        }
                    }
                    padHeld[idx] = true;

                    if (heldRow >= 0) {
                        // Two-button length gesture
                        int len = std::abs(row - heldRow);
                        snakeLenRows[col] = clamp(len, 1, 7);
                        ledsDirty = true;
                    } else if (row < 7) {
                        // Speed row: always set speed and restart phase
                        activeRow[col]  = row;
                        phase[col]      = 0;
                        gateHigh[col]   = false;
                        ledsDirty       = true;
                    } else {
                        // Row 7 alone: stop column
                        activeRow[col]  = -1;
                        phase[col]      = 0;
                        gateHigh[col]   = false;
                        ledsDirty       = true;
                    }
                } else {
                    padHeld[idx] = false;
                }
            }
        }
    }

    void pageInactive() override {
        // Keep pads cleared if this page loses focus while buttons are held
        memset(padHeld, 0, sizeof(padHeld));
    }

    void rebuildLeds() override {
        for (int col = 0; col < 8; col++) {
            int period = (activeRow[col] >= 0) ? FLIN_PERIODS[activeRow[col]] : 1;
            float vpos    = (activeRow[col] >= 0) ? (phase[col] * 32.f / period) : 32.f;
            int   headRow = (int) vpos;   // 0-7 = visible, 8+ = in invisible gap

            for (int row = 0; row < 8; row++) {
                uint8_t color = P64::LED_OFF;

                if (activeRow[col] >= 0 && bgColor != P64::LED_OFF)
                    color = bgColor;

                // Snake body: head at headRow, tail snakeLenRows below it
                if (headRow < 8 + snakeLenRows[col]) {
                    int tailRow = headRow - snakeLenRows[col] + 1;
                    if (row <= headRow && row >= tailRow)
                        color = snakeColor;
                }

                ledState[row * 8 + col] = color;
            }
        }
    }

    void updateOutputs() override {
        outputs[POLY_OUTPUT].setChannels(8);
        for (int i = 0; i < 8; i++) {
            float v = gateHigh[i] ? 10.f : 0.f;
            outputs[GATE_OUTPUT + i].setVoltage(v);
            outputs[POLY_OUTPUT].setVoltage(v, i);
        }
    }

    // ── serialisation ─────────────────────────────────────────────────────────

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "snakeColor", json_integer(snakeColor));
        json_object_set_new(root, "bgColor",    json_integer(bgColor));
        json_t* ar = json_array();
        json_t* sl = json_array();
        json_t* ph = json_array();
        for (int i = 0; i < 8; i++) {
            json_array_append_new(ar, json_integer(activeRow[i]));
            json_array_append_new(sl, json_integer(snakeLenRows[i]));
            json_array_append_new(ph, json_integer(phase[i]));
        }
        json_object_set_new(root, "activeRow",    ar);
        json_object_set_new(root, "snakeLenRows", sl);
        json_object_set_new(root, "phase",        ph);
        return root;
    }

    void dataFromJson(json_t* root) override {
        json_t* j;
        if ((j = json_object_get(root, "snakeColor"))) snakeColor = (uint8_t) json_integer_value(j);
        if ((j = json_object_get(root, "bgColor")))    bgColor    = (uint8_t) json_integer_value(j);
        if ((j = json_object_get(root, "activeRow")))
            for (int i = 0; i < 8; i++) {
                json_t* v = json_array_get(j, i);
                if (v) activeRow[i] = clamp((int) json_integer_value(v), -1, 6);
            }
        if ((j = json_object_get(root, "snakeLenRows")))
            for (int i = 0; i < 8; i++) {
                json_t* v = json_array_get(j, i);
                if (v) snakeLenRows[i] = clamp((int) json_integer_value(v), 1, 7);
            }
        if ((j = json_object_get(root, "phase")))
            for (int i = 0; i < 8; i++) {
                json_t* v = json_array_get(j, i);
                if (v) phase[i] = (int) json_integer_value(v);
            }
        ledsDirty = true;
    }
};

// ── Widget ────────────────────────────────────────────────────────────────────

struct Flin64Widget : ModuleWidget {
    Flin64Widget(Flin64* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Flin64.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addChild(createLightCentered<SmallLight<GreenRedLight>>(
            mm2px(Vec(6.0f, 18.0f)), module, Flin64::ACTIVE_LIGHT));

        // 8 mono gate outputs
        const float gateY[8] = {24.f, 34.f, 44.f, 54.f, 64.f, 74.f, 84.f, 94.f};
        for (int i = 0; i < 8; i++) {
            addOutput(createOutputCentered<PJ301MPort>(
                mm2px(Vec(15.0f, gateY[i])), module, Flin64::GATE_OUTPUT + i));
        }

        // Poly output
        addOutput(createOutputCentered<PJ301MPort>(
            mm2px(Vec(15.0f, 108.0f)), module, Flin64::POLY_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        Flin64* m = getModule<Flin64>();
        menu->addChild(new MenuSeparator);
        menu->addChild(createSubmenuItem("Snake color", "", [=](Menu* sub) {
            for (auto& c : P64::LED_COLOR_DEFS) {
                if (c.velocity == P64::LED_OFF) continue;
                uint8_t vel = c.velocity;
                sub->addChild(createCheckMenuItem(c.name, "",
                    [=]() { return m->snakeColor == vel; },
                    [=]() { m->snakeColor = vel; m->ledsDirty = true; }
                ));
            }
        }));
        menu->addChild(createSubmenuItem("Background color", "", [=](Menu* sub) {
            sub->addChild(createCheckMenuItem("Off", "",
                [=]() { return m->bgColor == P64::LED_OFF; },
                [=]() { m->bgColor = P64::LED_OFF; m->ledsDirty = true; }
            ));
            for (auto& c : P64::LED_COLOR_DEFS) {
                if (c.velocity == P64::LED_OFF) continue;
                uint8_t vel = c.velocity;
                sub->addChild(createCheckMenuItem(c.name, "",
                    [=]() { return m->bgColor == vel; },
                    [=]() { m->bgColor = vel; m->ledsDirty = true; }
                ));
            }
        }));
    }
};

Model* modelFlin64 = createModel<Flin64, Flin64Widget>("Flin64");
