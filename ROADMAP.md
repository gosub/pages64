# pages64 roadmap

Working plan as of July 2026. Versioning follows the project convention:
minor bump per new module, patch bump for fixes/refactors. Everything shipped
through 2.20.0 lives in CHANGELOG.md; this file is only what's ahead.

## Design principles (confirmed)

- **Positional page identity is intentional.** Max 8–16 pages per patch,
  arranged with a strong positional sense (monome sum style). No per-page
  colors in the page-select overlay; the two-color palette can't support it
  tastefully anyway.
- **Page switching stays a hardware gesture only.** No page-select CV input.
  (64Pads' click latch counts as the hardware gesture, mouse-shaped.)
- **Button role convention** (documented in CLAUDE.md): top round buttons 1–8
  carry static page configuration — 8 is page select, 6 is the global temp
  save/reload snapshot, 7 is reserved for the gesture recorder below; scene
  buttons A–H are interactive play only.
- **Modularity boundary**: page modules emit gates/triggers/CV; pitch mapping
  and voice allocation live in companion utility modules (64Notes, 8Notes),
  sound in companions (64Drums). Sanctioned exception: Mlr64's built-in
  sample playback. Any new exception needs an argument as strong as mlr's.
- **Companion module naming and accent** (documented in CLAUDE.md): companion
  modules reverse the name (64Notes, 8Notes, 64Pads, 64Drums) and swap the
  orange accent for the complementary blue `#22aff2`.
- **Seeded randomness is a contract** (established by Rhythm64/64Drums): any
  generative module serializes its seed, rerolls only on request, and
  *Initialize* returns the factory seed — patches always reload their music.

---

## Next milestones — new modules

One minor version bump each, design doc in `docs/design/` first. The order is
a suggestion; version numbers are assigned when work starts.

### Strum64 — the Omnichord

The left columns are **chord buttons** (scale degree × quality, following the
global key); the rest of the grid is a **strum plate**: drag a finger across
a row and the pads fire in sequence like brushing harp strings, row picking
the octave. The one idea in this list that exploits a physical gesture the
Launchpad genuinely supports (sliding across pads = clean sequential
note-ons) and that no mouse can fake. Poly pitch + gate out. Top pick for
*reach*: demoable in ten seconds, nothing like it in the Rack library.

### Kria64 — after monome's kria

Four tracks; each track's **trigger, note and duration lanes have independent
loop lengths**, so a 5-step pitch loop drifts across a 16-step trigger loop
and the music breathes polymetrically on its own. Top buttons 1–5 select the
edited lane (the sub-page convention), scenes mute tracks; four gate + degree
output pairs, pitched through 8Notes and the global key. Top pick for
*depth*: the sequencer you play for an hour, which the lineup lacks.

### Clips64 — session view for CV

Rows = four CV/gate input pairs, columns = **clip slots**: tap an empty slot
to record a clock-quantized loop of whatever's at the input, tap another to
switch loops on the next bar, scenes stop rows. A performance CV looper —
record a Sliders64 filter sweep and a Keys64 bassline, then *launch* them
like Ableton clips. Reuses the Mlr64 pattern-recorder idiom; sibling of the
gesture recorder below (that one records presses, this records signals).

### Topo64 — after Mutable Instruments' Grids

The drum-pattern topography, literally under your fingers: press a pad to
pick a point in the map, the module interpolates kick/snare/hat patterns
around it, scenes set per-part density, and a slewed cursor (XY64's glide,
reused) lets you travel the map during a build. Three trigger outs + accent.
Grids' firmware is GPL like pages64, so the map data is usable. Pairs with
64Drums' bottom rows out of the box.

### Walls64 — after Batuhan Bozkurt's Otomata

Place cells that travel in straight lines, bounce off walls and each other;
every wall hit fires that row's or column's trigger (8 + 8 mono outs or
16-ch poly). Reads as chaos, plays as counterpoint — and it's meaningfully
different from Life64: Life evolves *patterns*, Otomata evolves *voices with
trajectories*.

### 64Micro — deterministic micro-sound kit ([designed](docs/design/Micro64.md))

The fourth kit companion, Raster-Noton flavored: single designed
micro-sounds (0.2–20 ms), **bit-identical on every trigger** — the
deterministic temperament the other kits deliberately avoid (64Grains =
stochastic clouds; 64Micro = surgical events). Clicks, ticks, crush, 1-bit
data bursts, test-tone blips, zaps, pings, sub thumps, plus Fold as the
first off-grid catalog family (the shell supports > 8 families as of
2.20.3+). Sequenced varieties: alternate, ping-pong, dropout, doubler.

### Punch64 — audio punch-in FX

The audio counterpart of Rhythm64's punch-in: a whole page whose grid is a
performance effect surface for an audio signal running through it — press a
pad, the effect is in; release, it's gone. Rows = effects, columns = amount
(the punch-in grammar): distortion, bit/glitch, tempo-synced rhythm gates,
DJ-style high/low-pass sweeps, tape stop, delay, chorus/flanger, buffer
stutter… there are easily eight strong rows. Tempo-synced effects (gates,
delay, stutter) read `LeftMessage::clockPeriod`, which is the placement
argument: as a **page module** it gets the tempo and the grid for free, but
audio-through-a-page needs the **second sanctioned exception** to the
modularity boundary — it competes with Acid64 and Grain64 for that slot (its
case is strong: the grid-as-effect-surface *is* the module). The fallback is
a companion fed by a `sharedKey`-style tempo atomic, losing the grid.
Decide the exception fight before designing.

### Comet64 — the Tenori-on's random mode

Place dots; a spark travels dot-to-dot **in the order you placed them**,
firing each on arrival, travel time as rhythm. Placement order as melody
memory — the one Tenori-on mode nobody has cloned well.

## Module ideas (undesigned, quick-fire)

- **Corners64** — after monome's *corners* (grid physics, gravity pulls).
- **Turing64** — Music Thing Turing Machine: a visible shift register you
  flip bits in, lock probability on the scenes.
- **Phase64** — Reich's *Piano Phase*: one pattern, eight voices at
  micro-detuned rates; watch them drift and re-lock.
- **Snake64** — the game, as a sequencer: steer with four pads, the body is
  the pattern. Tenori-on whimsy.
- **Acid64** — a 303 line (pitch, slide, accent). Blocked on a design
  question: glide is a pitch-domain effect 8Notes can't express, so this
  either extends the companion protocol or needs the second sanctioned
  exception to the modularity boundary. Decide before designing.
- **Grain64** — granular *sampling* page module: X = buffer position,
  Y = pitch/grain size, held pads spawn grain streams, multiple fingers =
  multiple clouds. As grid-native as mlr, but sound-in-module: competes with
  Acid64 for the second sanctioned exception. Decide there first. (Synthetic
  microsound needs no exception — that's 64Grains above.)

## Global features

### Cross-page gesture recorder (reserved top button 7)

mlr-style pattern recorder, but global: record grid/scene presses across
pages with clock-relative timestamps, loop them quantized to the clock. Lives
entirely in **Base64** (it already sees every MIDI event and knows the active
page); the one protocol extension — tagging replayed events with a page index
so they reach the page they were recorded on even when inactive — has its
layout home ready in the compacted `LeftMessage` event list.

Interaction sketch: tap button 7 to arm, first press starts the loop, second
tap closes it (length quantized to clock); tap again to mute/clear
(long-press = clear).

## Polish & infrastructure backlog

- **Scene B as the punch-in convention.** Generalize Rhythm64's hold-B
  gesture (2.20.3) to the other clocked sequencer pages — Step64, Euclid64,
  Cafe64, Sequencer64: hold B, the grid becomes the time-effect selector
  (rows = effects, columns = amount), momentary and readout-only. The
  transforms are largely the same (loop, ratchet, time, density, shuffle,
  push/drag), so this wants a shared helper the way the clock divider is
  shared — and it would make B mean "punch-in" everywhere, the way A tends
  to mean latch. Design per module which rows apply.
- **Rhythm64: rhythm variations.** Per-pad-gated variations of the generated
  patterns, following the 64Drums Variety convention exactly — a menu
  submenu of toggles, every variation's parameters always drawn from the
  seed stream and only gated by the toggle (flipping one A/Bs the identical
  rhythms), each variation rolled per pad with a probability so part of the
  grid stays straight. Candidates from the brainstorm: **triplets** (a pad
  subdivides its steps in 3s — sub-tick timing via `clockPeriod`),
  **polymeters** (per-pad loop length ≠ the global bar, so parts drift and
  re-lock), **polymeasures** (patterns spanning 2×/4× the bar), **odd
  meters** (generation biased to 5- and 7-groupings).
- **Device profiles in Base64** (Launchpad MkIII / X, APC Mini): the 16-color
  `LED_COLOR_DEFS` palette is already the device-independent abstraction; a
  profile is the pad-note codec + LED encoding (newer Launchpads are RGB, so
  a 16-color → RGB lookup) + init/clear messages. Biggest audience multiplier
  available; page modules inherit it untouched. **Deferred until other grid
  hardware is actually on the desk — becomes top priority that day.**
- **64Notes: promote note parameters to the panel.** Arrangement, octave,
  intervals and chord type still hide in the right-click menu; the global key
  (2.16.1) already moved root + scale out of the critical path, which lowers
  this item's urgency. Panel controls for the rest when a panel redesign is
  due anyway.

## Hypotheticals (consider after everything else)

### 16 pages

Raise the per-patch page limit from 8 to 16: the selection overlay grows to
the top two grid rows, Base64's panel needs a second light row. If ever done,
**keep 1 V/page** on the page CV out (0–15 V) — rescaling to fit 0–10 V would
break existing patches for a cosmetic gain, and 1 V/page doubles as octave
transposition. Parked until a real patch hits the 8-page ceiling.
