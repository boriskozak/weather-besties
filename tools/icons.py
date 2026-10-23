"""Weather Besties artwork, drawn as SVG and rendered to PNG.

Every icon lives in a 100 x 100 box. Shapes get a navy "sticker" outline by
drawing them once fat in navy, then again in their fill colour on top.
"""
import math

O = "#2B3450"      # outline navy
SW = 3.2           # outline thickness (outward)
PINK = "#FF9EC4"
WHITE = "#FFFFFF"


def svg(body, vb="0 0 100 100"):
    return f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{vb}">{body}</svg>'


def stick(shapes, fill, sw=SW, extra=""):
    """Outlined union of shapes (list of svg elements without paint)."""
    s = "".join(shapes)
    return (f'<g fill="{O}" stroke="{O}" stroke-width="{2*sw}" stroke-linejoin="round" {extra}>{s}</g>'
            f'<g fill="{fill}" {extra}>{s}</g>')


def line_stick(d, color, w, sw=SW):
    """Outlined stroke (for rays, fog bars, flakes)."""
    return (f'<path d="{d}" fill="none" stroke="{O}" stroke-width="{w+2*sw}" stroke-linecap="round" stroke-linejoin="round"/>'
            f'<path d="{d}" fill="none" stroke="{color}" stroke-width="{w}" stroke-linecap="round" stroke-linejoin="round"/>')


# ---------------------------------------------------------------- faces
def face(cx, cy, k=1.0, mood="happy"):
    ex = 9.5 * k
    er = 2.8 * k
    out = ""
    for sx in (-1, 1):
        x = cx + sx * ex
        out += f'<circle cx="{x:.2f}" cy="{cy:.2f}" r="{er:.2f}" fill="{O}"/>'
        out += f'<circle cx="{x-0.9*k:.2f}" cy="{cy-1.0*k:.2f}" r="{0.95*k:.2f}" fill="#fff"/>'
        out += (f'<ellipse cx="{cx+sx*15*k:.2f}" cy="{cy+6*k:.2f}" rx="{3.8*k:.2f}" ry="{2.5*k:.2f}" '
                f'fill="{PINK}" opacity="0.85"/>')
    if mood == "happy":
        out += (f'<path d="M{cx-6*k:.2f},{cy+5.5*k:.2f} Q{cx:.2f},{cy+11*k:.2f} {cx+6*k:.2f},{cy+5.5*k:.2f}" '
                f'fill="none" stroke="{O}" stroke-width="{2.4*k:.2f}" stroke-linecap="round"/>')
    elif mood == "oh":
        out += f'<ellipse cx="{cx:.2f}" cy="{cy+7.5*k:.2f}" rx="{2.3*k:.2f}" ry="{2.8*k:.2f}" fill="{O}"/>'
    elif mood == "sleepy":
        # closed eyes drawn over the open ones
        out = ""
        for sx in (-1, 1):
            x = cx + sx * ex
            out += (f'<path d="M{x-3*k:.2f},{cy:.2f} Q{x:.2f},{cy+3*k:.2f} {x+3*k:.2f},{cy:.2f}" fill="none" '
                    f'stroke="{O}" stroke-width="{2.2*k:.2f}" stroke-linecap="round"/>')
            out += (f'<ellipse cx="{cx+sx*15*k:.2f}" cy="{cy+6*k:.2f}" rx="{3.8*k:.2f}" ry="{2.5*k:.2f}" '
                    f'fill="{PINK}" opacity="0.85"/>')
        out += (f'<path d="M{cx-4*k:.2f},{cy+6*k:.2f} Q{cx:.2f},{cy+9*k:.2f} {cx+4*k:.2f},{cy+6*k:.2f}" '
                f'fill="none" stroke="{O}" stroke-width="{2.2*k:.2f}" stroke-linecap="round"/>')
    return out


# ---------------------------------------------------------------- parts
CLOUD = ['<rect x="16" y="48" width="70" height="28" rx="14"/>',
         '<circle cx="34" cy="52" r="16"/>',
         '<circle cx="55" cy="42" r="21"/>',
         '<circle cx="73" cy="55" r="13"/>']


