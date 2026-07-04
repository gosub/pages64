# 64Objects (titled 64OBJECTS)

*Part of [pages64](../README.md).*

A kit companion for **modal percussion**: every grid cell is a struck object,
**generated from a seed**. Patch any 64-cell gate source — Rhythm64,
Buttons64, Gome64, Life64, Bounce64 — into the four poly inputs 1:1 and every
cell trigger strikes its own object through the stereo mix. Where
[64Drums](64Drums.md) is the drum machine, 64Objects is the acoustic other
half: mallets, bells, bowls, membranes, strings.

- **Row picks the object**, top → bottom: woodblock, music-box tine, glass,
  marimba bar, vibraphone bar, harp string, membrane, bell — short and dry at
  the top, long and resonant at the bottom, matching Rhythm64's density
  gradient.
- **Column picks the size** (pitch rises across the row); every cell carries
  its own strike position, mallet hardness, damping jitter and stereo pan.

Synthesis is physical-flavored and cheap: each voice is a small bank of
damped resonators whose frequency ratios come from the object's geometry
(free bar, clamped bar, Bessel membrane, bell partials) and whose decay
profile comes from the material (wood short with treble dying first, metal
and glass ringing long and even). The harp row is a plucked Karplus-Strong
string. A 24-voice pool with **self-choke** (re-striking a cell steals its
own ring, like re-striking a real object) keeps cost bounded.

## Menu

The kit shares the 64Drums menu system:

- **Reroll kit** — new seed, 64 new objects; the seed is saved with the
  patch, *Initialize* returns the factory kit.
- **Layout** — families by row / shuffled / fully random.
- **Row families** — point any row at any object: a full grid of bells, or
  your own top-to-bottom ordering.
- **Quantize** — off / nearest scale note / columns walk the scale, following
  Base64's global key by default (local Scale / Root note override). *Columns
  walk the scale* literally builds a marimba, a vibraphone and a harp on rows
  4–6.
- **Variety** — per-cell-gated extras (part of the kit always stays clean;
  toggles A/B the identical kit):
  - **Beating** — detuned mode pairs: vibraphone shimmer, cracked bells.
  - **Rattle** — a buzz that bites on loud hits and cleans up as the ring
    decays: snare wires, bottle-cap gamelan.
  - **Flam** — a second, softer strike a few milliseconds late.
  - **Mute** — felt-damped cells: shorter, darker.
- **Ring** (64Objects only) — choke / damped / natural scales every decay
  time, taming long bell tails under dense gate sources without touching the
  kit.

**I/O:** four 16-channel poly cell-gate inputs (rows 1-2, 3-4, 5-6, 7-8);
stereo **L / R** mix outputs. Like all companion modules it is pure CV/audio —
no Launchpad chain, blue accent. Design rationale:
[Objects64.md](design/Objects64.md).
