#include "plugin.hpp"
#include <mutex>

// ── 64Pads ────────────────────────────────────────────────────────────────────
// On-screen Launchpad: attach to the LEFT of Base64. Mirrors the LED state
// Base64 last sent to the device, and clicking a pad / round button feeds the
// same event path as hardware MIDI, so every page is playable without a
// Launchpad (multi-pad hold gestures excepted — one mouse, one press).
//
// Page select from the mouse is a latch: click top button 8 once (the ring
// lights), click a pad to pick the page, and the latch releases itself.

struct Pads64 : Module {
    enum ParamIds  { NUM_PARAMS };
    enum InputIds  { NUM_INPUTS };
    enum OutputIds { NUM_OUTPUTS };
    enum LightIds  { NUM_LIGHTS };

    // Mirror of the device LEDs (engine writes, UI reads; uint8 tearing is harmless)
    uint8_t grid[64] = {};
    uint8_t scene[8] = {};
    uint8_t top[8]   = {};
    bool    connected = false;

    // UI → engine click handoff
    std::mutex clickMutex;
    std::vector<P64::GridEvent> pending;
    bool selectLatched = false;

    Pads64() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        rightExpander.producerMessage = new P64::MirrorMessage;
        rightExpander.consumerMessage = new P64::MirrorMessage;
        memset(rightExpander.producerMessage, 0, sizeof(P64::MirrorMessage));
        memset(rightExpander.consumerMessage, 0, sizeof(P64::MirrorMessage));
    }

    ~Pads64() {
        delete (P64::MirrorMessage*) rightExpander.producerMessage;
        delete (P64::MirrorMessage*) rightExpander.consumerMessage;
    }

    // UI thread: a display press/release on a pad or round button.
    void uiPress(uint8_t type, uint8_t index, bool on) {
        std::lock_guard<std::mutex> lock(clickMutex);
        if (type == P64::GridEvent::CC && index == P64::CC_PAGE_SELECT) {
            // Page-select latch: a mouse cannot hold 8 and press a pad at once
            if (on) {
                selectLatched = !selectLatched;
                pending.push_back({type, index, (uint8_t)(selectLatched ? 127 : 0)});
            }
            return;
        }
        pending.push_back({type, index, (uint8_t)(on ? 127 : 0)});
        if (selectLatched && type == P64::GridEvent::PAD && !on) {
            // Page picked: release the latch behind the pad release
            selectLatched = false;
            pending.push_back({P64::GridEvent::CC, P64::CC_PAGE_SELECT, 0});
        }
    }

    void process(const ProcessArgs& args) override {
        Module* rm = rightExpander.module;
        bool base = rm && rm->model == modelBase;
        connected = base;

        auto* mm = (P64::MirrorMessage*) rightExpander.consumerMessage;
        if (base && mm) {
            memcpy(grid,  mm->grid,  sizeof(grid));
            memcpy(scene, mm->scene, sizeof(scene));
            memcpy(top,   mm->top,   sizeof(top));
        }
        rightExpander.messageFlipRequested = true;

        if (base) {
            auto* cm = (P64::ClickMessage*) rm->leftExpander.producerMessage;
            if (cm) {
                cm->count = 0;
                std::lock_guard<std::mutex> lock(clickMutex);
                for (const auto& ev : pending)
                    if (cm->count < P64::ClickMessage::MAX)
                        cm->events[cm->count++] = ev;
                pending.clear();
                rm->leftExpander.messageFlipRequested = true;
            }
        } else {
            std::lock_guard<std::mutex> lock(clickMutex);
            pending.clear();
            selectLatched = false;
        }
    }
};

// ── Display widget ────────────────────────────────────────────────────────────

struct PadsDisplay : OpaqueWidget {
    Pads64* module = nullptr;

    // Layout in mm (converted at draw/hit time)
    static constexpr float PITCH   = 6.4f;   // pad grid pitch
    static constexpr float PADSIZE = 5.4f;
    static constexpr float BTN_R   = 2.1f;   // round button radius
    static constexpr float GX      = 6.6f;   // first pad column center
    static constexpr float TOP_Y   = 33.0f;  // top round-button row center
    static constexpr float GY      = 39.8f;  // first pad row center
    static constexpr float SCN_X   = GX + 8 * PITCH;  // scene column center

    // Hit target: type/index per GridEvent encoding, or type = 255 for none
    struct Target { uint8_t type = 255, index = 0; };
    Target dragTarget;

