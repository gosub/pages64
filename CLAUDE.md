# pages64 — project notes for Claude

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
- **Title** immediately below the trapezoid: module name in Montserrat Bold + "64" in Montserrat Light, both as pre-baked SVG `<path>` elements (NanoSVG cannot render `<text>`)
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
| Label badge | white (`#d0d0d0`) rounded rect, black (`#1e1e1e`) text, height 3.5 mm, rx 1.0 mm |

These three colors come directly from the physical Launchpad hardware.
