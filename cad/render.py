"""Offscreen VTK renders of the assembly: front, back, exploded."""
import glob, os, sys
import vtk
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from weather_besties import COLORS

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "out")

def colour(name):
    if name == "REF_screen_glass": return COLORS["glass"]
    if name == "REF_screen_pcb": return COLORS["pcb"]
    if name == "screen_retainer": return (0.78, 0.80, 0.84)
    if name.endswith("_white") or name.endswith("_text"): return COLORS["white"]
    if name.endswith("_black"): return COLORS["black"]
    if name.endswith("_pink") or name.endswith("_pads"): return COLORS["pink"]
    if name in ("bezel", "nameplate", "cat_pink"): return COLORS["pink"]
    if name == "cat_black": return COLORS["black"]
    return COLORS["white"]

SCREEN_ACTIVE = (95.54, 54.36)   # visible LCD area, mm (800 x 480 px)

def screen_actor(png, cx=100.0, cy=69.0, z=45.05):
    """The UI screenshot as a self-lit plane on the glass front."""
    w, h = SCREEN_ACTIVE
    pl = vtk.vtkPlaneSource()
    pl.SetOrigin(cx - w / 2, cy - h / 2, z); pl.SetPoint1(cx + w / 2, cy - h / 2, z); pl.SetPoint2(cx - w / 2, cy + h / 2, z)
    rd = vtk.vtkPNGReader(); rd.SetFileName(png)
    tex = vtk.vtkTexture(); tex.SetInputConnection(rd.GetOutputPort()); tex.InterpolateOn()
    mp = vtk.vtkPolyDataMapper(); mp.SetInputConnection(pl.GetOutputPort())
    a = vtk.vtkActor(); a.SetMapper(mp); a.SetTexture(tex)
    pr = a.GetProperty(); pr.SetAmbient(1.0); pr.SetDiffuse(0.0); pr.SetSpecular(0.0); pr.SetAmbientColor(1, 1, 1); pr.SetColor(1, 1, 1)
    pr.LightingOff()
    return a

def render(fname, cam_pos, focal=(100, 85, 24), explode=None, size=(1400, 1100), hide=(), view_up=(0, 1, 0), zoom=1.0, screen_png=None):
    ren = vtk.vtkRenderer(); ren.SetBackground(0.957, 0.945, 0.925)
    win = vtk.vtkRenderWindow(); win.SetOffScreenRendering(1); win.AddRenderer(ren); win.SetSize(*size)
    win.SetMultiSamples(8)
    for p in sorted(glob.glob(f"{OUT}/_asm_*.stl")):
        n = os.path.basename(p)[5:-4]
        if n in hide: continue
        rd = vtk.vtkSTLReader(); rd.SetFileName(p)
        nm = vtk.vtkPolyDataNormals(); nm.SetInputConnection(rd.GetOutputPort()); nm.SetFeatureAngle(35)
        mp = vtk.vtkPolyDataMapper(); mp.SetInputConnection(nm.GetOutputPort())
        a = vtk.vtkActor(); a.SetMapper(mp)
        col = (0.05, 0.05, 0.06) if (screen_png and n == "REF_screen_glass") else colour(n)
        pr = a.GetProperty(); pr.SetColor(*col); pr.SetAmbient(0.25); pr.SetDiffuse(0.75); pr.SetSpecular(0.12); pr.SetSpecularPower(20)
        if explode: a.SetPosition(*explode(n))
        ren.AddActor(a)
    if screen_png:
        sa = screen_actor(screen_png)
        if explode: sa.SetPosition(*explode("REF_screen_glass"))
        ren.AddActor(sa)
    cam = ren.GetActiveCamera(); cam.SetFocalPoint(*focal); cam.SetPosition(*cam_pos); cam.SetViewUp(*view_up)
    cam.SetViewAngle(28); ren.ResetCameraClippingRange(); cam.Zoom(zoom)
    ren.ResetCameraClippingRange()
    lk = vtk.vtkLightKit(); lk.AddLightsToRenderer(ren)
    win.Render()
    w2i = vtk.vtkWindowToImageFilter(); w2i.SetInput(win); w2i.Update()
    wr = vtk.vtkPNGWriter(); wr.SetFileName(f"{OUT}/{fname}"); wr.SetInputConnection(w2i.GetOutputPort()); wr.Write()

if __name__ == "__main__":
    if not glob.glob(f"{OUT}/_asm_*.stl"):
        sys.exit("no assembly meshes in out/: run weather_besties.py, export_print.py and sculpt_cats.py first")
    render("preview_front.png", (-60, 170, 470))
    render("preview_back.png", (330, 160, -360))
    def ex(n):
        if n == "back_cover": return (0, 0, -90)
        if n == "screen_retainer": return (0, 0, -50)
        if n.startswith("cat_"): return (0, 40, 0)
        if n.startswith(("bezel", "nameplate", "foot_")): return (0, 0, 45)
        if n.startswith("REF_"): return (0, 0, -25)
        return (0, 0, 0)
    render("preview_exploded.png", (420, 230, -330), focal=(100, 95, 10), explode=ex)
    render("preview_inside.png", (60, 140, -330), hide=("back_cover",))
    render("preview_cats_side.png", (330, 170, 260), focal=(100, 140, 40), size=(1400, 900))
    shot = os.path.join(os.path.dirname(OUT), "..", "sim", "out", "03_today_oct_morning.png")
    if os.path.exists(shot):
        render("concept_front.png", (-60, 170, 470), screen_png=shot)
        render("concept_straight.png", (100, 95, 520), focal=(100, 92, 24), size=(1400, 1250), screen_png=shot)
    print("ok")
