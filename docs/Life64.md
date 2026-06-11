# Life64 (titled LIFE64)

*Part of [pages64](../README.md).*

This module is Conway's Game of Life on the grid. Each clock tick steps one
generation of the classic B3/S23 rules; every live cell is a lit pad and a
high gate. Press a pad to toggle a cell at any time — running or frozen — so
you seed and perturb the colony live. Edges are bounded by default; a
right-click option (**Wrap edges**) joins them into a torus, where gliders
and spaceships orbit forever instead of dying on the wall. The colony keeps
evolving while another page is active.

**Scene buttons (top to bottom):**

- **A — Freeze:** stop the clock; the frame holds still and stays editable.
  Press again to resume. Lit while frozen.
- **B — Clear:** all cells die.
- **C — Randomize:** reroll the frame; each cell comes alive with the
  probability set in the right-click menu (10/20/30 %, default 20 %).
- **D — Loop:** tap to toggle frame looping — the start frame is restored
  every N ticks, so the colony replays the same N-frame evolution forever.
  Hold D and the grid becomes the length selector (pads 1–64, row-major; tap
  to set N, default 16). The loop start is re-captured by enabling the loop,
  unfreezing, randomizing, recalling memory, loading from the library, and by
  drawing onto a cleared frame before the next tick — whatever you just did
  is the new material, the loop replays it.
- **E — Recall:** redraw the memorized frame. The memory starts out holding a
  glider.
- **F — Save:** store the current frame to memory (the LED flashes to
  confirm). Saved with the patch.
- **G — Library:** opens a pattern browser on the grid — each lit pad is a
  famous Life pattern, one family per row: still lifes (block, beehive, loaf,
  pond, boat, ship, tub, barge), oscillators (blinker, toad, beacon, clock,
  jam, mazing, octagon 2, fumarole), spaceships (glider, LWSS, MWSS, HWSS)
  and methuselahs (R-pentomino, pi-heptomino, acorn, diehard). Tap a pad to
  load the pattern centered; tap G again to exit without loading. The
  simulation keeps running underneath, so browsing never interrupts the
  output. Spaceships are best with wrap on.

**Outputs:** the four **CELLS** jacks carry the 64 cells as sustained 10 V
gates in the Gome64/Buttons64 format (4 × 16-channel poly) — patch them into
64Notes for pitch (its *fixed time* note length articulates births; *gate
follow* sustains lives). **ROWS** and **COLS** are 8-channel poly outputs
where each row (and column) is read as an 8-bit binary number, MSB at the
left (top), scaled to 0–10 V — chaotic but correlated modulation that dances
with the colony. **DENS** is the percentage of live cells as 0–10 V.

In the right-click menu you can select a **clock divider** (÷1 through ÷64),
toggle **wrap edges (torus)**, set the **randomize density**, switch the
**binary decode** of ROWS/COLS between classic and Gray code (Gray turns
small colony changes into smaller CV moves), and choose the **cell color**
and **UI color** (used by the loop-length selector and the library page).
A RESET tick restarts the loop when looping; it never clears the colony.
