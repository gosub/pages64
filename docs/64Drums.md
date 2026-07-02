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

**I/O:** four 16-channel poly cell-gate inputs (rows 1-2, 3-4, 5-6, 7-8);
stereo **L / R** mix outputs. Like all companion modules it is pure CV/audio —
no Launchpad chain, blue accent. Design rationale:
[Drums64.md](design/Drums64.md).