    Target locate(Vec posPx) {
        math::Vec p = posPx.div(mm2px(1.f));
        Target t;
        for (int c = 0; c < 8; c++) {
            float cx = GX + c * PITCH;
            if (std::fabs(p.x - cx) <= BTN_R + 0.8f && std::fabs(p.y - TOP_Y) <= BTN_R + 0.8f) {
                t.type = P64::GridEvent::CC;
                t.index = (uint8_t)(104 + c);
                return t;
            }
        }
        for (int r = 0; r < 8; r++) {
            float cy = GY + r * PITCH;
            if (std::fabs(p.x - SCN_X) <= BTN_R + 0.8f && std::fabs(p.y - cy) <= BTN_R + 0.8f) {
                t.type = P64::GridEvent::SCENE;
                t.index = (uint8_t) r;
                return t;
            }
        }
        int col = (int) std::floor((p.x - GX + PITCH / 2) / PITCH);
        int row = (int) std::floor((p.y - GY + PITCH / 2) / PITCH);
        if (col >= 0 && col < 8 && row >= 0 && row < 8) {
            t.type = P64::GridEvent::PAD;
            t.index = (uint8_t)(row * 8 + col);
        }
        return t;
    }

    void onButton(const ButtonEvent& e) override {
        if (e.button != GLFW_MOUSE_BUTTON_LEFT)
            return;   // let right-click reach the context menu
        if (e.action == GLFW_PRESS && module) {
            Target t = locate(e.pos);
            if (t.type != 255) {
                dragTarget = t;
                module->uiPress(t.type, t.index, true);
                e.consume(this);
            }
        }
    }

    void onDragEnd(const DragEndEvent& e) override {
        if (module && dragTarget.type != 255)
            module->uiPress(dragTarget.type, dragTarget.index, false);
        dragTarget = Target{};
    }

    static NVGcolor ledColor(uint8_t vel) {
        int g = (vel >> 4) & 3;
        int r = vel & 3;
        if (g == 0 && r == 0)
            return nvgRGB(0x2a, 0x2a, 0x2a);   // off: unlit pad
        return nvgRGB((uint8_t)(60 + r * 65), (uint8_t)(60 + g * 65), 30);
    }

    void draw(const DrawArgs& args) override {
        NVGcontext* vg = args.vg;
        float alpha = (!module || module->connected) ? 1.f : 0.35f;
        nvgGlobalAlpha(vg, alpha);

        for (int c = 0; c < 8; c++) {
            uint8_t v = module ? module->top[c] : 0;
            nvgBeginPath(vg);
            nvgCircle(vg, mm2px(GX + c * PITCH), mm2px(TOP_Y), mm2px(BTN_R));
            nvgFillColor(vg, ledColor(v));
            nvgFill(vg);
        }
        // page-select latch ring on top button 8
        if (module && module->selectLatched) {
            nvgBeginPath(vg);
            nvgCircle(vg, mm2px(GX + 7 * PITCH), mm2px(TOP_Y), mm2px(BTN_R + 0.6f));
            nvgStrokeColor(vg, nvgRGB(0x22, 0xaf, 0xf2));
            nvgStrokeWidth(vg, mm2px(0.35f));
            nvgStroke(vg);
        }

        for (int r = 0; r < 8; r++) {
            uint8_t v = module ? module->scene[r] : 0;
            nvgBeginPath(vg);
            nvgCircle(vg, mm2px(SCN_X), mm2px(GY + r * PITCH), mm2px(BTN_R));
            nvgFillColor(vg, ledColor(v));
            nvgFill(vg);
        }

        for (int i = 0; i < 64; i++) {
            int row = i / 8, col = i % 8;
            uint8_t v = module ? module->grid[i] : 0;
            float x = mm2px(GX + col * PITCH - PADSIZE / 2);
            float y = mm2px(GY + row * PITCH - PADSIZE / 2);
            nvgBeginPath(vg);
            nvgRoundedRect(vg, x, y, mm2px(PADSIZE), mm2px(PADSIZE), mm2px(0.7f));
            nvgFillColor(vg, ledColor(v));
            nvgFill(vg);
        }

        nvgGlobalAlpha(vg, 1.f);
    }
};

// ── Widget ────────────────────────────────────────────────────────────────────

struct Pads64Widget : ModuleWidget {
    Pads64Widget(Pads64* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Pads64.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        PadsDisplay* display = new PadsDisplay;
        display->module = module;
        display->box.pos  = Vec(0, 0);
        display->box.size = box.size;
        addChild(display);
    }
};

Model* modelPads64 = createModel<Pads64, Pads64Widget>("64Pads");
