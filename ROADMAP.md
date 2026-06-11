# pages64 roadmap

Working plan as of June 2026. Versioning follows the project convention:
minor bump per new module, patch bump for fixes/refactors.

Shipped through 2.9.0: Base64, Buttons64, Grid64, Sliders64, Flin64, Step64,
Cafe64, Gome64, 64Notes, Euclid64, Bounce64, Mlr64, 8Notes, and the example
patches in `patches/`.

## Design principles (confirmed)

- **Positional page identity is intentional.** Max 8–16 pages per patch,
  arranged with a strong positional sense (monome sum style). No per-page
  colors in the page-select overlay; the two-color palette can't support it
  tastefully anyway.
- **Page switching stays a hardware gesture only.** No page-select CV input.
- **Button role convention** (documented in CLAUDE.md): top round buttons 1–8
  carry static page configuration — 8 is page select, 6 and 7 are reserved for
  the global features below; scene buttons A–H are interactive play only.
- **Modularity boundary**: page modules emit gates/triggers/CV; pitch mapping
  and voice allocation live in companion utility modules (64Notes, 8Notes).
  The one sanctioned exception is Mlr64's built-in sample playback.
- **Companion module naming and accent** (documented in CLAUDE.md): companion
  modules reverse the name (64Notes, 8Notes) and swap the orange accent for
  the complementary blue `#22aff2`.

---

## Next milestones — new modules

One minor version bump each. The order below is a suggestion: any of these can
be pulled forward or pushed back, and version numbers are assigned when work
starts.

### Milestone 1 — Life64

Conway's Game of Life as a page module. The clock steps the generations
(standard clock divider); pressing a pad toggles a cell, so the player seeds
and perturbs the colony live. Outputs:

- **64-cell gates** in the Gome64/Buttons64 format (4 × 16-channel poly), one
  gate per live cell — feed 64Notes for pitch.
- **Row/column binary CVs**: each of the 8 rows and each of the 8 columns is
  read as an 8-bit binary number and scaled to 0–10 V. Two extra 8-channel
  poly outputs (ROWS, COLS). A right-click switch selects how the bit pattern
  is decoded: classical binary or Gray code (Gray makes single-cell changes
  produce single-step CV moves).
- **Density**: one mono CV output carrying the percentage of live cells
  (0–10 V).

### Milestone 2 — Sequencer64

A cross between Step64 and Sliders64: a clockable CV sequencer where each
column holds a value set with the slider idiom (tap a row to set the level),
and the clock walks the columns, sending the current column's value to the
output. All 8 rows are value resolution; the playing column is highlighted.

- **Loop / jump (scene A, momentary):** while scene A is held, the bottom row
  becomes a Step64-style control strip — tap a pad to jump to that step, hold
  one and press a second to set the loop range. Release A and the row goes
  back to showing values, so the full 8-row resolution stays available.
- **Slew:** right-click menu, Off (default) plus Sliders64-style rates —
  stepped by default, glide when used as a smooth modulation sequencer.
- **Outputs:** the scanned CV out, a trigger out firing on every step advance,
  and an 8-channel poly out carrying all eight column values continuously
  (a clocked Sliders64 for free).
- Standard clock divider; reset returns to step 1; LED colors in the menu.

### Milestone 3 — Inertia64

Each column is a spinning disc with mass and inertia, and the column's light
cursor travels at the disc's speed. The top four buttons of a column
accelerate the disc and the bottom four apply momentary friction — accelerator
and brake pedals — each at four different intensities. Per column, two
outputs: the X coordinate of a fixed point on the disc rim (reads as a
sine-like wave whose frequency follows the disc speed) and the angular
velocity. Two 8-channel poly outputs (X, VEL).

### Milestone 4 — Keys64

The grid as an isomorphic / scale keyboard page emitting poly pitch + gate
directly, no companion module needed. Shares `Scales.hpp` with 64Notes and
8Notes.

