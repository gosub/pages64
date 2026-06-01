# pages64

**pages64** is a modular plugin for VCV-Rack designed to transform a *Novation
Launchpad Mini MKII* into an expressive musical instrument. It consists of a
**Base64** MIDI interface module for the Launchpad, to which other modules can be
attached to create your own instrument. Each module corresponds to a page, which
can be selected directly from the launchpad grid. Each page module aspires to
turn the grid in a different musical instrument and a source of expression.

## Current Modules

### Base64 (BASE64)

This is the initial module. Once loaded, you can select the Launchpad MIDI
interface and attach the "page" modules to its right. The LEDs at the top
indicate the number of modules connected and the currently active module (green)
and inactive ones (yellow). The output jacks at the bottom provide a CV for the
currently active page (1V per page) and a trigger signal when a page is changed.

To switch to a different page, keep pressed the top-left button in the top row
of the Launchpad (labeled 1), and press a button from the top row of the grid;
each lighted button is a page.

### Buttons64 (BTTN64)

This module is divided into four sections. Each section corresponds to two
horizontal rows of grid buttons. The toggle input allows you to select whether
the corresponding buttons have a momentary action or behave as on/off
toggles. Each output jack consists of a 16-channel polyphonic signal, which
carries the state of the buttons in the corresponding rows, which can be split
with the **SPLIT** module available in *VCV Fundamentals*. In the module's
right-click menu, you can choose the color of the buttons when pressed for each
pair of rows.

## Sources of inspiration

- [Monome grid](https://monome.org/docs/grid/)
- Yamaha's [Tenori-on](https://en.wikipedia.org/wiki/Yamaha_Tenori-on)
- the [Controllerism](https://www.controllerism.com/) movement
- [forsitan modulare](https://github.com/gosub/forsitan) (my other plugin)

## Author

Giampaolo Guiducci <giampaolo.guiducci@gmail.com>

## License

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
