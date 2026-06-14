#!/usr/bin/env python3
"""
gen_launchpad.py — Draw schematics of the Launchpad Mini MkII for the docs.

`render()` returns a black-on-white SVG: the (square) device outline, the 8×8
grid of pads, the top row of round buttons (labelled 1–8) and the right column
of scene round buttons (labelled A–H). Pads / round buttons can be given fill
colours, and `notes` adds leader-line callouts (a diagonal then a horizontal
segment) out to text labels in the side margins.

Running the script writes the plain template plus the per-module figures into
docs/img/:

    python3 tools/gen_launchpad.py
"""
import os

CELL   = 42          # grid pitch
PAD    = 36          # pad size (rounded square)
PAD_R  = 5
GX, GY = 70, 126     # grid top-left
RR     = 16          # round-button radius
TOP_Y  = GY - 38     # top round-button row centre
SCN_X  = GX + 8 * CELL + 40   # scene round-button column centre
STROKE = 2.2
INNER  = 20          # margin from content (labels included) to the outline
MARGIN = 16          # margin from the outline to the canvas edge
WHITE  = "#ffffff"

HSEG    = 22         # length of a leader's horizontal segment
TEXTGAP = 12         # gap between the device canvas and the callout text
GUTTPAD = 14         # padding past the longest label, to the canvas edge
LEAD_W  = 1.5        # leader-line stroke
NOTE_FS = 16         # callout font size

def _tw(s): return len(s) * NOTE_FS * 0.6   # rough text width (Helvetica)

# illustration palette
ACCENT = "#f26522"   # the held / acted button (the plugin's orange)
GREEN  = "#3aae3a"   # active page
YELLOW = "#f2b705"   # connected-but-inactive page

def _cx(c): return GX + c * CELL + CELL / 2
def _cy(r): return GY + r * CELL + CELL / 2

def _el(el):
    """(centre_x, centre_y, radius) of a target in logical coords."""
    kind, i = el
    if kind == "top":   return _cx(i), TOP_Y, RR
    if kind == "scene": return SCN_X, _cy(i), RR
    r, c = divmod(i, 8)                       # pad
    return _cx(c), _cy(r), PAD / 2


