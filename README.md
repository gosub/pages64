# pages64

**pages64** is a modular plugin for VCV Rack designed to transform a *Novation
Launchpad Mini MkII* into an expressive musical instrument. It consists of a
**Base64** MIDI interface module for the Launchpad, to which other modules can be
attached to create your own instrument. Each module corresponds to a page, which
can be selected directly from the Launchpad grid. Each page module aspires to
turn the grid into a different musical instrument and a source of expression.

## Current Modules

### Base64 (titled BASE64)

This is the central module. Once loaded, you can select the Launchpad MIDI
interface and attach the "page" modules to its right. The LEDs at the top
indicate the number of modules connected and the currently active module (green)
and inactive ones (yellow). The output jacks at the bottom provide a CV signal
for the currently active page (0V for the first page, 1V for the second, and so
on) and a trigger signal when a page is changed.

To switch to a different page, keep pressed the rightmost button in the top round
button row of the Launchpad (labeled 8), and press a button from the top row of
the grid; each lighted button is a page.

### Buttons64 (titled BTTN64)

This module is divided into four sections. Each section corresponds to two
horizontal rows of grid buttons and has its own toggle switch to select whether
the buttons in that section have a momentary action or behave as on/off
toggles. Each output jack consists of a 16-channel polyphonic signal which
carries the state of the buttons in the corresponding rows, and can be split
with the **SPLIT** module available in *VCV Fundamental*. In the module's
right-click menu, you can choose the LED color for each pair of rows.

### Grid64 (titled GRID64)

This module maps all 64 grid buttons to individual mono gate outputs, arranged
in an 8×8 grid on the panel. A single toggle switch selects whether all buttons
are momentary or on/off toggles. Switching from momentary to toggle clears any
active state so no phantom presses carry over. In the module's right-click menu,
you can choose the LED color for the active buttons.

### Sliders64 (titled SLDR64)

This module turns each of the eight Launchpad columns into a slewed CV slider with
a 0–10V output. Pressing a grid button sets the target level for that column: the
button's row position maps linearly to voltage (bottom row = 0 V, top row = 10 V).
The output glides smoothly to the new target at the selected slew rate.

The scene buttons on the right side of the Launchpad (A–H) select the slew rate.
Button A (top) is instantaneous; B through G offer progressively slower rates
(80 V/s, 20 V/s, 10 V/s, 5 V/s, 2.5 V/s, 1.25 V/s); button H (bottom) slews at
0.625 V/s. Because the rate is constant, a small jump completes faster than a large
one — the times above are for a full 0→10 V sweep. The active rate is indicated by
the lit scene button. The default on load is C (20 V/s, 0.5 s full-range).

In the module's right-click menu you can choose the LED color and the display style
(full bar from the bottom, or a single dot at the current position).

### Flin64 (titled FLIN64)

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

### Step64 (titled STEP64)

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

The module provides 7 mono trigger outputs (T1–T7), a step CV output (0–10 V mapped across
the active loop), and a polyphonic output carrying all 7 triggers on channels 1–7.

In the right-click menu you can select a **clock divider** (÷1 through ÷64), and choose
colors for the **control bar**, **active steps**, and **step indicator**.

## Sources of inspiration

- [Monome grid](https://monome.org/docs/grid/)
- Yamaha's [Tenori-on](https://en.wikipedia.org/wiki/Yamaha_Tenori-on)
- the [Controllerism](https://www.controllerism.com/) movement
- [forsitan modulare](https://github.com/gosub/forsitan) (my other plugin)

## Author

Giampaolo Guiducci <giampaolo.guiducci@gmail.com>

## License

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
