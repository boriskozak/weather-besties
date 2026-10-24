"""
Weather Besties enclosure, parametric CadQuery model.
Revision B. Units: mm.

Coordinate frame (assembly):
  X = width (0 at left edge of shell), Y = up (0 = table), Z = depth (0 = back, D = front face).

Run:  python3 weather_besties.py   -> writes out/ (STEP assembly, assembly-frame STLs)
      python3 export_print.py      -> print-oriented STLs grouped by plate

Everything marked VERIFY is a guess that must be measured on the real board before printing.
"""
import os, math
import cadquery as cq

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "out")
os.makedirs(OUT, exist_ok=True)

# ---------------------------------------------------------------- parameters
W, H, D = 200.0, 118.0, 48.0      # shell body (cats add 62 on top -> 180 total)
R_CORNER = 16.0                   # vertical corner radius
FRONT_FILLET = 3.0
WALL = 2.6                        # side / top / bottom walls
FRONT = 3.0                       # front wall

# Waveshare ESP32-S3-Touch-LCD-4.3 (touch version), from Waveshare docs
GLASS_W, GLASS_H = 106.1, 67.8
HOLE_DX, HOLE_DY = 98.0, 60.0     # PCB mounting hole pattern
CLR = 0.3                         # per side clearance on the glass pocket
MODULE_T = 7.5                    # VERIFY: glass front -> PCB back face
FOAM = 0.8                        # foam tape between retainer pads and PCB

SCREEN_CX, SCREEN_CY = W / 2, 69.0
WIN_W, WIN_H, WIN_R = 100.0, 58.0, 2.0   # VERIFY: visible area is 95.54 x 54.36, offset not confirmed

BEZEL_W, BEZEL_H, BEZEL_R, BEZEL_T = 124.0, 76.0, 9.0, 4.0
DECO_PILOT_DEPTH = 3.2            # blind M3 pilot in the back of each decoration (M3x6 screws from inside)
CAT_PEG, CAT_PEG_HOLE, CAT_PEG_L = 4.0, 4.4, 8.0   # square pegs under each cat

PLATE_W, PLATE_H, PLATE_T = 112.0, 20.0, 4.0
PLATE_CY = 16.0
TEXT = "Cleo & Lila"

CAT_W, CAT_H, CAT_T = 96.0, 62.0, 16.0
FACE_T = 1.2                      # raised colour layer on cat faces
FOOT_D, FOOT_T = 32.0, 8.0

COVER_T = 3.0
SCREW_PILOT = 2.6                 # M3 self tapping into PLA/PETG
SCREW_CLEAR = 3.4

# ---------------------------------------------------------------- helpers
def rrect(w, h, r, t, cx=0, cy=0, z0=0):
    """Rounded rectangle prism in XY, from z0 to z0+t."""
    return (cq.Workplane("XY", origin=(0, 0, z0)).center(cx, cy)
            .sketch().rect(w, h).vertices().fillet(r).finalize().extrude(t))

def cyl(d, z0, z1, x, y):
    return cq.Workplane("XY", origin=(x, y, z0)).circle(d / 2).extrude(z1 - z0)

def ellipse(rx, ry, z0, t, x, y):
    return cq.Workplane("XY", origin=(x, y, z0)).ellipse(rx, ry).extrude(t)

ZF = D - FRONT                    # inner face of front wall
Z_PCB_BACK = ZF - MODULE_T
PAD_H = 6.0                       # keeps the retainer frame ~6.8 mm off the PCB back for components
Z_PAD_FACE = Z_PCB_BACK - FOAM
RET_T = 3.0
Z_RET_FRONT = Z_PAD_FACE - PAD_H
Z_RET_BACK = Z_RET_FRONT - RET_T
BOSS_X, BOSS_Y = 64.0, 30.0       # retainer bosses relative to screen centre
PEG_X = 59.0                      # bezel screws relative to screen centre
CAT_PEG_Z = D - CAT_T + CAT_PEG / 2   # peg sits flush with the cat back so it prints flat
CORNER_IN = 11.0                  # back cover screw inset from outer edge

# ---------------------------------------------------------------- layout of fasteners
def bezel_screws():
    return [(SCREEN_CX + sx * PEG_X, SCREEN_CY + sy * 22) for sx in (-1, 1) for sy in (-1, 1)]

def plate_screws():
    return [(W / 2 + sx * 40, PLATE_CY) for sx in (-1, 1)]

FOOT_Y = FOOT_D / 2 - 4
def foot_centres():
    return [FOOT_D / 2 - 2, W - FOOT_D / 2 + 2]

def foot_screws():
    l, r = foot_centres()
    return [(l + 6, FOOT_Y + 4), (r - 6, FOOT_Y + 4)]

