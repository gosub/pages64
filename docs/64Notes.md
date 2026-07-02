# 64Notes (titled 64NOTES)

*Part of [pages64](../README.md).*

This is the first *companion* module: it is not a page and does not join the
Base64 chain — it is pure CV in → CV out, and wears a blue accent instead of the
orange to mark the difference. It turns the 64-cell gate format emitted by
Gome64 or Buttons64 (four 16-channel polyphonic cables) into pitched polyphony:
patch the four cell outputs into the four cell inputs 1:1, and connect V/OCT and
GATE to a polyphonic voice.

Each grid cell is assigned a pitch by one of four **arrangements** (right-click
menu): **1D scale** (scale degrees running left→right, top→bottom), **2D
isomorphic** (fixed semitone intervals per column/row), **2D scale grid** (the
default — horizontal steps are scale degrees, each row down is a diatonic
fourth, so everything stays in key and matches Gome64's pattern geometry), and
**chord per cell** (each cell plays a diatonic triad or seventh). Scale, root
note, base octave and the interval parameters are all in the menu; scale and
root **follow Base64's global key** by default, and picking a local one
overrides the follow switch. Pitch rises toward the bottom-right of the grid,
matching Gome64's conventions.

A **voice allocator** collapses the 64 cells into 1–16 output voices (Polyphony
menu), so a single poly oscillator can play the whole grid. When all voices are
busy, the **voice stealing** strategy decides which one to take: oldest, newest,
lowest, highest, round-robin, or off (new notes are dropped). The **RTRG**
output emits a short trigger whenever a voice (re)starts — patch it to your
envelope's retrigger input so stolen voices articulate cleanly.

Because sources like Gome64 emit 5 ms triggers, 64Notes holds notes itself: the
**note length** menu offers *fixed time* (50 ms–2 s, the default at 200 ms),
*clock-synced* (1–8 ticks of the CLK input), or *gate follow* (for sustained
sources like Buttons64 toggles). The **TRN** input transposes everything at
1V/oct.
