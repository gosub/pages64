#!/usr/bin/env python3
"""
gen_launchpad.py — Draw a plain schematic of the Launchpad Mini MkII.

Black lines on white: the (square) device outline, the 8×8 grid of pads, the
top row of round buttons (labelled 1–8) and the right column of scene round
buttons (labelled A–H). Meant as a base for illustrations in the module docs —
overlay or recolour pads to show a layout.

    python3 tools/gen_launchpad.py > docs/img/launchpad.svg
"""

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

def cx(c): return GX + c * CELL + CELL / 2
def cy(r): return GY + r * CELL + CELL / 2

# Content bounding box, reaching the actual label edges (so INNER is the real
# gap between the 1-8 / A-H labels and the border). Left and bottom edges are
# the grid; top and right edges are the labels.
x_min = GX
x_max = SCN_X + RR + 14 + 7        # scene-label baseline x + half a glyph
y_min = TOP_Y - RR - 10 - 13       # top-label baseline y − a glyph ascent
y_max = GY + 8 * CELL
cw, ch = x_max - x_min, y_max - y_min

# A square outline that wraps the content with equal margins, content centred.
side = max(cw, ch) + 2 * INNER
tx = (MARGIN + side / 2) - (x_min + cw / 2)
ty = (MARGIN + side / 2) - (y_min + ch / 2)
canvas = side + 2 * MARGIN

out = []
out.append(f'<svg xmlns="http://www.w3.org/2000/svg" width="{canvas:g}" height="{canvas:g}" '
           f'viewBox="0 0 {canvas:g} {canvas:g}" font-family="Helvetica, Arial, sans-serif">')
out.append(f'<rect width="{canvas:g}" height="{canvas:g}" fill="#ffffff"/>')

# square device outline
out.append('<rect x="%g" y="%g" width="%g" height="%g" rx="26" '
           'fill="none" stroke="#000000" stroke-width="%g"/>'
           % (MARGIN, MARGIN, side, side, STROKE))

# content, centred inside the outline
out.append(f'<g transform="translate({tx:g},{ty:g})">')

out.append('<g fill="none" stroke="#000000" stroke-width="%g" stroke-linejoin="round">' % STROKE)
for r in range(8):                       # 8×8 grid of pads
    for c in range(8):
        x = GX + c * CELL + (CELL - PAD) / 2
        y = GY + r * CELL + (CELL - PAD) / 2
        out.append(f'<rect x="{x:g}" y="{y:g}" width="{PAD}" height="{PAD}" rx="{PAD_R}"/>')
for c in range(8):                       # top round buttons
    out.append(f'<circle cx="{cx(c):g}" cy="{TOP_Y}" r="{RR}"/>')
for r in range(8):                       # right scene round buttons
    out.append(f'<circle cx="{SCN_X}" cy="{cy(r):g}" r="{RR}"/>')
out.append('</g>')

# labels (real text — these illustrations are rendered by a normal SVG renderer)
out.append('<g fill="#000000" font-size="17" text-anchor="middle">')
for c in range(8):
    out.append(f'<text x="{cx(c):g}" y="{TOP_Y - RR - 10}">{c + 1}</text>')
for r in range(8):
    out.append(f'<text x="{SCN_X + RR + 14:g}" y="{cy(r) + 6:g}">{chr(ord("A") + r)}</text>')
out.append('</g>')

out.append('</g>')
out.append('</svg>')
print("\n".join(out))
