"""
Sculpted cat heads for Weather Besties, built as signed distance fields and meshed with marching cubes.

Local frame per cat: x = 0 at cat centre, y = 0 at the shell top, z = 0 at the flat back (print bed).
The shell front face sits at local z = SHELL_FRONT, so anything hanging below y = 0 stays in front of it.

Colour regions are "painted" into a skin layer of the same field, so every coloured part shares its
boundary exactly with the main body (no gaps, no overlaps) and follows the curved surface.

Run: python3 sculpt_cats.py   (writes assembly-frame and print-frame STLs, then the housing script re-renders)
"""
import os, sys, numpy as np, trimesh
from skimage.measure import marching_cubes
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import weather_besties as wb

STEP = 0.3                       # voxel size in mm
SHELL_FRONT = wb.CAT_T           # local z of the shell front (cat back plane is D - CAT_T)
SKIN = 1.4                       # depth of colour layers

# ------------------------------------------------------------------ grid
xs = np.arange(-54, 54 + STEP, STEP, dtype=np.float32)
ys = np.arange(-13, 67 + STEP, STEP, dtype=np.float32)
zs = np.arange(-1.5, 33 + STEP, STEP, dtype=np.float32)
X, Y, Z = np.meshgrid(xs, ys, zs, indexing="ij")

# ------------------------------------------------------------------ SDF primitives
def ellipsoid(c, r):
    px, py, pz = (X - c[0]) / r[0], (Y - c[1]) / r[1], (Z - c[2]) / r[2]
    k0 = np.sqrt(px * px + py * py + pz * pz)
    k1 = np.sqrt((px / r[0]) ** 2 + (py / r[1]) ** 2 + (pz / r[2]) ** 2)
    return k0 * (k0 - 1.0) / np.maximum(k1, 1e-6)

def round_cone(a, b, r1, r2, zscale=1.0):
    """iq's round cone, with the z axis squashed by zscale (approximate distance)."""
    a = np.array(a, np.float32); b = np.array(b, np.float32)
    a[2] *= zscale; b[2] *= zscale
    px, py, pz = X - a[0], Y - a[1], Z * zscale - a[2]
    ba = b - a; l2 = float(ba @ ba); rr = r1 - r2; a2 = l2 - rr * rr; il2 = 1.0 / l2
    y = px * ba[0] + py * ba[1] + pz * ba[2]
    z = y - l2
    qx, qy, qz = px * l2 - ba[0] * y, py * l2 - ba[1] * y, pz * l2 - ba[2] * y
    x2 = qx * qx + qy * qy + qz * qz
    y2 = y * y * l2; z2 = z * z * l2
    k = np.sign(rr) * rr * rr * x2
    d = np.where(np.sign(z) * a2 * z2 > k, np.sqrt(x2 + z2) * il2 - r2,
         np.where(np.sign(y) * a2 * y2 < k, np.sqrt(x2 + y2) * il2 - r1,
                  (np.sqrt(x2 * a2 * il2) + y * rr) * il2 - r1))
    return d / zscale

def capsule(a, b, r):
    a = np.array(a, np.float32); b = np.array(b, np.float32); ba = b - a
    px, py, pz = X - a[0], Y - a[1], Z - a[2]
    h = np.clip((px * ba[0] + py * ba[1] + pz * ba[2]) / float(ba @ ba), 0, 1)
    return np.sqrt((px - ba[0] * h) ** 2 + (py - ba[1] * h) ** 2 + (pz - ba[2] * h) ** 2) - r

def box(c, half):
    q = np.abs(np.stack([X - c[0], Y - c[1], Z - c[2]])) - np.array(half, np.float32)[:, None, None, None]
    return np.linalg.norm(np.maximum(q, 0), axis=0) + np.minimum(q.max(axis=0), 0)

def smin(a, b, k):
    h = np.clip(0.5 + 0.5 * (b - a) / k, 0, 1)
    return b * (1 - h) + a * h - k * h * (1 - h)

def smax(a, b, k):
    return -smin(-a, -b, k)

# 2D (x, y) regions, extruded through z; used for paint masks
def ell2(cx, cy, rx, ry):
    px, py = (X[..., :1] - cx) / rx, (Y[..., :1] - cy) / ry
    return (np.sqrt(px * px + py * py) - 1.0) * min(rx, ry)

def seg2(ax, ay, bx, by, r):
    px, py = X[..., :1] - ax, Y[..., :1] - ay; dx, dy = bx - ax, by - ay
    h = np.clip((px * dx + py * dy) / (dx * dx + dy * dy), 0, 1)
    return np.sqrt((px - dx * h) ** 2 + (py - dy * h) ** 2) - r

