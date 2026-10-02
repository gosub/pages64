#include "plugin.hpp"

// ── 16Poly ────────────────────────────────────────────────────────────────────
// Converts between 8- and 16-channel polyphony (docs/design/Poly16.md), in
// two identical strips side by side.
//
// Merge: inputs 1–8 and 9–16 → output 1–16. Each input contributes at most 8
// channels; with 9–16 patched, 1–8 is padded to 8 so 9–16 always lands on
// channels 9–16.
// Split: input 1–16 → outputs 1–8 and 9–16, each as wide as what's present.

struct Poly16 : Module {
    static constexpr int HALF   = 8;
    static constexpr int STRIPS = 2;   // two identical merge + split strips

    enum ParamIds  { NUM_PARAMS };
    enum InputIds  {
        ENUMS(MERGE_LO_INPUT, STRIPS),
        ENUMS(MERGE_HI_INPUT, STRIPS),
        ENUMS(SPLIT_INPUT, STRIPS),
        NUM_INPUTS
    };
    enum OutputIds {
        ENUMS(MERGE_OUTPUT, STRIPS),
        ENUMS(SPLIT_LO_OUTPUT, STRIPS),
        ENUMS(SPLIT_HI_OUTPUT, STRIPS),
        NUM_OUTPUTS
    };
    enum LightIds  { NUM_LIGHTS };

    Poly16() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (int s = 0; s < STRIPS; s++) {
            std::string n = s ? "B" : "A";
            configInput(MERGE_LO_INPUT + s, "Merge " + n + ": channels 1–8");
            configInput(MERGE_HI_INPUT + s, "Merge " + n + ": channels 9–16");
            configOutput(MERGE_OUTPUT + s, "Merge " + n + ": 16 channels");
            configInput(SPLIT_INPUT + s, "Split " + n + ": 16 channels");
            configOutput(SPLIT_LO_OUTPUT + s, "Split " + n + ": channels 1–8");
            configOutput(SPLIT_HI_OUTPUT + s, "Split " + n + ": channels 9–16");
        }
    }

    // Each input contributes at most 8 channels. With 9-16 patched, 1-8 is
    // padded to 8 so 9-16 lands on channels 9-16; without it, 1-8 passes.
    void merge(Input& lo, Input& hi, Output& out) {
        int loCh = std::min(lo.getChannels(), HALF);
        int hiCh = std::min(hi.getChannels(), HALF);
        if (hi.isConnected()) {
            out.setChannels(HALF + hiCh);
            for (int c = 0; c < HALF; c++)
                out.setVoltage(c < loCh ? lo.getVoltage(c) : 0.f, c);
            for (int c = 0; c < hiCh; c++)
                out.setVoltage(hi.getVoltage(c), HALF + c);
        } else {
            out.setChannels(loCh);   // Rack keeps a patched output at >= 1
            for (int c = 0; c < loCh; c++)
                out.setVoltage(lo.getVoltage(c), c);
        }
    }

    // Channels 1-8 and 9-16, each output as wide as what's present.
    void split(Input& in, Output& lo, Output& hi) {
        int ch     = in.getChannels();
        int lowCh  = std::min(ch, HALF);
        int highCh = std::max(ch - HALF, 0);
        lo.setChannels(lowCh);
        for (int c = 0; c < lowCh; c++)
            lo.setVoltage(in.getVoltage(c), c);
        hi.setChannels(highCh);
        for (int c = 0; c < highCh; c++)
            hi.setVoltage(in.getVoltage(HALF + c), c);
    }

    void process(const ProcessArgs& args) override {
        for (int s = 0; s < STRIPS; s++) {
            merge(inputs[MERGE_LO_INPUT + s], inputs[MERGE_HI_INPUT + s], outputs[MERGE_OUTPUT + s]);
            split(inputs[SPLIT_INPUT + s], outputs[SPLIT_LO_OUTPUT + s], outputs[SPLIT_HI_OUTPUT + s]);
        }
    }
};

// ── Widget ────────────────────────────────────────────────────────────────────

struct Poly16Widget : ModuleWidget {
    Poly16Widget(Poly16* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Poly16.svg")));
        P64::addScrews(this);

        // Two strips side by side: merge above the separator, split below.
        // Positions match tools/gen_panel.py.
        for (int s = 0; s < Poly16::STRIPS; s++) {
            float x = s ? P64::COL_R_MM : P64::COL_L_MM;
            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(x, 27.f)), module, Poly16::MERGE_LO_INPUT + s));
            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(x, 41.f)), module, Poly16::MERGE_HI_INPUT + s));
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(x, 55.f)), module, Poly16::MERGE_OUTPUT + s));
            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(x, 72.f)), module, Poly16::SPLIT_INPUT + s));
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(x, 86.f)), module, Poly16::SPLIT_LO_OUTPUT + s));
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(x, 100.f)), module, Poly16::SPLIT_HI_OUTPUT + s));
        }
    }
};

Model* modelPoly16 = createModel<Poly16, Poly16Widget>("16Poly");
