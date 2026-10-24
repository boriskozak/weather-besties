"""Write print-oriented STLs into out/print/, grouped so multi-colour parts share one frame."""
import os, sys, shutil
import cadquery as cq
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import weather_besties as wb

P = os.path.join(wb.OUT, "print")
os.makedirs(P, exist_ok=True)
parts = {k: v[0].val() for k, v in wb.build().items()}

# group name -> (members, flip_face_down)
groups = {
    "01_shell":            (["shell"], True),
    "02_back_cover":       (["back_cover"], False),
    "03_screen_retainer":  (["screen_retainer"], False),
    "04_bezel":            (["bezel"], False),
    "05_nameplate":        (["nameplate", "nameplate_text"], False),
    "06_foot_left":        (["foot_L", "foot_L_pads"], False),
    "07_foot_right":       (["foot_R", "foot_R_pads"], False),
}
colour_of = lambda n: (n.split("_")[-1] if n.split("_")[-1] in ("white", "black", "pink")
                       else {"nameplate_text": "white", "foot_L_pads": "pink", "foot_R_pads": "pink",
                             "bezel": "pink", "nameplate": "pink", "cat_pink": "pink", "cat_black": "black"}.get(n, "white"))
for g, (members, flip) in groups.items():
    shapes = [parts[m] for m in members]
    if flip:
        shapes = [s.rotate((0, 0, 0), (0, 1, 0), 180) for s in shapes]
    bb = shapes[0].BoundingBox()
    off = cq.Vector(-bb.xmin, -bb.ymin, -bb.zmin)
    for m, s in zip(members, shapes):
        s = s.translate(off)
        suffix = "" if len(members) == 1 else f"__{m}_{colour_of(m)}".replace("_white_white", "_white").replace("_black_black", "_black").replace("_pink_pink", "_pink")
        cq.exporters.export(s, os.path.join(P, f"{g}{suffix}.stl"), tolerance=0.03, angularTolerance=0.1)
        b = s.BoundingBox()
        print(f"{g}{suffix:42s} {b.xlen:6.1f} x {b.ylen:6.1f} x {b.zlen:5.1f}  zmin {b.zmin:5.2f}")
