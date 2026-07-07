# pages64 roadmap

Working plan as of July 2026. Versioning follows the project convention:
minor bump per new module, patch bump for fixes/refactors. Everything shipped
through 2.21.0 lives in CHANGELOG.md; this file is only what's ahead.

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

### MetaSurface64 (META64) — a four-corner morph pad

A cross of XY64 and Sliders64. The four **corners** of the grid each store an
8-value vector, and an XY cursor (XY64's, with its glide) bilinearly
interpolates between them: moving the cursor sweeps eight CV outputs through a
continuous blend of the four corner presets — vector-synthesis / Kaoss-style
morphing under the fingers. Top round buttons **1–4** select a corner to edit;
while editing, the columns become eight sliders (Sliders64's column-editing
idiom) setting that corner's vector. Menu options **reroll** random vectors —
each corner or all at once — or **nudge** them by small steps, following the
seeded-randomness contract. Eight CV outs (poly or mono), optionally the cursor
X/Y as well. Squarely inside the modularity boundary (a page emitting CV, like
Sliders64 and XY64) and genuinely new to the lineup: nothing here plays a 2-D
blend of multi-dimensional presets. Reuses XY64's cursor + slew and Sliders64's
column editor almost wholesale.

### Random64 (RND64) — a bank of 64 dice rolls

Every pad holds a **16-channel random vector**; press a pad and those sixteen
values go to the poly output, so the grid is a palette of 64 modulation
snapshots you punch between. No sliding — just a few-millisecond interpolation
on each switch to kill the zipper click. Menu **rerolls** all 64 vectors or
**nudges** them all by small steps (seeded contract: the bank reloads with the
patch, *Initialize* = the factory roll). Right-click sets each of the 16
channels **unipolar or bipolar** independently. A sample-and-hold palette with
memory: audition random states, keep a grid of the good ones, and perform by
jumping between them. Pairs with anything hungry for modulation; a CV-out page
module, in-bounds.

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

### Deep state slots — the Elektron pattern bank (undesigned)

What ships today (button 6, `docs/Base64.md`) is the Elektron *temp save*:
**one** whole-chain snapshot, held in memory only, restored the instant you
tap, gone when the patch closes. That's the scratch-pad half of the Elektron
idea. The deep half is missing: a **bank of persistent snapshot slots** you
save and recall like patterns on an Analog/Digitakt.

The mechanism reuses machinery that already exists. Each slot is the same
whole-chain JSON `PageModule::handleCommand`/`dataToJson` already produces for
button 6 (every page's state plus the active page index); the only new pieces
are *N* of them, *persisted in the patch*, plus a selector surface and recall
timing. Lives in **Base64**, like the gesture recorder, because Base64 already
brokers the whole-chain snapshot broadcast and owns the clock.

Design questions to settle:

- **Where the slots live.** A scene-button bank (A–H = eight slots, the most
  Elektron-like), or a dedicated slot page, or button 6 held + a grid row.
  Scenes are "interactive play," which fits recall; saving wants a distinct
  gesture (hold vs tap, as button 6 already distinguishes).
- **Persisted, not transient.** Unlike the temp snapshot, deep slots serialize
  with the patch, so a saved instrument reopens with its whole pattern bank.
  Decide whether button 6's one-deep scratch stays separate (recommended: keep
  6 as the quick throwaway, the bank as the persistent store) so they don't
  overload each other.
- **Clock-quantized recall — the actual new capability.** Today's reload is
  immediate; the arrangement move is **next-bar (or next-N-beats) quantized**
  recall off the clock Base64 already broadcasts, so switching whole-chain
  states lands on the grid. Offer immediate recall too (hold = now, tap =
  queued, or a menu setting).
- **Chaining / song mode.** Queue a sequence of slots that advance on the
  clock — the Elektron chain. This is where it stops being a snapshot toy and
  becomes an arranger.
- **Scope of a slot.** Simplest and matching temp save: a slot captures every
  page's full state. A later refinement could let a slot capture a subset
  (only some pages), but full-chain first.
- **Synergy with the gesture recorder.** Both are Base64-hosted whole-chain
  mechanisms; snapshots capture *state*, the recorder captures *gestures*. A
  slot could eventually bundle a gesture loop, so recalling a pattern also
  arms its performance — design them aware of each other.

### Arpeggiation as a cross-module mechanism (undesigned)

Today only **Keys64** arpeggiates (ten modes: up, down, up-down, down-up,
converge, diverge, as-played, alternating-root, random, random-no-repeat) and
**Gome64** does its spatial-pattern variant. The idea: make arpeggiation
available to *any* module that emits gates — Buttons64, Grid64, Life64,
Bounce64, Rhythm64, the sequencers — instead of re-implementing it per module.
Placement is the open question, and the options trade off differently:

- **Aux insert module** (a 64-cell gate → 64-cell gate transformer sitting
  between a page module and its consumer). Most modular, matches the "insert
  in the cable run" idiom, works with everything. This is the leading shape.
- **A page module** — awkward: page modules read the grid and *emit*, they
  don't take a gate bus *in*, so an arp page breaks the chain idiom.
- **A per-module sub-page/mode** (the Keys64 approach generalized) — no new
  module, but duplicates the arp engine everywhere and can't arp modules that
  have no menu room. Rejected as the primary path; fine as opt-in polish.
- **Merge the poly outs and feed an existing arp** — simplest, but as noted it
  throws away per-cell identity *and* the shared scale, so the arp can only
  order by voltage, not by musical degree.

**The crux — arpeggiation is pitch-ordered, but the 64-cell bus is pitchless
gates.** "Up-down" needs to know each active cell's pitch, which for a gate bus
only exists once 64Notes maps cells→degrees downstream. So a universal gate arp
must pick one of two characters:

1. **Geometric arp** (order by cell index / row / column, not pitch). Needs no
   pitch at all, so it's genuinely universal and cheap, and it composes with
   *any* gate source — but "up-down" means grid-up-down, not pitch-up-down.
2. **Scale-aware arp** — reads the global key the way the kits and 64Notes
   already do (`P64::sharedKey` atomics, no chain needed), adopts the same
   cell→degree convention, and orders by *musical* pitch. This is what
   dissolves the user's worry about "losing the shared scale info": an aux
   module **can** stay scale-aware without being wired to Base64, exactly like
   64Drums/64Objects follow the key today.

Likely answer: an aux insert that offers both — geometric ordering for free,
plus a scale-aware mode that reuses Keys64's ten-mode vocabulary and the shared
key. Clock sync comes from the existing `clockTick`/`clockPeriod` broadcast (or
a `sharedKey`-style tempo atomic if it lives off the chain). Design the
pitch-vs-geometry decision before anything else.

### Per-cell expression bus — grid control for the companions (undesigned)

The companion split (page emits gates, companion makes sound, configured by
menu) files one class of parameter on the wrong side. Pitch is a *mapping*:
set-and-forget, the menu is fine. But **velocity, per-cell tuning, per-cell
decay** are *performance-time expression* — they want to be under the fingers,
and today they live in the mouse-only right-click menu. There is no way to,
say, lift the velocity of a single 64Drums cell from the grid, because a
companion is off-chain and owns no page. The same ache would hit any future
arp aux the moment you want per-region control.

The wrong fix is to make the companion a page: that collapses the split *and*
spends the second sanctioned exception (sound-in-a-page, the Mlr64 boundary)
on every kit forever, and a kit-as-page is a strange instrument (it sounds but
its page is a parameter sheet, not something you play).

The proposed fix keeps the split and notices that **per-cell expression is
itself a grid instrument**, so it gets its own page that emits a 64-cell CV
bus the companion reads as modulation — the 64Notes pattern applied to
velocity instead of pitch:

```
Rhythm64 ──gate poly──▶ 64Drums
Paint64  ──vel  poly──▶ 64Drums   (new: a per-cell value surface)
```

A page module (working name **Paint64**) whose grid is a 64-cell value
surface: cell brightness shows the value, and you paint per-cell. It outputs
the values as a 64-cell (4×16) poly CV bus in the existing format, cell-aligned
1:1 with the gate bus. Companions grow **optional modulation inputs** (velocity,
tune, decay…) that, when patched, scale/offset the per-cell recipe by cell N's
painted value. The 64-cell format stops being only a gate bus and becomes a
thin stack — gate, velocity, one aux-mod lane — and each companion reads the
lanes it cares about.

Why this shape:

- **No boundary spent.** Page emits CV, companion consumes CV: the sanctioned
  64Notes split, unchanged. No exception needed.
- **Reusable.** One painter drives velocity on 64Drums, register on 64Objects,
  cloud-density on 64Grains, gate-length on the arp aux. Every companion
  benefits from one module.
- **Grid-native editing**, which is the whole point.
- **Composes.** Two expression lanes = two painters, or one page with a lane
  selector on the top buttons (the sub-page convention).

Open design questions:

- **Editing resolution.** A grid cell shows only a handful of distinguishable
  brightness levels, so painting a continuous value *as brightness* is coarse.
  Answer with a gesture that already has precedent: hold a cell and its column
  becomes an 8-step fader (the Sliders64 / Sequencer64 idiom), or tap-to-cycle
  levels for quick work. Display stays a coarse heatmap; entry stays precise.
- **Which lanes, and the input contract.** Settle the standard modulation lanes
  and how a companion advertises which it accepts (a menu toggle per input, or
  just "patched = active"). Velocity first; it's the sharpest case and the one
  the user actually reached for.
- **Relation to the arp aux.** These two explorations are the same unsolved
  question wearing two hats: how an off-grid companion gets grid-native,
  per-cell control. The expression bus is the proposed answer to both — design
  them together, and grow the modulation inputs on 64Drums / 64Objects /
  64Grains as part of this work.

## Polish & infrastructure backlog

- **Flood64: slower slew option (undecided).** The 8 scene-button slew rates
  currently top out at H = 16 s full-range. A slower setting (≈32 s) was
  requested, but the eight buttons are full and A (instant) vs B (0.125 s) are
  audibly distinct, so nothing is an obvious cut. Decide the trade before
  touching it: bump H to 32 s (drop 16 s, keep eight buttons), drop a fast rate
  to append 32 s, or rescale the whole table geometrically to span instant…32 s
  in eight steps.
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
