# Changelog

All notable changes to pages64. Versioning follows the project convention:
minor bump per new module, patch bump for fixes and enhancements.

## 2.21.1 — 2026-07-05

- Three world scales added to the shared scale list: **Hijaz** (Phrygian
  dominant — Middle Eastern / flamenco / klezmer), **Byzantine** (double
  harmonic — gypsy major / raga Bhairav), **Hirajoshi** (Japanese
  pentatonic). Available everywhere a scale is chosen: Base64's global key,
  Keys64, 64Notes, 8Notes, and the kits' Quantize. Appended to the end of the
  list so existing patches keep their scale.

## 2.21.0 — 2026-07-05

- **New module: 64Micro** — seeded deterministic micro-sound kit companion
  (Raster-Noton lineage): single designed events of 0.2–20 ms, bit-identical
  on every trigger. Families: click, tick, crush, data (1-bit bitstream
  bursts), blip (rect-gated sines, some dual-tone), zap, ping, thump, plus
  Fold off-grid via Row families — the first kit with a family catalog
  larger than the grid. Sequenced varieties (alternate, ping-pong, dropout,
  doubler): deterministic cycles, never probability.
- KitModule: family catalogs may exceed the 8 grid rows; extras are
  reachable through the Row families menu, and the fully random layout
  draws across the whole catalog.

## 2.20.3 — 2026-07-04

- **Rhythm64: punch-in FX** — hold scene B and the grid becomes a PO-style
  effect selector (rows = effects, columns = amount): loop, ratchet,
  time (÷/reverse/×), density, mask, shuffle, push/drag. Momentary and
  non-destructive — the step counter runs untouched underneath, release
  lands back in global time. Nothing is saved.

## 2.20.2 — 2026-07-04

- **Base64: swing** — menu sets amount (50–75%, 66% = triplet) and unit
  (every 2nd tick = 16th swing at a ×4 clock, or every 2nd pair = 8th
  swing). Feed a straight clock; Base64 measures the period and delays the
  odd ticks of the broadcast. RESET re-zeros the swing phase.
- The measured clock period is broadcast to the chain
  (`LeftMessage::clockPeriod`); Mlr64 reads it instead of measuring
  privately, keeping varispeed stable under swing.
- Clock dividers now fire on the *first* tick after RESET (the downbeat)
  instead of the div-th — divided modules align to the downbeat, and even
  divisions stay straight under swing. Divided patterns may shift by one
  divided step relative to RESET in existing patches.

## 2.20.1 — 2026-07-04

- Kits (64Drums, 64Objects, 64Grains): **Row families** menu — point any row
  at any generator type (a full grid of kicks, of bells, of crackle…), with
  *Reset to one per row*. Saved with the patch; the shuffled layout permutes
  whatever the row map generates.
- All page modules: the color pickers moved out of the root context menu
  into a single **Colors** submenu.

## 2.20.0 — 2026-07-03

- **New module: 64Grains** — seeded microsound kit companion: every cell
  triggers a synthesized micro-event cloud (row picks the texture — dust,
  crackle, glitch, chirp/glisson, trainlet, bubble, hiss, rumble). Cloud
  scheduler + 96-grain pool, no sample buffer. Shares the 64Drums menu
  system: Layout, Quantize (chirp rows become glisson keyboards, trainlet
  rates tune to the key), key follow, per-cell-gated Variety (reverse,
  accelerando, sweep, glide).

## 2.19.0 — 2026-07-03

- **New module: 64Objects** — seeded modal percussion kit companion: every
  cell is a struck object (row picks it — woodblock, tine, glass, marimba,
  vibraphone, harp string, membrane, bell; column picks the size). Modal
  resonator banks with per-material damping, Karplus-Strong harp row,
  24-voice pool with self-choke, Ring menu (choke/damped/natural). Shares
  the 64Drums menu system: Layout, Quantize, key follow, per-cell-gated
  Variety (beating, rattle, flam, mute).

## 2.18.3 — 2026-07-03

- Refactor: the kit-companion shell (cell-gate inputs, seed contract,
  Layout/Quantize/Variety menus and serialization) moved from 64Drums into
  the shared `KitModule` base. No behavior change; existing patches load
  bit-identical kits.

## 2.18.2 — 2026-07-03

- 64Drums: **Layout** menu — families by row (default), shuffled (same 64
  sounds, permuted by the seed), fully random (family drawn per cell).
- 64Drums: **Quantize** menu — off / nearest scale note / columns walk the
  scale (each oscillator row becomes a playable scale run); follows Base64's
  global key by default, local Scale/Root override.
- 64Drums: **Variety** menu — five per-cell-gated synthesis extras (fold,
  FM, ring mod, resonant noise, rising pitch). Ingredients are drawn from
  the seed whether or not enabled, so toggles A/B the identical kit and old
  patches keep their exact sound.

## 2.18.1 — 2026-07-03

- CPU: the LED path (rebuild + copy) now runs at ~1.5 kHz instead of audio
  rate, inactive pages no longer run their clear loops every frame, and
  Base64 stops rewriting the chain message header and 64Pads mirror when
  nothing changed. Trigger, gate and CV outputs are untouched (still
  sample-accurate).
- Fix: Grid64/Buttons64 no longer keep a stale held-pad gate when the
  toggle/momentary switch is flipped while a pad is held.

## 2.18.0 — 2026-07-02

- **New module: 64Drums** — companion drum synth: one seeded drum voice per
  cell (row picks the family, hats on top, kicks at the bottom; column and
  jitter vary the character). Reroll draws a new kit; the seed is saved
  with the patch. Patch Rhythm64 in 1:1 for an instant generative drum
  machine. Stereo mix outputs, 16-voice pool.

## 2.17.0 — 2026-07-02

- **New module: Rhythm64** — generative rhythm engine: every pad owns a
  fixed random rhythm (seeded, rerollable, saved with the patch). Top rows
  busy, bottom rows sparse and biased onto the strong beats; hold pads or
  latch them (scene A) to mix parts. 64-cell poly trigger outputs, ready
  for 64Drums or 64Notes.

## 2.16.1 — 2026-07-02

- Global key: Base64's menu now sets a root + scale that Keys64, 64Notes and
  8Notes follow by default (new instances; existing patches keep their local
  settings). Picking a local scale or root on a follower overrides its
  follow switch. The key is saved with the patch.

## 2.16.0 — 2026-07-02

- **New module: XY64** — the grid as a single XY pad: press a target, the
  cursor glides to it in a straight line at the scene-selected slew rate
  (Sliders64 idiom). The cursor is continuous, so glides sweep smoothly
  between the 8×8 targets. X/Y CV outputs plus a TRIG that fires when the
  cursor lands — at instant speed, a trigger pad with position CV.

## 2.15.1 — 2026-07-02

- Global temp save / temp reload on top button 6: hold ~1 s to snapshot the
  state of every page in the chain (the button flashes green), tap to snap
  back. Works from 64Pads too (hold the click). The snapshot is
  session-transient and is not written into the patch.

## 2.15.0 — 2026-07-02

- **New module: 64Pads** — the Launchpad on your screen. Attach to the left
  of Base64: it mirrors the grid, scene and top-button LEDs live, and every
  control is clickable — clicks enter Base64 exactly like hardware MIDI, so
  all single-press interactions work without a Launchpad. Page select works
  as a click latch on top button 8.

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