def cat_centres():
    return (W / 2 - CAT_W / 2 - 1.0, W / 2 + CAT_W / 2 + 1.0)

def cat_peg_xs():
    return [cx + dx for cx in cat_centres() for dx in (-22, 22)]

# ---------------------------------------------------------------- shell
def make_shell():
    s = rrect(W, H, R_CORNER, D, W / 2, H / 2)
    s = s.faces(">Z").edges().fillet(FRONT_FILLET)
    s = s.cut(rrect(W - 2 * WALL, H - 2 * WALL, R_CORNER - WALL, ZF, W / 2, H / 2))

    # glass locating rim
    pw, ph = GLASS_W + 2 * CLR, GLASS_H + 2 * CLR
    rim = rrect(pw + 4, ph + 4, 3, 4.0, SCREEN_CX, SCREEN_CY, ZF - 4.0)
    rim = rim.cut(rrect(pw, ph, 1.0, 4.0, SCREEN_CX, SCREEN_CY, ZF - 4.0))
    s = s.union(rim)

    # retainer bosses
    for sx in (-1, 1):
        for sy in (-1, 1):
            x, y = SCREEN_CX + sx * BOSS_X, SCREEN_CY + sy * BOSS_Y
            s = s.union(cyl(7.0, Z_RET_FRONT, ZF + 0.1, x, y))
            s = s.cut(cyl(SCREW_PILOT, Z_RET_FRONT - 1, ZF - 1.0, x, y))

    # back cover bosses
    for x in (CORNER_IN, W - CORNER_IN):
        for y in (CORNER_IN, H - CORNER_IN):
            s = s.union(cyl(8.0, COVER_T, ZF + 0.1, x, y))
            s = s.cut(cyl(SCREW_PILOT, COVER_T - 1, COVER_T + 14, x, y))

    # cat peg blocks under the top wall, with square sockets
    for x in cat_peg_xs():
        z0, z1 = CAT_PEG_Z - 4, ZF + 0.1      # block grows off the front wall so it prints without overhang
        blk = cq.Workplane("XY").box(12, 10, z1 - z0).translate((x, H - WALL - 5 + 0.05, (z0 + z1) / 2))
        s = s.union(blk)
        sock = cq.Workplane("XY").box(CAT_PEG_HOLE, CAT_PEG_L + 1.0, CAT_PEG_HOLE).translate(
            (x, H - (CAT_PEG_L + 1.0) / 2 + 0.01, CAT_PEG_Z))
        s = s.cut(sock)

    # screen window
    s = s.cut(rrect(WIN_W, WIN_H, WIN_R, FRONT + 2, SCREEN_CX, SCREEN_CY, ZF - 1))

    # clearance holes for M3 screws that hold the front decorations (driven from inside)
    for x, y in bezel_screws() + plate_screws() + foot_screws():
        s = s.cut(cyl(SCREW_CLEAR, ZF - 1, D + 1, x, y))
    return s

# ---------------------------------------------------------------- screen retainer
def make_retainer():
    r = rrect(2 * BOSS_X + 12, 2 * BOSS_Y + 12, 6, RET_T, SCREEN_CX, SCREEN_CY, Z_RET_BACK)
    r = r.cut(rrect(86, 46, 4, RET_T + 2, SCREEN_CX, SCREEN_CY, Z_RET_BACK - 1))
    for sx in (-1, 1):
        for sy in (-1, 1):
            hx, hy = SCREEN_CX + sx * HOLE_DX / 2, SCREEN_CY + sy * HOLE_DY / 2
            r = r.union(cyl(9.0, Z_RET_FRONT - 0.01, Z_PAD_FACE, hx, hy))
            r = r.cut(cyl(SCREW_CLEAR, Z_RET_BACK - 1, Z_PAD_FACE + 1, hx, hy))   # optional screw into board holes
            bx, by = SCREEN_CX + sx * BOSS_X, SCREEN_CY + sy * BOSS_Y
            r = r.cut(cyl(SCREW_CLEAR, Z_RET_BACK - 1, Z_RET_FRONT + 1, bx, by))
    return r