def poly2(pts):
    """Signed distance to a closed polygon in xy."""
    px, py = X[..., :1], Y[..., :1]
    pts = np.array(pts, np.float32)
    d = (px - pts[0, 0]) ** 2 + (py - pts[0, 1]) ** 2
    s = np.ones_like(px)
    n = len(pts)
    for i in range(n):
        j = (i - 1) % n
        ex, ey = pts[j] - pts[i]
        wx, wy = px - pts[i, 0], py - pts[i, 1]
        h = np.clip((wx * ex + wy * ey) / (ex * ex + ey * ey), 0, 1)
        d = np.minimum(d, (wx - ex * h) ** 2 + (wy - ey * h) ** 2)
        c1 = py >= pts[i, 1]; c2 = py < pts[j, 1]; c3 = ex * wy > ey * wx
        flip = (c1 & c2 & c3) | (~c1 & ~c2 & ~c3)
        s = np.where(flip, -s, s)
    return s * np.sqrt(d)

def heart2(cx, cy, size):
    t = np.linspace(0, 2 * np.pi, 60, endpoint=False)
    hx = 16 * np.sin(t) ** 3
    hy = 13 * np.cos(t) - 5 * np.cos(2 * t) - 2 * np.cos(3 * t) - np.cos(4 * t)
    s = size / 34.0
    return poly2(np.stack([cx + hx * s, cy + hy * s + size * 0.05], 1))

def arc2(cx, cy, R, w, lower=True):
    px, py = X[..., :1] - cx, Y[..., :1] - cy
    d = np.abs(np.sqrt(px * px + py * py) - R) - w
    keep = (py <= 0) if lower else (py >= 0)
    return np.where(keep, d, 99.0)

# ------------------------------------------------------------------ the sculpt
EYE_X, EYE_Y = 15.5, 27.0

def ear_dish():
    d = None
    for s in (-1, 1):
        c = round_cone((s * 30.5, 44.0, 15.0), (s * 37.8, 58.5, 13.0), 7.0, 1.4, zscale=1.0)
        d = c if d is None else np.minimum(d, c)
    return d

def head_body(kind):
    d = ellipsoid((0, 27, 3.5), (45, 29, 17.5))                       # skull dome
    for s in (-1, 1):
        d = smin(d, ellipsoid((s * 26, 15, 5), (20, 14, 13)), 6.0)    # cheek fluff
        ear = round_cone((s * 29, 41, 6), (s * 40, 63, 6), 12.5, 2.2, zscale=2.0)
        d = smin(d, ear, 4.0)
        d = smin(d, ellipsoid((s * EYE_X, EYE_Y, 16.8), (10.5, 12.5, 4.8)), 2.5)   # eye bulge
        paw = ellipsoid((s * 22, 0, 22.5), (11.5, 9.5, 7.0))
        d = smin(d, paw, 3.0)
    if kind == "pink":
        d = smin(d, ellipsoid((0, 12.5, 16.5), (15, 10, 6.5)), 4.0)   # big muzzle
    else:
        d = smin(d, ellipsoid((0, 14, 15.5), (11, 7.5, 5)), 4.0)       # small muzzle
    d = smin(d, ellipsoid((0, 15.5, 21.0), (3.6, 2.6, 2.2)), 1.5)     # nose bump
    # toe grooves
    for s in (-1, 1):
        for gx in (-3.8, 3.8):
            g = capsule((s * 22 + gx, -6.5, 29.6), (s * 22 + gx, 2.5, 29.0), 0.9)
            d = smax(d, -g, 0.8)
    # inner-ear dish
    d = smax(d, -ear_dish(), 1.2)
    # flat back, nothing behind the bed, nothing below the shell top except what hangs in front of it
    d = np.maximum(d, -Z)
    d = np.where(Y < 0, np.maximum(d, (SHELL_FRONT + 0.25) - Z), d)
    d = np.maximum(d, -(Y + 12.0))
    # square pegs into the shell top (hard union)
    for px in (-22, 22):
        peg = box((px, -(wb.CAT_PEG_L) / 2 + 0.1, wb.CAT_PEG / 2), (wb.CAT_PEG / 2, wb.CAT_PEG_L / 2 + 0.1, wb.CAT_PEG / 2))
        d = np.minimum(d, peg)
    return d

def paint(body, region2d, zmin):
    """Skin-deep colour region: inside the 2D mask, within SKIN of the surface, on the front side."""
    return np.maximum(np.maximum(np.maximum(region2d, -(body + SKIN)), zmin - Z), body)

