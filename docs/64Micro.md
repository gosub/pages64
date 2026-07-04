# 64Micro (titled 64MICRO)

*Part of [pages64](../README.md).*

A kit companion for **single, designed micro-sounds** (0.2–20 ms), in the
Raster-Noton lineage — Alva Noto's click grids and beeps, Ryoji Ikeda's test
tones and data buzzes. Patch any 64-cell gate source — Rhythm64, Buttons64,
Gome64, Life64, Bounce64 — into the four poly inputs 1:1.

The charter is **determinism**: every trigger of a cell is bit-identical,
sample-exact. Noise-like sounds come from a per-cell shift register re-seeded
identically at every strike; nothing in the voice path is random. Sixteen
hits in a row read as a texture *because* they are identical — machine-gun
precision is the sound. (The kit next door is the opposite temperament:
**[64Grains](64Grains.md) is stochastic clouds; 64Micro is single
deterministic events.**)

- **Row picks the family**, top → bottom: click (shaped micro-transients),
  tick (filtered noise impulses), crush (bit/rate artifacts), data (1-bit
  bitstream bursts — the *dataplex* sound; the pitch is the bit rate), blip
  (rect-gated sine bursts, some dual-tone), zap (millisecond pitch sweeps),
  ping (impulse through a high-Q resonator), thump (gated sub-sine pulses).
- **Column picks the register**; every cell carries its own duration,
  window shape, seed and pan.
- A ninth family, **Fold** (aliased/waveshaped transients), lives off the
  default grid — point any row at it via *Row families*.

## Menu

The kit shares the 64Drums menu system: **Reroll kit** (seed saved with the
patch, *Initialize* = factory), **Layout**, **Row families** (nine families
to choose from), **Quantize** with the Base64 global key — *columns walk the
scale* on the blip row makes test-pattern melodies, and on the data row it
tunes bit rates to musical ratios.

**Variety** here is sequenced, never random — deterministic cycles in the
glitch idiom, each rolled per cell so part of the kit stays clean:

- **Alternate** — a cell cycles through 2–4 pitch variants, A-B-A-B.
- **Ping-pong** — hits alternate hard left/right.
- **Dropout** — every Nth hit is silence: the anti-hit.
- **Doubler** — a sample-exact second hit a few milliseconds later.

Tip: the compositional half of this aesthetic comes from the gate source —
Rhythm64's punch-in ratchets into a data cell *is* the impulse train.

**I/O:** four 16-channel poly cell-gate inputs (rows 1-2, 3-4, 5-6, 7-8);
stereo **L / R** mix outputs. Like all companion modules it is pure CV/audio —
no Launchpad chain, blue accent. Design rationale:
[Micro64.md](design/Micro64.md).