def cloud(tx=0, ty=0, s=1.0, fill=WHITE, mood="happy", show_face=True):
    g = stick(CLOUD, fill)
    if show_face:
        g += face(54.5, 59, 1.0, mood)
    return f'<g transform="translate({tx},{ty}) scale({s})">{g}</g>'


def sun(cx=50, cy=50, r=23, rays=True, show_face=True, ray_in=None, ray_out=None):
    out = ""
    if rays:
        ri = ray_in if ray_in else r + 8
        ro = ray_out if ray_out else r + 19
        d = ""
        for i in range(8):
            a = math.radians(i * 45 + 22.5)
            d += (f"M{cx+ri*math.cos(a):.2f},{cy+ri*math.sin(a):.2f} "
                  f"L{cx+ro*math.cos(a):.2f},{cy+ro*math.sin(a):.2f} ")
        out += line_stick(d, "#FFB020", 6.0)
    out += stick([f'<circle cx="{cx}" cy="{cy}" r="{r}"/>'], "#FFD54A")
    if show_face:
        out += face(cx, cy - 1, r / 23.0 * 0.95, "happy")
    return out


def crescent_path(cx, cy, r, ccx, ccy, cr):
    """Path for circle (cx,cy,r) minus circle (ccx,ccy,cr)."""
    d = math.hypot(ccx - cx, ccy - cy)
    a = (r * r - cr * cr + d * d) / (2 * d)
    h = math.sqrt(max(r * r - a * a, 0))
    ux, uy = (ccx - cx) / d, (ccy - cy) / d
    px, py = cx + a * ux, cy + a * uy
    p1 = (px + h * uy, py - h * ux)
    p2 = (px - h * uy, py + h * ux)
    return (f"M{p1[0]:.2f},{p1[1]:.2f} A{r},{r} 0 1 0 {p2[0]:.2f},{p2[1]:.2f} "
            f"A{cr},{cr} 0 0 1 {p1[0]:.2f},{p1[1]:.2f} Z")


def moon(cx=48, cy=50, r=27, show_face=True):
    d = crescent_path(cx, cy, r, cx + 13 * r / 27, cy - 10 * r / 27, 23 * r / 27)
    out = stick([f'<path d="{d}"/>'], "#FFE38A")
    if show_face:
        k = r / 27
        out += face(cx - 9 * k, cy + 5 * k, 0.72 * k, "sleepy")
    return out


def sparkle(cx, cy, s, fill="#FFE38A"):
    d = (f"M{cx},{cy-s} Q{cx+s*0.22},{cy-s*0.22} {cx+s},{cy} Q{cx+s*0.22},{cy+s*0.22} {cx},{cy+s} "
         f"Q{cx-s*0.22},{cy+s*0.22} {cx-s},{cy} Q{cx-s*0.22},{cy-s*0.22} {cx},{cy-s} Z")
    return stick([f'<path d="{d}"/>'], fill, sw=2.2)


def drop(x, y, s, fill="#4DA6FF", sw=2.6):
    d = (f"M{x:.2f},{y-1.25*s:.2f} C{x+0.4*s:.2f},{y-0.6*s:.2f} {x+s:.2f},{y-0.15*s:.2f} {x+s:.2f},{y+0.3*s:.2f} "
         f"A{s:.2f},{s:.2f} 0 0 1 {x-s:.2f},{y+0.3*s:.2f} "
         f"C{x-s:.2f},{y-0.15*s:.2f} {x-0.4*s:.2f},{y-0.6*s:.2f} {x:.2f},{y-1.25*s:.2f} Z")
    return stick([f'<path d="{d}"/>'], fill, sw=sw)


def flake(x, y, r, color="#FFFFFF"):
    d = ""
    for i in range(3):
        a = math.radians(90 + i * 60)
        d += (f"M{x-r*math.cos(a):.2f},{y-r*math.sin(a):.2f} L{x+r*math.cos(a):.2f},{y+r*math.sin(a):.2f} ")
    return (f'<path d="{d}" stroke="{O}" stroke-width="{3.2+2*2.4}" stroke-linecap="round"/>'
            f'<path d="{d}" stroke="#8FD0FF" stroke-width="3.2" stroke-linecap="round"/>'
            f'<circle cx="{x}" cy="{y}" r="2" fill="{color}"/>')