def cat_fields(kind):
    B = head_body(kind)
    regions = {}
    inner_ear = None
    for s in (-1, 1):
        r = poly2([(s * 31.5, 42.5), (s * 38.5, 59.5), (s * 22.5, 47.5)])
        inner_ear = r if inner_ear is None else np.minimum(inner_ear, r)
    whisk = None
    for s in (-1, 1):
        for i, ang in enumerate((7, -9)):
            x0, y0 = s * 27.0, 17.0 - i * 5.0
            a = np.radians(ang)
            x1, y1 = x0 + s * 14 * np.cos(a), y0 + 14 * np.sin(a)
            w = seg2(x0, y0, x1, y1, 0.85)
            whisk = w if whisk is None else np.minimum(whisk, w)
    if kind == "black":
        eyes = np.minimum(ell2(-EYE_X, EYE_Y, 9.6, 11.6), ell2(EYE_X, EYE_Y, 9.6, 11.6))
        pupils = np.minimum(ell2(-EYE_X + 1.8, EYE_Y - 1, 5.2, 7.2), ell2(EYE_X + 1.8, EYE_Y - 1, 5.2, 7.2))
        glints = np.minimum(ell2(-EYE_X + 3.6, EYE_Y + 2.6, 1.7, 1.7), ell2(EYE_X + 3.6, EYE_Y + 2.6, 1.7, 1.7))
        white2d = np.minimum(np.minimum(np.maximum(eyes, -pupils), glints), np.minimum(inner_ear, whisk))
        regions["white"] = np.minimum(paint(B, white2d, 9.0), np.maximum(np.maximum(ear_dish() - 0.9, -(B + SKIN)), B))
        regions["pink"] = paint(B, heart2(0, 15.5, 8.5), 14.0)
    else:
        eyes = np.minimum(ell2(-EYE_X, EYE_Y, 6.3, 8.3), ell2(EYE_X, EYE_Y, 6.3, 8.3))
        glints = np.minimum(ell2(-EYE_X + 2.0, EYE_Y + 3.0, 1.6, 1.6), ell2(EYE_X + 2.0, EYE_Y + 3.0, 1.6, 1.6))
        nose = ell2(0, 15.8, 3.9, 2.8)
        mouth = np.minimum(arc2(-3.0, 12.6, 3.0, 0.7), arc2(3.0, 12.6, 3.0, 0.7))
        mouth = np.minimum(mouth, seg2(0, 13.2, 0, 12.0, 0.7))
        muzzle = ell2(0, 12.5, 14.5, 9.6)
        black2d = np.minimum(np.minimum(np.maximum(eyes, -glints), nose), np.minimum(mouth, whisk))
        white2d = np.minimum(np.maximum(muzzle, -np.minimum(nose, mouth)), np.minimum(inner_ear, glints))
        white2d = np.maximum(white2d, -whisk)
        regions["black"] = paint(B, black2d, 9.0)
        regions["white"] = np.minimum(paint(B, np.maximum(white2d, -black2d), 9.0), np.maximum(np.maximum(ear_dish() - 0.9, -(B + SKIN)), B))
    union = None
    for r in regions.values():
        union = r if union is None else np.minimum(union, r)
    main = np.maximum(B, -union)
    return main, regions

def mesh(field):
    v, f, _, _ = marching_cubes(field, 0.0, spacing=(STEP, STEP, STEP))
    v += np.array([xs[0], ys[0], zs[0]])
    m = trimesh.Trimesh(v, f[:, ::-1], process=True)
    m.fix_normals()
    return m

if __name__ == "__main__":
    out_print = os.path.join(wb.OUT, "print")
    cx1, cx2 = wb.cat_centres()
    for kind, cx, body_col, num in (("black", cx1, "black", "08"), ("pink", cx2, "pink", "09")):
        main, regions = cat_fields(kind)
        parts = {f"cat_{kind}": mesh(main)}
        for col, r in regions.items():
            parts[f"cat_{kind}_{col}"] = mesh(r)
        for f in os.listdir(out_print):
            if f.startswith(f"{num}_cat"):
                os.remove(os.path.join(out_print, f))
        for f in os.listdir(wb.OUT):
            if f.startswith(f"_asm_cat_{kind}"):
                os.remove(os.path.join(wb.OUT, f))
        base = parts[f"cat_{kind}"].bounds[0]
        for name, m in parts.items():
            col = body_col if name == f"cat_{kind}" else name.split("_")[-1]
            a = m.copy(); a.apply_translation((cx, wb.H, wb.D - wb.CAT_T))
            a.export(os.path.join(wb.OUT, f"_asm_{name}.stl"))
            p = m.copy(); p.apply_translation(-base)
            fn = f"{num}_cat_{kind}__{name}_{col}.stl".replace(f"cat_{kind}_{col}_{col}", f"cat_{kind}_{col}")
            p.export(os.path.join(out_print, fn))
            print(f"{fn:42s} watertight={m.is_watertight} faces={len(m.faces):7d} size={np.round(m.extents, 1)}")
