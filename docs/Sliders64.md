# Sliders64 (titled SLDR64)

*Part of [pages64](../README.md).*

This module turns each of the eight Launchpad columns into a slewed CV slider.
Pressing a grid button sets the target level for that column: the button's row
position maps to the output over the selected voltage range (bottom row = range
minimum, top row = range maximum; 0–10 V by default). The output glides smoothly
to the new target at the selected slew rate.

The scene buttons on the right side of the Launchpad (A–H) select the slew rate.
Button A (top) is instantaneous; B through G offer progressively slower rates
(80 V/s, 20 V/s, 10 V/s, 5 V/s, 2.5 V/s, 1.25 V/s); button H (bottom) slews at
0.625 V/s. Because the rate is constant, a small jump completes faster than a large
one — the times above are for a full 0→10 V sweep. The active rate is indicated by
the lit scene button. The default on load is C (20 V/s, 0.5 s full-range).

In the module's right-click menu you can choose the output **voltage range**
(0–10 V, 0–5 V, 0–2 V, 0–1 V, ±1 V, ±2 V, or ±5 V), the LED color, and the
display style (full bar from the bottom, or a single dot at the current position).