def bolt(tx=0, ty=0):
    pts = "57,57 45,77 54,77 48,96 69,71 59,71 65,57"
    return f'<g transform="translate({tx},{ty})">' + stick([f'<polygon points="{pts}"/>'], "#FFD43B") + "</g>"


# ---------------------------------------------------------------- weather icons
def w_clear_day():
    return svg(sun(50, 50, 24))


def w_clear_night():
    return svg(moon(46, 52, 28) + sparkle(78, 24, 7) + sparkle(84, 56, 4.5) + sparkle(22, 20, 4))


def w_partly_day():
    return svg(sun(36, 34, 17, show_face=False, ray_in=23, ray_out=31) + cloud(10, 14, 0.9))


def w_partly_night():
    return svg(moon(34, 34, 19, show_face=False) + sparkle(78, 18, 5) + cloud(10, 14, 0.9, mood="sleepy"))


def w_cloudy():
    return svg(cloud(22, -8, 0.72, fill="#CFD9E8", show_face=False) + cloud(2, 12, 0.92))


def up_cloud(fill=WHITE, mood="happy"):
    return cloud(6, -9, 0.9, fill=fill, mood=mood)


def w_fog():
    bars = ""
    for (x1, x2, y) in ((20, 66, 74), (34, 84, 84), (24, 60, 94)):
        bars += line_stick(f"M{x1},{y} L{x2},{y}", "#B7C3D4", 6.0, sw=2.6)
    return svg(up_cloud("#E6ECF3", "sleepy") + bars)


def w_drizzle():
    return svg(up_cloud() + drop(36, 80, 4.2, "#7CC0FF") + drop(56, 86, 4.2, "#7CC0FF") + drop(74, 78, 4.2, "#7CC0FF"))


def w_rain():
    return svg(up_cloud("#DCE4EF", "oh") + drop(32, 80, 5.6) + drop(52, 88, 5.6) + drop(72, 80, 5.6))


def w_heavy_rain():
    d = (drop(28, 76, 4.8) + drop(50, 76, 4.8) + drop(72, 76, 4.8) +
         drop(39, 92, 4.4) + drop(61, 92, 4.4))
    return svg(up_cloud("#BFCBDD", "oh") + d)


def w_snow():
    return svg(up_cloud() + flake(32, 82, 7.5) + flake(54, 90, 7.5) + flake(74, 80, 7.5))


def w_sleet():
    return svg(up_cloud("#DCE4EF", "oh") + drop(32, 80, 5.2) + flake(54, 88, 7.0) + drop(74, 80, 5.2))


def w_thunder():
    return svg(bolt(0, 0) + up_cloud("#A9B6CC", "oh") + drop(28, 82, 4.6) + drop(84, 84, 4.6))


WEATHER = {
    "clear_day": w_clear_day, "clear_night": w_clear_night,
    "partly_day": w_partly_day, "partly_night": w_partly_night,
    "cloudy": w_cloudy, "fog": w_fog, "drizzle": w_drizzle, "rain": w_rain,
    "heavy_rain": w_heavy_rain, "snow": w_snow, "sleet": w_sleet, "thunder": w_thunder,
}


# ---------------------------------------------------------------- clothing
def c_tshirt():
    body = ('<path d="M30,22 L42,17 Q50,25 58,17 L70,22 L87,37 L77,50 L68,43 L68,85 Q50,89 32,85 '
            'L32,43 L23,50 L13,37 Z"/>')
    return svg(stick([body], "#FF8FB1") +
               f'<path d="M42,17 Q50,28 58,17" fill="none" stroke="{O}" stroke-width="2.6"/>'
               f'<circle cx="50" cy="52" r="7" fill="#fff" opacity="0.9"/>')


def sleeves_body(fill, extra=""):
    body = ('<path d="M31,21 L43,16 Q50,23 57,16 L69,21 Q80,26 83,40 L86,79 L74,81 L70,47 L70,86 '
            'Q50,90 30,86 L30,47 L26,81 L14,79 L17,40 Q20,26 31,21 Z"/>')
    return stick([body], fill) + extra


