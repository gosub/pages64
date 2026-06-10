#include "PageModule.hpp"

#define DR_WAV_IMPLEMENTATION
#define DRWAV_MALLOC(sz) malloc(sz)
#include "dr_wav.h"

#include <atomic>
#include <mutex>
#include <regex>
#include <osdialog.h>

// ── Mlr64 ─────────────────────────────────────────────────────────────────────
// Performance sample cutter after mlr (tehn). 8 grid rows = 8 sample lanes,
// 8 slices per lane; press = quantized jump. Playback is varispeed-synced to
// the Base64 clock: each lane declares its musical length in beats and the
// playback rate follows the measured tick period, so tempo mismatch becomes
// pitch shift. Full design: Mlr64.md.
//
// Stage 1: lanes, loading, varispeed loop playback, quantized jumps, LEDs.

static constexpr int    MLR_LANES       = 8;
static constexpr int    MLR_SLICES      = 8;
static constexpr size_t MLR_MAX_FRAMES  = 1 << 24;   // ~5.8 min at 48 kHz
static constexpr float  MLR_FADE_SEC    = 0.002f;    // jump crossfade

static const int MLR_BEAT_CHOICES[]  = {1, 2, 4, 8, 16, 32, 48, 64};
static const int MLR_TICKS_CHOICES[] = {1, 2, 4, 24};

struct MlrSample {
    std::vector<float> left, right;   // right empty = mono
    size_t      frames     = 0;
    float       nativeRate = 44100.f;
    std::string path;

    bool stereo() const { return !right.empty(); }
};
typedef std::shared_ptr<MlrSample> MlrSamplePtr;

static MlrSamplePtr mlrLoadWav(const std::string& path) {
    drwav wav;
    if (!drwav_init_file(&wav, path.c_str(), nullptr))
        return nullptr;
    unsigned channels = wav.channels;
    size_t   frames   = (size_t) wav.totalPCMFrameCount;
    if (frames < 2 || channels < 1) {
        drwav_uninit(&wav);
        return nullptr;
    }
    if (frames > MLR_MAX_FRAMES)
        frames = MLR_MAX_FRAMES;

    std::vector<float> inter(frames * channels);
    size_t read = (size_t) drwav_read_pcm_frames_f32(&wav, frames, inter.data());
    drwav_uninit(&wav);
    if (read < 2)
        return nullptr;

    auto s = std::make_shared<MlrSample>();
    s->frames     = read;
    s->nativeRate = (float) wav.sampleRate;
    s->path       = path;
    s->left.resize(read);
    if (channels >= 2) {
        s->right.resize(read);
        for (size_t i = 0; i < read; i++) {
            s->left[i]  = inter[i * channels];
            s->right[i] = inter[i * channels + 1];
        }
    } else {
        memcpy(s->left.data(), inter.data(), read * sizeof(float));
    }
    return s;
}

// Linear-interpolated, clamped buffer read.
static inline float mlrRead(const std::vector<float>& b, double p) {
    if (p < 0.0) p = 0.0;
    size_t i = (size_t) p;
    if (i >= b.size() - 1)
        return b.back();
    float f = (float)(p - (double)i);
    return b[i] + (b[i + 1] - b[i]) * f;
}

struct Mlr64 : PageModule {
    enum ParamIds { NUM_PARAMS };
    enum InputIds  { NUM_INPUTS };
    enum OutputIds {
        MIXL_OUTPUT,
        MIXR_OUTPUT,
        POLY_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        ENUMS(ACTIVE_LIGHT, 2),
        NUM_LIGHTS
    };

    struct Lane {
        MlrSamplePtr sample;          // engine-side current sample
        double pos     = 0.0;         // playhead, source frames
        int    beats   = 4;
        int    group   = 0;           // 0-3 (scene stops, stage 2)
        bool   oneShot = false;       // stage 2
        int    loopStart = 0;         // sub-loop slices, inclusive (stage 2 gesture)
        int    loopEnd   = MLR_SLICES - 1;
        int    pendingSlice = -1;     // quantized jump target
        // jump declick: old position fading out
        double fadePos = 0.0;
        float  fade    = 0.f;
        int    shownSlice = -1;       // last slice drawn on the LEDs
    };
    Lane lanes[MLR_LANES];

