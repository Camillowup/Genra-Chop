#!/usr/bin/env python3
"""GENRA CHOP BREAK's panel (36 HP, 2.7.0): a drum break loaded, cut and played on the drums of GENRA CHOP (or on its
own trigger inputs), as it is or mangled; in GENRA CHOP's look.

Header: TUNE, DRIVE, LEVEL. LCD, PLAY (Hits / Loop / Chop) and MIX (layer / replace the kit). The waveform with the chops
in the colours of their drum voices, the row for loading and cutting under it. Pads and voices in one section (2 x 8
pads, a voice button and level under every column), PATTERN (LOCK, UNDO, CLEAR, COPY, PASTE, SAVE, ROLL with RATE),
MANGLE (PITCH, DECAY, STUTTER, SHUFFLE, NEW, REVERSE). Jacks: clock, reset, eight triggers, CV; eight voices, the mix.

Writes res/ChopBreak.svg (all text as paths, Inter) and src/ChopBreakLayout.hpp (positions in Rack pixels). The LCD,
the fields, the buttons and the pads are drawn by the module. Every printed text is checked against every other.
Run from anywhere: python3 tools/break_panel.py   (--preview x.png: a picture with the parts drawn in)
"""
import os
import sys

from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HP = 36
W = HP * 5.08
H = 128.5
PX = 75 / 25.4

CASE, CASE_TOP, SECTION, EDGE = "#d8d1bf", "#cbc4b1", "#cdc6b3", "#a9a290"
GRAPH, GRAPH_D, CREAM, RED, PAD, LCD = "#3b3a38", "#2a2928", "#ece7da", "#c8312b", "#b9b4a6", "#a9b98c"
YELLOW = "#d6a640"  # LOAD SAMPLE
INK, INK_DIM = "#3b3a38", "#77736a"

FONT_DIR = "/usr/share/fonts/opentype/inter/"
FONTS = {700: "Inter-Bold.otf", 900: "Inter-Black.otf", 600: "Inter-SemiBold.otf"}
_f = {}


def font(w):
    if w not in _f:
        f = TTFont(FONT_DIR + FONTS[w])
        _f[w] = (f.getGlyphSet(), f.getBestCmap(), f["head"].unitsPerEm, f["hmtx"])
    return _f[w]


def twidth(s, size, w, sp):
    gs, cmap, upm, hmtx = font(w)
    return sum(hmtx[cmap.get(ord(c), "space")][0] * size / upm + sp for c in s) - sp


out = []
boxes = []
comps = {}


def e(s):
    out.append(s)


def text(x, y, s, size=1.6, color=INK, w=700, anchor="middle", sp=0.15, check=True):
    gs, cmap, upm, hmtx = font(w)
    sc = size / upm
    tw = twidth(s, size, w, sp)
    cx = x - (tw / 2 if anchor == "middle" else (tw if anchor == "end" else 0))
    if check:
        boxes.append((cx, y - size * 0.75, cx + tw, y + 0.05, s))
    paths = []
    for c in s:
        g = cmap.get(ord(c), "space")
        pen = SVGPathPen(gs)
        gs[g].draw(TransformPen(pen, (sc, 0, 0, -sc, cx, y)))
        d = pen.getCommands()
        if d:
            paths.append(d)
        cx += hmtx[g][0] * sc + sp
    if paths:
        e(f'<path fill="{color}" d="{" ".join(paths)}"/>')
    return tw


def rect(x, y, w, h, fill, rx=1.0, stroke=None, sw=0.3):
    st = f' stroke="{stroke}" stroke-width="{sw}"' if stroke else ""
    e(f'<rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h:.2f}" rx="{rx}" fill="{fill}"{st}/>')


def comp(name, x, y, *extra):
    comps[name] = (x, y) + extra


