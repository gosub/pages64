# pages64 roadmap

Working plan as of June 2026. Versioning follows the project convention:
minor bump per new module, patch bump for fixes/refactors.

Shipped through 2.13.0: Base64, Buttons64, Grid64, Sliders64, Flin64, Step64,
Cafe64, Gome64, 64Notes, Euclid64, Bounce64, Mlr64, 8Notes, Life64,
Sequencer64, Inertia64, Keys64, and the example patches in `patches/`.

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

### Milestone 1 — Meadow64

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

Full design (cross-rules become an 8×8 matrix):
[docs/design/Meadow64.md](docs/design/Meadow64.md).

### Milestone 2 — XY64

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

## Hypotheticals (consider after everything else)

Open ideas and questions, not committed work — to weigh once the milestones
and backlog above are done.

### DRUMS64 (or 64DRUMS)

Each button is a **randomly generated drum sound**. Likely generated once at
startup (a fixed kit for the session) rather than synthesized per-trigger, so
the cost is paid up front and playing is just sample playback. Open questions:

- **Companion or page module?** As a pure-CV helper (the 64Notes family) it
  would take gate-format input and emit audio — simple, composable. As a
  proper **page module** it owns the grid and can offer richer interaction.
- **Per-button random rhythm** (a long-wanted idea): instead of (or as well as)
  one-shot hits, each button triggers a *random rhythm* that plays while held
  or latched — a generative drum machine. This pulls toward the page-module
  form (it needs the clock and the grid), but could also live in a separate
  page that drives DRUMS64's sounds. Decide whether the sound source and the
  rhythm engine are one module or two.

### 64Notes as an optional page module

Today 64Notes is a pure-CV companion configured only from the right-click menu.
A **page-module variant** (joining the chain, owning a grid page) could expose
its parameters as on-grid config pages — the Keys64 idiom — so the arrangement,
scale, root, octave, intervals and chord type are editable live from the
Launchpad rather than the mouse. Question: ship a second module, or make one
module switch roles? (Relates to the backlog item "promote note parameters to
the panel" — the grid is another answer to the same problem.)

### Shared scale selection broadcast from Base64

Should the musical context — **root, scale, intervals** — be chosen once on
**Base64** and broadcast down the chain (a few fields in `LeftMessage`), so
64Notes, 8Notes, Keys64 and any future pitched module stay in key together
without setting each one by hand? Trade-offs to think through: a global "key"
is convenient and keeps an ensemble coherent, but some patches want modules in
*different* keys on purpose. Likely answer is a per-module **"follow Base64 /
override"** switch — global by default, local when you want it. Cheap on the
wire; the design work is the override ergonomics and which params are shared.

### 16 pages

Raise the per-patch page limit from 8 to **16** (the upper bound the design
principles already name). The selection gesture is the main change: today
holding button 8 lights the **top grid row** (8 pads = 8 pages); 16 pages
would use the **top two rows** (rows 1–2 = pages 1–16), keeping the strong
positional sense. Things to settle:

- **Page-select CV out.** At the current 1 V/page, 16 pages span 0–15 V, past
  the usual 0–10 V range. Either rescale (e.g. ⅔ V/page → 0–10 V) — a breaking
  change to existing patches — or keep 1 V/page and accept the wider range.
- **Overlay layout.** Two rows of 8; active page green, the rest yellow, as
  now. The reserved top round buttons (6–8) are unaffected.
- No page-module changes — pages already learn their index from the chain;
  only Base64's overlay and counter logic grow.