    // UI → engine sample handoff
    std::mutex        sampleMutex;
    MlrSamplePtr      pendingSample[MLR_LANES];
    bool              pendingSet[MLR_LANES] = {};
    std::atomic<bool> samplesDirty{false};

    // Tempo measurement
    int    ticksPerBeatIndex = 0;     // index into MLR_TICKS_CHOICES, default 1 tick/beat
    int    quantize   = 1;            // 0=off, 1=1 tick, 2=2 ticks, 3=1 beat
    float  secPerTick = 0.5f;         // smoothed; 120 BPM at 1 tick/beat
    float  tickTimer  = 0.f;          // seconds since last tick
    bool   anyTick    = false;
    float  tickHist[3] = {0.5f, 0.5f, 0.5f};
    int    tickHistIdx = 0;
    int64_t tickCount  = 0;

    // Display colors
    uint8_t loopColor     = P64::LED_GREEN_DIM;
    uint8_t playheadColor = P64::LED_GREEN;

    Mlr64() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configOutput(MIXL_OUTPUT, "Mix left");
        configOutput(MIXR_OUTPUT, "Mix right");
        configOutput(POLY_OUTPUT, "Lanes (poly 8ch)");
    }

    void onReset() override {
        PageModule::onReset();
        std::lock_guard<std::mutex> lock(sampleMutex);
        for (int i = 0; i < MLR_LANES; i++) {
            lanes[i] = Lane{};
            pendingSample[i].reset();
            pendingSet[i] = true;     // engine drops its sample too
        }
        samplesDirty = true;
        ticksPerBeatIndex = 0;
        quantize    = 1;
        secPerTick  = 0.5f;
        tickTimer   = 0.f;
        anyTick     = false;
        tickHist[0] = tickHist[1] = tickHist[2] = 0.5f;
        tickHistIdx = 0;
        tickCount   = 0;
        loopColor     = P64::LED_GREEN_DIM;
        playheadColor = P64::LED_GREEN;
    }

    // ── helpers ───────────────────────────────────────────────────────────────

    int ticksPerBeat() const { return MLR_TICKS_CHOICES[ticksPerBeatIndex]; }

    float secPerBeat() const { return secPerTick * ticksPerBeat(); }

    bool tempoKnown() const { return anyTick; }

    // UI thread: stage a freshly loaded (or null) sample for the engine.
    void stageSample(int lane, MlrSamplePtr s) {
        std::lock_guard<std::mutex> lock(sampleMutex);
        pendingSample[lane] = s;
        pendingSet[lane]    = true;
        samplesDirty        = true;
    }

    // Guess a musical length for a freshly loaded sample.
    int guessBeats(const MlrSamplePtr& s) {
        float dur = (float) s->frames / s->nativeRate;
        float spb = 0.5f;   // 120 BPM fallback
        std::smatch m;
        static const std::regex bpmRe("([0-9]+)\\s*bpm", std::regex::icase);
        std::string name = system::getFilename(s->path);
        if (std::regex_search(name, m, bpmRe))
            spb = 60.f / (float) std::stoi(m[1]);
        else if (tempoKnown())
            spb = secPerBeat();
        float beats = dur / spb;
        int best = MLR_BEAT_CHOICES[0];
        for (int b : MLR_BEAT_CHOICES)
            if (std::abs(beats - b) < std::abs(beats - best))
                best = b;
        return best;
    }

    double sliceFrame(const Lane& L, int slice) const {
        return (double) slice / MLR_SLICES * (double) L.sample->frames;
    }

    // Begin a crossfaded jump of lane l to the start of `slice`.
    void doJump(int l, int slice) {
        Lane& L = lanes[l];
        if (!L.sample) return;
        L.fadePos = L.pos;
        L.fade    = 1.f;
        L.pos     = sliceFrame(L, slice);
    }

    void requestJump(int l, int slice) {
        if (quantize == 0)
            doJump(l, slice);
        else
            lanes[l].pendingSlice = slice;
    }

    bool quantizeBoundary() const {
        switch (quantize) {
            case 2:  return tickCount % 2 == 0;
            case 3:  return tickCount % ticksPerBeat() == 0;
            default: return true;   // quantize == 1 (and 0 never reaches here)
        }
    }

    int playheadSlice(const Lane& L) const {
        if (!L.sample) return -1;
        int s = (int)(L.pos / (double) L.sample->frames * MLR_SLICES);
        return clamp(s, 0, MLR_SLICES - 1);
    }

    // ── virtual hooks ─────────────────────────────────────────────────────────

    void pagePreProcess() override {
        auto* msg = reinterpret_cast<P64::LeftMessage*>(leftExpander.consumerMessage);
        if (msg) {
            if (msg->resetTick) {
                for (int l = 0; l < MLR_LANES; l++) {
                    Lane& L = lanes[l];
                    if (!L.sample) continue;
                    L.fadePos = L.pos;
                    L.fade    = 1.f;
                    L.pos     = sliceFrame(L, L.loopStart);
                    L.pendingSlice = -1;
                }
                tickCount = 0;
                ledsDirty = true;
            }

            if (msg->clockTick) {
                // Tempo measurement: median of last 3 periods, then smoothing
                if (anyTick && tickTimer > 0.02f && tickTimer < 4.f) {
                    tickHist[tickHistIdx] = tickTimer;
                    tickHistIdx = (tickHistIdx + 1) % 3;
                    float a = tickHist[0], b = tickHist[1], c = tickHist[2];
                    float med = std::max(std::min(a, b), std::min(std::max(a, b), c));
                    secPerTick += 0.5f * (med - secPerTick);
                }
                anyTick   = true;
                tickTimer = 0.f;
                tickCount++;

                // Execute pending quantized jumps on the boundary
                if (quantize != 0 && quantizeBoundary()) {
                    for (int l = 0; l < MLR_LANES; l++) {
                        if (lanes[l].pendingSlice >= 0) {
                            doJump(l, lanes[l].pendingSlice);
                            lanes[l].pendingSlice = -1;
                        }
                    }
                }
            }
        }
        tickTimer += sampleTime;

        // LED playhead tracking (cheap; rebuildLeds only runs when active)
        for (int l = 0; l < MLR_LANES; l++) {
            int s = playheadSlice(lanes[l]);
            if (s != lanes[l].shownSlice) {
                lanes[l].shownSlice = s;
                ledsDirty = true;
            }
        }
    }

    void pageActive(const P64::LeftMessage& msg) override {
        for (int row = 0; row < MLR_LANES; row++) {
            for (int col = 0; col < MLR_SLICES; col++) {
                int note = row * 16 + col;
                if (!msg.noteEvent[note] || msg.noteVelocity[note] == 0) continue;
                if (!lanes[row].sample) continue;
                requestJump(row, col);
            }
        }
    }

    void rebuildLeds() override {
        memset(ledState, P64::LED_OFF, sizeof(ledState));
        for (int l = 0; l < MLR_LANES; l++) {
            const Lane& L = lanes[l];
            if (!L.sample) continue;
            for (int col = L.loopStart; col <= L.loopEnd; col++)
                ledState[l * 8 + col] = loopColor;
            int s = L.shownSlice;
            if (s >= 0)
                ledState[l * 8 + s] = playheadColor;
        }
    }

    void updateOutputs() override {
        // Pick up staged samples (uncontended in the common case)
        if (samplesDirty.load(std::memory_order_acquire)) {
            std::lock_guard<std::mutex> lock(sampleMutex);
            for (int l = 0; l < MLR_LANES; l++) {
                if (!pendingSet[l]) continue;
                lanes[l].sample = pendingSample[l];
                pendingSample[l].reset();
                pendingSet[l] = false;
                lanes[l].pos       = 0.0;
                lanes[l].fade      = 0.f;
                lanes[l].loopStart = 0;
                lanes[l].loopEnd   = MLR_SLICES - 1;
                lanes[l].pendingSlice = -1;
            }
            samplesDirty.store(false, std::memory_order_release);
            ledsDirty = true;
        }

        float mixL = 0.f, mixR = 0.f;
        float fadeStep = sampleTime / MLR_FADE_SEC;
        outputs[POLY_OUTPUT].setChannels(MLR_LANES);

        for (int l = 0; l < MLR_LANES; l++) {
            Lane& L = lanes[l];
            if (!L.sample || L.sample->frames < 2) {
                outputs[POLY_OUTPUT].setVoltage(0.f, l);
                continue;
            }
            const MlrSample& S = *L.sample;

            // Varispeed increment: musical length is beats × secPerBeat
            double inc = (double) S.frames / ((double) L.beats * secPerBeat())
                         * sampleTime;

            float vl = mlrRead(S.left, L.pos);
            float vr = S.stereo() ? mlrRead(S.right, L.pos) : vl;

            if (L.fade > 0.f) {
                float ol = mlrRead(S.left, L.fadePos);
                float orr = S.stereo() ? mlrRead(S.right, L.fadePos) : ol;
                vl = ol * L.fade + vl * (1.f - L.fade);
                vr = orr * L.fade + vr * (1.f - L.fade);
                L.fadePos += inc;
                L.fade -= fadeStep;
            }

            // Advance and wrap inside the (sub-)loop region
            L.pos += inc;
            double regionStart = sliceFrame(L, L.loopStart);
            double regionEnd   = (double)(L.loopEnd + 1) / MLR_SLICES
                                 * (double) S.frames;
            if (L.pos >= regionEnd) {
                L.fadePos = L.pos - inc;
                L.fade    = 1.f;
                L.pos     = regionStart + (L.pos - regionEnd);
            }

            mixL += vl;
            mixR += vr;
            outputs[POLY_OUTPUT].setVoltage((vl + vr) * 0.5f * 5.f, l);
        }

        outputs[MIXL_OUTPUT].setVoltage(mixL * 5.f);
        outputs[MIXR_OUTPUT].setVoltage(mixR * 5.f);
    }

    // ── serialisation ─────────────────────────────────────────────────────────

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "ticksPerBeatIndex", json_integer(ticksPerBeatIndex));
        json_object_set_new(root, "quantize",          json_integer(quantize));
        json_object_set_new(root, "loopColor",         json_integer(loopColor));
        json_object_set_new(root, "playheadColor",     json_integer(playheadColor));
        json_t* jl = json_array();
        for (int i = 0; i < MLR_LANES; i++) {
            const Lane& L = lanes[i];
            json_t* o = json_object();
            json_object_set_new(o, "path",
                json_string(L.sample ? L.sample->path.c_str() : ""));
            json_object_set_new(o, "beats",     json_integer(L.beats));
            json_object_set_new(o, "group",     json_integer(L.group));
            json_object_set_new(o, "oneShot",   json_boolean(L.oneShot));
            json_object_set_new(o, "loopStart", json_integer(L.loopStart));
            json_object_set_new(o, "loopEnd",   json_integer(L.loopEnd));
            json_array_append_new(jl, o);
        }
        json_object_set_new(root, "lanes", jl);
        return root;
    }

    void dataFromJson(json_t* root) override {
        json_t* j;
        if ((j = json_object_get(root, "ticksPerBeatIndex")))
            ticksPerBeatIndex = clamp((int)json_integer_value(j), 0, 3);
        if ((j = json_object_get(root, "quantize")))
            quantize = clamp((int)json_integer_value(j), 0, 3);
        if ((j = json_object_get(root, "loopColor")))
            loopColor = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "playheadColor")))
            playheadColor = (uint8_t)json_integer_value(j);
        if ((j = json_object_get(root, "lanes"))) {
            for (int i = 0; i < MLR_LANES; i++) {
                json_t* o = json_array_get(j, i);
                if (!o) continue;
                json_t* v;
                if ((v = json_object_get(o, "beats")))
                    lanes[i].beats = clamp((int)json_integer_value(v), 1, 64);
                if ((v = json_object_get(o, "group")))
                    lanes[i].group = clamp((int)json_integer_value(v), 0, 3);
                if ((v = json_object_get(o, "oneShot")))
                    lanes[i].oneShot = json_boolean_value(v);
                int ls = 0, le = MLR_SLICES - 1;
                if ((v = json_object_get(o, "loopStart")))
                    ls = clamp((int)json_integer_value(v), 0, MLR_SLICES - 1);
                if ((v = json_object_get(o, "loopEnd")))
                    le = clamp((int)json_integer_value(v), 0, MLR_SLICES - 1);
                lanes[i].loopStart = std::min(ls, le);
                lanes[i].loopEnd   = std::max(ls, le);
                if ((v = json_object_get(o, "path"))) {
                    std::string path = json_string_value(v) ? json_string_value(v) : "";
                    bool same = lanes[i].sample && lanes[i].sample->path == path;
                    if (!path.empty() && !same) {
                        MlrSamplePtr s = mlrLoadWav(path);
                        if (s) stageSample(i, s);
                    } else if (path.empty()) {
                        stageSample(i, nullptr);
                    }
                }
            }
        }
        ledsDirty = true;
    }
};