# ---------------------------------------------------------------- back cover
def make_cover():
    c = rrect(W - 2 * WALL - 0.4, H - 2 * WALL - 0.4, R_CORNER - WALL, COVER_T, W / 2, H / 2, 0)
    for x in (CORNER_IN, W - CORNER_IN):
        for y in (CORNER_IN, H - CORNER_IN):
            c = c.cut(cyl(SCREW_CLEAR, -1, COVER_T + 1, x, y))
            c = c.cut(cq.Workplane("XY", origin=(x, y, -0.01)).circle(3.2).workplane(offset=1.8).circle(1.7).loft())
    # keyhole slots for wall hanging, 75 mm apart
    for sx in (-1, 1):
        x, y = W / 2 + sx * 37.5, H - 34
        c = c.cut(cyl(8.5, -1, COVER_T + 1, x, y - 6))
        c = c.cut(rrect(4.2, 12, 2.0, COVER_T + 2, x, y, -1))
    # vents
    for x in (24, W - 24):
        for i in range(5):
            c = c.cut(rrect(3, 22, 1.4, COVER_T + 2, x + (i - 2) * 6, H / 2 + 6, -1))
    # USB-C cable notch, bottom centre
    c = c.cut(rrect(16, 16, 3, COVER_T + 2, W / 2, WALL + 2, -1))
    return c

# ---------------------------------------------------------------- front decorations
def pilots(wp, pts):
    for x, y in pts:
        wp = wp.cut(cyl(SCREW_PILOT, D - 1, D + DECO_PILOT_DEPTH, x, y))
    return wp

def make_bezel():
    b = rrect(BEZEL_W, BEZEL_H, BEZEL_R, BEZEL_T, SCREEN_CX, SCREEN_CY, D)
    b = b.cut(rrect(WIN_W, WIN_H, WIN_R + 1, BEZEL_T + 2, SCREEN_CX, SCREEN_CY, D - 1))
    b = b.faces(">Z").edges().fillet(1.2)
    return pilots(b, bezel_screws())

def heart(size, x, y, z0, t):
    return (cq.Workplane("XY", origin=(0, 0, z0))
            .moveTo(x, y - size * 0.5)
            .lineTo(x - size * 0.5, y + size * 0.05)
            .threePointArc((x - size * 0.25, y + size * 0.42), (x, y + size * 0.15))
            .threePointArc((x + size * 0.25, y + size * 0.42), (x + size * 0.5, y + size * 0.05))
            .close().extrude(t))

def make_plate():
    p = rrect(PLATE_W, PLATE_H, PLATE_H / 2 - 0.01, PLATE_T, W / 2, PLATE_CY, D)
    p = p.faces(">Z").edges().fillet(1.2)
    return pilots(p, plate_screws())

def make_plate_text():
    z = D + PLATE_T
    t = (cq.Workplane("XY", origin=(W / 2, PLATE_CY - 0.5, z))
         .text(TEXT, 11.0, 0.8, font="DejaVu Sans", kind="bold", halign="center", valign="center"))
    bb = t.val().BoundingBox()
    for sx, edge in ((-1, bb.xmin), (1, bb.xmax)):
        t = t.union(heart(7.0, edge + sx * 7.5, PLATE_CY, z, 0.8))
    return t

def make_feet():
    out = []
    for i, x in enumerate(foot_centres()):
        y = FOOT_Y
        f = cyl(FOOT_D, D, D + FOOT_T, x, y)
        f = f.cut(cq.Workplane("XY").box(60, 20, FOOT_T + 2).translate((x, -10, D + FOOT_T / 2)))  # flat bottom on table
        f = f.faces(">Z").edges().fillet(2.0)
        f = pilots(f, [foot_screws()[i]])
        pads = ellipse(6.5, 5.5, D + FOOT_T - 0.01, 0.8, x, y - 3)
        for dx, dy in ((-7.5, 5.0), (-2.6, 9.0), (2.6, 9.0), (7.5, 5.0)):
            pads = pads.union(ellipse(2.6, 3.0, D + FOOT_T - 0.01, 0.8, x + dx, y + dy))
        out.append((f, pads))
    return out

# ---------------------------------------------------------------- cats
def cat_outline(cx):
    z0 = D - CAT_T
    base = H
    head = ellipse(CAT_W / 2, 30, z0, CAT_T, cx, base + 22)
    head = head.cut(cq.Workplane("XY").box(120, 40, CAT_T + 2).translate((cx, base - 20, z0 + CAT_T / 2)))
    for s in (-1, 1):
        ear = (cq.Workplane("XY", origin=(0, 0, z0))
               .polyline([(cx + s * 44, base + 30), (cx + s * 38, base + CAT_H), (cx + s * 12, base + 46)])
               .close().extrude(CAT_T))
        head = head.union(ear)
    return head

