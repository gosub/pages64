# Sequencer64 — design (pass 1)

A cross between Step64 and Sliders64: a clockable CV sequencer where each
column holds a slider-style value and the clock walks the columns. Version
target: 2.11.0. Status: **drafted 2026-06-12**, from the ROADMAP milestone
(loop idiom, slew and outputs were resolved in the 2026-06 design round).

## Scope summary

8 columns = 8 steps; tap a row to set a column's value (bottom = 0 V, top =
10 V, 8 levels — all 8 rows are value resolution). The divided clock advances
a playhead through a loop range; the playing column's value goes to the CV
output. Scene A momentarily reveals a Step64-style control strip on the
bottom row for jumps and loop ranges. Optional output slew. Title **SEQ64**.

## 1. Grid

- **All 64 pads are value editors:** press a pad → that column's value is set
  to its row height, exactly Sliders64's mapping (`value = (7 − row) / 7`,
  bottom row = 0 V, top row = 10 V). Acting on press.
- **Display:** each column shows its value as a bar from the bottom (or a
  single dot — *display style* menu, shared with Sliders64's idiom) in the
  value color; the playing column is drawn in the indicator color.

## 2. Scene A — control strip (momentary)

While scene A is held, the **bottom row** becomes Step64's control row;
release A and the row returns to showing values, so the full 8-row value
resolution stays available (the decision from the design round).

- The strip shows the loop as a solid bar (control color) with the playing
  step highlighted (indicator color).
- **Tap a strip pad:** jump — the playhead moves to that step *immediately*
  (the CV is a continuous output, so unlike Step64's wait-for-tick jump, an
  immediate move is what you hear; the next tick advances from there).
- **Hold one strip pad + press another:** loop range from the left pad to
  the right pad inclusive (Step64's two-button gesture, resolved the same
  way: two-button on second press, single tap on release). The playhead is
  clamped into the new range.
- Rows 1–7 keep editing values while the strip is held; only the bottom row
  is borrowed.
- No tap-vs-hold delay needed (the Life64 lesson does not apply): scene A
  has no tap action, so showing the strip immediately is correct.
- Scene A LED lit while held; all other scene buttons are unused.

## 3. Clock, playhead, reset

- Standard **clock divider** (÷1…÷64). On each divided tick the playhead
  advances one step within `[loopStart, loopEnd]`, wrapping to `loopStart`,
  and the **TRIG** output fires a 5 ms pulse (every advance, including wraps
  and post-jump steps).
- **RESET tick:** playhead to `loopStart`, divider re-zeroed.
- Default loop: full 1–8.

## 4. Outputs

- **CV** (mono): the playing column's value, 0–10 V, slewed.
- **TRIG** (mono): 5 ms pulse on every step advance.
- **POLY** (8 ch): all eight column values continuously, each slewed — a
  clocked Sliders64 for free.

## 5. Slew

Right-click menu: **Off** (default, stepped) plus the Sliders64 rate ladder
labeled by full-range time {0.125 s, 0.5 s, 1 s, 2 s, 4 s, 8 s, 16 s}. One
setting; applies to the main CV (glide between steps) and the poly channels
(glide when a value is edited).

## 6. Menu, panel, serialization

- Menu: clock divider, slew, display style (full bar / dot), **value
  color** (green), **step indicator color** (amber), **control bar color**
  (yellow, matching Step64).
- Panel: 10 HP, page grammar, title **SEQ64**, three badged jacks: CV,
  TRIG, POLY.
- Serialized: the 8 values (ints 0–7), loop start/end, playhead, slew index,
  display style, colors, clock divider. Transient: strip held state, slewed
  output positions.

## 7. Implementation stages (each a commit, panel first as usual)

1. **Core:** panel, value editing, clocked scan with loop state, CV/TRIG/POLY
   outputs (unslewed), serialization.
2. **Control strip:** scene A momentary strip, jump and two-button loop
   range, strip LEDs.
3. **Slew + polish:** slew menu and engine, display style, docs page, README
   entry, version bump 2.11.0.
