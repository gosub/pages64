# pages64 roadmap

Working plan as of June 2026. Versioning follows the project convention:
minor bump per new module, patch bump for fixes/refactors.

Shipped through 2.18.0: Base64, Buttons64, Grid64, Sliders64, Flin64, Step64,
Cafe64, Gome64, 64Notes, Euclid64, Bounce64, Mlr64, 8Notes, Life64,
Sequencer64, Inertia64, Keys64, Meadow64, 64Pads, XY64, Rhythm64, 64Drums,
the example patches in `patches/`, plus the global features: temp
save/reload (button 6) and the global key broadcast. See CHANGELOG.md.

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

### ~~Milestone 1 — XY64~~ — shipped in 2.16.0

As designed, with two refinements (see `docs/design/XY64.md`): the cursor is
continuous (pads quantize targets, not the glide) and a TRIG output fires on
arrival — at instant slew, a trigger pad with position CV.

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

### ~~State snapshot / reload (top button 6)~~ — shipped in 2.15.1

Hold button 6 to save (flashes green when the hold matures), tap to reload.
Implemented exactly as sketched: Base64 broadcasts `CMD_SAVE`/`CMD_RESTORE` in
`LeftMessage.command`, `PageModule::handleCommand` does the JSON round-trip for
every page, active or not; Base64 snapshots the active page index itself. The
snapshot is session-transient by design.

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
- ~~**Grid LED visualizer**~~ — shipped in 2.15.0 as **64Pads**, and it went
  further than the read-only sketch: clicks are synthesized into the hardware
  MIDI path, so everything single-press is playable with the mouse (only
  multi-pad hold gestures still want hardware).
- ~~**`LeftMessage` compaction**~~ — shipped in 2.14.2 exactly as sketched
  (`GridEvent{type, index, value}` list, ~4.5× smaller, press+release
  preserved). The gesture recorder's page tags have their layout home.

## Hypotheticals (consider after everything else)

Open ideas and questions, not committed work — to weigh once the milestones
and backlog above are done.

### ~~DRUMS64 (or 64DRUMS)~~ — shipped in 2.17.0 + 2.18.0 as a split

The "companion or page module?" question resolved to **both, split along the
modularity boundary**: **Rhythm64** (page module, the per-pad random rhythm
engine, 64-cell trigger outputs) and **64Drums** (companion, seeded drum kit,
64-cell gates in → stereo out). Row families and density gradients line up so
pairing them 1:1 is a zero-config drum machine; both serialize their seeds so
patches reload deterministically. Design: `docs/design/Drums64.md`.

### 64Notes as an optional page module

Today 64Notes is a pure-CV companion configured only from the right-click menu.
A **page-module variant** (joining the chain, owning a grid page) could expose
its parameters as on-grid config pages — the Keys64 idiom — so the arrangement,
scale, root, octave, intervals and chord type are editable live from the
Launchpad rather than the mouse. Question: ship a second module, or make one
module switch roles? (Relates to the backlog item "promote note parameters to
the panel" — the grid is another answer to the same problem.)

### ~~Shared scale selection broadcast from Base64~~ — shipped in 2.16.1

Root + scale, set in Base64's menu, followed by Keys64 / 64Notes / 8Notes via
a per-module "Follow Base64 global key" switch (on for new instances, off for
patches predating the feature). Picking a local scale/root is the override
gesture. Implemented as `P64::sharedKey` (plugin-global atomics with a change
serial) rather than `LeftMessage` fields, because the companions sit outside
the expander chain. Octave and intervals stay local per module.

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
