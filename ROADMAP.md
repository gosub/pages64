# pages64 roadmap

Working plan as of June 2026. Versioning follows the project convention:
minor bump per new module, patch bump for fixes/refactors.

## Design principles (confirmed)

- **Positional page identity is intentional.** Max 8–16 pages per patch, arranged
  with a strong positional sense (monome sum style). No per-page colors in the
  page-select overlay; the two-color palette can't support it tastefully anyway.
- **Page switching stays a hardware gesture only.** No page-select CV input.
- **Button role convention** (to be added to CLAUDE.md):
  - **Top round buttons 1–8**: *static* page configuration. Button 8 is reserved
    globally for page select; buttons 6 and 7 are reserved for future global
    features (state snapshot, gesture recorder — see Next steps). Pages may use
    1–5 for sub-page/config selection (Cafe64 uses 1–3).
  - **Scene buttons A–H**: *interactive play* (latch modes, mute groups, pattern
    recorders). Never static config.
- **Modularity boundary**: page modules emit gates/triggers/CV; pitch mapping and
  voice allocation live in companion utility modules (64Notes family). The one
  sanctioned exception is Mlr64, where playability requires built-in sample
  playback.
- **Companion module naming and accent.** Companion (non-page) modules reverse
  the naming convention: **64Notes**, **8Notes** (vs. page modules Gome64,
  Flin64, …). Visually they use a complementary **blue accent** in place of the
  orange — `#22aff2` (the exact complement of `#f26522`, same saturation and
  brightness) — for the trapezoid, bottom rule, and any accent strokes. The rest
  of the panel grammar (background, text, badges, title style) is unchanged.

---

## Milestone 1 — Foundation cleanup (2.4.1)

Pure refactor, one commit per item.

1. **Centralize clock/reset edge detection in Base64.** Add `bool clockTick` and
   `bool resetTick` to `LeftMessage` (keep the raw voltages for anything that
   wants them). Base64 computes edges once; page modules drop their
   `prevClock`/`prevReset` code.
2. **Shared `P64::ClockDivider`.** Small struct (`div`, `count`, `process(tick)`)
   plus `appendClockDivMenu()` and `appendColorMenu()` helpers in a shared header.
   Refactor Flin64, Step64, Cafe64, Gome64 to use them; deletes ~40 duplicated
   lines per module and turns the CLAUDE.md "clock divider convention" from
   documentation into code.
3. **Cache expander neighbors.** Override `onExpanderChange()` in `PageModule`
   and Base64 instead of running `dynamic_cast` per sample in
   `isLeftNeighbour`/`isRightNeighbour`.
4. **Document the button role convention** in CLAUDE.md and README, including the
   reservation of top buttons 6–8.

## Milestone 2 — 64Notes (2.5.0)

The note-mapper companion; design already locked in `Notes64.md` (module renamed to **64Notes** per the companion naming convention). Completes
Gome64 musically and gives every future 64-cell page module (Life64, …) a path
to pitch.

- New `src/Scales.hpp` (shared scale math — also used later by 8Notes/Keys64).
- 4× poly gate in (64-cell format), poly PITCH + GATE out, plus a poly **RETRIG**
  trigger output so external envelopes retrigger cleanly on voice steal.
- Voice allocator: `maxPoly` 1–16, steal strategies (oldest / newest / lowest /
  highest / round-robin / no-steal).
- Note-length modes: **fixed-time (default**, makes Gome64's 5 ms triggers
  musical**)**, clock-synced, gate-follow.
- Mapping modes: 1D linear scale, 2D isomorphic, 2D scale grid (gome-native),
  chord-per-cell.
- All config in the right-click menu, no panel knobs. Velocity output skipped in v1.

## Milestone 3 — Example patches

Two or three patches checked into `patches/` and linked from the README:

1. Flin64 + Step64 drum/polyrhythm patch.
2. Gome64 → 64Notes → poly oscillator melodic patch.
3. A "full instrument" patch with 4+ pages showing live page switching.

Cheap, and directly attacks setup friction — the main ease-of-use cost of the
modular design.

## Milestone 4 — Euclid64 (2.6.0)

8 columns = 8 euclidean trigger voices. Tap a row to set fill (1–8); two-button
hold gesture (the established Flin/Step idiom) sets length + fill. Rotation via
scene buttons or a second tap idiom — to be decided in design. 8 mono trigger
outs + poly out, standard clock divider. Small module, big playability.

## Milestone 5 — Bounce64 (2.7.0)

Tenori-on bounce mode / boiingg: press a pad to drop a ball in that column from
that height; it bounces and fires a trigger on the floor hit; drop height sets
the period. Sibling of Flin64 with a different physical feel; the two interlock
well in a patch. 8 mono + poly trigger outs.

