#!/usr/bin/env python3
"""Audit pages64 panels for overlaps, clearances and the panel grammar.

Geometry comes from the real thing: dump_panels (built here from the plugin's
own build objects) constructs every ModuleWidget and reports each widget's box
and each shape of the panel SVG. SVG shapes are clustered into elements
(title, a label with its badge, the domino logo) by proximity: shapes closer
than MERGE_MM belong to one element.

Spacing rules (ported from forsitan modulare's panel audit):
  - >= 1.5 mm between any two controls / jacks
  - >= 1.0 mm between a label (any SVG element) and anything else
  - >= 1.0 mm to a screw, >= 0.5 mm to a light
  - every element >= 0.3 mm inside the panel edge (screws and the accent
    trapezoid / bottom rule excepted)
Panel grammar (CLAUDE.md "Panel structure"):
  - the title's cap top sits at y = 7.0 mm
  - page modules have a SmallLight active-page light centered at (6.0, 18.0)

Usage, from the repo root:
    python3 tools/panel_audit/panel_audit.py [Slug ...]
Builds the plugin and dump_panels first (make). Exits nonzero on any issue.
"""
import json
import math
import os
import subprocess
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
RACK_SYSTEM_DIR = os.environ.get("RACK_SYSTEM_DIR", "/home/gg/dl/audio/Rack-2.6.6")

MERGE_MM = 0.9          # SVG shapes closer than this form one element
WORD_GAP_MM = 1.5       # ...as do words on one text line up to this far apart
TITLE_BAND = 14.0       # everything wholly above this is the title (name + "64")
BACKGROUND = "#1e1e1e"
ACCENTS = {"#f26522", "#22aff2"}
TITLE_CAP_TOP = 7.0
ACTIVE_LIGHT = (6.0, 18.0)


def dump(slugs):
    subprocess.run(["make", "-s", "-j8"], cwd=ROOT, check=True,
                   stdout=subprocess.DEVNULL)
    subprocess.run(["make", "-s", "-C", "tools/panel_audit"], cwd=ROOT, check=True)
    out = subprocess.run(
        [os.path.join(ROOT, "tools/panel_audit/dump_panels"), ROOT, RACK_SYSTEM_DIR]
        + list(slugs),
        cwd=ROOT, check=True, capture_output=True, text=True).stdout
    return json.loads(out)


# ── geometry ────────────────────────────────────────────────────────────────

def rect_gap(a, b):
    """Gap between two (x0, y0, x1, y1) rects; negative = overlap depth."""
    dx = max(a[0] - b[2], b[0] - a[2])
    dy = max(a[1] - b[3], b[1] - a[3])
    if dx < 0 and dy < 0:
        return -min(-dx, -dy)
    return max(dx, dy) if dx >= 0 and dy >= 0 else max(dx, dy)


def circle_rect_gap(c, r):
    cx, cy, cr = c
    px = min(max(cx, r[0]), r[2])
    py = min(max(cy, r[1]), r[3])
    return math.hypot(cx - px, cy - py) - cr


def contains(outer, inner):
    return (outer[0] <= inner[0] + 1e-6 and outer[1] <= inner[1] + 1e-6
            and outer[2] >= inner[2] - 1e-6 and outer[3] >= inner[3] - 1e-6)


class Element:
    def __init__(self, kind, name, rect, circle=None):
        self.kind = kind        # input output param light screw display other label accent
        self.name = name
        self.rect = rect
        self.circle = circle    # (cx, cy, r) for round widgets

    def gap(self, other):
        if self.circle and other.circle:
            a, b = self.circle, other.circle
            return math.hypot(a[0] - b[0], a[1] - b[1]) - a[2] - b[2]
        if self.circle:
            return circle_rect_gap(self.circle, other.rect)
        if other.circle:
            return circle_rect_gap(other.circle, self.rect)
        return rect_gap(self.rect, other.rect)


def widget_elements(module):
    els, counts = [], {}
    area = module["w"] * module["h"]
    for w in module["widgets"]:
        k = w["kind"]
        if w["w"] * w["h"] > 0.9 * area:
            continue    # a full-panel overlay (64Pads' pad display) has no clearances
        counts[k] = counts.get(k, 0) + 1
        rect = (w["x"], w["y"], w["x"] + w["w"], w["y"] + w["h"])
        cx, cy = w["x"] + w["w"] / 2, w["y"] + w["h"] / 2
        name = "%s@(%.1f,%.1f)" % (k, cx, cy)
        circle = None
        if k in ("input", "output", "param", "light", "screw") and abs(w["w"] - w["h"]) < 0.2:
            circle = (cx, cy, min(w["w"], w["h"]) / 2)
        els.append(Element(k, name, rect, circle))
    return els


