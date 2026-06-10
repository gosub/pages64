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

Across all page modules the Launchpad's extra buttons follow one convention: the
**top round buttons (1–8)** carry static page configuration (button 8 is always
page select; 6 and 7 are reserved for future global features), while the **scene
buttons (A–H)** on the right are for interactive play — latch modes, mute groups
and the like.

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

The module provides 7 mono trigger outputs (T1–T7), a step CV output, and a polyphonic
output carrying all 7 triggers on channels 1–7. The step CV always reflects the absolute
column position: step 1 = 0 V, step 8 = 10 V, so a loop over steps 5–8 outputs 5.71–10 V
rather than 0–10 V. This makes the CV useful as a pitch or modulation source whose range
shifts as you move the loop.

In the right-click menu you can select a **clock divider** (÷1 through ÷64), and choose
colors for the **control bar**, **active steps**, and **step indicator**.

### Cafe64 (titled CAFE64)

This module is a polyrhythm performance sequencer inspired by [Press Cafe](https://llllllll.co/t/press-cafe) by stretta. It has eight rhythm patterns and eight independent trigger voices. Three sub-pages are selected with the first three top round buttons of the Launchpad:

**Page 1 — Play:** Each column is a voice, each row selects a rhythm pattern. Hold a button to arm that voice: it waits for the next clock tick (for tight sync) then starts playing the chosen pattern. Release the button to stop. While a voice is playing, pressing a different row in the same column switches it to a new pattern immediately. A scrolling display shows the rhythm falling downward in sync with the clock; the bottom row always shows the most recently fired step.

**Page 2 — Rhythm editor:** Each column shows one rhythm pattern. The bottom button is step 1, the top button is step 8. Press a button to toggle that step on or off.

**Page 3 — Length editor:** Each column shows the length of its rhythm as a filled bar from the bottom. Press a button to set the length to that row's height (bottom = 1 step, top = 8 steps).

On each clock tick, any active voice checks whether the current step of its pattern is on — if so, it fires a 5 ms trigger on the corresponding output. The pattern loops: if the length is 5, steps play 1 2 3 4 5 1 2 3 4 5 … The three sub-page selector buttons are lit to show which page is active.

**Latch mode:** pressing scene button A switches from momentary (hold) to latch play. In latch mode, a tap arms a column; the pattern keeps playing after you release, even while you navigate to other pages. Tapping the same row again stops that column; tapping a different row changes the pattern immediately. Pressing A again turns latch mode off and stops all voices. The active column's selected row is always shown as a fixed overlay on the scrolling display (latch-on color when that scroll position is a lit step, latch-off color otherwise), so you can see at a glance which rhythm each column is using. In pages 2 and 3, active rhythm columns are also drawn in latch-on color, making it easy to identify and edit playing patterns without switching back to the play page.

The module provides 8 mono trigger outputs (T1–T8) and a polyphonic output carrying all 8 triggers on channels 1–8.

In the right-click menu you can select a **clock divider** (÷1 through ÷64), and choose colors for the **active page button**, **inactive page buttons**, **step indicator**, **latch indicator (on step)**, and **latch indicator (off step)**.

### Gome64 (titled GOME64)

This module is a two-dimensional pattern arpeggiator inspired by [gome](https://monome.org/docs/grid/app/sum/#gome) from the monome sum collection (itself a descendant of stretta's polygome). Where Cafe64 plays a fixed rhythm per column, Gome64 plays a *pattern shape*: an ordered sequence of grid-cell offsets relative to a root.

**Playing:** Press a grid cell in rows 2–8 to set a *root*. On each clock tick the current pattern walks one step, firing the cell at `root + offset`. Hold several cells at once to run several arpeggios in parallel from different roots. Because each button is meant to be a note, Gome64 itself only emits gates — it pairs with a note-mapping companion that turns the 64 cell gates into pitches (any module reading the 4 × 16-channel gate format works, including Buttons64). Held roots are shown dimly; the cell currently firing flashes brightly.

**Pattern select:** The top row of the grid (8 pads) is a selector strip for the eight patterns (a radio group); the lower seven rows are the playing field. The selected pattern is used by all running arpeggios, and the active pattern's pad is highlighted.

**Loop mode (scene A):** Toggles between momentary play (arpeggio runs while the cell is held) and latched play (tap to start, tap again to stop) for hands-free, sustained arpeggios — the same idiom as Cafe64's latch.

**Record mode (scene B):** Arm record, then tap grid cells in order to capture a new pattern into the selected slot. The first tap is the root (offset 0,0); each later tap stores its position relative to that root. Disarm to finish. Eight built-in patterns are provided on load.

**Off-grid behavior:** When `root + offset` lands outside the 8×8 grid, the right-click menu lets you choose **Skip** (rest that step — the default), **Wrap** (cycle around the edges), or **Clamp** (pin to the nearest edge cell).

The module provides four 16-channel polyphonic gate outputs (rows 1-2, 3-4, 5-6, 7-8), one gate channel per grid cell, matching Buttons64's layout. A clock and reset (from Base64) drive and re-zero the walkers. The right-click menu also offers a **clock divider** (÷1–÷64) and colors for the **root**, **fire**, **record**, **active pattern**, and **inactive pattern** indicators.

### 64Notes (titled 64NOTES)

This is the first *companion* module: it is not a page and does not join the
Base64 chain — it is pure CV in → CV out, and wears a blue accent instead of the
orange to mark the difference. It turns the 64-cell gate format emitted by
Gome64 or Buttons64 (four 16-channel polyphonic cables) into pitched polyphony:
patch the four cell outputs into the four cell inputs 1:1, and connect V/OCT and
GATE to a polyphonic voice.

Each grid cell is assigned a pitch by one of four **arrangements** (right-click
menu): **1D scale** (scale degrees running left→right, top→bottom), **2D
isomorphic** (fixed semitone intervals per column/row), **2D scale grid** (the
default — horizontal steps are scale degrees, each row down is a diatonic
fourth, so everything stays in key and matches Gome64's pattern geometry), and
**chord per cell** (each cell plays a diatonic triad or seventh). Scale, root
note, base octave and the interval parameters are all in the menu. Pitch rises
toward the bottom-right of the grid, matching Gome64's conventions.

A **voice allocator** collapses the 64 cells into 1–16 output voices (Polyphony
menu), so a single poly oscillator can play the whole grid. When all voices are
busy, the **voice stealing** strategy decides which one to take: oldest, newest,
lowest, highest, round-robin, or off (new notes are dropped). The **RTRG**
output emits a short trigger whenever a voice (re)starts — patch it to your
envelope's retrigger input so stolen voices articulate cleanly.

Because sources like Gome64 emit 5 ms triggers, 64Notes holds notes itself: the
**note length** menu offers *fixed time* (50 ms–2 s, the default at 200 ms),
*clock-synced* (1–8 ticks of the CLK input), or *gate follow* (for sustained
sources like Buttons64 toggles). The **TRN** input transposes everything at
1V/oct.

## Sources of inspiration

- [Monome grid](https://monome.org/docs/grid/)
- Yamaha's [Tenori-on](https://en.wikipedia.org/wiki/Yamaha_Tenori-on)
- the [Controllerism](https://www.controllerism.com/) movement
- [forsitan modulare](https://github.com/gosub/forsitan) (my other plugin)

## Author

Giampaolo Guiducci <giampaolo.guiducci@gmail.com>

## License

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
