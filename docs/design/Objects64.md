# 64Objects — design

A second **kit companion**: modal percussion — struck objects of different
materials and shapes — behind the same 64-cell gate bus as 64Drums. The July
2026 brainstorm settled the placement question: the gate format is an
*impulse bus* ("strike now, here"), so anything impulse-excited composes with
every emitter (Rhythm64, Buttons64, Gome64, Life64, Bounce64) and belongs in
a companion with the 64Drums shell. Swap the kit module and the whole patch
changes instrument — that composability, not any single sound, is the
product.

64Drums stays what it is: the cheap, punchy drum machine. 64Objects is the
acoustic other half — mallets, bells, bowls, membranes, strings — at roughly
3–4× the per-voice cost, which is exactly why it is not a sixth Variety
toggle inside 64Drums.

## Decision: Karplus-Strong lives inside, as families

A plucked/struck string is impulse-excited like everything else here, and
Rings proved modal + string belong in one box. So 64Objects gets one or two
**string rows** implemented as Karplus-Strong (delay line + loop damping)
rather than a separate 64Strings module. Split-out criteria, should it ever
earn depth: coupled/sympathetic strings, per-string preparation, or a
dedicated pairing with Strum64's strum plate. Until then, one module.

## The kit shell (refactor before building)

64Drums grew reusable infrastructure that 64Objects must not re-implement.
Extract into a shared header (`KitModule.hpp` or `P64::` helpers) as a pure
refactor of 64Drums first — patch bump, zero behavior change, existing
patches bit-identical:

- 4 × 16-ch poly cell-gate inputs, edge detection, `prevGate[64]`
- seed + Reroll menu item + serialization (the seeded-randomness contract)
- **Layout** menu (families by row / shuffled / fully random) including the
  seed-derived Fisher–Yates shuffle
- **Quantize** menu (off / nearest scale note / columns walk the scale),
  `quantizeFreq`, the walk fallback outside family layout, and the global-key
  follow + local Scale/Root override idiom
- the **Variety** idiom: bitmask + gated per-cell recipe (always drawn from
  the RNG stream, toggles only gate application — flipping one A/Bs the
  identical kit) + the checkbox submenu with All on/off
- stereo mix output with clamp; per-cell pan draw

Each kit then supplies: its family table, its per-cell recipe draws, its
voice struct and render loop, and its own Variety ingredient list.

## Engine

A voice is a small bank of **damped resonators** (up to 8 modes), each an
exponentially decaying sinusoid implemented as a complex phasor rotation
(4 multiplies/mode/sample, unconditionally stable, exact decay). Rotation and
decay coefficients per mode are precomputed at kit generation (recomputed on
sample-rate change).

- **Body (geometry) = mode frequency ratios.** Physical constants, free
  variety:
  - free bar (marimba, vibraphone): 1, 2.756, 5.404, 8.933
  - clamped bar (music-box tine): 1, 6.267, 17.547
  - circular membrane (Bessel): 1, 1.594, 2.136, 2.296, 2.653, 2.918
  - bell partials: 0.5, 1, 1.2, 1.5, 2, 2.5, 2.67
  - glass/bowl: sparse near-pure set (empirically tuned)
  - plate/gong: dense inharmonic spray drawn from the seed
- **Material = damping law**: per-mode decay `T(f) = T0 · (f0/f)^γ`. Wood is
  short with treble dying fastest (γ high), metal rings long and even
  (γ low), glass long and pure with few modes, clay/rubber thuddy.
- **Exciter = two free per-cell jitter dimensions**: strike *hardness* (a
  soft mallet's wider pulse rolls off high modes — a lowpass over mode gains)
  and strike *position* (mode gains scaled by `sin(n·π·pos)`). Both fold into
  the initial phasor amplitudes, so excitation costs nothing per sample.
- **String rows** are Karplus-Strong: delay = `fs/f0`, one-pole loop lowpass
  as the material knob (nylon dull, steel bright), noise-burst length and a
  pluck-position comb as the per-cell character draws.

## Families (row → object, top → bottom)

Same grammar as 64Drums — register and decay grow toward the bottom, matching
Rhythm64's density gradient. Column = size (base pitch), per-cell jitter on
strike position, hardness, damping. Exact table tunable during
implementation:

| Row | Object | Engine |
|---|---|---|
| 0 | woodblock / claves | modal, wood, driest |
| 1 | music-box tine | modal, clamped bar |
| 2 | glass / bowl | modal, glass |
| 3 | marimba bar | modal, wood free bar |
| 4 | vibraphone bar | modal, metal free bar |
| 5 | harp / pluck | Karplus-Strong |
| 6 | membrane (tom / tabla) | modal, Bessel |
| 7 | bell | modal, bell partials, longest |

Quantize is even more at home here than in 64Drums — *columns walk the scale*
on rows 3–5 literally builds a marimba, a vibraphone and a harp. The landing
pitch rule carries over unchanged (membranes may add a tabla-style pitch
bend later; not in v1).

## Voice pool and ring time

The one real design problem: a bell rings for seconds, so 16 voices with
quietest-steal would choke on a busy Life64 feed.

- Pool of **24 voices**; steal quietest by summed mode energy.
- **Self-choke**: retriggering a cell steals that cell's own ringing voice
  first — cheaper, and physically what re-striking an object does.
- A **Ring** menu (choke / damped / natural) scales all decay times, for
  taming dense sources without touching the kit.

Budget: 24 voices × 8 modes × 4 mul ≈ 800 ops/sample worst case — heavier
than 64Drums, comfortably light for Rack.

## Variety ingredients (per-cell gated, kit-specific list)

- **Beating** — detuned mode pairs (vibraphone shimmer, cracked bell)
- **Rattle** — a nonlinearity that buzzes above a threshold (snare wires,
  bottle-cap gamelan)
- **Flam** — a second, softer strike a few ms late
- **Mute** — felt-damped variant (extra damping, darker gains)

## I/O and panel

Identical to 64Drums: four poly cell-gate inputs, stereo L/R mix, blue
companion accent, no Launchpad chain. The panel is the 64Drums layout with
the new title.

## Versions

Kit-shell refactor of 64Drums first (patch bump), then 64Objects as the next
minor version when implementation starts.