def svg_elements(module):
    W, H = module["w"], module["h"]
    shapes = []
    for s in module["shapes"]:
        r = (s["x0"], s["y0"], s["x1"], s["y1"])
        if s["color"] == BACKGROUND and (r[2] - r[0]) > 0.9 * W and (r[3] - r[1]) > 0.9 * H:
            continue                                     # panel background
        shapes.append((s, r))

    accents = [Element("accent", "accent@y%.1f" % r[1], r)
               for s, r in shapes if s["color"] in ACCENTS and (r[2] - r[0]) > 0.5 * W]
    rest = [r for s, r in shapes
            if not (s["color"] in ACCENTS and (r[2] - r[0]) > 0.5 * W)]

    # union-find clustering by proximity
    parent = list(range(len(rest)))

    def find(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i

    def same_line(a, b):
        overlap = min(a[3], b[3]) - max(a[1], b[1])
        return overlap > 0.6 * min(a[3] - a[1], b[3] - b[1])

    for i in range(len(rest)):
        for j in range(i + 1, len(rest)):
            g = rect_gap(rest[i], rest[j])
            if g < MERGE_MM or (g < WORD_GAP_MM and same_line(rest[i], rest[j])):
                parent[find(i)] = find(j)
    groups = {}
    for i, r in enumerate(rest):
        groups.setdefault(find(i), []).append(r)

    def union(rs):
        return (min(r[0] for r in rs), min(r[1] for r in rs),
                max(r[2] for r in rs), max(r[3] for r in rs))

    labels, title = [], []
    for rs in groups.values():
        u = union(rs)
        if u[3] < TITLE_BAND:
            title.append(u)      # bold name and light "64" are one title
            continue
        labels.append(Element("label", "svg@(%.1f,%.1f)" % ((u[0] + u[2]) / 2, (u[1] + u[3]) / 2), u))
    if title:
        labels.append(Element("label", "title", union(title)))
    return labels, accents


# ── rules ───────────────────────────────────────────────────────────────────

def need(a, b):
    kinds = {a.kind, b.kind}
    if "light" in kinds:
        return 0.5
    if "screw" in kinds:
        return 1.0
    if "label" in kinds or "accent" in kinds:
        return 1.0
    return 1.5


def skip_pair(a, b):
    kinds = {a.kind, b.kind}
    if kinds == {"screw", "accent"}:
        return True                       # screws sit on the accent band
    # artwork drawn under (or around) a display-like widget is its bezel
    for x, y in ((a, b), (b, a)):
        if x.kind in ("display", "other") and y.kind == "label":
            if contains(x.rect, y.rect) or contains(y.rect, x.rect):
                return True
    return False


def audit(module):
    W, H = module["w"], module["h"]
    widgets = widget_elements(module)
    labels, accents = svg_elements(module)
    els = widgets + labels + accents
    issues = []

    for i in range(len(els)):
        for j in range(i + 1, len(els)):
            a, b = els[i], els[j]
            if skip_pair(a, b):
                continue
            g, n = a.gap(b), need(a, b)
            if g < n - 1e-6:
                issues.append("%s <-> %s: gap %.2fmm (need %.1f)" % (a.name, b.name, g, n))

    for e in widgets + labels:
        if e.kind == "screw":
            continue
        r = e.rect
        if r[0] < 0.3 or r[1] < 0.3 or r[2] > W - 0.3 or r[3] > H - 0.3:
            issues.append("%s: closer than 0.3mm to the panel edge" % e.name)

    # grammar: title cap top
    title = next((l for l in labels if l.name == "title"), None)
    if title:
        if abs(title.rect[1] - TITLE_CAP_TOP) > 0.15:
            issues.append("title cap top at y=%.2f (expected %.1f)" % (title.rect[1], TITLE_CAP_TOP))
    else:
        issues.append("no title found")

    # grammar: active-page light on page modules (Base64 and companions have none)
    slug = module["slug"]
    is_page = slug.endswith("64") and not slug[0].isdigit() and slug != "Base64"
    if is_page:
        ok = any(e.kind == "light" and e.circle
                 and abs(e.circle[0] - ACTIVE_LIGHT[0]) < 0.05
                 and abs(e.circle[1] - ACTIVE_LIGHT[1]) < 0.05 for e in widgets)
        if not ok:
            issues.append("no active-page light at (%.1f, %.1f)" % ACTIVE_LIGHT)
    return issues


def main():
    modules = dump(sys.argv[1:])
    total = 0
    for m in modules:
        issues = audit(m)
        total += len(issues)
        hp = round(m["w"] / 5.08)
        print("== %s (%dHP): %d issue(s)" % (m["slug"], hp, len(issues)))
        for s in issues:
            print("   " + s)
    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main())