// ── Widget ────────────────────────────────────────────────────────────────────

struct Mlr64Widget : ModuleWidget {
    Mlr64Widget(Mlr64* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Mlr64.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addChild(createLightCentered<SmallLight<GreenRedLight>>(
            mm2px(Vec(6.0f, 18.0f)), module, Mlr64::ACTIVE_LIGHT));

        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.f, 84.f)),  module, Mlr64::MIXL_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.f, 96.f)),  module, Mlr64::MIXR_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.f, 108.f)), module, Mlr64::POLY_OUTPUT));
    }

    void loadSampleDialog(Mlr64* m, int lane) {
        std::string dir = m->lanes[lane].sample
            ? system::getDirectory(m->lanes[lane].sample->path)
            : asset::user("");
        char* path = osdialog_file(OSDIALOG_OPEN, dir.c_str(), NULL, NULL);
        if (!path) return;
        MlrSamplePtr s = mlrLoadWav(path);
        if (s) {
            m->lanes[lane].beats = m->guessBeats(s);
            m->stageSample(lane, s);
        }
        free(path);
    }

    void appendContextMenu(Menu* menu) override {
        Mlr64* m = getModule<Mlr64>();
        menu->addChild(new MenuSeparator);

        for (int l = 0; l < MLR_LANES; l++) {
            std::string name = m->lanes[l].sample
                ? system::getFilename(m->lanes[l].sample->path)
                : "(empty)";
            menu->addChild(createSubmenuItem(
                    string::f("Lane %d", l + 1), name, [=](Menu* sub) {
                sub->addChild(createMenuItem("Load sample…", "",
                    [=]() { loadSampleDialog(m, l); }));
                sub->addChild(createMenuItem("Clear", "",
                    [=]() { m->stageSample(l, nullptr); }));
                sub->addChild(createSubmenuItem("Beats", string::f("%d", m->lanes[l].beats),
                    [=](Menu* bs) {
                        for (int b : MLR_BEAT_CHOICES) {
                            bs->addChild(createCheckMenuItem(string::f("%d", b), "",
                                [=]() { return m->lanes[l].beats == b; },
                                [=]() { m->lanes[l].beats = b; }));
                        }
                    }));
            }));
        }

        menu->addChild(new MenuSeparator);
        menu->addChild(createIndexSubmenuItem("Ticks per beat",
            {"1 (clock = quarter notes)", "2 (8ths)", "4 (16ths)", "24 (MIDI)"},
            [=]() { return m->ticksPerBeatIndex; },
            [=](int v) { m->ticksPerBeatIndex = v; }));
        menu->addChild(createIndexSubmenuItem("Quantize jumps",
            {"Off", "1 tick", "2 ticks", "1 beat"},
            [=]() { return m->quantize; },
            [=](int v) { m->quantize = v; }));
        P64::appendColorMenu(menu, m, "Loop color",     &m->loopColor, true);
        P64::appendColorMenu(menu, m, "Playhead color", &m->playheadColor);
    }
};

Model* modelMlr64 = createModel<Mlr64, Mlr64Widget>("Mlr64");
