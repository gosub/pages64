# Rhythm64 + 64Drums — design

The ROADMAP's DRUMS64 hypothetical bundled two separable ideas: *each button
is a randomly generated drum sound* and *each button triggers a random rhythm
while held*. The July 2026 product review resolved the open "companion or
page module?" question by splitting along the confirmed modularity boundary
(page modules emit gates/CV; sound sources are companions):

- **Rhythm64** (page module) — the rhythm engine. Needs the clock and the
  grid, emits gates in the 64-cell format, drives *anything*: 64Drums,
  64Notes, or plain drum modules.
- **64Drums** (companion) — the sound source. 64-cell gate format in, stereo
  audio out. Composable with every 64-cell emitter: Rhythm64, Buttons64,
  Gome64, Life64.

Each half is useful alone; together they are the generative drum machine.

## Rhythm64

Every pad owns a **fixed random rhythm** derived deterministically from
(seed, pad). Arm a pad and its rhythm plays on the clock; the grid becomes a
mixing desk of 64 pre-composed parts.

- **Row sets density and feel** (positional sense, monome style): top rows
  are busy (hat territory, up to ~12 hits/16), bottom rows sparse (kick
  territory, ~2–4 hits/16) with hits **biased toward strong beats** — the
  bias grows toward the bottom, so low parts land on the grid of the bar.
  Columns are siblings: same density, different placement.
- **Momentary by default**: hold a pad, its rhythm plays; release stops it.
  **Scene A toggles latch mode** (the Cafe64 idiom): taps arm/disarm pads,
  turning latch off silences everything.
- All patterns share one step counter (length 8/16/32 from the menu, default
  16), stepped by the divided clock; RESET re-zeros it.
- **Reroll** (menu) draws a new seed: 64 new rhythms. The seed is saved with
  the patch, so a patch always reloads *its* rhythms — randomness is a choice
  at reroll time, never an accident at load time.
- LEDs: armed pads lit; a pad flashes bright on its hits.
- Outputs: the 64-cell format (4 × 16-channel poly), 5 ms triggers.

## 64Drums

A drum synth kit of 64 voices, one per cell, **generated from a seed**:

- **Row picks the family** (top → bottom): click, open hat, closed hat,
  perc blip, clap, snare, tom, kick — matching Rhythm64's density gradient,
  so pairing the two 1:1 immediately sounds like a drum machine.
- **Column varies the character** inside the family: pitch, decay and tone
  drift across the row plus per-cell jitter, all deterministic from the seed.
- Synthesis is computed live but parameterized at generation: sine with
  pitch-drop envelope (kick/tom/blip), filtered noise (snare/clap/hats/click).
  A 16-voice pool with oldest-steal keeps the cost bounded.
- **Reroll kit** (menu) draws a new seed; the seed is serialized (same
  guarantee as Rhythm64: patches reload the same kit).
- Random per-cell pan gives the kit a stereo spread; outputs are a stereo
  mix pair.

## Versions

Rhythm64 ships as 2.17.0, 64Drums as 2.18.0.
