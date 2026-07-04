# 64Micro — design

The fourth **kit companion**: single, designed micro-sounds (0.2–20 ms),
inspired by the Raster-Noton lineage — Alva Noto's click grids and beeps,
Ryoji Ikeda's test tones and data buzzes. Decided in the July 2026
brainstorm. Naming: *64Micro* over *64Glitch* — the palette (test tones,
data bursts, sub thumps) is wider than the glitch genre; "micro" says small
sounds without claiming an aesthetic.

## Charter: determinism

The axis the other kits deliberately don't occupy. 64Drums has per-sample
noise, 64Objects has ringing physics, 64Grains free-runs its cloud timing so
every hit shimmers differently. 64Micro is the opposite temperament: **every
trigger of a cell is bit-identical, sample-exact** — machine-gun repeatable,
so sixteen in a row read as a texture *because* they are identical. That
precision is the Raster-Noton sound.

Rules that follow from the charter:

- Voice rendering never calls `random::uniform()`. Anything noise-like
  (ticks, data bursts) comes from a per-cell LFSR/xorshift seeded at kit
  generation and **re-seeded identically at every trigger**.
- The Variety extras are *sequenced*, not rolled: deterministic cycles
  (A-B-A-B), never probability.

The complement of the kit next door, stated once in both manuals:
**64Grains = stochastic clouds; 64Micro = single deterministic events.**

## Families

Default rows 0–7 top→bottom, register/weight growing downward (the Rhythm64
gradient); one extra family in the catalog beyond the grid, reachable
through Row families (the shell supports catalogs > 8 as of this design):

| Idx | Family | The sound | Reference |
|---|---|---|---|
| 0 | Click | shaped micro-transients (rect / gauss / exp windows) | Transform click grids |
| 1 | Tick | filtered noise impulses, driest static | dry static |
| 2 | Crush | bit/sample-rate artifact fragments | digital debris |
| 3 | Data | **1-bit LFSR bitstream bursts, 5–50 ms** | dataplex, test pattern |
| 4 | Blip | rect-gated sine bursts; per-cell dual-tone draw (DTMF flavor) | Ikeda test tones, Noto beeps |
| 5 | Zap | ms-scale FM / pitch-drop hits | percussive glitches |
| 6 | Ping | impulse through a high-Q resonator, cut short | tonal ticks |
| 7 | Thump | gated sub-sine pulses (20–60 ms, 40–80 Hz) | +/- kicks |
| 8 | Fold | aliased / waveshaped transients (catalog extra, off-grid) | — |

What deliberately does **not** fit: sustained sine drones and tone beds
(that's a VCO held by Buttons64/64Notes), granular washes (64Grains), and
the grid-like compositional precision (that's the gate source — Rhythm64
punch-in ratchets into a Data cell *is* the impulse train).

Quantize applies to the pitched families (blip, zap, ping, thump, and the
data burst's bit rate); columns-walk on the blip row = test-pattern
melodies.

## Variety (per-cell gated, deterministic cycles)

- **Alternate** — a cell cycles through 2–4 micro-variants, A-B-A-B (the
  signature glitch-music move)
- **Ping-pong** — hits alternate hard L/R
- **Dropout** — every Nth hit is silence: the anti-hit, a deterministic
  cycle, not a probability roll
- **Doubler** — a sample-exact second hit a few ms later

Gate-don't-skip as in every kit: parameters always drawn, toggles only gate,
per-cell probability decides which cells carry each ingredient (the *which
cells* roll happens at generation and is therefore itself deterministic).

## Engine

One-shot renderers with tiny per-voice state (phase, LFSR, filter states,
sample countdown); voices live for milliseconds, so a 16-voice pool is
generous and CPU is negligible. All per-cell recipes (frequencies,
durations, window shapes, LFSR seeds, variant tables) fixed at generation
from the `KitRng` stream, per the kit conventions.

## I/O and panel

The kit-companion shell: four poly cell-gate inputs, stereo L/R mix, blue
accent, the standard kit panel with the new title.

## Version

Next minor version when implementation starts.
