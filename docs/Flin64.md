# Flin64 (titled FLIN64)

*Part of [pages64](../README.md).*

This module is a cyclic polyrhythm sequencer, inspired by the *flin* instrument from the
[monome\_sum](https://github.com/monome/monome_sum) collection. Each of the eight columns
is an independent voice. A ray of lit LEDs cascades downward through the column and
wraps around in a continuous loop; the gate output for that column fires at the moment the
ray's head appears at the top, and stays open until the tail exits the top border.

The eight rows select the speed of the ray. Row A (top) is the fastest; row H (bottom)
is the slowest. The speed ratios are 1 : 2 : 3 : 4 : 5 : 6 : 7 — a natural harmonic
series that produces interlocking polyrhythmic patterns when multiple columns run together.

**Interaction:**

- **Start a column:** press and release a speed row button (rows A–G) while the column is
  off. The gate fires immediately on the first loop. The ray length defaults to 1.
- **Stop a column:** press and release row H while the column is on. The length resets to 1.
- **Change speed:** press and release a different speed row while the column is on. The
  phase continues uninterrupted; only the speed changes.
- **Set ray length (1–7 rows):** hold one row and press a second row in the same column.
  The distance between the two rows sets the length. This works in both on and off states.
  When used while off, the length is remembered and used for the next start.

All single-button actions are resolved on release, so the sequencer can always distinguish
a tap from the first press of a two-button length gesture.

The module provides 8 mono gate outputs (G1–G8) and a polyphonic output carrying all 8
gates on channels 1–8.

In the right-click menu you can select a **clock divider** (÷1 through ÷64), choose the
**ray color** and an optional **background color** that dims active columns.
