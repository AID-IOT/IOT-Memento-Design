"""Convert a raster logo into a KiCad silkscreen footprint.

Traces the ink/background boundary along pixel edges, chains the segments into
closed loops, classifies them as outlines or holes by signed area, folds each
hole into its parent outline with a zero-width bridge (the standard keyhole
trick, since fp_poly has no hole support), simplifies with Douglas-Peucker and
emits one fp_poly per outline.
"""
import sys, uuid, math
import numpy as np
from PIL import Image

SRC        = sys.argv[1]
OUT        = sys.argv[2]
NAME       = sys.argv[3]
WIDTH_MM   = float(sys.argv[4])
MASK_W     = int(sys.argv[5]) if len(sys.argv) > 5 else 400
LAYER      = sys.argv[6] if len(sys.argv) > 6 else "F.SilkS"
ASPECT     = sys.argv[7] if len(sys.argv) > 7 else None  # e.g. "5:3", or None
EPS_PX     = 0.8      # Douglas-Peucker tolerance, in mask pixels
MIN_AREA   = 4.0      # drop loops smaller than this many mask pixels
BG_TOL     = 40       # channel distance from background before a pixel is ink

# ---------------------------------------------------------------- raster prep
im = Image.open(SRC).convert("RGBA")
a = np.array(im)
# Background is whatever colour dominates the image - NOT necessarily white.
# Testing "min channel < 235" breaks on a grey backdrop (230,230,230 reads as
# solid ink) and a plain "max channel" test drops saturated reds and yellows.
# Measuring distance from the actual background colour handles both.
flat = a[..., :3].reshape(-1, 3)
cols, counts = np.unique(flat, axis=0, return_counts=True)
bg = cols[np.argmax(counts)].astype(int)
ink = (np.abs(a[..., :3].astype(int) - bg).max(axis=2) > BG_TOL) & (a[..., 3] >= 128)
print("background RGB %s -> ink %.1f%% of image"
      % (tuple(int(v) for v in bg), 100 * ink.mean()))

