// kit_golden - the seed contract, as sound: each kit's factory kit (and the
// same kit with every Variety toggle on) renders the same 64 cells as the
// checked-in fixture. Compiled once per kit:
//   -DKIT_SRC='"../src/Drums64.cpp"' -DKIT_T=Drums64
//
// Each cell is triggered alone and rendered for 0.5 s at 48 kHz from a fixed
// noise seed; its fingerprint is five features (left/right RMS, RMS of the
// first difference, zero crossings, RMS of the second half). A feature off
// by more than 1e-3 relative (plus a small absolute floor) fails: enough to
// absorb compiler differences, far below any change to a recipe.
//
//   kit_golden_<Kit>            compare with fixtures/<Kit>.txt
//   kit_golden_<Kit> --write    rewrite the fixture (an intentional change)
#include KIT_SRC

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#define STR2(x) #x
#define STR(x) STR2(x)

void rackHostInit(float sampleRate);   // rack_host.cpp

static const float SR = 48000.f;
static const int   N  = 24000;
static const int   FEATURES = 5;

static std::vector<double> fingerprint(KIT_T& m, int cell) {
    m.kitReset();
    rack::random::local().seed(0x1234567, 0x89abcdef);
    m.cellTriggered(cell);
    double l2 = 0, r2 = 0, d2 = 0, tail2 = 0;
    int zc = 0;
    float prev = 0.f;
    for (int i = 0; i < N; i++) {
        float l = 0.f, r = 0.f;
        m.renderMix(l, r, 1.f / SR);
        float mono = 0.5f * (l + r);
        l2 += l * l;
        r2 += r * r;
        d2 += (mono - prev) * (mono - prev);
        if (i >= N / 2) tail2 += mono * mono;
        if ((mono >= 0.f) != (prev >= 0.f)) zc++;
        prev = mono;
    }
    return {std::sqrt(l2 / N), std::sqrt(r2 / N), std::sqrt(d2 / N),
            (double) zc, std::sqrt(tail2 / (N / 2))};
}

static std::string configName(int variety) { return variety ? "variety_all" : "factory"; }

int main(int argc, char** argv) {
    bool write = false, header = true;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--write")) write = true;
        if (!std::strcmp(argv[i], "--no-header")) header = false;
    }
    rack::random::init();
    rackHostInit(SR);

    std::ostringstream table;
    std::vector<std::vector<double>> rows;
    for (int pass = 0; pass < 2; pass++) {
        KIT_T m;
        m.onReset();                          // Initialize = the factory kit
        m.variety = pass ? m.varietyAll : 0;  // toggles never regenerate
        for (int c = 0; c < 64; c++) {
            std::vector<double> f = fingerprint(m, c);
            table << configName(pass) << " " << c;
            for (double v : f) table << " " << std::scientific << v;
            table << "\n";
            rows.push_back(f);
        }
    }

    // fixtures/ sits next to the binary, wherever it is run from
    std::string self = argv[0];
    size_t slash = self.rfind('/');
    std::string dir = slash == std::string::npos ? "." : self.substr(0, slash);
    std::string path = dir + "/fixtures/" + STR(KIT_T) + ".txt";
    if (write) {
        std::ofstream(path) << table.str();
        printf("wrote %s\n", path.c_str());
        return 0;
    }

    if (header) printf("module,check,pass\n");
    std::ifstream in(path);
    if (!in) {
        printf("%s,fixture_missing,FAIL\n", STR(KIT_T));
        return 1;
    }
    int bad = 0, line = 0;
    std::string cfg;
    int cell;
    while (in >> cfg >> cell) {
        std::vector<double> want(FEATURES);
        for (double& v : want) in >> v;
        const std::vector<double>& got = rows[line++];
        for (int k = 0; k < FEATURES; k++) {
            double tol = 1e-3 * std::fabs(want[k]) + 1e-6;
            if (std::fabs(got[k] - want[k]) > tol) {
                if (bad < 5)
                    fprintf(stderr, "%s %s cell %d feature %d: %g, fixture %g\n",
                            STR(KIT_T), cfg.c_str(), cell, k, got[k], want[k]);
                bad++;
                break;
            }
        }
    }
    bool pass = bad == 0 && line == (int) rows.size();
    printf("%s,seed_contract,%s\n", STR(KIT_T), pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
