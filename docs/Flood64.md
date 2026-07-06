# Flood64 (titled FLOOD64)

*Part of [pages64](../README.md).*

Where [Sliders64](Sliders64.md) turns each column into its own coarse 8-step
slider, Flood64 hands the **entire 8×8 grid to a single high-resolution fader**.
The cells fill in reading order — from the top-left corner toward the
bottom-right corner — so the value reads like a flood rising across the grid,
row by row.

Press any cell to set the level: the cell you press becomes the water line, and
every cell up to it lights. Each cell is one 1/64th quantum, so pressing cell
*n* floods *n* cells, and the bottom-right cell fills the whole grid (maximum).
The top-left cell is a toggle — tap it to drop the last quantum and empty the
grid to exactly zero, tap again to bring it back to 1/64. Both extremes are
therefore exact: an empty grid is the range minimum, a full grid the range
maximum. The output glides smoothly to the new target at the selected slew rate.

## Four faders in one module

A single value would be a thin use of a whole page, so Flood64 holds **four
independent faders**. The top round buttons **1–4** (CC 104–107) select which
fader is shown on the grid; the lit button shows the current one. Each fader has
its own output, and all four are also summed onto the poly output. Faders you
are not looking at keep slewing toward their targets in the background, so a slow
move you started on one fader continues while you edit another.

## Slew rate

The scene buttons on the right side of the Launchpad (A–H) select the slew rate,
exactly as in Sliders64. Button A (top) is instantaneous; B through G offer
progressively slower rates (80 V/s, 20 V/s, 10 V/s, 5 V/s, 2.5 V/s, 1.25 V/s);
button H (bottom) slews at 0.625 V/s. Because the rate is constant, a small jump
completes faster than a large one — the times above are for a full 0→10 V sweep.
The rate is shared by all four faders. The active rate is indicated by the lit
scene button. The default on load is C (20 V/s, 0.5 s full-range).

## Outputs

- **1 – 4** — the four faders, each a mono CV over the selected voltage range
  (0–10 V by default).
- **POLY** — a 4-channel polyphonic CV carrying all four faders.

## Options

In the module's right-click menu you can choose:

- **Voltage range** — the span the flood maps onto: 0–10 V (default), 0–5 V,
  0–2 V, 0–1 V, ±1 V, ±2 V, or ±5 V. An empty grid is always the low end, a full
  grid the high end.
- **Flood** color — the LED color of the filled cells (and the selected
  fader/scene indicators).
- **Selector** color — the dim color of the unselected fader buttons 1–4.
- **Fill style** — *Flood (full fill)* lights every cell up to the water line;
  *Water line (single cell)* lights only the boundary cell at the current level.
