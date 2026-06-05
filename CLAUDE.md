# pages64 — project notes for Claude

## Build

```
HOME=/home/gg/dl/temp/rackhome/ make install
```

## Module architecture

All page modules inherit from `PageModule` (defined in `src/PageModule.hpp`), which seals `process()` and provides five virtual hooks:

| Hook | Purpose |
|---|---|
| `pagePreProcess()` | Runs every frame before expander logic (e.g. mode-switch transitions) |
| `pageActive(msg)` | Handle MIDI events when this is the active page |
| `pageInactive()` | Clear transient state (e.g. momentary gates) when not active |
| `rebuildLeds()` | Recompute `ledState[64]`; set `ledsDirty` if changed |
| `updateOutputs()` | Push output voltages |

`PageModule` owns: expander buffer alloc/free, LeftMessage/RightMessage routing, `ledState[64]`, `ledsDirty`, `wasActive`, `myPageIndex`, and the active-page light (lights[0,1]).

Neighbor detection uses `dynamic_cast<PageModule*>` so no model list needs updating when a new page module is added.

## Philosophy

pages64 turns a Novation Launchpad Mini MkII into a modular instrument inside VCV Rack.
The **Base64** module handles MIDI I/O and page selection; page modules (e.g. **Buttons64**)
attach to its right as expanders. Each page module occupies one page of the Launchpad grid.
The user switches pages by holding the top-left Launchpad button (CC 104, labeled "1") and
pressing a grid button. The design goal is simplicity: one physical controller, many
instrument personalities, swappable live.

## Panel structure

All panels share the same visual grammar:

- **Trapezoid accent** at the very top (filled `#f26522`), narrowing inward from the screw centers
- **Title** immediately below the trapezoid: module name in Montserrat Bold + "64" in Montserrat Light, both as pre-baked SVG `<path>` elements (NanoSVG cannot render `<text>`). Generate with `tools/gen_title_paths.py --cap-height 4.125 --baseline 11.125` (cap-top y=7mm). Using a different cap-height produces a noticeably different title size.
- **Active page light** (GreenRedLight): `SmallLight<GreenRedLight>` at x=6.0mm (left border), y=18.0mm on all page modules. Same size as Base64's chain lights; left-border placement keeps it visually close to the chain connection point.
- **Thin horizontal rule** at the bottom (stroke `#f26522`)
- **Domino logo** bottom-right corner (scaled SVG paths)
- **Screws** at the four standard VCV corners
- Controls and jacks in the body area between title and bottom rule

## Color palette

| Role        | Value       |
|-------------|-------------|
| Background  | `#1e1e1e`   |
| Text/labels | `#d0d0d0`   |
| Accent      | `#f26522`   |
| Output label badge | white (`#d0d0d0`) rounded rect, black (`#1e1e1e`) text, height 3.5 mm, rx 1.0 mm. **All output labels always use this badge style** — never plain text on dark background. |

These three colors come directly from the physical Launchpad hardware.
