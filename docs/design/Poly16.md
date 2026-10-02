# 16Poly — design

A narrow companion that converts between 8-channel and 16-channel polyphony.

## Why

pages64 speaks two poly widths. Eight channels is "one per row or column"
(Flin64, Sliders64, Meadow64, Mlr64's lanes, 8Notes); sixteen is the cell bus,
two grid rows per jack (Buttons64, Gome64, Life64, Rhythm64, the kits).
Third-party modules are just as inconsistent. Joining two 8-voice sources into
one 16-voice input, or splitting a 16-channel bus into two halves, otherwise
takes VCV Merge/Split and sixteen cables.

## Shape

A 6HP companion (blue accent, reversed name) with **two identical strips**
side by side, each one merger above a separator and one splitter below:

- **Merge**: inputs **1–8** and **9–16** → output **1–16**.
- **Split**: input **1–16** → outputs **1–8** and **9–16**.

Two of each because conversions tend to come in pairs (two cell buses, two
8-voice sources), and the 6HP two-column grid holds exactly two strips on the
rows a single 4HP strip would use; one strip alone would save only 2HP.

The labels name the channels a jack carries, which also says where each input
lands, so the panel needs no other explanation.

## Behavior

**Merge.** Each input contributes at most its first 8 channels.

- With 9–16 patched, the 1–8 input is padded to 8 channels (missing channels
  are 0 V), so 9–16 always lands on channels 9–16 and cell alignment survives
  a short first input. Output channels = 8 + the 9–16 input's (up to 8).
- With 9–16 unpatched, the output carries the 1–8 input as is (up to 8
  channels), so a merge with one cable behaves like a plain truncation.
- With neither input patched, the output is silent mono: Rack keeps a patched
  output at one channel or more.

**Split.** Output 1–8 carries channels 1–8 of the input (as many as are
present, up to 8); output 9–16 carries channels 9–16 (as many as are present;
silent mono when the input has 8 or fewer). A mono input therefore comes out
of 1–8 as mono.

## Not in scope

Other widths (4+4+4+4, 8 → 1+1+…) are VCV Merge/Split's job. No state, no
context menu, nothing saved.