def c_sweater():
    ex = (f'<path d="M30,78 Q50,82 70,78" fill="none" stroke="{O}" stroke-width="2.4"/>'
          f'<path d="M15,72 L25,74 M75,74 L85,72" stroke="{O}" stroke-width="2.4" stroke-linecap="round"/>'
          f'<path d="M43,16 Q50,26 57,16" fill="none" stroke="{O}" stroke-width="2.4"/>'
          f'<path d="M38,40 L44,46 L38,52 M50,40 L56,46 L50,52 M62,40" fill="none" stroke="#fff" '
          f'stroke-width="2.6" stroke-linecap="round" stroke-linejoin="round" opacity="0.9"/>')
    return svg(sleeves_body("#7FD1AE", ex))


def c_jacket():
    ex = (f'<path d="M50,20 L50,86" stroke="{O}" stroke-width="2.6"/>'
          f'<path d="M43,16 L50,28 L57,16" fill="#FFD9A0" stroke="{O}" stroke-width="2.4" stroke-linejoin="round"/>'
          f'<rect x="36" y="58" width="9" height="11" rx="2" fill="none" stroke="{O}" stroke-width="2.2"/>'
          f'<rect x="55" y="58" width="9" height="11" rx="2" fill="none" stroke="{O}" stroke-width="2.2"/>'
          f'<circle cx="50" cy="34" r="2.2" fill="{O}"/>')
    return svg(sleeves_body("#FFB454", ex))


def c_coat():
    q = ""
    for y in (36, 52, 68):
        q += f'<path d="M31,{y} Q50,{y+4} 69,{y}" fill="none" stroke="{O}" stroke-width="2.2" opacity="0.8"/>'
    q += f'<path d="M50,24 L50,87" stroke="{O}" stroke-width="2.6"/>'
    hood = stick(['<path d="M33,24 Q50,8 67,24 Q50,32 33,24 Z"/>'], "#F4F1EA", sw=2.6)
    return svg(sleeves_body("#6F8CFF", q) + hood)


def c_beanie():
    dome = '<path d="M22,66 Q22,24 50,24 Q78,24 78,66 Z"/>'
    out = stick([dome], "#FF6B6B")
    for x in (36, 50, 64):
        out += f'<path d="M{x},30 L{x},64" stroke="#E2504F" stroke-width="3" stroke-linecap="round"/>'
    out += stick(['<rect x="17" y="60" width="66" height="18" rx="7"/>'], "#FFFFFF")
    out += stick(['<circle cx="50" cy="20" r="10"/>'], "#FFFFFF")
    return svg(out)


def mitten(tx, rot, fill):
    shapes = ['<rect x="38" y="28" width="30" height="50" rx="15"/>',
              '<ellipse cx="36" cy="58" rx="7.5" ry="12" transform="rotate(-28 36 58)"/>']
    g = stick(shapes, fill)
    g += stick(['<rect x="36" y="72" width="34" height="14" rx="5"/>'], "#FFFFFF")
    return f'<g transform="translate({tx},0) rotate({rot} 52 60)">{g}</g>'


def c_mittens():
    return svg(mitten(-14, -10, "#9C8CFF") + mitten(16, 10, "#B6AAFF"))


def c_umbrella():
    canopy = ('<path d="M12,54 Q14,18 50,16 Q86,18 88,54 Q82,47 75,54 Q69,47 62.5,54 Q56,47 50,54 '
              'Q44,47 37.5,54 Q31,47 25,54 Q18,47 12,54 Z"/>')
    out = (f'<path d="M50,52 L50,82 Q50,90 43,90 Q36,90 36,83" fill="none" stroke="{O}" stroke-width="5.4" '
           f'stroke-linecap="round"/>')
    out += stick([canopy], "#FF5C8A")
    out += (f'<path d="M50,17 Q40,32 37.5,52 M50,17 Q60,32 62.5,52" fill="none" stroke="{O}" '
            f'stroke-width="2.2" opacity="0.7"/>')
    out += f'<path d="M50,9 L50,16" stroke="{O}" stroke-width="4" stroke-linecap="round"/>'
    return svg(out)


def c_boots():
    boot = '<path d="M28,18 L52,18 L52,60 L74,64 Q84,66 84,76 L84,80 L28,80 Z"/>'
    out = stick([boot], "#FFD23F")
    out += stick(['<rect x="24" y="78" width="64" height="9" rx="4"/>'], "#2B3450", sw=1.2)
    out += f'<rect x="26" y="14" width="30" height="9" rx="4" fill="#FFE98A" stroke="{O}" stroke-width="2.6"/>'
    return svg(out)