ys, xs = np.nonzero(ink)
ink = ink[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
src_h, src_w = ink.shape

mask_h = max(1, int(round(MASK_W * src_h / src_w)))
small = Image.fromarray((ink * 255).astype(np.uint8)).resize(
    (MASK_W, mask_h), Image.LANCZOS)
m = np.array(small) > 127

# letterbox: pad symmetrically out to the requested box aspect, preserving
# the artwork's own proportions
pad_x = pad_y = 1
if ASPECT:
    aw, ah = (float(v) for v in ASPECT.split(":"))
    want_h = MASK_W * ah / aw
    want_w = mask_h * aw / ah
    if want_h >= mask_h:
        pad_y = max(1, int(round((want_h - mask_h) / 2)))
    else:
        pad_x = max(1, int(round((want_w - MASK_W) / 2)))
m = np.pad(m, ((pad_y, pad_y), (pad_x, pad_x)), constant_values=False)
H, W = m.shape
scale_chk = WIDTH_MM / (MASK_W + 2 * pad_x)
print("source ink %dx%d -> mask %dx%d, pad x%d y%d -> box %dx%d (%.4f mm/px)"
      % (src_w, src_h, MASK_W, mask_h, pad_x, pad_y, W, H, scale_chk))
print("  artwork on board: %.2f x %.2f mm | box %.2f x %.2f mm (%.3f:1)"
      % (MASK_W * scale_chk, mask_h * scale_chk, W * scale_chk, H * scale_chk, W / H))

# ------------------------------------------------- boundary edges (ink on left)
up    = m & ~np.roll(m,  1, axis=0)
down  = m & ~np.roll(m, -1, axis=0)
left  = m & ~np.roll(m,  1, axis=1)
right = m & ~np.roll(m, -1, axis=1)

nxt = {}          # start point -> list of end points
def add(p, q):
    nxt.setdefault(p, []).append(q)

for y, x in zip(*np.nonzero(up)):    add((x, y),         (x + 1, y))
for y, x in zip(*np.nonzero(right)): add((x + 1, y),     (x + 1, y + 1))
for y, x in zip(*np.nonzero(down)):  add((x + 1, y + 1), (x, y + 1))
for y, x in zip(*np.nonzero(left)):  add((x, y + 1),     (x, y))

print("boundary segments:", sum(len(v) for v in nxt.values()))

# --------------------------------------------------------- chain into loops
loops = []
remaining = {p: list(q) for p, q in nxt.items()}
for start in list(remaining):
    while remaining.get(start):
        loop = [start]
        cur, indir = start, None
        while True:
            outs = remaining.get(cur)
            if not outs:
                loop = None
                break
            if len(outs) == 1 or indir is None:
                nx = outs[0]
            else:
                # pinch point: prefer the sharpest turn so 8-connected ink
                # stays a single blob
                def cross(c):
                    d = (c[0] - cur[0], c[1] - cur[1])
                    return indir[0] * d[1] - indir[1] * d[0]
                nx = min(outs, key=cross)
            outs.remove(nx)
            if not outs:
                remaining.pop(cur, None)
            indir = (nx[0] - cur[0], nx[1] - cur[1])
            cur = nx
            if cur == start:
                break
            loop.append(cur)
        if loop and len(loop) >= 4:
            loops.append(loop)

def signed_area(p):
    s = 0.0
    for i in range(len(p)):
        x1, y1 = p[i]
        x2, y2 = p[(i + 1) % len(p)]
        s += x1 * y2 - x2 * y1
    return s / 2.0

loops = [l for l in loops if abs(signed_area(l)) >= MIN_AREA]
print("closed loops:", len(loops))

# ------------------------------------------------------------ simplify (DP)
def dp(pts, eps):
    if len(pts) < 3:
        return pts
    keep = [False] * len(pts)
    keep[0] = keep[-1] = True
    stack = [(0, len(pts) - 1)]
    while stack:
        i, j = stack.pop()
        if j <= i + 1:
            continue
        x1, y1 = pts[i]; x2, y2 = pts[j]
        dx, dy = x2 - x1, y2 - y1
        nrm = math.hypot(dx, dy)
        best, bi = -1.0, -1
        for k in range(i + 1, j):
            x0, y0 = pts[k]
            d = (abs(dy * x0 - dx * y0 + x2 * y1 - y2 * x1) / nrm) if nrm > 0 \
                else math.hypot(x0 - x1, y0 - y1)
            if d > best:
                best, bi = d, k
        if best > eps:
            keep[bi] = True
            stack.append((i, bi)); stack.append((bi, j))
    return [p for p, k in zip(pts, keep) if k]

simplified = []
for l in loops:
    s = dp(l + [l[0]], EPS_PX)
    if s[0] == s[-1]:
        s = s[:-1]
    if len(s) >= 3 and abs(signed_area(s)) >= MIN_AREA:
        simplified.append(s)
loops = simplified
print("after simplify: %d loops, %d points"
      % (len(loops), sum(len(l) for l in loops)))

# ------------------------------------------------- classify + assign holes
outers = [l for l in loops if signed_area(l) > 0]
holes  = [l for l in loops if signed_area(l) < 0]
print("outlines: %d   holes: %d" % (len(outers), len(holes)))

def inside(pt, poly):
    x, y = pt
    c = False
    n = len(poly)
    for i in range(n):
        x1, y1 = poly[i]; x2, y2 = poly[(i + 1) % n]
        if (y1 > y) != (y2 > y):
            xin = (x2 - x1) * (y - y1) / (y2 - y1) + x1
            if x < xin:
                c = not c
    return c

assign = {i: [] for i in range(len(outers))}
orphan = 0
for h in holes:
    cands = [i for i, o in enumerate(outers) if inside(h[0], o)]
    if not cands:
        orphan += 1
        continue
    assign[min(cands, key=lambda i: abs(signed_area(outers[i])))].append(h)
if orphan:
    print("WARNING: %d holes had no parent outline" % orphan)

# ------------------------------------------------------- bridge holes in
def bridge(outer, hole):
    bi, bj, bd = 0, 0, float("inf")
    for i, (ax, ay) in enumerate(outer):
        for j, (bx, by) in enumerate(hole):
            d = (ax - bx) ** 2 + (ay - by) ** 2
            if d < bd:
                bd, bi, bj = d, i, j
    return outer[:bi + 1] + hole[bj:] + hole[:bj + 1] + outer[bi:]

polys = []
for i, o in enumerate(outers):
    p = o
    for h in sorted(assign[i], key=lambda z: -abs(signed_area(z))):
        p = bridge(p, h)
    polys.append(p)
print("final polygons: %d, %d points total"
      % (len(polys), sum(len(p) for p in polys)))

# -------------------------------------------------------------- emit footprint
# WIDTH_MM is the width of the whole box, padding included
scale = WIDTH_MM / W
cx, cy = W / 2.0, H / 2.0
w_mm, h_mm = W * scale, H * scale

def mm(p):
    return ((p[0] - cx) * scale, (p[1] - cy) * scale)

out = []
out.append('(footprint "%s"\n\t(version 20260206)\n\t(generator "pcbnew")'
           '\n\t(generator_version "10.0")\n\t(layer "F.Cu")' % NAME)
out.append('\t(descr "Imported logo artwork, %s, %.1f x %.1f mm")'
           % (LAYER, w_mm, h_mm))
out.append('\t(tags "logo silkscreen graphic")')
# board_only keeps "Update PCB from Schematic" from offering to delete this,
# since a logo has no matching schematic symbol
out.append('\t(attr board_only exclude_from_pos_files exclude_from_bom)')
out.append('\t(property "Reference" "REF**"\n\t\t(at 0 0 0)\n\t\t(layer "F.SilkS")'
           '\n\t\t(hide yes)\n\t\t(uuid "%s")\n\t\t(effects (font (size 1 1) '
           '(thickness 0.15)))\n\t)' % uuid.uuid4())
out.append('\t(property "Value" "%s"\n\t\t(at 0 %.3f 0)\n\t\t(layer "F.Fab")'
           '\n\t\t(hide yes)\n\t\t(uuid "%s")\n\t\t(effects (font (size 1 1) '
           '(thickness 0.15)))\n\t)' % (NAME, h_mm / 2 + 1.0, uuid.uuid4()))

if ASPECT:
    # document the requested box on F.Fab (not fabricated, just a guide)
    hx, hy = w_mm / 2, h_mm / 2
    for x1, y1, x2, y2 in ((-hx, -hy, hx, -hy), (hx, -hy, hx, hy),
                           (hx, hy, -hx, hy), (-hx, hy, -hx, -hy)):
        out.append('\t(fp_line\n\t\t(start %.4f %.4f)\n\t\t(end %.4f %.4f)\n\t\t'
                   '(stroke\n\t\t\t(width 0.1)\n\t\t\t(type dash)\n\t\t)\n\t\t'
                   '(layer "F.Fab")\n\t\t(uuid "%s")\n\t)'
                   % (x1, y1, x2, y2, uuid.uuid4()))

for p in polys:
    pts = [mm(q) for q in p]
    chunks, line = [], []
    for x, y in pts:
        line.append("(xy %.4f %.4f)" % (x, y))
        if len(line) == 6:
            chunks.append(" ".join(line)); line = []
    if line:
        chunks.append(" ".join(line))
    body = "\n\t\t\t".join(chunks)
    out.append('\t(fp_poly\n\t\t(pts\n\t\t\t%s\n\t\t)\n\t\t(stroke\n\t\t\t'
               '(width 0.01)\n\t\t\t(type solid)\n\t\t)\n\t\t(fill yes)\n\t\t'
               '(layer "%s")\n\t\t(uuid "%s")\n\t)' % (body, LAYER, uuid.uuid4()))

out.append('\t(embedded_fonts no)\n)')
open(OUT, "w", encoding="utf-8", newline="\n").write("\n".join(out) + "\n")
print("wrote %s  (%.1f x %.1f mm)" % (OUT, w_mm, h_mm))
