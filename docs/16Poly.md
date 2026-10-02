# 16Poly (titled 16POLY)

*Part of [pages64](../README.md).*

pages64 speaks two widths of polyphony: **8 channels** for the modules with
one voice per row or column (Flin64, Sliders64, Meadow64, Mlr64's lanes,
8Notes) and **16 channels** for the cell bus, two grid rows per jack
(Buttons64, Gome64, Life64, Rhythm64, the kits). Plenty of third-party modules
pick one or the other too. 16Poly converts between the two.

It has two identical strips, side by side. In each strip:

- **Merge** (above the line): the **1–8** and **9–16** inputs become one
  16-channel **1–16** output. Each input contributes at most its first 8
  channels. When 9–16 is patched, a 1–8 input with fewer than 8 channels is
  padded with silent channels, so the second input always lands on channels
  9–16 and cell positions stay where you expect them. With only 1–8 patched,
  the output is that input, cut to 8 channels.
- **Split** (below the line): the **1–16** input comes out as channels **1–8**
  and channels **9–16**. Each output is as wide as what's there: a 10-channel
  input gives 8 + 2 channels, and anything with 8 channels or fewer leaves
  9–16 silent.

The labels name the channels a jack carries; inputs are plain text, outputs
the white badges, as on every pages64 panel. There is nothing to configure.
