#include "PageModule.hpp"

// Cycle periods in clock ticks, indexed by speed row (0 = top/fastest, 6 = slowest)
static constexpr int FLIN_PERIODS[7] = {1, 2, 3, 4, 5, 6, 7};

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
    int  stepTimer[8]     = {};   // clock-divider counter (0..period-1)
    int  virtualStep[8]   = {};   // position in 32-step cycle (0-7 visible, 8-31 gap)
    bool gateHigh[8]      = {};

    // Pad tracking for two-button length gestures
    bool padHeld[64]        = {};
    bool twoButtonFired[8]  = {};   // set when two-button gesture fires; cleared on full release

    P64::ClockDivider clockDiv;

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
            stepTimer[i]    = 0;
            virtualStep[i]  = 0;
            gateHigh[i]     = false;
        }
        memset(padHeld,        0, sizeof(padHeld));
        memset(twoButtonFired, 0, sizeof(twoButtonFired));
        clockDiv.set(1);
        snakeColor  = P64::LED_GREEN;
        bgColor     = P64::LED_OFF;
    }

    // ── virtual hooks ─────────────────────────────────────────────────────────

    void pagePreProcess() override {
        auto* msg = reinterpret_cast<P64::LeftMessage*>(leftExpander.consumerMessage);
        if (!msg) return;

        if (msg->resetTick) {
            for (int c = 0; c < 8; c++) {
                stepTimer[c]   = 0;
                virtualStep[c] = 0;
                gateHigh[c]    = false;
            }
            ledsDirty = true;
        }

        bool tick = clockDiv.process(msg->clockTick);

        if (tick) {
            for (int c = 0; c < 8; c++) {
                if (activeRow[c] < 0) continue;
                int period = FLIN_PERIODS[activeRow[c]];
                stepTimer[c]++;
                if (stepTimer[c] >= period) {
                    stepTimer[c] = 0;
                    virtualStep[c]++;
                    if (virtualStep[c] >= 32) {
                        virtualStep[c] = 0;
                        gateHigh[c] = true;
                    }
                    // Close gate once tail clears the top border (row 0)
                    if (virtualStep[c] >= snakeLenRows[c])
                        gateHigh[c] = false;
                    ledsDirty = true;
                }
            }
        }
    }

    void pageActive(const P64::LeftMessage& msg) override {
        for (int e = 0; e < msg.eventCount; e++) {
            const P64::GridEvent& ev = msg.events[e];
            if (ev.type == P64::GridEvent::PAD) {
                bool pressed = ev.value > 0;
                int  idx     = ev.index;
                int  row     = idx / 8;
                int  col     = idx % 8;

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
                        // Two-button length gesture (works in both on and off state)
                        snakeLenRows[col]   = clamp(std::abs(row - heldRow), 1, 7);
                        twoButtonFired[col] = true;
                        ledsDirty           = true;
                        // If on: update gate threshold immediately
                        if (activeRow[col] >= 0 && virtualStep[col] >= snakeLenRows[col])
                            gateHigh[col] = false;
                    }
                    // else: single press — defer all action to note-off
                } else {
                    // Note-off
                    padHeld[idx] = false;

                    // Check if any button is still held in this column
                    bool anyHeld = false;
                    for (int r2 = 0; r2 < 8; r2++) {
                        if (padHeld[r2 * 8 + col]) { anyHeld = true; break; }
                    }

                    if (!twoButtonFired[col] && !anyHeld) {
                        if (row < 7) {
                            if (activeRow[col] < 0) {
                                // Tap to start from off state
                                virtualStep[col] = 0;
                                stepTimer[col]   = 0;
                                gateHigh[col]    = true;
                                activeRow[col]   = row;
                            } else {
                                // Speed change while on: keep phase/gate, update speed
                                activeRow[col] = row;
                                stepTimer[col] = 0;
                            }
                            ledsDirty = true;
                        } else if (activeRow[col] >= 0) {
                            // Row 7 tap while on: stop and reset length
                            activeRow[col]    = -1;
                            stepTimer[col]    = 0;
                            virtualStep[col]  = 0;
                            gateHigh[col]     = false;
                            snakeLenRows[col] = 1;
                            ledsDirty         = true;
                        }
                    }

                    if (!anyHeld)
                        twoButtonFired[col] = false;
                }
            }
        }
    }

    void pageInactive() override {
        memset(padHeld,        0, sizeof(padHeld));
        memset(twoButtonFired, 0, sizeof(twoButtonFired));
    }

    void rebuildLeds() override {
        for (int col = 0; col < 8; col++) {
            int headRow = (activeRow[col] >= 0) ? virtualStep[col] : 32;  // 0-7 visible, 8+ gap

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
        json_object_set_new(root, "clockDiv",   json_integer(clockDiv.div));
        json_t* ar = json_array();
        json_t* sl = json_array();
        json_t* vs = json_array();
        for (int i = 0; i < 8; i++) {
            json_array_append_new(ar, json_integer(activeRow[i]));
            json_array_append_new(sl, json_integer(snakeLenRows[i]));
            json_array_append_new(vs, json_integer(virtualStep[i]));
        }
        json_object_set_new(root, "activeRow",    ar);
        json_object_set_new(root, "snakeLenRows", sl);
        json_object_set_new(root, "virtualStep",  vs);
        return root;
    }

    void dataFromJson(json_t* root) override {
        json_t* j;
        if ((j = json_object_get(root, "snakeColor"))) snakeColor = (uint8_t) json_integer_value(j);
        if ((j = json_object_get(root, "bgColor")))    bgColor    = (uint8_t) json_integer_value(j);
        if ((j = json_object_get(root, "clockDiv")))   clockDiv.set(clamp((int) json_integer_value(j), 1, 64));
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
        if ((j = json_object_get(root, "virtualStep")))
            for (int i = 0; i < 8; i++) {
                json_t* v = json_array_get(j, i);
                if (v) virtualStep[i] = clamp((int) json_integer_value(v), 0, 31);
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
                mm2px(Vec(20.0f, gateY[i])), module, Flin64::GATE_OUTPUT + i));
        }

        // Poly output
        addOutput(createOutputCentered<PJ301MPort>(
            mm2px(Vec(20.0f, 108.0f)), module, Flin64::POLY_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        Flin64* m = getModule<Flin64>();
        menu->addChild(new MenuSeparator);
        P64::appendClockDivMenu(menu, &m->clockDiv);
        P64::appendColorMenu(menu, m, "Snake color",      &m->snakeColor);
        P64::appendColorMenu(menu, m, "Background color", &m->bgColor, true);
    }
};

Model* modelFlin64 = createModel<Flin64, Flin64Widget>("Flin64");
