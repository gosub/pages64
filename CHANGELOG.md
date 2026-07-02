# Changelog

All notable changes to pages64. Versioning follows the project convention:
minor bump per new module, patch bump for fixes and enhancements.

## 2.14.2 — 2026-07-02

- Internal: the expander message from Base64 to the page chain is now a
  compact ordered event list (~4.5× smaller per-frame copy). A press and
  release of the same pad within one audio frame are no longer collapsed,
  so very fast gestures land correctly at large buffer sizes.

## 2.14.1 — 2026-07-02

- Base64: clock and reset inputs now use Schmitt-trigger hysteresis
  (0.1 V / 1 V), so slow or noisy edges tick exactly once.
- Base64: LED diff caches are invalidated when leaving page-select mode,
  removing a race that could leave overlay LEDs stuck on the device.
- Docs: annotated grid figures for the Mlr64, Keys64, Cafe64 and Meadow64
  play pages; README states the hardware requirement up front; this
  changelog.

## 2.14.0 — 2026-06-14

- **New module: Meadow64** — meadowphysics-style cascading counters: eight
  countdown rows that reset and bump each other through a glyph rule page
  into evolving cross-rhythms. 8 mono + poly trigger outputs.
- New example patch: `06_meadow.vcv`.

## 2.13.3 — 2026-06-13

- Keys64: top-button config pages (Play / Scale options / Arp options).

## 2.13.2 — 2026-06-13

- Keys64: scene-B arpeggiator with 10 modes.

## 2.13.1 — 2026-06-13

- Keys64: scene-A note latch, replacing the octave scenes.

## 2.13.0 — 2026-06-13

- **New module: Keys64** — the grid as a playable isomorphic / scale
  keyboard: in-key or chromatic layouts, voice allocation, poly pitch,
  gate and retrigger outputs.

## 2.12.10 — 2026-06-13

- Inertia64: WRAP poly trigger output on cursor border-crossings.

## 2.12.9 — 2026-06-13

- Inertia64: disengage friction while pedaling (menu option, on by default).

## 2.12.8 — 2026-06-13

- Inertia64: cap friction at the former level-6 value, rescale the curve.

## 2.12.7 — 2026-06-13

- Inertia64: gentler, geometric friction curve.

## 2.12.6 — 2026-06-13

- Inertia64: Absolute VEL output option.

## 2.12.5 — 2026-06-13

- Inertia64: per-lane viscous friction + Friction config page.

## 2.12.4 — 2026-06-13

- Inertia64: bidirectional lanes + Direction config page.

## 2.12.3 — 2026-06-13

- Inertia64: scene button sends a stopped column home.

## 2.12.2 — 2026-06-13

- Inertia64: declick the POS output with a 1 ms slew limiter, on by default.

## 2.12.1 — 2026-06-13

- Inertia64: rising-and-wrapping mass instead of spinning disc, gentler pedals.

## 2.12.0 — 2026-06-12

- **New module: Inertia64** — eight rising masses with inertia: throttle and
  brake pedals per column, handbrake scenes; ramp and velocity outputs.

## 2.11.0 — 2026-06-12

- **New module: Sequencer64** — clocked CV sequencer with slider-style steps,
  hidden control strip for jumps and loop range, optional slew.

## 2.10.2 — 2026-06-12

- Life64: delay the loop length selector so a quick tap doesn't flash it.

## 2.10.1 — 2026-06-12

- Life64: push freeze and save-flash scene LEDs on static frames.

## 2.10.0 — 2026-06-11

- **New module: Life64** — Conway's Game of Life: the clock steps
  generations, pads toggle cells; freeze, randomize, frame looping and a
  famous-pattern library.
- New example patch: `05_life.vcv`.

## 2.9.1 — 2026-06-11

- Per-module manuals: each module's `manualUrl` now points to its own page
  in `docs/`.

## 2.9.0 — 2026-06-11

- **New module: 8Notes** — companion scale pitch source for the 8-voice page
  modules: poly gate in, in-key poly pitch + gate out.

## 2.8.0 — 2026-06-10

- **New module: Mlr64** — performance sample cutter after *mlr*: eight
  varispeed loop lanes cut live from the grid, choke groups, pattern
  recorders.
- New example patch: `04_mlr.vcv`.

## 2.7.0 — 2026-06-10

- **New module: Bounce64** — bouncing-ball rhythm machine after the
  Tenori-on's bounce mode; drop height sets the period.

## 2.6.0 — 2026-06-10

- **New module: Euclid64** — euclidean rhythm sequencer, one E(fill, length)
  voice per column.

## 2.5.0 — 2026-06-10

- **New module: 64Notes** — companion note mapper and voice allocator turning
  the 64-cell gate format into pitched polyphony.
- New example patches: `02_gome_64notes.vcv`, `03_four_pages.vcv`.

## 2.4.1 — 2026-06-10

- Clock divider infrastructure shared by all clock-driven page modules
  (÷1 … ÷64 in the context menu).
- Panel and LED polish across the early modules.

## 2.4.0 — 2026-06-07

- **New module: Gome64** — 2D pattern arpeggiator after monome's *gome*:
  hold roots, a pattern shape walks the grid.

## 2.3.0 — 2026-06-07

- **New module: Cafe64** — polyrhythm performance sequencer inspired by
  stretta's *Press Cafe*: eight patterns, eight voices, three edit sub-pages.

## 2.2.0 — 2026-06-06

- **New module: Step64** — classic 8-step trigger sequencer with a
  loop-range control row and step CV.

## 2.1.1 — 2026-06-06

- Fixes across Flin64 (period timing, phase reset, tap-to-start) and panel
  alignment; versioning policy documented.

## 2.1.0 — 2026-06-05

- **New module: Sliders64** — each column is a slewed CV slider; scene
  buttons pick the slew rate.
- **New module: Flin64** — cyclic polyrhythm sequencer after monome's
  *flin*: eight looping rays at harmonic speed ratios.
- **New module: Grid64** — 64 momentary/toggle buttons, 64 individual mono
  gate outputs.
- New example patch: `01_flin_sliders.vcv`.

## 2.0.0 — 2026-06-01

- Initial release: **Base64** (Launchpad Mini MkII MIDI hub, page selection,
  clock/reset distribution) and **Buttons64** (64 momentary/toggle buttons,
  4 × 16-channel polyphonic gate outputs).
