#include "plugin.hpp"

struct Grid64 : Module {
    enum ParamIds {
        MODE_PARAM,     // 0 = toggle, 1 = momentary (applies to all 64 buttons)
        NUM_PARAMS
    };
    enum InputIds  { NUM_INPUTS };
    enum OutputIds {
        ENUMS(GRID_OUTPUT, 64),  // one mono gate output per grid button
        NUM_OUTPUTS
    };
    enum LightIds {
        ENUMS(ACTIVE_LIGHT, 2),  // GreenRedLight: green = active, yellow = connected
        NUM_LIGHTS
    };

    int     myPageIndex   = 0;
    bool    toggleState[64]    = {};
    bool    momentaryState[64] = {};
    uint8_t ledState[64];
    bool    ledsDirty    = true;
    bool    wasActive    = false;
    bool    prevMomentary = true;
    uint8_t activeColor  = P64::LED_GREEN;

    Grid64() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configSwitch(MODE_PARAM, 0.f, 1.f, 1.f, "Mode", {"Toggle", "Momentary"});
        for (int i = 0; i < 64; i++)
            configOutput(GRID_OUTPUT + i,
                string::f("Grid r%d c%d", i / 8 + 1, i % 8 + 1));
        memset(ledState, P64::LED_OFF, sizeof(ledState));

        leftExpander.producerMessage  = new P64::LeftMessage;
        leftExpander.consumerMessage  = new P64::LeftMessage;
        memset(leftExpander.producerMessage,  0, sizeof(P64::LeftMessage));
        memset(leftExpander.consumerMessage,  0, sizeof(P64::LeftMessage));
        rightExpander.producerMessage = new P64::RightMessage;
        rightExpander.consumerMessage = new P64::RightMessage;
        memset(rightExpander.producerMessage, 0, sizeof(P64::RightMessage));
        memset(rightExpander.consumerMessage, 0, sizeof(P64::RightMessage));
    }

    ~Grid64() {
        delete (P64::LeftMessage*)  leftExpander.producerMessage;
        delete (P64::LeftMessage*)  leftExpander.consumerMessage;
        delete (P64::RightMessage*) rightExpander.producerMessage;
        delete (P64::RightMessage*) rightExpander.consumerMessage;
    }

    void onReset() override {
        memset(toggleState,    0,            sizeof(toggleState));
        memset(momentaryState, 0,            sizeof(momentaryState));
        memset(ledState,       P64::LED_OFF, sizeof(ledState));
        ledsDirty     = true;
        wasActive     = false;
        prevMomentary = true;
        activeColor   = P64::LED_GREEN;
    }

    // ── helpers ──────────────────────────────────────────────────────────────

    bool isLeftNeighbour(Module* m) const {
        return m && (m->model == modelBase || m->model == modelButtons64 || m->model == modelGrid64);
    }

    bool isRightNeighbour(Module* m) const {
        return m && (m->model == modelButtons64 || m->model == modelGrid64);
    }

    bool isMomentary() { return params[MODE_PARAM].getValue() > 0.5f; }

    void rebuildLeds() {
        bool momentary = isMomentary();
        for (int i = 0; i < 64; i++) {
            bool active = momentary ? momentaryState[i] : toggleState[i];
            uint8_t color = active ? activeColor : P64::LED_OFF;
            if (color != ledState[i]) {
                ledState[i] = color;
                ledsDirty   = true;
            }
        }
    }

    void setOutputs() {
        bool momentary = isMomentary();
        for (int i = 0; i < 64; i++) {
            bool active = momentary ? momentaryState[i] : toggleState[i];
            outputs[GRID_OUTPUT + i].setVoltage(active ? 5.f : 0.f);
        }
    }

    // ── process ──────────────────────────────────────────────────────────────

    void process(const ProcessArgs& args) override {
        // momentary→toggle transition clears toggle state
        bool momentary = isMomentary();
        if (prevMomentary && !momentary) {
            memset(toggleState, 0, sizeof(toggleState));
            ledsDirty = true;
        }
        prevMomentary = momentary;

        bool amActive = false;

        if (isLeftNeighbour(leftExpander.module)) {
            // 1. Read LeftMessage
            auto* fromLeft = reinterpret_cast<P64::LeftMessage*>(leftExpander.consumerMessage);
            myPageIndex = fromLeft ? fromLeft->pageCounter : 0;
            amActive    = fromLeft && (fromLeft->activePage == myPageIndex);

            // 2. Forward LeftMessage rightward
            if (isRightNeighbour(rightExpander.module)) {
                auto* toRight = reinterpret_cast<P64::LeftMessage*>(
                    rightExpander.module->leftExpander.producerMessage);
                if (toRight && fromLeft) {
                    *toRight = *fromLeft;
                    toRight->pageCounter = myPageIndex + 1;
                }
                rightExpander.module->leftExpander.messageFlipRequested = true;
            }

            // 3. Force LED refresh when becoming active or on repaint request
            if (amActive && (!wasActive || (fromLeft && fromLeft->repaintRequested)))
                ledsDirty = true;

            // 4. Process MIDI note events
            if (amActive && fromLeft) {
                for (int row = 0; row < 8; row++) {
                    for (int col = 0; col < 8; col++) {
                        int note = row * 16 + col;
                        if (!fromLeft->noteEvent[note]) continue;
                        bool on  = fromLeft->noteVelocity[note] > 0;
                        int  idx = row * 8 + col;
                        if (momentary) {
                            momentaryState[idx] = on;
                            ledsDirty = true;
                        } else if (on) {
                            toggleState[idx] = !toggleState[idx];
                            ledsDirty = true;
                        }
                    }
                }
            } else if (!amActive && momentary) {
                for (int i = 0; i < 64; i++) {
                    if (momentaryState[i]) {
                        momentaryState[i] = false;
                        ledsDirty = true;
                    }
                }
            }

            // 5. Read RightMessage from chain
            P64::RightMessage chainMsg = {};
            if (isRightNeighbour(rightExpander.module)) {
                auto* fromRight = reinterpret_cast<P64::RightMessage*>(rightExpander.consumerMessage);
                if (fromRight) chainMsg = *fromRight;
            }
            rightExpander.messageFlipRequested = true;

            // 6. Build and send RightMessage to left neighbour
            auto* toLeft = reinterpret_cast<P64::RightMessage*>(
                leftExpander.module->rightExpander.producerMessage);
            if (toLeft) {
                if (amActive) {
                    rebuildLeds();
                    memcpy(toLeft->gridLeds, ledState, 64);
                    toLeft->dirty = ledsDirty;
                    ledsDirty     = false;
                } else {
                    *toLeft = chainMsg;
                }
                toLeft->chainLength = 1 + chainMsg.chainLength;
            }

            leftExpander.messageFlipRequested = true;
        }

        wasActive = amActive;
        setOutputs();
        bool connected = isLeftNeighbour(leftExpander.module);
        lights[ACTIVE_LIGHT + 0].setBrightness(amActive ? 1.f : (connected ? 0.25f : 0.f));
        lights[ACTIVE_LIGHT + 1].setBrightness((connected && !amActive) ? 0.25f : 0.f);
    }

    json_t* dataToJson() override {
        json_t* root  = json_object();
        json_t* state = json_array();
        for (int i = 0; i < 64; i++)
            json_array_append_new(state, json_boolean(toggleState[i]));
        json_object_set_new(root, "toggleState", state);
        json_object_set_new(root, "activeColor", json_integer(activeColor));
        return root;
    }

    void dataFromJson(json_t* root) override {
        json_t* state = json_object_get(root, "toggleState");
        if (state)
            for (int i = 0; i < 64; i++) {
                json_t* v = json_array_get(state, i);
                if (v) toggleState[i] = json_boolean_value(v);
            }
        json_t* color = json_object_get(root, "activeColor");
        if (color) activeColor = (uint8_t) json_integer_value(color);
        ledsDirty = true;
    }
};

