#pragma once
#include "plugin.hpp"

// ── PageModule ────────────────────────────────────────────────────────────────
// Base class for all page modules in the expander chain.
// Handles expander buffer lifecycle, LeftMessage/RightMessage routing,
// ledState management, and the active-page indicator light.
//
// Subclasses implement virtual hooks; process() is sealed (final).
// Convention: lights[0] = green (active page), lights[1] = red (→ yellow when
// connected but inactive). Both must exist in the subclass enum starting at 0.
//
// `sampleTime` is updated each frame and available to all hooks.

struct PageModule : Module {

    int     myPageIndex = 0;
    uint8_t ledState[64];
    bool    ledsDirty   = true;
    bool    wasActive   = false;
    float   sampleTime  = 1.f / 48000.f;   // refreshed each process() frame

    PageModule() {
        memset(ledState, P64::LED_OFF, sizeof(ledState));
        leftExpander.producerMessage  = new P64::LeftMessage;
        leftExpander.consumerMessage  = new P64::LeftMessage;
        memset(leftExpander.producerMessage,  0, sizeof(P64::LeftMessage));
        memset(leftExpander.consumerMessage,  0, sizeof(P64::LeftMessage));
        rightExpander.producerMessage = new P64::RightMessage;
        rightExpander.consumerMessage = new P64::RightMessage;
        memset(rightExpander.producerMessage, 0, sizeof(P64::RightMessage));
        memset(rightExpander.consumerMessage, 0, sizeof(P64::RightMessage));
    }

    ~PageModule() {
        delete (P64::LeftMessage*)  leftExpander.producerMessage;
        delete (P64::LeftMessage*)  leftExpander.consumerMessage;
        delete (P64::RightMessage*) rightExpander.producerMessage;
        delete (P64::RightMessage*) rightExpander.consumerMessage;
    }

    void onReset() override {
        memset(ledState, P64::LED_OFF, sizeof(ledState));
        ledsDirty = true;
        wasActive = false;
    }

    // ── virtual hooks (subclasses implement these) ────────────────────────────

    // Called every frame before expander logic (e.g. slew, mode transitions).
    // `sampleTime` is already updated before this is called.
    virtual void pagePreProcess() {}

    // Called when this is the active page and a LeftMessage is available.
    virtual void pageActive(const P64::LeftMessage& msg) = 0;

    // Called every frame when this page is not active; clear transient state.
    virtual void pageInactive() {}

    // Rebuild ledState[64] from internal state; set ledsDirty if anything changed.
    virtual void rebuildLeds() = 0;

    // Fill sceneLeds[8] for the right-column scene buttons (index 0=bottom=H).
    // Default: all off. Override to illuminate scene buttons.
    virtual void buildSceneLeds(uint8_t sceneLeds[8]) {
        memset(sceneLeds, P64::LED_OFF, 8);
    }

    // Fill topLeds[8] for the top round buttons (index 0=left=CC104 … 7=right=CC111).
    // Default: all off. Override to illuminate top buttons (e.g. sub-page selectors).
    virtual void buildTopLeds(uint8_t topLeds[8]) {
        memset(topLeds, P64::LED_OFF, 8);
    }

    // Push output voltages.
    virtual void updateOutputs() = 0;

    // ── process (sealed) ──────────────────────────────────────────────────────

    void process(const ProcessArgs& args) final {
        sampleTime = args.sampleTime;
        pagePreProcess();

        bool amActive = false;

        if (isLeftNeighbour(leftExpander.module)) {
            auto* fromLeft = reinterpret_cast<P64::LeftMessage*>(leftExpander.consumerMessage);
            myPageIndex = fromLeft ? fromLeft->pageCounter : 0;
            amActive    = fromLeft && (fromLeft->activePage == myPageIndex);

            if (isRightNeighbour(rightExpander.module)) {
                auto* toRight = reinterpret_cast<P64::LeftMessage*>(
                    rightExpander.module->leftExpander.producerMessage);
                if (toRight && fromLeft) {
                    *toRight = *fromLeft;
                    toRight->pageCounter = myPageIndex + 1;
                }
                rightExpander.module->leftExpander.messageFlipRequested = true;
            }

            if (amActive && (!wasActive || (fromLeft && fromLeft->repaintRequested)))
                ledsDirty = true;

            if (amActive && fromLeft)
                pageActive(*fromLeft);
            else if (!amActive)
                pageInactive();

            P64::RightMessage chainMsg = {};
            if (isRightNeighbour(rightExpander.module)) {
                auto* fromRight = reinterpret_cast<P64::RightMessage*>(rightExpander.consumerMessage);
                if (fromRight) chainMsg = *fromRight;
            }
            rightExpander.messageFlipRequested = true;

            auto* toLeft = reinterpret_cast<P64::RightMessage*>(
                leftExpander.module->rightExpander.producerMessage);
            if (toLeft) {
                if (amActive) {
                    rebuildLeds();
                    memcpy(toLeft->gridLeds, ledState, 64);
                    buildSceneLeds(toLeft->sceneLeds);
                    buildTopLeds(toLeft->topLeds);
                    toLeft->dirty = ledsDirty;
                    ledsDirty     = false;
                } else {
                    *toLeft = chainMsg;
                }
                toLeft->chainLength = 1 + chainMsg.chainLength;
            }

            leftExpander.messageFlipRequested = true;
        }

        wasActive = amActive;
        updateOutputs();
        bool connected = isLeftNeighbour(leftExpander.module);
        lights[0].setBrightness(amActive ? 1.f : (connected ? 0.25f : 0.f));
        lights[1].setBrightness((connected && !amActive) ? 0.25f : 0.f);
    }

private:
    bool isLeftNeighbour(Module* m) const {
        return m && (m->model == modelBase || dynamic_cast<PageModule*>(m));
    }
    bool isRightNeighbour(Module* m) const {
        return m && dynamic_cast<PageModule*>(m);
    }
};

// ── Shared context-menu builders ──────────────────────────────────────────────

namespace P64 {

// LED color picker submenu bound to a uint8_t color field of a page module.
inline void appendColorMenu(Menu* menu, PageModule* m, const std::string& label,
                            uint8_t* field, bool includeOff = false) {
    menu->addChild(createSubmenuItem(label, "", [=](Menu* sub) {
        for (auto& c : LED_COLOR_DEFS) {
            if (!includeOff && c.velocity == LED_OFF) continue;
            uint8_t vel = c.velocity;
            sub->addChild(createCheckMenuItem(c.name, "",
                [=]() { return *field == vel; },
                [=]() { *field = vel; m->ledsDirty = true; }
            ));
        }
    }));
}

// Standard "÷1 … ÷64" clock divider submenu.
inline void appendClockDivMenu(Menu* menu, ClockDivider* divider) {
    menu->addChild(createSubmenuItem("Clock divider", "", [=](Menu* sub) {
        for (int d : CLOCK_DIVS) {
            sub->addChild(createCheckMenuItem(string::f("÷%d", d), "",
                [=]() { return divider->div == d; },
                [=]() { divider->set(d); }
            ));
        }
    }));
}

} // namespace P64
