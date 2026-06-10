#include "PageModule.hpp"

// ── Euclid64 ──────────────────────────────────────────────────────────────────
// 8 columns = 8 euclidean trigger voices. Each column shows its pattern as a
// bar growing from the bottom (step 1 = bottom row); onsets are lit, rests are
// dim, and the playing step is highlighted each clock tick.
//
// Gestures (resolved on release, like Flin64/Step64):
//   - tap a row: set fill to that height (1-8); tapping the marked pad at
//     the current fill height clears the voice; tapping above the length
//     grows the length
//   - hold one row + press another in the same column: length = the higher
//     of the two heights, fill = the lower (sets E(fill, length) in one move)
// Scene buttons A-H mute/unmute voices 1-8.

// True if step i of E(fill, len) is an onset (Bjorklund via Bresenham).
static bool euclidOnset(int i, int fill, int len) {
    if (fill <= 0) return false;
    return (i + 1) * fill / len - i * fill / len >= 1;
}

struct Euclid64 : PageModule {
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

    // Per-voice state
    int  len[8];     // pattern length 1-8
    int  fill[8];    // onsets 0-8 (0 = voice off)
    int  pos[8];     // current step
    bool muted[8] = {};

    // Pad tracking for two-button gestures
    bool padHeld[64]       = {};
    bool twoButtonFired[8] = {};

    P64::ClockDivider clockDiv;
    dsp::PulseGenerator trigPulse[8];

    // Colors
    uint8_t onsetColor     = P64::LED_GREEN;
    uint8_t restColor      = P64::LED_GREEN_DIM;
    uint8_t indicatorColor = P64::LED_AMBER;
    uint8_t fillColor      = P64::LED_OLIVE;     // playhead accent at the fill height (the clear pad)
    uint8_t muteColor      = P64::LED_RED;