// ── Widget ────────────────────────────────────────────────────────────────────

struct Grid64Widget : ModuleWidget {
    Grid64Widget(Grid64* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Grid64.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Active light: SmallLight at x=6mm (left border), y=18.0mm — matches all page modules
        addChild(createLightCentered<SmallLight<GreenRedLight>>(
            mm2px(Vec(6.0f, 18.0f)), module, Grid64::ACTIVE_LIGHT));

        // Mode switch centered at bottom below jack grid
        addParam(createParamCentered<CKSS>(
            mm2px(Vec(40.64f, 108.0f)), module, Grid64::MODE_PARAM));

        // 8×8 jack grid: col x = 10.89 + col*8.5, row y = 24 + row*9
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                float x = 10.89f + col * 8.5f;
                float y = 24.0f  + row * 9.0f;
                addOutput(createOutputCentered<PJ301MPort>(
                    mm2px(Vec(x, y)), module, Grid64::GRID_OUTPUT + row * 8 + col));
            }
        }
    }

    void appendContextMenu(Menu* menu) override {
        Grid64* m = getModule<Grid64>();
        menu->addChild(new MenuSeparator);
        menu->addChild(createSubmenuItem("Button color", "", [=](Menu* sub) {
            for (auto& c : P64::LED_COLOR_DEFS) {
                if (c.velocity == P64::LED_OFF) continue;
                uint8_t vel = c.velocity;
                sub->addChild(createCheckMenuItem(c.name, "",
                    [=]() { return m->activeColor == vel; },
                    [=]() { m->activeColor = vel; m->ledsDirty = true; }
                ));
            }
        }));
    }
};

Model* modelGrid64 = createModel<Grid64, Grid64Widget>("Grid64");
