# Keys64 — design (pass 1)

The grid as a playable isomorphic / scale keyboard, emitting polyphonic pitch
and gate directly (no companion module). Version target: 2.13.0. Status:
**drafted 2026-06-13**, from the ROADMAP milestone.

## Scope summary

A page module (orange, joins the chain) that turns the 8×8 grid into a
keyboard: each cell is a note, pressing it plays it. A voice allocator
collapses the held cells onto a polyphonic V/OCT + GATE output (plus a
retrigger), so it drives a poly voice with no 64Notes in the patch. Scale
math is shared from `Scales.hpp`. Title **KEYS64**.

## 1. Note layout

Each cell `(row, col)` maps to a MIDI note, rebuilt whenever a parameter
changes. Pitch rises **up and to the right** (bottom-left = lowest), the
natural keyboard reading — the vertical mirror of 64Notes, which follows
Gome's downward geometry. `up = 7 − row` (rows from the bottom). Two
**arrangements** (menu):

- **Scale grid** (in-key, default): `degree = col + rowDegrees · up`, through
  `degreeToSemitone` so every cell is in key. `rowDegrees` (menu, default 3 =
  a diatonic fourth per row) makes chord shapes transpose anywhere — the
  isomorphic in-key feel.
- **Isomorphic** (chromatic): `semitones = col · colSemis + up · rowSemis`,
  fixed intervals. Defaults `colSemis = 2` (whole tone), `rowSemis = 5`
  (fourth).

Shared params: **scale**, **root note**, **base octave** (`base MIDI =
12·(octave+1) + root`, the Notes-module convention). The pitch is
`(midi − 60) / 12` V.

## 2. Playing & voice allocation

- **Press a cell → note on**, **release → note off** (gate-follow, the only
  sensible keyboard behavior).
- **Voice allocator** (the 64Notes model, gate-follow only): `maxPoly` 1–16
  (menu, default 8). A press takes a free voice, else **steals** per the menu
  strategy (oldest / newest / lowest / highest / round-robin / off=drop). Each
  voice records its note and age; re-pressing a held cell retriggers it in
  place.
- **RTRG**: each voice fires a short trigger when it (re)starts, so envelopes
  re-strike on a stolen voice.
- Leaving the page (or the page going inactive) releases all voices — no
  stuck notes; a Launchpad doesn't send note-offs you can't see.

> **Revision (2026-06-13).** Shipped 2.13.0 with the octave on scene buttons.
> Reworked the same day: scene A is now a **latch**, scene B an
> **arpeggiator**, and the octave moved to a config page (§3, §3b, §6).

## 3. Scene A — latch

Notes are momentary by default. Scene A latches sustained notes:

- **Hold A + play** → those notes toggle latched (sustain after release);
  other notes stay momentary. A press toggles a latched note whenever
  `latchMode || aHeld`.
- **Tap A** (release with no note played during the hold) → toggles a global
  `latchMode` where every press is a latch toggle. Leaving `latchMode` clears
  all sustained notes.
- A note **sounds** when `held || latched`; voices are driven by a sounding-set
  sync each frame. Latched notes persist across page switches; momentary notes
  release when the page goes inactive (no note-offs arrive once you leave).
- Scene A LED: bright (latch color) in `latchMode`, dim while A is held.

## 3b. Scene B — arpeggiator

Tap B to arpeggiate the sounding notes. On each divided clock tick one note
plays, **monophonically** (poly output drops to 1 channel); the first note of
a chord sounds immediately. Standard clock divider; RESET restarts. Ten modes
(menu or the Arp page): up, down, up-down, down-up, converge, diverge,
as-played (press order), alternating-root, random, random-no-repeat. The pool
is sorted by pitch (by press stamp for as-played); the per-mode index is
`seqPos`-driven and wraps with the pool size.

## 4. Display & pages

- **Play page**: held cells bright (play color), latched cells in the latch
  color, tonic cells (`midi % 12 == root`) dim, else off.
- **Top buttons 1–3** select a page (Cafe64/Mlr64 idiom; active bright, others
  dim): **1 Play**, **2 Scale options**, **3 Arp options**.
- **Scale page**: scale selector (rows 0–1), layout (row 1 cols 6/7 = scale
  grid / isomorphic), a piano **root** selector (white keys row 4, black keys
  row 3), **octave** (row 6). Available cells dim, the current selection bright.
- **Arp page**: one pad per mode (rows 0–1), current bright. Leaving the play
  page releases momentary notes; latch and arp keep running on every page.

## 5. Outputs, menu, serialization

- **PITCH** (poly, 1 V/oct), **GATE** (poly, 10 V), **RTRG** (poly trigger);
  `maxPoly` channels, or 1 while the arp runs.
- Clock and reset arrive through the chain (for the arp); no jack inputs.
- Menu: **arrangement**, **scale**, **root**, **base octave**, **row degrees**
  / **column & row semitones**, **polyphony**, **voice stealing**, **arp mode**,
  **clock divider**, and the **play / latch / root / arp / page** colors.
- Serialized: arrangement, scale, root, octave, interval params, maxPoly, steal
  mode, latchMode, latched[64], sub-page, arpOn, arpMode, clockDiv, colors.
  Transient: held cells, voices, arp position.

## 6. Panel

10 HP, page grammar (orange), title **KEYS64**, active-page light. Three
badged jacks: PITCH, GATE, RTRG.

## Resolved decisions (2026-06-13)

1. **Page module, not a companion** — it emits pitch directly, per the
   milestone; orange accent.
2. **Pitch rises up-and-right** (bottom-left lowest), keyboard convention —
   the vertical flip of 64Notes.
3. **Gate-follow** for momentary notes; **scene-A latch** adds sustain
   (revised — the original octave-on-scenes is replaced).
4. **Scene B arpeggiator**, monophonic, 10 modes, clock-synced (added).
5. **Octave and all scale options on a config page** (and the menu); top
   buttons carry the Play / Scale / Arp pages (revised).