### Milestone 5 — Meadow64

meadowphysics-style cascading counters. Each row is a countdown counter: it
decrements on every clock tick and, on expiry, fires its trigger output and
reloads. Press a pad in a row to set its count (column position = 1–8 ticks);
the row displays the remaining count.

- **Cross-rules** (the heart of meadowphysics — without them this is just 8
  clock dividers): on expiry a row can also act on another row — reset it, or
  increment / decrement its count. Rule type and target row are edited on a
  config sub-page (top button 2); default is no rule. Chained rules build the
  evolving cross-rhythms the original is known for.
- **Scene buttons A–H** mute rows 1–8; muted counters keep running so they
  re-enter in phase (the Euclid64/Bounce64 idiom).
- 8 mono trigger outs + poly; standard clock divider; reset reloads all
  counters.

### Milestone 6 — XY64

A slewed 2D pad, the 2D sibling of Sliders64: the whole 8×8 grid is a single
XY surface. Pressing a pad sets the target; a cursor glides toward it at the
selected slew rate, and two mono CV outputs carry the cursor's X and Y
position (0–10 V each, left→right and bottom→top). Scene buttons A–H select
the slew rate (the Sliders64 idiom: A instant, H slowest). The target pad is
shown dim, the gliding cursor bright; LED colors in the right-click menu.

## Module ideas (name only, undesigned)

- **Corners64**

## Global features

### Cross-page gesture recorder (reserved top button 7)

mlr-style pattern recorder, but global: record grid/scene presses across pages
with clock-relative timestamps, loop them quantized to the clock. Lives entirely
in **Base64** (it already sees every MIDI event and knows the active page), so no
page module changes — *except* one protocol extension: replayed events must reach
the page they were recorded on even when it isn't active. That means tagging
replay events with a page index in `LeftMessage` and letting `PageModule` deliver
them to the matching page. Do the `LeftMessage` compaction (below) first so the
message layout doesn't have to break twice.

Interaction sketch: tap button 7 to arm, first press starts the loop, second tap
closes it (length quantized to clock); tap again to mute/clear (long-press =
clear).

### State snapshot / reload (reserved top button 6)

The Elektron feature is **Temp Save / Temp Reload**: save the current state with
one gesture, mangle everything live, snap back with another. Implementation is
surprisingly cheap because every page module already serializes via
`dataToJson`/`dataFromJson`: Base64 broadcasts a save/restore command flag in
`LeftMessage`, and `PageModule` handles it generically with a JSON round-trip —
all modules get it for free, including future ones. Transient live state (held
pads) is already excluded by the existing `dataFromJson` implementations, which
is the correct restore behavior anyway.

Interaction sketch: hold button 6 to save, tap to reload.

## Polish & infrastructure backlog

- **64Notes: promote note parameters to the panel.** Arrangement, scale, root,
  octave, intervals and chord type currently hide in the right-click menu;
  consider panel controls (small knobs/switches with value displays) so the
  musical identity of the mapper is visible and tweakable live. Voice/length
  settings (polyphony, stealing, note length) can stay in the menu.
- **Device profiles in Base64** (Launchpad MkIII / X / APC Mini): the 16-color
  `LED_COLOR_DEFS` palette is already the device-independent abstraction; only
  the note mapping and LED encoding in Base64 vary per profile. Biggest audience
  multiplier available; page modules inherit it untouched.
- **Grid LED visualizer**: read-only on-screen mirror of the current LED state
  (on Base64 or a small companion module). Not interactive — multi-button
  gestures don't translate to a mouse — but useful for demos, screenshots, and
  development without hardware.
- **`LeftMessage` compaction**: replace the per-note flag arrays with a small
  event list (`{type, index, value}` + count). Shrinks the per-sample chain copy
  ~10×, fixes same-frame press+release collapsing, and makes room for the
  gesture-recorder page tags. Do it when the message next needs to change —
  at the latest, right before the gesture recorder.
