# Sequencer64 (titled SEQ64)

*Part of [pages64](../README.md).*

This module is a clocked CV sequencer that crosses Step64 with Sliders64:
each of the eight columns holds a value set like a slider — tap a row and the
column's value becomes that height (bottom row = 0 V, top row = 10 V, eight
levels) — and on each clock tick the playhead walks one column to the right
within the loop, sending the playing column's value to the **CV** output. The
playing column is drawn in the indicator color, so you can watch the scan.

**Control strip (scene A, momentary):** hold scene button A and the bottom
row becomes a Step64-style control strip showing the loop as a solid bar:

- **Jump:** tap a strip pad inside the loop — the playhead moves there
  immediately (the CV follows at once; the next tick advances from there).
- **Set loop range:** hold one strip pad and press another; the loop spans
  the two pads inclusive. The playhead is pulled into the new range.

Release A and the bottom row goes back to showing values — the strip is
normally hidden so all eight rows stay available as value resolution. Rows
above the strip keep editing values even while A is held.

**Slew:** by default the outputs step. The right-click **Slew** menu offers
the Sliders64 rate ladder (0.125 s to 16 s full-range): the main CV then
glides between steps — turning the sequencer into a smooth modulation
source — and each poly channel glides when you edit its value.

The module provides the **CV** output (the playing column, 0–10 V), a
**TRIG** output firing a 5 ms pulse on every step advance, and a **POLY**
output carrying all eight column values continuously — a clocked Sliders64
for free. A RESET tick returns the playhead to the loop start.

In the right-click menu you can select a **clock divider** (÷1 through ÷64),
the **slew** rate, the **display style** (full bar or single dot), and colors
for the **values**, the **step indicator**, and the **control bar**.
