#pragma once
#include <rack.hpp>
#include <app/MidiDisplay.hpp>
#include <atomic>

using namespace rack;

extern Plugin* pluginInstance;

extern Model* modelBase;
extern Model* modelButtons64;
extern Model* modelGrid64;
extern Model* modelSliders64;
extern Model* modelFlin64;

// ── Launchpad Mini MkII MIDI mapping ────────────────────────────────────────
//
// Grid pads (8×8):  note = row * 16 + col   (row 0 = top, col 0 = left)
//   top-left = note 0,  bottom-left = note 112,  bottom-right = note 119
// Right-column scene buttons: note = row * 16 + 8  (row 0 = bottom)
// Top round buttons (left→right): CC 104 … CC 111
//
// LED color is set by Note-On velocity on the grid notes:
//   velocity = (green << 4) | red | 12    (green and red each 0–3; 12 = Copy+Clear flags)
//   off=12, green=60, red=15, yellow=62, amber=63

namespace P64 {

inline int gridNoteToIndex(int note) {
    int row = note / 16;
    int col = note % 16;
    if (col > 7 || row > 7) return -1;
    return row * 8 + col;   // 0 = top-left (row 0 = physical top)
}

inline int gridIndexToNote(int idx) {
    int row = idx / 8;
    int col = idx % 8;
    return row * 16 + col;
}

// All 16 LED colors: velocity = (green << 4) | red | 12  (green, red each 0–3)
static constexpr uint8_t LED_OFF        = 12;  // g=0 r=0
static constexpr uint8_t LED_RED_DIM    = 13;  // g=0 r=1
static constexpr uint8_t LED_RED_MED    = 14;  // g=0 r=2
static constexpr uint8_t LED_RED        = 15;  // g=0 r=3
static constexpr uint8_t LED_GREEN_DIM  = 28;  // g=1 r=0
static constexpr uint8_t LED_AMBER_DIM  = 29;  // g=1 r=1
static constexpr uint8_t LED_SIENNA     = 30;  // g=1 r=2
static constexpr uint8_t LED_RUST       = 31;  // g=1 r=3
static constexpr uint8_t LED_GREEN_MED  = 44;  // g=2 r=0
static constexpr uint8_t LED_OLIVE      = 45;  // g=2 r=1
static constexpr uint8_t LED_AMBER_MED  = 46;  // g=2 r=2
static constexpr uint8_t LED_ORANGE     = 47;  // g=2 r=3
static constexpr uint8_t LED_GREEN      = 60;  // g=3 r=0
static constexpr uint8_t LED_LIME       = 61;  // g=3 r=1
static constexpr uint8_t LED_YELLOW     = 62;  // g=3 r=2
static constexpr uint8_t LED_AMBER      = 63;  // g=3 r=3

struct LedColorDef { uint8_t velocity; const char* name; };
static const LedColorDef LED_COLOR_DEFS[16] = {
    {LED_OFF,       "Off"},
    {LED_RED_DIM,   "Red Dim"},
    {LED_RED_MED,   "Red Mid"},
    {LED_RED,       "Red"},
    {LED_GREEN_DIM, "Green Dim"},
    {LED_AMBER_DIM, "Amber Dim"},
    {LED_SIENNA,    "Sienna"},
    {LED_RUST,      "Rust"},
    {LED_GREEN_MED, "Green Mid"},
    {LED_OLIVE,     "Olive"},
    {LED_AMBER_MED, "Amber Mid"},
    {LED_ORANGE,    "Orange"},
    {LED_GREEN,     "Green"},
    {LED_LIME,      "Lime"},
    {LED_YELLOW,    "Yellow"},
    {LED_AMBER,     "Amber"},
};

// CC number of the page-select button (leftmost top round button)
static constexpr int CC_PAGE_SELECT     = 111;

// ── Clock divider (standard for clock-driven page modules) ──────────────────

static constexpr int CLOCK_DIVS[12] = {1,2,3,4,6,8,12,16,24,32,48,64};

struct ClockDivider {
    int div   = 1;
    int count = 0;

    // Feed a tick; returns true on every div-th one.
    bool process(bool tick) {
        if (!tick) return false;
        if (++count >= div) {
            count = 0;
            return true;
        }
        return false;
    }
    void set(int d) { div = d; count = 0; }
    void reset()    { count = 0; }
};

// ── Global key (root + scale) ────────────────────────────────────────────────
// Set from Base64's context menu; followed by the pitched modules (Keys64,
// 64Notes, 8Notes) whose "Follow Base64 global key" switch is on. This is a
// plugin-wide value rather than an expander field because the companions sit
// outside the chain, connected only by cables.

struct SharedKey {
    std::atomic<int>      root{0};    // 0–11, C…B
    std::atomic<int>      scale{0};   // index into SCALES
    std::atomic<uint32_t> serial{1};  // bumped on every change; followers poll it

