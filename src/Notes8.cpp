#include "plugin.hpp"
#include "Scales.hpp"

// ── 8Notes ────────────────────────────────────────────────────────────────────
// Companion module: channel n of the poly gate input → scale degree n.
// The themed pitch source for the 8-voice page modules (Flin64, Step64,
// Cafe64, Euclid64, Bounce64): patch their poly output in, take poly pitch +
// pass-through gates out, and every voice is in key. Pure CV in → CV out.

struct Notes8 : Module {
    enum ParamIds  { NUM_PARAMS };
    enum InputIds  {
        GATE_INPUT,
        TRANSPOSE_INPUT,
        NUM_INPUTS
    };
    enum OutputIds {
        PITCH_OUTPUT,
        GATE_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds  { NUM_LIGHTS };

    int scaleIndex = 0;   // Major
    int rootNote   = 0;   // C
    int octave     = 3;   // base MIDI = 12*(octave+1) + rootNote → C3 = 48
    int chInterval = 1;   // scale degrees per channel

    float pitchV[8] = {};

    Notes8() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configInput(GATE_INPUT, "Poly gates (8ch)");
        configInput(TRANSPOSE_INPUT, "Transpose (1V/oct)");
        configOutput(PITCH_OUTPUT, "Pitch (1V/oct, poly 8ch)");
        configOutput(GATE_OUTPUT,  "Gates (poly 8ch, pass-through)");
        rebuildPitches();
    }

    void onReset() override {
        scaleIndex = 0;
        rootNote   = 0;
        octave     = 3;
        chInterval = 1;
        rebuildPitches();
    }

    void rebuildPitches() {
        const P64::Scale& sc = P64::SCALES[scaleIndex];
        int base = 12 * (octave + 1) + rootNote;
        for (int n = 0; n < 8; n++) {
            int midi = clamp(base + P64::degreeToSemitone(sc, n * chInterval), 0, 127);
            pitchV[n] = (midi - 60) / 12.f;
        }
    }

    void process(const ProcessArgs& args) override {
        float transpose = inputs[TRANSPOSE_INPUT].getVoltage();
        outputs[PITCH_OUTPUT].setChannels(8);
        outputs[GATE_OUTPUT].setChannels(8);
        for (int n = 0; n < 8; n++) {
            outputs[PITCH_OUTPUT].setVoltage(pitchV[n] + transpose, n);
            outputs[GATE_OUTPUT].setVoltage(inputs[GATE_INPUT].getVoltage(n), n);
        }
    }

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "scale",      json_integer(scaleIndex));
        json_object_set_new(root, "root",       json_integer(rootNote));
        json_object_set_new(root, "octave",     json_integer(octave));
        json_object_set_new(root, "chInterval", json_integer(chInterval));
        return root;
    }

    void dataFromJson(json_t* root) override {
        json_t* j;
        if ((j = json_object_get(root, "scale")))
            scaleIndex = clamp((int)json_integer_value(j), 0, P64::NUM_SCALES - 1);
        if ((j = json_object_get(root, "root")))
            rootNote = clamp((int)json_integer_value(j), 0, 11);
        if ((j = json_object_get(root, "octave")))
            octave = clamp((int)json_integer_value(j), 0, 7);
        if ((j = json_object_get(root, "chInterval")))
            chInterval = clamp((int)json_integer_value(j), 1, 4);
        rebuildPitches();
    }
};

// ── Widget ────────────────────────────────────────────────────────────────────

struct Notes8Widget : ModuleWidget {
    Notes8Widget(Notes8* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Notes8.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15.24f, 30.f)), module, Notes8::GATE_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15.24f, 50.f)), module, Notes8::TRANSPOSE_INPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(15.24f, 74.f)), module, Notes8::PITCH_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(15.24f, 96.f)), module, Notes8::GATE_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        Notes8* m = getModule<Notes8>();
        menu->addChild(new MenuSeparator);

        std::vector<std::string> scaleNames;
        for (int i = 0; i < P64::NUM_SCALES; i++)
            scaleNames.push_back(P64::SCALES[i].name);
        menu->addChild(createIndexSubmenuItem("Scale", scaleNames,
            [=]() { return m->scaleIndex; },
            [=](int v) { m->scaleIndex = v; m->rebuildPitches(); }));

        menu->addChild(createIndexSubmenuItem("Root note",
            {P64::NOTE_NAMES, P64::NOTE_NAMES + 12},
            [=]() { return m->rootNote; },
            [=](int v) { m->rootNote = v; m->rebuildPitches(); }));

        std::vector<std::string> octaveNames;
        for (int i = 0; i <= 7; i++)
            octaveNames.push_back(string::f("%d", i));
        menu->addChild(createIndexSubmenuItem("Base octave", octaveNames,
            [=]() { return m->octave; },
            [=](int v) { m->octave = v; m->rebuildPitches(); }));

        menu->addChild(createIndexSubmenuItem("Channel interval (scale degrees)",
            {"1", "2", "3", "4"},
            [=]() { return m->chInterval - 1; },
            [=](int v) { m->chInterval = v + 1; m->rebuildPitches(); }));
    }
};

Model* modelNotes8 = createModel<Notes8, Notes8Widget>("8Notes");
