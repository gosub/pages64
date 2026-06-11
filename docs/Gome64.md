# Gome64 (titled GOME64)

*Part of [pages64](../README.md).*

This module is a two-dimensional pattern arpeggiator inspired by [gome](https://monome.org/docs/grid/app/sum/#gome) from the monome sum collection (itself a descendant of stretta's polygome). Where Cafe64 plays a fixed rhythm per column, Gome64 plays a *pattern shape*: an ordered sequence of grid-cell offsets relative to a root.

**Playing:** Press a grid cell in rows 2–8 to set a *root*. On each clock tick the current pattern walks one step, firing the cell at `root + offset`. Hold several cells at once to run several arpeggios in parallel from different roots. Because each button is meant to be a note, Gome64 itself only emits gates — it pairs with a note-mapping companion that turns the 64 cell gates into pitches (any module reading the 4 × 16-channel gate format works, including Buttons64). Held roots are shown dimly; the cell currently firing flashes brightly.

**Pattern select:** The top row of the grid (8 pads) is a selector strip for the eight patterns (a radio group); the lower seven rows are the playing field. The selected pattern is used by all running arpeggios, and the active pattern's pad is highlighted.

**Loop mode (scene A):** Toggles between momentary play (arpeggio runs while the cell is held) and latched play (tap to start, tap again to stop) for hands-free, sustained arpeggios — the same idiom as Cafe64's latch.

**Record mode (scene B):** Arm record, then tap grid cells in order to capture a new pattern into the selected slot. The first tap is the root (offset 0,0); each later tap stores its position relative to that root. Disarm to finish. Eight built-in patterns are provided on load.

**Off-grid behavior:** When `root + offset` lands outside the 8×8 grid, the right-click menu lets you choose **Skip** (rest that step — the default), **Wrap** (cycle around the edges), or **Clamp** (pin to the nearest edge cell).

The module provides four 16-channel polyphonic gate outputs (rows 1-2, 3-4, 5-6, 7-8), one gate channel per grid cell, matching Buttons64's layout. A clock and reset (from Base64) drive and re-zero the walkers. The right-click menu also offers a **clock divider** (÷1–÷64) and colors for the **root**, **fire**, **record**, **active pattern**, and **inactive pattern** indicators.
