# Step64 (titled STEP64)

*Part of [pages64](../README.md).*

This module is a classic 8-step sequencer. The grid is divided into one control row (top)
and seven trigger rows. Each of the eight columns represents one step.

**Trigger rows (rows 2–8):** pressing a button toggles that step on or off for that row.
On each clock tick, a 5 ms trigger pulse fires on the outputs of every active step in the
current column. Because the outputs are triggers rather than gates, consecutive active steps
each produce a distinct pulse — suitable for driving envelope generators directly.

**Control row (row 1):** displays the active loop as a solid bar of lit buttons.
- **Set loop range:** hold one button and press a second in the control row. The loop spans
  from the left button to the right button (inclusive). Both the start position and the
  length are set this way.
- **Jump to step:** tap a single button within the active loop. The sequencer will play
  that step on the next clock tick.

The module provides 7 mono trigger outputs (T1–T7), a step CV output, and a polyphonic
output carrying all 7 triggers on channels 1–7. The step CV always reflects the absolute
column position: step 1 = 0 V, step 8 = 10 V, so a loop over steps 5–8 outputs 5.71–10 V
rather than 0–10 V. This makes the CV useful as a pitch or modulation source whose range
shifts as you move the loop.

In the right-click menu you can select a **clock divider** (÷1 through ÷64), and choose
colors for the **control bar**, **active steps**, and **step indicator**.
