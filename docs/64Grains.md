# 64Grains (titled 64GRAINS)

*Part of [pages64](../README.md).*

A kit companion for **synthetic microsound** — Curtis Roads by way of a drum
machine. Every grid cell is a seeded recipe for a *micro-event cloud*: a
trigger spawns a handful of windowed sine or noise grains scheduled over tens
to hundreds of milliseconds. Patch any 64-cell gate source — Rhythm64,
Buttons64, Gome64, Life64, Bounce64 — into the four poly inputs 1:1.

There is no sample buffer and no granular sampling: everything is
synthesized, which is why a whole kit of it costs almost nothing.

- **Row picks the texture**, top → bottom: dust (sparse clicks), crackle
  (dense micro-pops), glitch (bit-reduced chirp fragments), chirp/glisson
  (pitch-swept sine grains), trainlet (pitched click trains — the repetition
  rate *is* the pitch), bubble (upward blips), hiss (shaped noise bursts),
  rumble (low granular rolls).
- **Column picks the register and density**; every cell carries its own
  grain count, cloud length, scatter and stereo behavior.

The seed fixes the *recipes*; the micro-timing inside each hit is
free-running, so clouds shimmer differently on every trigger while the kit
always reloads with the patch.

## Menu

The kit shares the 64Drums menu system:

- **Reroll kit** — new seed, 64 new textures; *Initialize* returns the
  factory kit.
- **Layout** — families by row / shuffled / fully random.
- **Row families** — point any row at any texture: a full grid of crackle,
  or your own top-to-bottom ordering.
- **Quantize** — off / nearest scale note / columns walk the scale, following
  Base64's global key by default. Applies to the pitched textures (glitch,
  chirp, trainlet, bubble); *columns walk the scale* turns the chirp row into
  a melodic glisson keyboard and tunes trainlet rates to musical ratios.
- **Variety** — per-cell-gated extras (part of the kit always stays clean;
  toggles A/B the identical kit):
  - **Reverse** — the cloud swells into its end instead of decaying away.
  - **Accelerando** — grain spacing tightens or relaxes across the cloud.
  - **Sweep** — the cloud pans across the stereo field grain by grain.
  - **Glide** — extra per-grain pitch trajectories on otherwise static cells.

**I/O:** four 16-channel poly cell-gate inputs (rows 1-2, 3-4, 5-6, 7-8);
stereo **L / R** mix outputs. Like all companion modules it is pure CV/audio —
no Launchpad chain, blue accent. Design rationale:
[Grains64.md](design/Grains64.md).
