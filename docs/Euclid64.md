# Euclid64 (titled EUCL64)

*Part of [pages64](../README.md).*

This module is a euclidean rhythm sequencer: each of the eight columns is an
independent voice playing E(fill, length), the euclidean distribution of *fill*
onsets over *length* steps. The column displays its pattern as a bar growing
from the bottom (step 1 = bottom row): onsets are lit, rests are dim, and the
step currently playing is highlighted on every clock tick.

**Interaction:**

- **Set fill:** tap a row; the height of the tap (bottom = 1, top = 8) becomes
  the number of onsets. When the playhead crosses the current fill height it
  flashes in the fill color, marking the pad that clears the voice when tapped.
  Tapping above the current length grows the length to match.
- **Set length and fill together:** hold one pad and press another in the same
  column — the higher pad sets the length, the lower one the fill. One gesture
  programs the whole voice.
- **Mute:** scene buttons A–H mute/unmute voices 1–8. A muted voice keeps
  stepping (the highlight turns the mute color) so it re-enters in phase.

Patterns keep running while another page is active, so Euclid64 works as the
rhythmic backbone of a multi-page instrument. The module provides 8 mono
trigger outputs (T1–T8) and a polyphonic output carrying all 8 triggers.

In the right-click menu you can select a **clock divider** (÷1 through ÷64) and
colors for **onsets**, **rests**, the **step indicator**, the **fill marker**,
and **mutes**.
