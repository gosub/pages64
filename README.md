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

To switch to a different page, keep pressed the leftmost button in the top round
button row of the Launchpad (labeled 1), and press a button from the top row of
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

## Sources of inspiration

- [Monome grid](https://monome.org/docs/grid/)
- Yamaha's [Tenori-on](https://en.wikipedia.org/wiki/Yamaha_Tenori-on)
- the [Controllerism](https://www.controllerism.com/) movement
- [forsitan modulare](https://github.com/gosub/forsitan) (my other plugin)

## Author

Giampaolo Guiducci <giampaolo.guiducci@gmail.com>

## License

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