def make_cat(cx, kind):
    body = cat_outline(cx)
    body = body.faces(">Z").edges().fillet(2.5)
    for s in (-1, 1):                       # paws draped over the front edge
        p = ellipse(11, 9, D - 0.01, 6.0, cx + s * 22, H - 1).edges(">Z").fillet(2.5)
        body = body.union(p)
    for dx in (-22, 22):                    # square pegs into the shell top
        body = body.union(cq.Workplane("XY").box(CAT_PEG, CAT_PEG_L + 0.2, CAT_PEG)
                          .translate((cx + dx, H - CAT_PEG_L / 2 + 0.1, CAT_PEG_Z)))
    zf, base = D, H
    feats = {"white": None, "black": None, "pink": None}
    def add(col, solid):
        feats[col] = solid if feats[col] is None else feats[col].union(solid)
    ex, ey = 15.0, base + 24
    for s in (-1, 1):                       # inner ears
        add("white", cq.Workplane("XY", origin=(0, 0, zf - 0.01))
            .polyline([(cx + s * 38, base + 36), (cx + s * 35.5, base + CAT_H - 7), (cx + s * 20, base + 46)])
            .close().extrude(FACE_T))
    if kind == "black":
        for s in (-1, 1):
            add("white", ellipse(9.5, 11.5, zf - 0.01, FACE_T, cx + s * ex, ey))
            add("black", ellipse(5.0, 7.0, zf + FACE_T - 0.01, FACE_T, cx + s * ex + 1.5, ey - 1))
            add("white", ellipse(1.6, 1.6, zf + 2 * FACE_T - 0.01, 0.6, cx + s * ex + 3.0, ey + 2.5))
        add("pink", heart(7.0, cx, ey - 12, zf - 0.01, FACE_T))
        wcol = "white"
    else:
        add("white", ellipse(15, 10, zf - 0.01, FACE_T, cx, ey - 15))
        for s in (-1, 1):
            add("black", ellipse(5.5, 7.5, zf - 0.01, FACE_T, cx + s * ex, ey))
        add("black", ellipse(3.5, 2.4, zf + FACE_T - 0.01, FACE_T, cx, ey - 9))
        wcol = "black"
    for s in (-1, 1):                       # whiskers
        for i, ang in enumerate((8, -8)):
            add(wcol, cq.Workplane("XY", origin=(cx + s * 30, ey - 10 - i * 5, zf - 0.01))
                .transformed(rotate=(0, 0, s * ang)).rect(14, 1.6).extrude(FACE_T))
    return body, feats

# ---------------------------------------------------------------- reference only (not printed)
def make_screen_dummy():
    g = rrect(GLASS_W, GLASS_H, 1.0, 1.2, SCREEN_CX, SCREEN_CY, ZF - 1.2)
    pcb = rrect(GLASS_W - 0.4, GLASS_H - 0.4, 1.0, MODULE_T - 1.2, SCREEN_CX, SCREEN_CY, Z_PCB_BACK)
    return g, pcb

# ---------------------------------------------------------------- build
COLORS = {"white": (0.95, 0.94, 0.92), "pink": (0.93, 0.18, 0.50), "black": (0.12, 0.12, 0.14),
          "glass": (0.10, 0.35, 0.75), "pcb": (0.10, 0.40, 0.20)}

def build():
    parts = {}
    parts["shell"] = (make_shell(), "white")
    parts["back_cover"] = (make_cover(), "white")
    parts["screen_retainer"] = (make_retainer(), "white")
    parts["bezel"] = (make_bezel(), "pink")
    parts["nameplate"] = (make_plate(), "pink")
    parts["nameplate_text"] = (make_plate_text(), "white")
    for i, (f, pads) in enumerate(make_feet()):
        parts[f"foot_{'LR'[i]}"] = (f, "white")
        parts[f"foot_{'LR'[i]}_pads"] = (pads, "pink")
    c1, c2 = cat_centres()
    for name, cx, kind in (("cat_black", c1, "black"), ("cat_pink", c2, "pink")):
        body, feats = make_cat(cx, kind)
        parts[name] = (body, kind)
        for k, v in feats.items():
            if v is not None:
                parts[f"{name}_{k}"] = (v, k)
    g, pcb = make_screen_dummy()
    parts["REF_screen_glass"] = (g, "glass")
    parts["REF_screen_pcb"] = (pcb, "pcb")
    return parts

if __name__ == "__main__":
    import glob
    for f in glob.glob(os.path.join(OUT, "_asm_*.stl")):
        os.remove(f)
    parts = build()
    assy = cq.Assembly(name="weather_besties")
    for n, (wp, col) in parts.items():
        if n.startswith("cat_"):
            continue          # cats are sculpted meshes, see sculpt_cats.py
        assy.add(wp, name=n, color=cq.Color(*COLORS[col]))
    assy.export(os.path.join(OUT, "weather_besties_assembly.step"))
    for n, (wp, col) in parts.items():
        if n.startswith("cat_"):
            continue
        cq.exporters.export(wp, os.path.join(OUT, f"_asm_{n}.stl"), tolerance=0.05, angularTolerance=0.15)
    print("parts:", list(parts))
