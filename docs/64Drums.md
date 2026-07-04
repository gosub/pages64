# 64Drums (titled 64DRUMS)

*Part of [pages64](../README.md).*

A companion drum synthesizer: one drum voice per grid cell, **generated from
a seed**. Patch a 64-cell gate source — **[Rhythm64](Rhythm64.md)** for the
instant drum machine, or Buttons64, Gome64, Life64 — into the four poly
inputs 1:1 and every cell trigger plays its own drum through the stereo mix.

The kit has the same positional shape as Rhythm64's rhythms:

- **Row picks the family**, top → bottom: click, open hat, closed hat, perc
  blip, clap, snare, tom, kick. Pair it with Rhythm64 and the busy top rows
  play hats while the sparse bottom rows play kicks — a drum machine with no
  configuration at all.
- **Column varies the character** within the family: pitch rises across the
  row, decay and tone drift, and every cell carries its own deterministic
  jitter and a gentle random stereo pan.

Synthesis is a compact recipe per voice — sine with a pitch-drop envelope for
the drums and blips, filtered noise for hats, clap and click — with a
16-voice pool (quietest-steal) keeping the cost bounded no matter how dense
the input gets.

**Reroll kit (right-click menu):** draws a new seed for 64 new sounds. The
seed is saved with the patch, so it always reloads *its* kit; *Initialize*
returns the factory kit. Reroll 64Drums and Rhythm64 independently — new
sounds on old rhythms, or new rhythms on a kit you like.

**Layout (right-click menu):** how the 64 sounds are placed on the grid.

- **Families by row** (default) — the positional pairing with Rhythm64
  described above.
- **Shuffled** — the exact same 64 sounds, permuted by the seed. Every family
  still appears eight times, just scattered; the natural choice for gate
  sources where position is arbitrary (Life64, Gome64, Bounce64).
- **Fully random** — each cell draws its family independently. Kits come out
  lopsided (a wall of kicks, two claps) — more surprising, occasionally great.

**Row families (right-click menu):** point any row at any family — eight
rows of kicks for a full grid of bass drums, or your own top-to-bottom
ordering. Each row's submenu picks its generator type; *Reset to one per
row* restores the default. The shuffled layout permutes whatever the row map
generates; the fully random layout ignores it.

**Quantize (right-click menu):** tunes the oscillator families (kick, tom,
snare, blip, click — hats and clap are pure noise and unaffected). The
*landing* pitch is quantized: the pitch-drop envelope transposes with it, so
the note the ear hears is the one the sweep settles on.

- **Off** (default) — free frequencies with per-cell jitter, deliberately drummy.
- **Nearest scale note** — each cell's frequency snaps to the closest note of
  the scale; the kit keeps its character but sits in key.
- **Columns walk the scale** — in the *Families by row* layout each row
  becomes a playable melody: columns 1–8 step through consecutive scale
  degrees from the family's register, jitter-free. In the other layouts this
  behaves like *Nearest scale note* (the generation column isn't visible
  there).

The key follows **Base64's global key** by default (*Follow Base64 global
key*); picking a local **Scale** or **Root note** overrides it, like the
other pitched modules.

**Variety (right-click menu):** five switchable synthesis extras, each rolled
per cell — every cell has its own chance of carrying each ingredient, so part
of the kit always stays clean:

- **Fold** — phase-distortion brightening, from warm drive to buzzy.
- **FM** — an enveloped modulator: metallic percs, cowbells, DX toms.
- **Ring mod** — enveloped sidebands, growly attacks.
- **Resonant noise** — a ringing bandpass on the noise: zaps and lasers.
- **Rising pitch** — the sweep runs upward instead of down: whoops.

The recipe for every ingredient is drawn from the seed whether or not it is
enabled — the toggles only gate what plays. Flipping one on and off A/Bs the
identical kit, and old patches keep their exact sound with everything off
(the default). *All on / All off* shortcuts sit at the top of the submenu.

**I/O:** four 16-channel poly cell-gate inputs (rows 1-2, 3-4, 5-6, 7-8);
stereo **L / R** mix outputs. Like all companion modules it is pure CV/audio —
no Launchpad chain, blue accent. Design rationale:
[Drums64.md](design/Drums64.md).