# ---------------------------------------------------------------- case
rect(0, 0, W, H, CASE, 0)
rect(0, 0, W, 1.6, CASE_TOP, 0)
tw = text(4, 10.2, "GENRA", 8.0, INK, 900, "start", 0.5)
bx = 4 + tw + 2.2
rect(bx, 3.4, 1.2, 7.0, RED, 0.2)
text(bx + 2.6, 6.0, "CHOP BREAK", 1.8, INK_DIM, 700, "start", 0.35)
text(bx + 2.6, 9.5, "DRUM CHOPPER", 1.8, INK_DIM, 700, "start", 0.35)
for name, x, label in (("TUNE", W - 32.0, "TUNE"), ("DRIVE", W - 20.0, "DRIVE"), ("LEVEL", W - 8.0, "LEVEL")):
    comp(name, x, 6.4)
    text(x, 12.7, label, 1.35, INK, 700)
rect(3, 14.6, W * 0.62 - 4, 13.0, GRAPH, 1.0)
comp("LCD", 4.2, 15.8, W * 0.62 - 6.4, 10.6)
rect(W * 0.62 + 0.5, 14.6, W * 0.38 - 3.5, 6.0, GRAPH, 0.6)
comp("MODE", W * 0.62 + 0.5, 14.6, W * 0.38 - 3.5, 6.0)
rect(W * 0.62 + 0.5, 21.6, W * 0.38 - 3.5, 6.0, GRAPH, 0.6)
comp("MIX", W * 0.62 + 0.5, 21.6, W * 0.38 - 3.5, 6.0)
# waveform with the chops, the break's own row under it
rect(3, 29.6, W - 6, 20.0, GRAPH, 1.0)
rect(4.2, 30.6, W - 8.4, 11.6, "#2a2928", 0.4)
comp("WAVE", 4.2, 30.6, W - 8.4, 11.6)
bw = [("LOAD", 26.0), ("CMODE", 20.0), ("WARP", 11.0), ("WBPM", 22.0), ("WX2", 7.0), ("WD2", 7.0)]
x = 4.2
for k, w in bw:
    if k == "CMODE":
        rect(x, 43.2, w, 5.2, "#55534f", 0.6)
    comp(k, x, 43.2, w, 5.2)
    x += w + 1.0
comp("CHOPSK", W - 30.0, 45.8)
text(W - 30.0 + 4.8, 46.4, "CHOPS", 1.3, CREAM, 700, "start")
# pads and voices in one: 2 rows of 8 pads, under every column one drum voice (GENRA CHOP's order of the drum jacks)
VX, VW = 3.0, 88.0
rect(VX, 51.4, VW, 41.0, SECTION, 1.2, EDGE, 0.4)
PS, PG = 9.6, 0.8
CWID = PS + PG
PX0 = VX + (VW - 8 * CWID + PG) / 2
for r in range(2):
    for c in range(8):
        comp(f"PAD{r * 8 + c}", PX0 + c * CWID, 53.2 + r * CWID, PS, PS)
VO = ("KICK", "SNR", "CH", "OH", "CLAP", "RIM", "TOM", "PERC")
for i, n in enumerate(VO):
    cx = PX0 + i * CWID + PS / 2
    comp(f"VSEL{i}", PX0 + i * CWID, 75.0, PS, 5.0)
    comp(f"VLVL{i}", cx, 85.8)
