#pragma once
#include <rack.hpp>
#include <app/MidiDisplay.hpp>

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

// ── Expander messages ────────────────────────────────────────────────────────

// Sent left→right: Base to page modules (and forwarded along the chain)
struct LeftMessage {
    int  activePage;        // currently active page index
    int  pageCounter;       // each page reads this as its own index; pass (pageCounter+1) rightward
    bool repaintRequested;  // Base sets this for one frame when exiting page-select mode
    float clockVoltage;     // raw voltage of Base64 CLOCK input jack
    float resetVoltage;     // raw voltage of Base64 RESET input jack
    bool  clockTick;        // rising edge on CLOCK this frame (computed by Base64)
    bool  resetTick;        // rising edge on RESET this frame (computed by Base64)
    bool noteEvent[128];    // true  = a Note-On/Off arrived this frame
    uint8_t noteVelocity[128]; // velocity (0 = note-off)
    bool ccEvent[128];
    uint8_t ccValue[128];
    // Right column scene buttons A–H: index = Launchpad row (0 = top = A, 7 = bottom = H)
    bool sceneEvent[8];
    uint8_t sceneVelocity[8]; // 0 = released
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