def c_sunglasses():
    lens = ['<rect x="10" y="38" width="35" height="28" rx="12"/>', '<rect x="55" y="38" width="35" height="28" rx="12"/>']
    out = (f'<path d="M45,46 Q50,40 55,46" fill="none" stroke="{O}" stroke-width="4.5" stroke-linecap="round"/>'
           f'<path d="M10,44 L4,36 M90,44 L96,36" stroke="{O}" stroke-width="4.5" stroke-linecap="round"/>')
    out += stick(lens, "#3E4A6B")
    out += ('<path d="M17,46 L26,46 M62,46 L71,46" stroke="#ffffff" stroke-width="3" stroke-linecap="round" '
            'opacity="0.75"/>')
    return svg(out)


def c_water():
    out = stick(['<rect x="32" y="28" width="36" height="62" rx="12"/>'], "#BDEBFF")
    out += ('<path d="M34,52 Q50,47 66,52 L66,78 Q66,88 56,88 L44,88 Q34,88 34,78 Z" fill="#4DC3FF"/>')
    out += f'<rect x="32" y="28" width="36" height="62" rx="12" fill="none" stroke="{O}" stroke-width="3.2"/>'
    out += stick(['<rect x="39" y="12" width="22" height="16" rx="5"/>'], "#FF8FB1")
    out += '<path d="M40,60 L40,78" stroke="#fff" stroke-width="3.4" stroke-linecap="round" opacity="0.8"/>'
    return svg(out)


CLOTHES = {
    "tshirt": c_tshirt, "sweater": c_sweater, "jacket": c_jacket, "coat": c_coat,
    "beanie": c_beanie, "mittens": c_mittens, "umbrella": c_umbrella, "boots": c_boots,
    "sunglasses": c_sunglasses, "water": c_water,
}


# ---------------------------------------------------------------- cats (splash / setup screens)
def cat(body, inner, eye, nose="#FF7EB3"):
    head = ['<ellipse cx="50" cy="58" rx="38" ry="32"/>',
            '<path d="M16,48 L20,10 L44,32 Z"/>', '<path d="M84,48 L80,10 L56,32 Z"/>']
    out = stick(head, body)
    out += f'<path d="M22,40 L24,20 L37,32 Z" fill="{inner}"/><path d="M78,40 L76,20 L63,32 Z" fill="{inner}"/>'
    for x in (36, 64):
        out += f'<ellipse cx="{x}" cy="56" rx="6.5" ry="8" fill="{eye}" stroke="{O}" stroke-width="2"/>'
        out += f'<ellipse cx="{x}" cy="57" rx="3.4" ry="5.6" fill="{O}"/>'
        out += f'<circle cx="{x-2}" cy="53" r="1.8" fill="#fff"/>'
    out += f'<path d="M46,66 L54,66 L50,71 Z" fill="{nose}" stroke="{O}" stroke-width="1.6" stroke-linejoin="round"/>'
    out += (f'<path d="M50,71 Q46,77 42,74 M50,71 Q54,77 58,74" fill="none" stroke="{O}" stroke-width="2" '
            f'stroke-linecap="round"/>')
    wc = "#FFFFFF" if body.lower() in ("#2e2e3a",) else O
    for sgn in (-1, 1):
        for dy, ang in ((0, 0), (6, 6)):
            x1 = 50 + sgn * 22
            x2 = 50 + sgn * 42
            out += (f'<path d="M{x1},{66+dy} L{x2},{64+dy+ang}" stroke="{wc}" stroke-width="1.8" '
                    f'stroke-linecap="round" opacity="0.85"/>')
    for x in (26, 74):
        out += f'<ellipse cx="{x}" cy="70" rx="5" ry="3.2" fill="{PINK}" opacity="0.8"/>'
    return svg(out)


CATS = {
    "cat_black": lambda: cat("#2E2E3A", "#FF9EC4", "#C8F07A"),
    "cat_pink": lambda: cat("#FFB3CF", "#FF7EB3", "#7FD7FF"),
}


def tiny_drop():
    return svg(drop(50, 56, 30, "#4DA6FF", sw=6))
