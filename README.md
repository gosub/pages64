# pages64

**pages64** is a modular plugin for VCV Rack designed to transform a *Novation
Launchpad Mini MkII* into an expressive musical instrument. It consists of a
**Base64** MIDI interface module for the Launchpad, to which other modules can be
attached to create your own instrument. Each module corresponds to a page, which
can be selected directly from the Launchpad grid. Each page module aspires to
turn the grid into a different musical instrument and a source of expression.

## Modules

Each module has its own documentation page in the [docs/](docs/) folder.

### Core

- **[Base64](docs/Base64.md)** — the central module: Launchpad MIDI I/O, page
  selection, clock and reset distribution. Everything starts here.

### Page modules

Page modules attach to the right of Base64 and each occupies one page of the
grid. Across all of them the Launchpad's extra buttons follow one convention:
the **top round buttons (1–8)** carry static page configuration (button 8 is
always page select; 6 and 7 are reserved for future global features), while
the **scene buttons (A–H)** on the right are for interactive play — latch
modes, mute groups and the like.

- **[Buttons64](docs/Buttons64.md)** — 64 momentary/toggle buttons,
  4 × 16-channel polyphonic gate outputs.
- **[Grid64](docs/Grid64.md)** — 64 momentary/toggle buttons, 64 individual
  mono gate outputs.
- **[Sliders64](docs/Sliders64.md)** — each column is a slewed CV slider;
  scene buttons pick the slew rate.
- **[Flin64](docs/Flin64.md)** — cyclic polyrhythm sequencer after monome's
  *flin*: eight looping rays at harmonic speed ratios.
- **[Step64](docs/Step64.md)** — classic 8-step trigger sequencer with a
  loop-range control row and step CV.
- **[Cafe64](docs/Cafe64.md)** — polyrhythm performance sequencer inspired by
  stretta's *Press Cafe*: eight patterns, eight voices, three edit sub-pages.
- **[Gome64](docs/Gome64.md)** — 2D pattern arpeggiator after monome's *gome*:
  hold roots, a pattern shape walks the grid.
- **[Euclid64](docs/Euclid64.md)** — euclidean rhythm sequencer, one E(fill,
  length) voice per column.
- **[Bounce64](docs/Bounce64.md)** — bouncing-ball rhythm machine after the
  Tenori-on's bounce mode; drop height sets the period.
- **[Mlr64](docs/Mlr64.md)** — performance sample cutter after *mlr*: eight
  varispeed loop lanes cut live from the grid.
- **[Life64](docs/Life64.md)** — Conway's Game of Life: the clock steps
  generations, pads toggle cells; freeze, randomize, frame looping and a
  famous-pattern library.
- **[Sequencer64](docs/Sequencer64.md)** — clocked CV sequencer with
  slider-style steps: the clock walks eight 0–10 V columns; hidden control
  strip for jumps and loop range, optional slew.

### Companion modules

Companion modules are pure CV utilities (no Launchpad chain) and wear a blue
accent instead of the orange.

- **[64Notes](docs/64Notes.md)** — note mapper and voice allocator turning the
  64-cell gate format (Gome64, Buttons64) into pitched polyphony.
- **[8Notes](docs/8Notes.md)** — scale pitch source for the 8-voice page
  modules: poly gate in, in-key poly pitch + gate out.

## Example patches

The [patches/](patches/) folder contains ready-made starting points (they only
need VCV Core and Fundamental besides pages64). After opening one, select your
Launchpad in Base64's MIDI input *and* output displays — device selection is
not stored in the examples.

1. **01_flin_sliders.vcv** — Flin64's eight polyrhythm gates play eight voices
   whose pitches you set live on the Sliders64 page: raise a slider, switch to
   the Flin page, start some rays.
2. **02_gome_64notes.vcv** — the flagship pairing: Gome64 arpeggios through
   64Notes into a polyphonic saw voice with a touch of delay. Hold a pad and
   it plays in key immediately.
3. **03_four_pages.vcv** — a small four-page instrument: Step64 drives two
   noise drum voices, Buttons64 toggles hold a chord drone (64Notes in
   gate-follow mode), Sliders64 column 1 sweeps the lead's filter cutoff, and
   Gome64 plays the lead.
4. **04_mlr.vcv** — Mlr64 starter: a 120 BPM clock and the stereo mix wired to
   the audio device. Load your own loops into the lanes and start cutting.
5. **05_life.vcv** — Life64 starter: a glider orbiting the wrapped grid plays
   a looping melody through 64Notes; randomize, freeze and draw from there.

## Sources of inspiration

- [Monome grid](https://monome.org/docs/grid/)
- Yamaha's [Tenori-on](https://en.wikipedia.org/wiki/Yamaha_Tenori-on)
- the [Controllerism](https://www.controllerism.com/) movement
- [forsitan modulare](https://github.com/gosub/forsitan) (my other plugin)

## Author

Giampaolo Guiducci <giampaolo.guiducci@gmail.com>

## License

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
