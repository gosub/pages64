#include "PageModule.hpp"

// ── Life64 ────────────────────────────────────────────────────────────────────
// Conway's Game of Life on the grid. The clock steps the generations (B3/S23);
// pressing a pad toggles a cell at any time, frozen or running. Edges are
// bounded by default; a context-menu option wraps them into a torus.
//
// Outputs: 64 cell gates in the Gome64/Buttons64 4 × 16-channel poly format
// (live cell = sustained 10 V), ROWS/COLS binary CVs and a density CV.

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

    bool cells[64] = {};
    bool wrap      = false;

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
    }

    void onReset() override {
        PageModule::onReset();
        memset(cells, 0, sizeof(cells));
        wrap = false;
        clockDiv.set(1);
        cellColor = P64::LED_GREEN;
        uiColor   = P64::LED_AMBER;
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
        auto* msg = reinterpret_cast<P64::LeftMessage*>(leftExpander.consumerMessage);
        if (!msg) return;

        if (msg->resetTick)
            clockDiv.reset();

        if (clockDiv.process(msg->clockTick)) {
            stepGeneration();
            ledsDirty = true;
        }
    }

    void pageActive(const P64::LeftMessage& msg) override {
        // Toggle cells on press (no two-button gestures; edit latency matters
        // when punching cells into a running colony).
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                int note = row * 16 + col;
                if (!msg.noteEvent[note] || msg.noteVelocity[note] == 0) continue;
                int idx = row * 8 + col;
                cells[idx] = !cells[idx];
                ledsDirty  = true;
            }
        }
    }

    void rebuildLeds() override {
        for (int i = 0; i < 64; i++) {
            uint8_t color = cells[i] ? cellColor : P64::LED_OFF;
            if (color != ledState[i]) {
                ledState[i] = color;
                ledsDirty   = true;
            }
        }
    }

    void updateOutputs() override {
        for (int out = 0; out < 4; out++) {
            outputs[CELL_OUTPUT + out].setChannels(16);
            for (int localRow = 0; localRow < 2; localRow++) {
                for (int col = 0; col < 8; col++) {
                    int idx = (out * 2 + localRow) * 8 + col;
                    outputs[CELL_OUTPUT + out].setVoltage(cells[idx] ? 10.f : 0.f,
                                                          localRow * 8 + col);
                }
            }
        }
    }

    // ── serialisation ─────────────────────────────────────────────────────────

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_t* jc = json_array();
        for (int i = 0; i < 64; i++)
            json_array_append_new(jc, json_boolean(cells[i]));
        json_object_set_new(root, "cells",     jc);
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
        if ((j = json_object_get(root, "wrap")))
            wrap = json_boolean_value(j);
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
        P64::appendColorMenu(menu, m, "Cell color", &m->cellColor);
        P64::appendColorMenu(menu, m, "UI color",   &m->uiColor);
    }
};

Model* modelLife64 = createModel<Life64, Life64Widget>("Life64");
