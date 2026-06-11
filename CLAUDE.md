# pages64 — project notes for Claude

## Versioning

- **Minor version** (`2.x.0`): bump when adding a new module
- **Patch version** (`2.1.x`): bump for fixes and enhancements to existing modules
- Keep `plugin.json` `"version"` in sync with the git tag

## Documentation structure

- `README.md` carries the plugin overview, a categorized module list with
  one-line descriptions, the example patches, and the license — no full
  module documentation.
- Each module's full documentation lives in `docs/<Slug>.md` (e.g.
  `docs/Flin64.md`), linked from its README list entry.
- Each module entry in `plugin.json` has a `manualUrl` pointing to
  `https://github.com/gosub/pages64/blob/main/docs/<Slug>.md`.
- When adding a module, do all three: create `docs/<Slug>.md`, add the README
  list entry, and set the module's `manualUrl`.

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

Neighbor detection uses `dynamic_cast<PageModule*>` (cached in `onExpanderChange`, not per-sample) so no model list needs updating when a new page module is added.

## Philosophy

pages64 turns a Novation Launchpad Mini MkII into a modular instrument inside VCV Rack.
The **Base64** module handles MIDI I/O and page selection; page modules (e.g. **Buttons64**)
attach to its right as expanders. Each page module occupies one page of the Launchpad grid.
The user switches pages by holding the top-right Launchpad button (CC 111, labeled "8") and
pressing a grid button. The design goal is simplicity: one physical controller, many
instrument personalities, swappable live.

## Button role convention

- **Top round buttons 1–8** (CC 104–111): *static* page configuration only.
  - Button 8 (CC 111) is reserved globally for page select.
  - Buttons 6 and 7 (CC 109, 110) are reserved for future global features
    (state snapshot/reload and the cross-page gesture recorder — see ROADMAP.md).
  - Page modules may use buttons 1–5 (CC 104–108) for sub-page/config selection
    (Cafe64 uses 1–3).
- **Scene buttons A–H** (right column): *interactive play* only — latch modes,
  mute groups, pattern recorders. Never static configuration.

## Companion modules

Non-page utility modules (pure CV in → CV out, no Launchpad chain) reverse the
naming convention — **64Notes**, **8Notes** — and swap the orange accent for its
complementary blue `#22aff2` (trapezoid, bottom rule, accent strokes). The rest
of the panel grammar is unchanged.

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
| Input label | white (`#d0d0d0`) text directly on dark background, no badge. |

## Badge label text generation

Always generate badge label paths with **Montserrat Bold** via `gen_title_paths.py` using `--panel-width <badge_width>` (typically 11 mm) so the text is centered within the badge. Then in the SVG wrap the paths in `<g transform="translate(<badge_x>, 0)">` where `badge_x` is the left edge of the badge rect. Standard parameters:

```
gen_title_paths.py \
  --bold /home/gg/dl/Montserrat/static/Montserrat-Bold.ttf \
  --light /home/gg/dl/Montserrat/static/Montserrat-Light.ttf \
  --titles "LABEL:bold" \
  --panel-width 11 \
  --cap-height 2.0 \
  --baseline <center_y + 1.0> \
  --color "#1e1e1e"
```

The same method applies for input labels rendered as plain white text (use `--color "#d0d0d0"`, no badge rect, translate to center on jack x).

These three colors come directly from the physical Launchpad hardware.

## Clock divider (standard for clock-driven page modules)

Base64 computes clock/reset rising edges once per frame and broadcasts them as
`LeftMessage::clockTick` / `resetTick` (the raw voltages remain available).
Any page module that consumes the clock must include a **clock divider** in its
right-click context menu, using the shared infrastructure:

- Field: `P64::ClockDivider clockDiv;` (defined in `plugin.hpp`)
- Tick logic: `bool tick = clockDiv.process(msg->clockTick);`
- Menu: `P64::appendClockDivMenu(menu, &m->clockDiv);` (÷1 … ÷64)
- Serialize `clockDiv.div` under the key `"clockDiv"`; load with `clockDiv.set(...)`
- Reset to ÷1 in `onReset()` via `clockDiv.set(1)`

Color picker submenus use the shared `P64::appendColorMenu(menu, m, label,
&m->field, includeOff)` helper (defined in `PageModule.hpp`).