(Milestones 4 and 5 are independent fillers — either can be skipped or reordered
if Mlr64 wants to come sooner.)

## Milestone 6 — Mlr64 (2.8.0)

The headliner. Sample playback lives **inside** the page module (the delegation
design — phase CV out to an external sampler, phase back in for display — dies
on latency, missing protocol standards, and setup burden).

**Grid layout:**
- All **8 grid rows = 8 sample lanes** (mute groups / recorders are on scene
  buttons, not the grid, per the button convention).
- Each lane = one loaded loop divided into 8 slices; pressing a pad jumps the
  playhead to that slice, quantized to a configurable subdivision.
- Playhead position shown as the moving lit pad per lane.

**Scene buttons A–H (interactive):** mute groups and pattern recorders (exact
split decided in design — e.g. A–D recorders, E–H group mutes/stops).

**Top buttons (static config):** sub-pages for lane setup (beats-per-loop,
group assignment, loop/one-shot), within buttons 1–5.

**Sync (no offline resampling, no timestretch):**
- Base64's clock is the tempo reference: measure the smoothed period between
  ticks, with a ticks-per-beat menu setting.
- Each sample declares its length in beats (menu: 1/2/4/8/16; auto-guess from
  duration vs. tempo; parse `*_120bpm*`-style filenames when present).
- Playback increment = `sampleFrames / (beats × secondsPerBeat)` → **varispeed**:
  tempo mismatch becomes pitch shift (the classic mlr aesthetic), sample-accurate
  and nearly free. Reset re-zeros all lane phases.
- Loop prep (clean cuts) remains the user's job, as in original mlr.

**I/O:** per-lane audio out (8 mono or one 8-ch poly) + stereo/mono mix out.
Sample loading via context menu per lane and drag-drop.

---

## Next steps (out of scope for now)

### Cross-page gesture recorder (reserve top button 7)

mlr-style pattern recorder, but global: record grid/scene presses across pages
with clock-relative timestamps, loop them quantized to the clock. Lives entirely
in **Base64** (it already sees every MIDI event and knows the active page), so no
page module changes — *except* one protocol extension: replayed events must reach
the page they were recorded on even when it isn't active. That means tagging
replay events with a page index in `LeftMessage` and letting `PageModule` deliver
them to the matching page. Worth keeping in mind during Milestone 1 so the
message layout doesn't have to break twice.

Interaction sketch: tap button 7 to arm, first press starts the loop, second tap
closes it (length quantized to clock); tap again to mute/clear (long-press =
clear).

### State snapshot / reload (reserve top button 6)

The Elektron feature is **Temp Save / Temp Reload**: save the current state with
one gesture, mangle everything live, snap back with another. Implementation is
surprisingly cheap because every page module already serializes via
`dataToJson`/`dataFromJson`: Base64 broadcasts a save/restore command flag in
`LeftMessage`, and `PageModule` handles it generically with a JSON round-trip —
all modules get it for free, including future ones. Transient live state (held
pads) is already excluded by the existing `dataFromJson` implementations, which
is the correct restore behavior anyway.

Interaction sketch: hold button 6 to save, tap to reload.

### Strategic backlog

- **Device profiles in Base64** (Launchpad MkIII / X / APC Mini): the 16-color
  `LED_COLOR_DEFS` palette is already the device-independent abstraction; only
  the note mapping and LED encoding in Base64 vary per profile. Biggest audience
  multiplier available; page modules inherit it untouched.
- **Grid LED visualizer**: read-only on-screen mirror of the current LED state
  (on Base64 or a small companion module). Not interactive — multi-button
  gestures don't translate to a mouse — but useful for demos, screenshots, and
  development without hardware.
- **8Notes**: poly gate in, channel *n* → scale degree *n*, pitch + gate out.
  Trivial once `Scales.hpp` exists; the ergonomic companion for Flin64 / Step64 /
  Cafe64.
- **Keys64**: the grid as an isomorphic/scale keyboard page emitting poly
  pitch + gate directly. Shares `Scales.hpp`.
- **Module backlog**: Life64 (Conway, 64-cell out → 64Notes), Meadow64
  (meadowphysics-style cascading counters), Corners64, XY64 (slewed 2D pad).
- **`LeftMessage` compaction**: replace the per-note flag arrays with a small
  event list (`{type, index, value}` + count). Shrinks the per-sample chain copy
  ~10×, fixes same-frame press+release collapsing, and makes room for the
  gesture-recorder page tags. Do it when the message next needs to change.
