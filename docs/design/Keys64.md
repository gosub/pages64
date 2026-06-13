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

## 3. Scene buttons — octave selector

Scene A–H pick the **base octave** directly (A = top = octave 7 … H = bottom
= octave 0, so higher button = higher pitch), the live performance control;
the current octave's button is lit. Changing octave rebuilds the map for new
presses; voices already sounding keep their pitch.

## 4. Display

- **Held cells**: bright (play color).
- **Tonic markers**: every cell whose pitch class equals the root
  (`midi % 12 == root`) is lit dim (root color), so you can always find “home”
  in either arrangement.
- Everything else off.

## 5. Outputs, menu, serialization

- **PITCH** (poly, 1 V/oct), **GATE** (poly, 10 V), **RTRG** (poly trigger) —
  all `maxPoly` channels.
- No clock, no inputs (it's an instrument, not a processor); transpose is via
  the base-octave scene control and the menu.
- Menu: **arrangement**, **scale**, **root note**, **base octave**, **row
  degrees** (scale grid) / **column & row semitones** (isomorphic),
  **polyphony**, **voice stealing**, **play / root / octave colors**.
- Serialized: arrangement, scale, root, octave, the interval params, maxPoly,
  steal mode, colors. Transient: held cells, voices.

## 6. Panel

10 HP, page grammar (orange), title **KEYS64**, active-page light. Three
badged jacks: PITCH, GATE, RTRG.

## 7. Implementation stages (each a commit, panel first as usual)

1. **Core:** panel, both arrangements + note map, gate-follow voice
   allocation, PITCH/GATE/RTRG, held + tonic LEDs, scene octave selector,
   full menu, serialization.
2. **Docs + release:** docs page, README entry, version bump 2.13.0.

## Resolved decisions (2026-06-13)

1. **Page module, not a companion** — it emits pitch directly, per the
   milestone; orange accent.
2. **Pitch rises up-and-right** (bottom-left lowest), keyboard convention —
   the vertical flip of 64Notes.
3. **Gate-follow only** — a keyboard; no fixed-time / clock note lengths.
4. **Scene buttons = base octave**, the one live control worth a hardware
   gesture.
