"""Generate panel title paths via Inkscape, then split into outer/inner subpaths.

VCV Rack's NanoSVG renders each M…Z subpath of a <path> independently,
so inner counters (B bowl, A hole, 6 bowl…) get filled incorrectly.
Fix: split the Inkscape-generated path at Z-M boundaries; emit each
outer subpath in text colour and each inner subpath in panel background
colour (painter's algorithm). Winding sign (shoelace formula on the
approximated polygon) identifies which is which.
"""

import re, os, subprocess, sys

PANEL_W   = 71.12
FONT_SIZE = 4.5      # mm
CX        = PANEL_W / 2
BASELINE  = 8.5      # mm
TEXT_COL  = "#d0d0d0"
BG_COL    = "#1e1e1e"

TITLES = {
    "Base":      ("BASE", "64"),
    "Buttons64": ("BTTN", "64"),
}

SVG_DIR = os.path.join(os.path.dirname(__file__), "..", "res")

# ── path helpers ──────────────────────────────────────────────────────────────

def split_subpaths(d):
    """Split a compound SVG path d string at Z…M boundaries."""
    parts = re.split(r'(?i)[Zz]\s*(?=[Mm])', d.strip())
    result = []
    for p in parts:
        p = p.strip()
        if p:
            if not re.search(r'[Zz]\s*$', p):
                p += ' Z'
            result.append(p)
    return result


def signed_area(d):
    """Approximate signed area using shoelace on linearised path (curves → endpoints).
    In SVG y-down: positive = CW (inner/hole), negative = CCW (outer/solid).
    """
    pts = []
    cx = cy = 0.0

    def push(x, y):
        pts.append((x, y))
        return x, y

    tokens = re.findall(r'[MmLlHhVvCcQqSsTtAaZz]|[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?', d)
    i = 0
    cmd = 'M'
    while i < len(tokens):
        t = tokens[i]
        if re.match(r'^[MmLlHhVvCcQqSsTtAaZz]$', t):
            cmd = t
            i += 1
            continue

        def num():
            nonlocal i
            v = float(tokens[i]); i += 1
            return v

        if cmd in ('M', 'L'):
            cx, cy = push(num(), num())
        elif cmd in ('m', 'l'):
            dx, dy = num(), num()
            cx += dx; cy += dy
            push(cx, cy)
        elif cmd == 'H':
            cx = num(); push(cx, cy)
        elif cmd == 'h':
            cx += num(); push(cx, cy)
        elif cmd == 'V':
            cy = num(); push(cx, cy)
        elif cmd == 'v':
            cy += num(); push(cx, cy)
        elif cmd == 'C':
            num(); num(); num(); num()
            cx, cy = push(num(), num())
        elif cmd == 'c':
            num(); num(); num(); num()
            dx, dy = num(), num()
            cx += dx; cy += dy; push(cx, cy)
        elif cmd == 'Q':
            num(); num()
            cx, cy = push(num(), num())
        elif cmd == 'q':
            num(); num()
            dx, dy = num(), num()
            cx += dx; cy += dy; push(cx, cy)
        elif cmd in ('Z', 'z'):
            if pts:
                cx, cy = pts[0]
        else:
            i += 1  # skip unknown args

    if len(pts) < 3:
        return 0.0
    area = sum(pts[j][0] * pts[(j+1) % len(pts)][1] -
               pts[(j+1) % len(pts)][0] * pts[j][1]
               for j in range(len(pts)))
    return area / 2.0   # positive = CW (inner), negative = CCW (outer) in y-down


# ── Inkscape helpers ──────────────────────────────────────────────────────────

def make_input_svg(bold_text, light_text):
    return f"""<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg"
     width="{PANEL_W}mm" height="15mm"
     viewBox="0 0 {PANEL_W} 15">
  <text x="{CX}" y="{BASELINE}"
        text-anchor="middle"
        style="fill:{TEXT_COL};font-size:{FONT_SIZE}mm;font-family:'Elms Sans'"
        id="title_text"
        xml:space="preserve"
        ><tspan style="font-weight:700">{bold_text}</tspan><tspan style="font-weight:300">{light_text}</tspan></text>
</svg>"""


def inkscape_convert(in_svg, out_svg):
    r = subprocess.run(
        ["inkscape", in_svg,
         "--actions=select-all;object-to-path",
         f"--export-filename={out_svg}",
         "--export-type=svg"],
        capture_output=True, text=True)
    return r.returncode == 0


def extract_d(svg_path):
    with open(svg_path) as f:
        content = f.read()
    m = re.search(r'<path\b[^>]*\bid="title_text"[^>]*\bd="([^"]*)"', content, re.DOTALL)
    if m:
        return m.group(1)
    # fallback: first path
    m = re.search(r'<path\b[^>]*\bd="([^"]*)"', content, re.DOTALL)
    return m.group(1) if m else None


# ── main ──────────────────────────────────────────────────────────────────────

def build_title_paths(d):
    """Return list of <path fill=... d=.../> strings, outer in text colour, inner in bg."""
    subpaths = split_subpaths(d)
    paths = []
    for sp in subpaths:
        area  = signed_area(sp)
        fill  = BG_COL if area > 0 else TEXT_COL   # CW=inner=bg, CCW=outer=text
        paths.append(f'<path fill="{fill}" d="{sp}"/>')
    return paths


def update_svg_title(panel_svg, path_elements, module_name):
    with open(panel_svg) as f:
        content = f.read()
    inner = "\n    ".join(path_elements)
    new_block = (f'  <!-- title: {module_name} ElmsSans via Inkscape -->\n'
                 f'  <g id="title">\n'
                 f'    {inner}\n'
                 f'  </g>')
    new_content = re.sub(
        r'  <!-- title:.*?</g>', new_block, content, flags=re.DOTALL)
    with open(panel_svg, "w") as f:
        f.write(new_content)
    print(f"  {len(path_elements)} path elements → {panel_svg}")


for module, (bold_part, light_part) in TITLES.items():
    print(f"\n{module}: '{bold_part}' Bold + '{light_part}' Light")

    in_svg  = f"/tmp/pages64_{module}_text.svg"
    out_svg = f"/tmp/pages64_{module}_paths.svg"

    with open(in_svg, "w") as f:
        f.write(make_input_svg(bold_part, light_part))

    if not inkscape_convert(in_svg, out_svg):
        print("  Inkscape failed — skipping"); continue

    d = extract_d(out_svg)
    if not d:
        print("  No path found — skipping"); continue

    subpath_count = len(split_subpaths(d))
    print(f"  {subpath_count} subpaths found")

    path_elements = build_title_paths(d)
    outer = sum(1 for p in path_elements if TEXT_COL in p)
    inner = sum(1 for p in path_elements if BG_COL   in p)
    print(f"  outer={outer}  inner={inner}")

    panel_svg = os.path.join(SVG_DIR, f"{module}.svg")
    update_svg_title(panel_svg, path_elements, module)

print("\nDone.")