def render(pads=None, tops=None, scenes=None, notes=None):
    """pads {row*8+col: fill}, tops {0-7: fill}, scenes {0-7: fill};
    notes [{el, side, ly, text}] — el is ('top'|'scene'|'pad', index)."""
    pads, tops, scenes, notes = pads or {}, tops or {}, scenes or {}, notes or []

    # Content bounding box, reaching the actual label edges (so INNER is the
    # real gap between the 1-8 / A-H labels and the border). Left and bottom
    # edges are the grid; top and right edges are the labels.
    x_min = GX
    x_max = SCN_X + RR + 14 + 7
    y_min = TOP_Y - RR - 10 - 13
    y_max = GY + 8 * CELL
    cw, ch = x_max - x_min, y_max - y_min

    # Square outline wrapping the content with equal margins, content centred.
    side = max(cw, ch) + 2 * INNER
    TX = (MARGIN + side / 2) - (x_min + cw / 2)
    TY = (MARGIN + side / 2) - (y_min + ch / 2)
    dev = side + 2 * MARGIN                    # device canvas (square)

    # Side gutters sized to the widest label on each side (0 if none); the
    # device shifts right by the left gutter.
    lw = max([_tw(n["text"]) for n in notes if n["side"] == "left"],  default=0)
    rw = max([_tw(n["text"]) for n in notes if n["side"] == "right"], default=0)
    gl = TEXTGAP + lw + GUTTPAD if lw else 0
    gr = TEXTGAP + rw + GUTTPAD if rw else 0
    dx = gl
    W, H = gl + dev + gr, dev

    o = []
    o.append(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W:g}" height="{H:g}" '
             f'viewBox="0 0 {W:g} {H:g}" font-family="Helvetica, Arial, sans-serif">')
    o.append(f'<rect width="{W:g}" height="{H:g}" fill="{WHITE}"/>')

    # device (outline + content), shifted into place
    o.append(f'<g transform="translate({dx:g},0)">')
    o.append('<rect x="%g" y="%g" width="%g" height="%g" rx="26" fill="none" '
             'stroke="#000000" stroke-width="%g"/>' % (MARGIN, MARGIN, side, side, STROKE))
    o.append(f'<g transform="translate({TX:g},{TY:g})" stroke="#000000" '
             f'stroke-width="{STROKE:g}" stroke-linejoin="round">')
    for r in range(8):
        for c in range(8):
            x = GX + c * CELL + (CELL - PAD) / 2
            y = GY + r * CELL + (CELL - PAD) / 2
            o.append(f'<rect x="{x:g}" y="{y:g}" width="{PAD}" height="{PAD}" rx="{PAD_R}" '
                     f'fill="{pads.get(r * 8 + c, WHITE)}"/>')
    for c in range(8):
        o.append(f'<circle cx="{_cx(c):g}" cy="{TOP_Y}" r="{RR}" fill="{tops.get(c, WHITE)}"/>')
    for r in range(8):
        o.append(f'<circle cx="{SCN_X}" cy="{_cy(r):g}" r="{RR}" fill="{scenes.get(r, WHITE)}"/>')
    o.append('</g>')
    o.append(f'<g transform="translate({TX:g},{TY:g})" fill="#000000" font-size="17" '
             'text-anchor="middle">')
    for c in range(8):
        o.append(f'<text x="{_cx(c):g}" y="{TOP_Y - RR - 10}">{c + 1}</text>')
    for r in range(8):
        o.append(f'<text x="{SCN_X + RR + 14:g}" y="{_cy(r) + 6:g}">{chr(ord("A") + r)}</text>')
    o.append('</g></g>')

    # callouts: diagonal from the target, then horizontal out to the label
    for n in notes:
        ex, ey, rad = _el(n["el"])
        cxf, cyf = ex + TX + dx, ey + TY
        lyf = n["ly"] + TY
        if n["side"] == "right":
            sx = cxf + rad
            text_x = dx + dev + TEXTGAP
            turn_x = text_x - HSEG
            anchor, tlx = "start", text_x + 4
        else:
            sx = cxf - rad
            text_x = dx - TEXTGAP
            turn_x = text_x + HSEG
            anchor, tlx = "end", text_x - 4
        o.append(f'<path d="M{sx:g},{cyf:g} L{turn_x:g},{lyf:g} L{text_x:g},{lyf:g}" '
                 f'fill="none" stroke="#000000" stroke-width="{LEAD_W:g}"/>')
        o.append(f'<text x="{tlx:g}" y="{lyf + NOTE_FS * 0.34:g}" fill="#000000" '
                 f'font-size="{NOTE_FS:g}" text-anchor="{anchor}">{n["text"]}</text>')

    o.append('</svg>')
    return "\n".join(o) + "\n"


FIGURES = {
    # plain template
    "launchpad.svg": dict(),
    # Base64: hold button 8 (page select); the top grid row shows the pages —
    # active green, others yellow.
    "base64_pages.svg": dict(
        tops={7: ACCENT},
        pads={0: GREEN, 1: YELLOW, 2: YELLOW, 3: YELLOW},
        notes=[
            {"el": ("top", 7), "side": "right", "ly": _cy(0), "text": "Hold to switch page"},
            {"el": ("pad", 0),  "side": "left",  "ly": _cy(1), "text": "Active page"},
            {"el": ("pad", 2),  "side": "left",  "ly": _cy(3), "text": "Other pages"},
        ],
    ),
}

if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    img = os.path.join(here, "..", "docs", "img")
    os.makedirs(img, exist_ok=True)
    for name, kw in FIGURES.items():
        with open(os.path.join(img, name), "w") as f:
            f.write(render(**kw))
        print("wrote docs/img/" + name)