    Euclid64() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (int i = 0; i < 8; i++)
            configOutput(TRIG_OUTPUT + i, string::f("Trigger %d", i + 1));
        configOutput(POLY_OUTPUT, "Poly triggers (8-channel)");
        for (int i = 0; i < 8; i++) {
            len[i]  = 8;
            fill[i] = 0;
            pos[i]  = 0;
        }
    }

    void onReset() override {
        PageModule::onReset();
        for (int i = 0; i < 8; i++) {
            len[i]   = 8;
            fill[i]  = 0;
            pos[i]   = 0;
            muted[i] = false;
        }
        memset(padHeld,        0, sizeof(padHeld));
        memset(twoButtonFired, 0, sizeof(twoButtonFired));
        clockDiv.set(1);
        onsetColor     = P64::LED_GREEN;
        restColor      = P64::LED_GREEN_DIM;
        indicatorColor = P64::LED_AMBER;
        fillColor      = P64::LED_OLIVE;
        muteColor      = P64::LED_RED;
    }

    // ── virtual hooks ─────────────────────────────────────────────────────────

    void pagePreProcess() override {
        auto* msg = reinterpret_cast<P64::LeftMessage*>(leftExpander.consumerMessage);
        if (!msg) return;

        if (msg->resetTick) {
            for (int c = 0; c < 8; c++)
                pos[c] = 0;
            clockDiv.reset();
            ledsDirty = true;
        }

        if (clockDiv.process(msg->clockTick)) {
            for (int c = 0; c < 8; c++) {
                if (fill[c] <= 0) continue;
                if (!muted[c] && euclidOnset(pos[c], fill[c], len[c]))
                    trigPulse[c].trigger(0.005f);
                pos[c] = (pos[c] + 1) % len[c];
            }
            ledsDirty = true;
        }
    }

    void pageActive(const P64::LeftMessage& msg) override {
        // Scene buttons A-H: mute toggles for voices 1-8
        for (int i = 0; i < 8; i++) {
            if (msg.sceneEvent[i] && msg.sceneVelocity[i] > 0) {
                muted[i] = !muted[i];
                ledsDirty = true;
            }
        }

        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                int note = row * 16 + col;
                if (!msg.noteEvent[note]) continue;

                bool pressed = msg.noteVelocity[note] > 0;
                int  idx     = row * 8 + col;
                int  height  = 8 - row;   // bottom row = 1, top row = 8

                if (pressed) {
                    int heldRow = -1;
                    for (int r2 = 0; r2 < 8; r2++)
                        if (r2 != row && padHeld[r2 * 8 + col]) { heldRow = r2; break; }
                    padHeld[idx] = true;

                    if (heldRow >= 0) {
                        // Two-button: higher pad = length, lower pad = fill
                        int h2 = 8 - heldRow;
                        len[col]  = std::max(height, h2);
                        fill[col] = std::min(height, h2);
                        pos[col] %= len[col];
                        twoButtonFired[col] = true;
                        ledsDirty = true;
                    }
                    // else: single press — resolve on release
                } else {
                    padHeld[idx] = false;
                    bool anyHeld = false;
                    for (int r2 = 0; r2 < 8; r2++)
                        if (padHeld[r2 * 8 + col]) { anyHeld = true; break; }

                    if (!twoButtonFired[col] && !anyHeld) {
                        if (fill[col] == height) {
                            fill[col] = 0;          // tap current fill: clear voice
                        } else {
                            fill[col] = height;
                            if (height > len[col]) {  // tap above length: grow it
                                len[col] = height;
                                pos[col] %= len[col];
                            }
                        }
                        ledsDirty = true;
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
        memset(ledState, P64::LED_OFF, sizeof(ledState));
        for (int c = 0; c < 8; c++) {
            if (fill[c] <= 0) continue;
            for (int i = 0; i < len[c]; i++) {
                int     row   = 7 - i;   // step 1 at the bottom
                uint8_t color = euclidOnset(i, fill[c], len[c]) ? onsetColor : restColor;
                if (i == pos[c]) {
                    // Playhead; in the fill color when crossing the fill
                    // height (the pad that clears the voice when tapped).
                    if (i == fill[c] - 1)
                        color = fillColor;
                    else
                        color = muted[c] ? muteColor : indicatorColor;
                }
                ledState[row * 8 + c] = color;
            }
        }
    }

    void buildSceneLeds(uint8_t sceneLeds[8]) override {
        memset(sceneLeds, P64::LED_OFF, 8);
        for (int i = 0; i < 8; i++) {
            if (muted[i])
                sceneLeds[i] = muteColor;
            else if (fill[i] > 0)
                sceneLeds[i] = onsetColor;
        }
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
        json_object_set_new(root, "clockDiv",       json_integer(clockDiv.div));
        json_object_set_new(root, "onsetColor",     json_integer(onsetColor));
        json_object_set_new(root, "restColor",      json_integer(restColor));
        json_object_set_new(root, "indicatorColor", json_integer(indicatorColor));
        json_object_set_new(root, "fillColor",      json_integer(fillColor));
        json_object_set_new(root, "muteColor",      json_integer(muteColor));
        json_t* jl = json_array();
        json_t* jf = json_array();
        json_t* jm = json_array();
        for (int i = 0; i < 8; i++) {
            json_array_append_new(jl, json_integer(len[i]));
            json_array_append_new(jf, json_integer(fill[i]));
            json_array_append_new(jm, json_boolean(muted[i]));
        }
        json_object_set_new(root, "len",   jl);
        json_object_set_new(root, "fill",  jf);
        json_object_set_new(root, "muted", jm);
        return root;
    }

    void dataFromJson(json_t* root) override {
        json_t* j;
        if ((j = json_object_get(root, "clockDiv")))
            clockDiv.set(clamp((int)json_integer_value(j), 1, 64));
        if ((j = json_object_get(root, "onsetColor")))
            onsetColor = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "restColor")))
            restColor = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "indicatorColor")))
            indicatorColor = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "fillColor")))
            fillColor = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "muteColor")))
            muteColor = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "len")))
            for (int i = 0; i < 8; i++) {
                json_t* v = json_array_get(j, i);
                if (v) len[i] = clamp((int)json_integer_value(v), 1, 8);
            }
        if ((j = json_object_get(root, "fill")))
            for (int i = 0; i < 8; i++) {
                json_t* v = json_array_get(j, i);
                if (v) fill[i] = clamp((int)json_integer_value(v), 0, 8);
            }
        if ((j = json_object_get(root, "muted")))
            for (int i = 0; i < 8; i++) {
                json_t* v = json_array_get(j, i);
                if (v) muted[i] = json_boolean_value(v);
            }
        for (int i = 0; i < 8; i++) {
            fill[i] = std::min(fill[i], len[i]);
            pos[i]  = 0;
        }
        ledsDirty = true;
    }
};

// ── Widget ────────────────────────────────────────────────────────────────────

struct Euclid64Widget : ModuleWidget {
    Euclid64Widget(Euclid64* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Euclid64.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addChild(createLightCentered<SmallLight<GreenRedLight>>(
            mm2px(Vec(6.0f, 18.0f)), module, Euclid64::ACTIVE_LIGHT));

        const float trigY[8] = {24.f, 34.f, 44.f, 54.f, 64.f, 74.f, 84.f, 94.f};
        for (int i = 0; i < 8; i++) {
            addOutput(createOutputCentered<PJ301MPort>(
                mm2px(Vec(20.0f, trigY[i])), module, Euclid64::TRIG_OUTPUT + i));
        }

        addOutput(createOutputCentered<PJ301MPort>(
            mm2px(Vec(20.0f, 108.0f)), module, Euclid64::POLY_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        Euclid64* m = getModule<Euclid64>();
        menu->addChild(new MenuSeparator);
        P64::appendClockDivMenu(menu, &m->clockDiv);
        P64::appendColorMenu(menu, m, "Onset color",          &m->onsetColor);
        P64::appendColorMenu(menu, m, "Rest color",           &m->restColor, true);
        P64::appendColorMenu(menu, m, "Step indicator color", &m->indicatorColor);
        P64::appendColorMenu(menu, m, "Fill marker color",    &m->fillColor);
        P64::appendColorMenu(menu, m, "Mute color",           &m->muteColor);
    }
};

Model* modelEuclid64 = createModel<Euclid64, Euclid64Widget>("Euclid64");
