// mlr_snapshot - Mlr64's button-6 temp snapshot restores samples from memory.
//
// The saved samples point at files that don't exist, so a restore that went
// back to disk (the old path-based dataFromJson) could not bring them back.
// The restore must restage the very same in-memory sample, and leave lanes
// whose sample didn't change alone.
#include "../src/Mlr64.cpp"

#include <cstdio>
#include <cstring>

static int failures = 0;

static void report(const char* check, bool pass) {
    printf("Mlr64,%s,%s\n", check, pass ? "PASS" : "FAIL");
    if (!pass) failures++;
}

static MlrSamplePtr fakeSample(const char* path) {
    auto s = std::make_shared<MlrSample>();
    s->left.assign(1000, 0.f);
    s->frames = 1000;
    s->path   = path;
    return s;
}

int main(int argc, char** argv) {
    bool header = !(argc > 1 && !std::strcmp(argv[1], "--no-header"));
    if (header) printf("module,check,pass\n");
    rack::random::init();

    Mlr64 m;
    MlrSamplePtr a = fakeSample("/nonexistent/pages64-test/a.wav");
    MlrSamplePtr b = fakeSample("/nonexistent/pages64-test/b.wav");
    MlrSamplePtr c = fakeSample("/nonexistent/pages64-test/c.wav");
    m.lanes[0].sample = a;
    m.lanes[1].sample = c;

    m.handleCommand(P64::CMD_SAVE);
    m.lanes[0].sample = b;            // lane 0 swapped after the save
    m.handleCommand(P64::CMD_RESTORE);

    report("snapshot_restages_saved_sample",
           m.pendingSet[0] && m.pendingSample[0] == a);
    report("snapshot_keeps_unchanged_lane", !m.pendingSet[1]);

    return failures ? 1 : 0;
}
