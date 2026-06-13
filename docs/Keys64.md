# Keys64 (titled KEYS64)

*Part of [pages64](../README.md).*

This module turns the grid into a playable polyphonic keyboard. Each cell is a
note and pressing it plays it; pitch rises up and to the right, so the
bottom-left pad is the lowest note. Unlike Gome64 or Buttons64 — which emit
gates for a companion to pitch — Keys64 maps to pitch itself and drives a
polyphonic voice directly, no 64Notes required.

**Layout (right-click menu):** two arrangements share the plugin's scale math.

- **Scale grid** (the default) keeps everything in key: steps to the right are
  scale degrees, and each row up adds a fixed number of degrees (the **row
  degrees** setting, default 3 — a diatonic fourth), so a chord or run has the
  same shape wherever you play it.
- **Isomorphic** uses fixed chromatic intervals instead: a set number of
  semitones per column and per row (**column / row semitones**), for a
  uniform, every-note layout.

The **scale**, **root note** and **base octave** are in the menu too. Tonic
cells — every pad whose note is the root — are lit dimly so you can always find
home, whichever arrangement you use; held pads light bright.

**Octave (scene buttons A–H):** the right column picks the base octave live,
with A (top) the highest and H (bottom) the lowest; the current octave's button
is lit. Changing octave moves where new presses sound — notes already held keep
their pitch.

**Polyphony:** a voice allocator spreads the held pads across the output
channels. Set the **polyphony** (1–16 voices) and, when more pads are held than
there are voices, the **voice stealing** rule that decides which voice to take
(oldest, newest, lowest, highest, round-robin, or off to drop new notes). The
**RTRG** output fires a short trigger whenever a voice (re)starts, so an
envelope re-strikes cleanly on a stolen voice.

**Outputs:** **PITCH** (1 V/oct), **GATE** and **RTRG**, all polyphonic with as
many channels as the polyphony setting. Patch PITCH and GATE into a poly
oscillator and envelope and play. Leaving the page releases all held notes, so
nothing sticks.

In the right-click menu you also choose the **play**, **root** and **octave**
colors.