FX0 = VX + VW + 2.0
FW = 48.0
rect(FX0, 51.4, FW, 41.0, SECTION, 1.2, EDGE, 0.4)
rect(FX0, 51.4, FW, 5.0, GRAPH, 1.0)
rect(FX0, 54.2, FW, 2.2, GRAPH, 0)
text(FX0 + FW / 2, 55.0, "PATTERN", 1.8, CREAM, 700, sp=0.35)
GB = (FW - 3.2 - 1.6) / 3
for i, k in enumerate(("LOCK", "UNDO", "CLEAR", "COPY", "PASTE", "SAVE")):
    comp(k, FX0 + 1.6 + (i % 3) * (GB + 0.8), 59.0 + (i // 3) * 7.0, GB, 5.6)
comp("ROLL", FX0 + 1.6, 74.4, FW - 3.2 - GB - 0.8, 16.0)
comp("RDISP", FX0 + FW - 1.6 - GB, 74.4, GB, 4.6)  # the rate's little screen (drawn by the module)
comp("MRATE", FX0 + FW - 1.6 - GB / 2, 83.6)
text(FX0 + FW - 1.6 - GB / 2, 90.9, "RATE", 1.3, INK, 700)
MX0 = FX0 + FW + 2.0
# mangle
MX = MX0
MW = W - 3 - MX
rect(MX, 51.4, MW, 41.0, SECTION, 1.2, EDGE, 0.4)
rect(MX, 51.4, MW, 5.0, GRAPH, 1.0)
rect(MX, 54.2, MW, 2.2, GRAPH, 0)
text(MX + MW / 2, 55.0, "MANGLE", 1.8, CREAM, 700, sp=0.35)
# the dice of the pattern decide where, these knobs how much (live)
for i, n in enumerate(("PITCH", "DECAY", "STUTTER", "SHUFFLE")):
    kx = MX + MW / 2 + (-1 if i % 2 == 0 else 1) * MW / 4
    ky = 63.2 + (i // 2) * 14.0
    comp(f"M{i}", kx, ky)
    text(kx, ky + 7.4, n, 1.35, INK, 700)
comp("MNEW", MX + 1.6, 85.6, (MW - 4.2) / 2, 5.0)
comp("MREV", MX + 2.6 + (MW - 4.2) / 2, 85.6, (MW - 4.2) / 2, 5.0)

rect(3, 94.6, W - 6, 31.7, GRAPH, 1.4)
Y0, Y1 = 107.2, 121.0
J = 9.4


def jack(name, x, y, label):
    comp(name, x, y)
    text(x, y - 5.4, label, 1.2, CREAM, 700)


def plate(x0, n, y, col=RED, top=8.0):
    w = n * J
    rect(x0 - J / 2 + 0.3, y - top, w - 0.6, top + 4.8, GRAPH_D, 0.8)
    rect(x0 - J / 2 + 1.0, y - top, w - 2.0, 0.5, col, 0.2)


x = 9.0
jack("CLK_IN", x, Y0, "CLK")
jack("RST_IN", x + J, Y0, "RST")
x = 9.0 + 2 * J + 3.0
plate(x, 8, Y0, YELLOW)
for i, n in enumerate(VO):
    jack(f"TRIG{i}_IN", x + i * J, Y0, n)
xr = x + 8 * J + 3.0
plate(xr, 4, Y0, CREAM)
jack("NEW_IN", xr + 3 * J, Y0, "NEW")
jack("PITCH_IN", xr, Y0, "PITCH")
jack("STUT_IN", xr + J, Y0, "STUT")
jack("ROLL_IN", xr + 2 * J, Y0, "ROLL")
plate(x, 8, Y1, RED, 7.4)
for i, n in enumerate(VO):
    jack(f"V{i}_OUT", x + i * J, Y1, n)
xo = W - 9.0 - J
plate(xo, 2, Y1)
jack("OUTL_OUT", xo, Y1, "MIX L")
jack("OUTR_OUT", xo + J, Y1, "MIX R")

# ---------------------------------------------------------------- checks
bad = []
for i in range(len(boxes)):
    for j in range(i + 1, len(boxes)):
        a, b = boxes[i], boxes[j]
        if a[0] < b[2] - 0.05 and b[0] < a[2] - 0.05 and a[1] < b[3] - 0.05 and b[1] < a[3] - 0.05:
            bad.append((a[4], b[4]))
if bad:
    print("texts overlap:", bad)
    sys.exit(1)
jk = [(k, v) for k, v in comps.items() if len(v) == 2 and k.endswith(("_IN", "_OUT"))]
for i in range(len(jk)):
    for j in range(i + 1, len(jk)):
        (n1, (x1, y1)), (n2, (x2, y2)) = jk[i], jk[j]
        if abs(y1 - y2) < 1 and abs(x1 - x2) < 8.6:
            print("jacks too close:", n1, n2)
            sys.exit(1)
if max(v[0] + (v[2] if len(v) == 4 else 4.2) for v in comps.values()) > W - 2.5:
    print("something past the right edge")
    sys.exit(1)


svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W:.2f}mm" height="{H}mm" viewBox="0 0 {W:.2f} {H}">' +
       "".join(out) + "</svg>")
open(os.path.join(ROOT, "res", "ChopBreak.svg"), "w").write(svg)
lines = ["// generated by tools/break_panel.py - do not edit", "#pragma once", "namespace cbl {",
         f"static const float W = {W * PX:.3f}f;"]
for k, v in comps.items():
    vals = ", ".join(f"{a * PX:.3f}f" for a in v)
    lines.append(f"static const float {k}[{len(v)}] = {{{vals}}};")
lines.append("} // namespace cbl")
open(os.path.join(ROOT, "src", "ChopBreakLayout.hpp"), "w").write("\n".join(lines) + "\n")
print(f"res/ChopBreak.svg: {HP} HP, {len(comps)} parts")

# ---------------------------------------------------------------- preview (python3 tools/break_panel.py --preview x.png)
if "--preview" in sys.argv:
    out.clear()
    CL = ["#c8312b", "#d6a640", "#9fd3ff", "#7fb3d8", "#e08a5a", "#b48ad8", "#8fbf6a", "#d88ab4"]
    VOC = {0: 0, 4: 1, 2: 2, 10: 3, 8: 4, 9: 5, 12: 6, 14: 7}
    for k, v in comps.items():
        if len(v) == 2:
            if k.endswith(("_IN", "_OUT")):
                e(f'<circle cx="{v[0]}" cy="{v[1]}" r="3.9" fill="#1e1d1c" stroke="#c9c4b6" stroke-width="0.6"/><circle cx="{v[0]}" cy="{v[1]}" r="1.6" fill="#000"/>')
            else:
                r = 4.2 if k.startswith("M") else (2.4 if k == "CHOPSK" else 3.3)
                e(f'<circle cx="{v[0]}" cy="{v[1]}" r="{r}" fill="#2e2d2b"/><circle cx="{v[0]}" cy="{v[1]}" r="{r * 0.86}" fill="{GRAPH}"/><rect x="{v[0] - 0.3}" y="{v[1] - r * 0.84}" width="0.6" height="{r * 0.62}" rx="0.3" fill="{CREAM}"/>')
            continue
        x, y, w, h = v
        if k == "LCD":
            rect(x, y, w, h, LCD, 0.4)
            text(x + 1.6, y + 3.4, "BRK GENRA FUNK BREAK   16 CHOPS   96 > 92 BPM", 1.9, "#24301a", 700, "start", check=False)
            text(x + 1.6, y + 6.6, "HITS   LAYER   KICK 1  SNR 5  CH 3  OH 11", 1.7, "#24301a", 700, "start", check=False)
            text(x + 1.6, y + 9.3, "LINK: GENRA CHOP (left)", 1.6, "#3a4a2a", 700, "start", check=False)
        elif k in ("MODE", "MIX"):
            rect(x, y, w, h, GRAPH, 0.6)
            text(x + 1.5, y + h / 2 + 0.6, {"MODE": "PLAY  Hits  v", "MIX": "MIX  Layer with kit  v"}[k], 1.55, CREAM, 700, "start", check=False)
        elif k == "WAVE":
            import math
            rect(x, y, w, h, "#2a2928", 0.4)
            pts = " ".join(f"{x + i * w / 300:.2f},{y + h / 2 + math.sin(i * 1.3) * (0.15 + abs(math.sin(i * 0.105))) * h * 0.35 * (1 - (i % 19) / 25):.2f}" for i in range(300))
            e(f'<polyline points="{pts}" fill="none" stroke="{LCD}" stroke-width="0.22"/>')
            for i in range(16):
                col = CL[VOC[i]] if i in VOC else "#77736a"
                e(f'<rect x="{x + i * w / 16:.2f}" y="{y}" width="0.35" height="{h}" fill="{col}"/>')
                text(x + i * w / 16 + 1.4, y + 2.0, str(i + 1), 1.2, col, 700, "start", check=False)
        elif k in ("LOAD", "CMODE", "WARP", "WBPM", "WX2", "WD2", "ROLL", "LOCK", "UNDO", "CLEAR", "COPY", "PASTE", "SAVE", "RRATE"):
            col = {"LOAD": YELLOW, "WBPM": LCD, "ROLL": RED, "CMODE": "#55534f", "WARP": "#55534f", "WX2": "#55534f", "WD2": "#55534f"}.get(k, GRAPH)
            lab = {"LOAD": "LOAD BREAK", "CMODE": "Transient v", "WARP": "WARP", "WBPM": "96 > 92 BPM", "WX2": "x2", "WD2": "/2", "ROLL": "ROLL (hold)", "LOCK": "LOCK", "UNDO": "UNDO", "CLEAR": "CLEAR", "COPY": "COPY", "PASTE": "PASTE", "SAVE": "SAVE", "RRATE": "1/16 v"}[k]
            rect(x, y, w, h, col, 0.6)
            text(x + w / 2, y + h / 2 + 0.6, lab, 1.5, "#2a2416" if k in ("LOAD", "WBPM") else CREAM, 700, check=False)
        elif k.startswith("PAD"):
            i = int(k[3:])
            n = (1 - i // 8) * 8 + i % 8
            rect(x, y, w, h, PAD, 1.0, "#8f8a7c", 0.4)
            text(x + w / 2, y + 2.6, str(n + 1), 1.4, INK_DIM, 700, check=False)
            if n in VOC:
                rect(x + 1.2, y + h - 3.2, w - 2.4, 2.2, CL[VOC[n]], 0.4)
                text(x + w / 2, y + h - 1.5, VO[VOC[n]], 1.2, "#1e1d1c", 700, check=False)
        elif k.startswith("VCHOP"):
            i = int(k[5:])
            val = ["1", "5", "3", "11", "9", "10", "13", "15"][i]
            rect(x, y, w, h, GRAPH, 0.6)
            rect(x, y, w, 0.6, CL[i], 0.2)
            text(x + w / 2, y + h / 2 + 0.7, val + " v", 1.6, CREAM, 700, check=False)
        elif k.startswith("VSEL"):
            i = int(k[4:])
            rect(x, y, w, h, CL[i] if i == 0 else ("#7a2e22" if i == 5 else "#55534f"), 0.6)
            rect(x, y + h - 0.6, w, 0.6, CL[i], 0.2)
            text(x + w / 2, y + h / 2 + 0.55, VO[i], 1.2, "#1e1d1c" if i == 0 else CREAM, 700, check=False)
        elif k.startswith("VMUTE"):
            i = int(k[5:])
            rect(x, y, w, h, RED if i == 5 else "#55534f", 0.6)
            text(x + w / 2, y + h / 2 + 0.55, "MUTE", 1.25, CREAM, 700, check=False)
        elif k == "RDISP":
            rect(x, y, w, h, "#141413", 0.5, "#55534f", 0.3)
            text(x + w / 2, y + h / 2 + 0.6, "1/16T", 1.6, "#9fd3ff", 700, check=False)
        elif k == "MNEW":
            rect(x, y, w, h, RED, 0.6)
            text(x + w / 2, y + h / 2 + 0.6, "NEW", 1.5, CREAM, 700, check=False)
        elif k == "MREV":
            rect(x, y, w, h, "#55534f", 0.6)
            text(x + w / 2, y + h / 2 + 0.6, "REVERSE", 1.4, CREAM, 700, check=False)
    prev = svg.replace("</svg>", "".join(out) + "</svg>")
    open("/tmp/brk_preview.svg", "w").write(prev)
    import subprocess
    subprocess.run(["rsvg-convert", "-w", "1200", "/tmp/brk_preview.svg", "-o", sys.argv[sys.argv.index("--preview") + 1]], check=True)
