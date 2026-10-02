// poly16 - 16Poly's merge and split rules (docs/design/Poly16.md).
#include "../src/Poly16.cpp"

#include <cstdio>
#include <cstring>
#include <vector>

static int failures = 0;

static void report(const char* check, bool pass) {
    printf("16Poly,%s,%s\n", check, pass ? "PASS" : "FAIL");
    if (!pass) failures++;
}

// Patch input `port` with `ch` channels carrying base, base+1, ...
// (0 = unpatched). Rack's engine sets channel counts when cables connect, so
// a test sets them directly; every output is patched (at least 1 channel).
static Poly16 fresh() {
    Poly16 m;
    for (auto& o : m.outputs) o.channels = 1;
    return m;
}

static void feed(Poly16& m, int port, int ch, float base) {
    m.inputs[port].channels = ch;
    for (int c = 0; c < ch; c++)
        m.inputs[port].setVoltage(base + c, c);
}

static std::vector<float> out(Poly16& m, int output) {
    std::vector<float> v;
    for (int c = 0; c < m.outputs[output].getChannels(); c++)
        v.push_back(m.outputs[output].getVoltage(c));
    return v;
}

static void run(Poly16& m) {
    Module::ProcessArgs args;
    args.sampleRate = 48000.f;
    args.sampleTime = 1.f / 48000.f;
    args.frame = 0;
    m.process(args);
}

typedef std::vector<float> V;

static void strip(int s) {
    char name[64];
#define CHECK(label, cond) \
    do { snprintf(name, sizeof name, "%s_%c", label, s ? 'b' : 'a'); report(name, cond); } while (0)
    {   // short first input is padded so 9-16 lands on channels 9-16
        Poly16 m = fresh();
        feed(m, Poly16::MERGE_LO_INPUT + s, 3, 1.f);
        feed(m, Poly16::MERGE_HI_INPUT + s, 2, 9.f);
        run(m);
        CHECK("merge_pads_first_half",
              out(m, Poly16::MERGE_OUTPUT + s) == (V{1, 2, 3, 0, 0, 0, 0, 0, 9, 10}));
    }
    {   // only 1-8 patched: passes up to 8 channels
        Poly16 m = fresh();
        feed(m, Poly16::MERGE_LO_INPUT + s, 12, 1.f);
        run(m);
        CHECK("merge_one_input_truncates",
              out(m, Poly16::MERGE_OUTPUT + s) == (V{1, 2, 3, 4, 5, 6, 7, 8}));
    }
    {   // only 9-16 patched: channels 1-8 are silent
        Poly16 m = fresh();
        feed(m, Poly16::MERGE_HI_INPUT + s, 8, 9.f);
        run(m);
        V v = out(m, Poly16::MERGE_OUTPUT + s);
        CHECK("merge_second_half_only",
              v.size() == 16 && v[0] == 0.f && v[7] == 0.f && v[8] == 9.f && v[15] == 16.f);
    }
    {   // nothing patched: silent mono (Rack keeps a patched output >= 1 channel)
        Poly16 m = fresh();
        run(m);
        CHECK("merge_unpatched_is_silent", out(m, Poly16::MERGE_OUTPUT + s) == (V{0}));
    }
    {   // 16 channels split in halves
        Poly16 m = fresh();
        feed(m, Poly16::SPLIT_INPUT + s, 16, 1.f);
        run(m);
        CHECK("split_halves",
              out(m, Poly16::SPLIT_LO_OUTPUT + s) == (V{1, 2, 3, 4, 5, 6, 7, 8})
              && out(m, Poly16::SPLIT_HI_OUTPUT + s) == (V{9, 10, 11, 12, 13, 14, 15, 16}));
    }
    {   // a 10-channel input: 8 + 2
        Poly16 m = fresh();
        feed(m, Poly16::SPLIT_INPUT + s, 10, 1.f);
        run(m);
        CHECK("split_follows_width",
              m.outputs[Poly16::SPLIT_LO_OUTPUT + s].getChannels() == 8
              && out(m, Poly16::SPLIT_HI_OUTPUT + s) == (V{9, 10}));
    }
    {   // mono in: mono on 1-8, silence on 9-16
        Poly16 m = fresh();
        feed(m, Poly16::SPLIT_INPUT + s, 1, 5.f);
        run(m);
        CHECK("split_mono",
              out(m, Poly16::SPLIT_LO_OUTPUT + s) == (V{5})
              && out(m, Poly16::SPLIT_HI_OUTPUT + s) == (V{0}));
    }
    {   // the strips are independent: driving this one leaves the other silent
        Poly16 m = fresh();
        feed(m, Poly16::SPLIT_INPUT + s, 16, 1.f);
        run(m);
        CHECK("strips_independent", out(m, Poly16::SPLIT_LO_OUTPUT + (1 - s)) == (V{0}));
    }
#undef CHECK
}

int main(int argc, char** argv) {
    bool header = !(argc > 1 && !std::strcmp(argv[1], "--no-header"));
    if (header) printf("module,check,pass\n");
    for (int s = 0; s < Poly16::STRIPS; s++)
        strip(s);
    return failures ? 1 : 0;
}