    void set(int r, int s) { root = r; scale = s; serial++; }
};
extern SharedKey sharedKey;

// Poll helper for followers. Returns true when the global key changed and was
// copied into root/scale — the caller rebuilds its note map then.
inline bool followSharedKey(bool follow, uint32_t& lastSerial, int& root, int& scale) {
    if (!follow) return false;
    uint32_t s = sharedKey.serial.load(std::memory_order_relaxed);
    if (s == lastSerial) return false;
    lastSerial = s;
    int r  = sharedKey.root.load(std::memory_order_relaxed);
    int sc = sharedKey.scale.load(std::memory_order_relaxed);
    if (r == root && sc == scale) return false;
    root  = r;
    scale = sc;
    return true;
}

// ── Expander messages ────────────────────────────────────────────────────────

// One Launchpad event, decoded to grid coordinates by Base64.
struct GridEvent {
    enum Type : uint8_t {
        PAD,    // index = grid index 0–63 (0 = top-left), value = velocity (0 = release)
        CC,     // index = controller number (104–110 = top round buttons), value = CC value
        SCENE,  // index = scene row 0–7 (0 = top = A), value = velocity (0 = release)
    };
    uint8_t type;
    uint8_t index;
    uint8_t value;
};

// Global one-frame commands broadcast to every page in the chain
enum ChainCommand : uint8_t {
    CMD_NONE = 0,
    CMD_SAVE,      // snapshot your state (button 6 held)
    CMD_RESTORE,   // reload the snapshot (button 6 tapped)
};

// Sent left→right: Base to page modules (and forwarded along the chain)
struct LeftMessage {
    static constexpr int MAX_EVENTS = 32;

    int  activePage;        // currently active page index
    int  pageCounter;       // each page reads this as its own index; pass (pageCounter+1) rightward
    bool repaintRequested;  // Base sets this for one frame when exiting page-select mode
    uint8_t command;        // ChainCommand, delivered to every page (active or not)
    float clockVoltage;     // raw voltage of Base64 CLOCK input jack
    float resetVoltage;     // raw voltage of Base64 RESET input jack
    bool  clockTick;        // rising edge on CLOCK this frame (computed by Base64)
    bool  resetTick;        // rising edge on RESET this frame (computed by Base64)
    int   eventCount;       // events this frame, in arrival order
    GridEvent events[MAX_EVENTS];

    // Physically unreachable within one audio frame; overflow drops the event.
    void pushEvent(uint8_t type, uint8_t index, uint8_t value) {
        if (eventCount < MAX_EVENTS)
            events[eventCount++] = GridEvent{type, index, value};
    }
};

// True if scene button `row` was pressed (velocity > 0) this frame.
inline bool sceneOn(const LeftMessage& msg, int row) {
    for (int e = 0; e < msg.eventCount; e++)
        if (msg.events[e].type == GridEvent::SCENE
                && msg.events[e].index == row && msg.events[e].value > 0)
            return true;
    return false;
}

// True if CC `cc` arrived with value > 0 this frame.
inline bool ccOn(const LeftMessage& msg, int cc) {
    for (int e = 0; e < msg.eventCount; e++)
        if (msg.events[e].type == GridEvent::CC
                && msg.events[e].index == cc && msg.events[e].value > 0)
            return true;
    return false;
}

// Sent right→left: Base64 to a 64Pads mirror attached on its LEFT side —
// the LED state as last sent to the device (mirror allocates the buffers).
struct MirrorMessage {
    uint8_t grid[64];   // 0 = top-left
    uint8_t scene[8];   // 0 = top = A
    uint8_t top[8];     // 0 = left = CC104
};

// Sent left→right: 64Pads clicks into Base64 (Base64 allocates the buffers).
// Events use the GridEvent encoding and enter the same path as hardware MIDI.
struct ClickMessage {
    static constexpr int MAX = 16;
    int count;
    GridEvent events[MAX];
};

// Sent right→left: page module to Base (aggregated / forwarded along the chain)
struct RightMessage {
    uint8_t gridLeds[64];   // LED velocity for each of the 64 grid pads
    uint8_t sceneLeds[8];   // LED velocity for the 8 right-column scene buttons (index 0=bottom=H)
    uint8_t topLeds[8];     // LED velocity for the 8 top round buttons (index 0=left=CC104)
    bool    dirty;          // true = Base should push LED state to Launchpad
    int     chainLength;    // number of page modules in the chain (including this one)
};

} // namespace P64
extern Model* modelStep64;
extern Model* modelCafe64;
extern Model* modelGome64;
extern Model* modelNotes64;
extern Model* modelEuclid64;
extern Model* modelBounce64;
extern Model* modelMlr64;
extern Model* modelNotes8;
extern Model* modelLife64;
extern Model* modelSequencer64;
extern Model* modelInertia64;
extern Model* modelKeys64;
extern Model* modelMeadow64;
extern Model* modelPads64;
extern Model* modelXY64;
extern Model* modelRhythm64;
extern Model* modelDrums64;
