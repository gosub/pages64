#pragma once
#include "plugin.hpp"

// ── PageModule ────────────────────────────────────────────────────────────────
// Base class for all page modules in the expander chain.
// Handles expander buffer lifecycle, LeftMessage/RightMessage routing,
// ledState management, and the active-page indicator light.
//
// Subclasses implement five virtual hooks; process() is sealed (final).
// Convention: lights[0] = green (active page), lights[1] = red (→ yellow when
// connected but inactive). Both must exist in the subclass enum starting at 0.

struct PageModule : Module {

    int     myPageIndex = 0;
    uint8_t ledState[64];
    bool    ledsDirty   = true;
    bool    wasActive   = false;

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

    // Called every frame before expander logic (e.g. mode-switch transitions).
    virtual void pagePreProcess() {}

    // Called when this is the active page and a LeftMessage is available.
    virtual void pageActive(const P64::LeftMessage& msg) = 0;

    // Called every frame when this page is not active; clear transient state.
    virtual void pageInactive() {}

    // Rebuild ledState from internal state; set ledsDirty if anything changed.
    virtual void rebuildLeds() = 0;

    // Push output voltages.
    virtual void updateOutputs() = 0;

    // ── process (sealed) ──────────────────────────────────────────────────────

    void process(const ProcessArgs& args) final {
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
