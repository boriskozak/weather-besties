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


# ---------------------------------------------------------------- the yellow-eyed cat (today screen)
# Drawn in one shared "stage" (CAT_W x CAT_H px, 1 unit = 1 px) so every layer
# lines up: the cat peeks over the top edge of the first forecast card, which
# sits at y = CAT_EDGE. make_assets.py crops each layer to its pixels and
# records where it goes, so the firmware just stacks them.
CAT_W, CAT_H = 184, 100
CAT_EDGE = 94          # card top edge, in stage coordinates
CSW = 2.2              # outline at this scale (matches the 72 px period icons)
CAT_BODY = "#2E2E3A"
CAT_EYE = "#FFD23F"
CAT_LINE = "#C9C9DA"   # mouth / closed eyes, light enough to read on black


def cstick(shapes, fill, sw=CSW, rim=None, extra=""):
    """stick() at cat scale; rim adds a white die-cut edge outside the navy."""
    s = "".join(shapes)
    out = ""
    if rim:
        out += (f'<g fill="{rim}" stroke="{rim}" stroke-width="{2*(sw+1.8)}" stroke-linejoin="round" '
                f'opacity="0.9" {extra}>{s}</g>')
    out += (f'<g fill="{O}" stroke="{O}" stroke-width="{2*sw}" stroke-linejoin="round" {extra}>{s}</g>'
            f'<g fill="{fill}" {extra}>{s}</g>')
    return out


def stage(body):
    return svg(body, f"0 0 {CAT_W} {CAT_H}")


EYES = ((48, 63), (72, 63))


def cat_base():
    head = ['<ellipse cx="60" cy="66" rx="32" ry="26"/>',
            '<path d="M30,57 L34,24 L55,43 Z"/>', '<path d="M90,57 L86,24 L65,43 Z"/>']
    paws = ['<ellipse cx="45" cy="94" rx="10.5" ry="6.5"/>', '<ellipse cx="75" cy="94" rx="10.5" ry="6.5"/>']
    out = cstick(head + paws, CAT_BODY, rim="#FFFFFF")       # rim around the whole silhouette
    out += cstick(head, CAT_BODY)                               # head on top of the rim
    out += '<path d="M35.5,47 L37,31 L49,41.5 Z" fill="#FF9EC4"/><path d="M84.5,47 L83,31 L71,41.5 Z" fill="#FF9EC4"/>'
    for x, y in EYES:
        out += f'<ellipse cx="{x}" cy="{y}" rx="6.6" ry="7.6" fill="{CAT_EYE}" stroke="{O}" stroke-width="1.6"/>'
        out += f'<ellipse cx="{x}" cy="{y+1}" rx="3" ry="5.3" fill="{O}"/>'
        out += f'<circle cx="{x-2.2}" cy="{y-3}" r="1.7" fill="#fff"/>'
    out += f'<path d="M56.5,71 L63.5,71 L60,75 Z" fill="#FF7EB3" stroke="{O}" stroke-width="1.2" stroke-linejoin="round"/>'
    out += (f'<path d="M60,75 Q57,80 53.5,78 M60,75 Q63,80 66.5,78" fill="none" stroke="{CAT_LINE}" '
            f'stroke-width="1.6" stroke-linecap="round"/>')
    for sgn in (-1, 1):
        for dy in (0, 5):
            out += (f'<path d="M{60+sgn*17},{73+dy} L{60+sgn*36},{71+dy*1.6}" stroke="#FFFFFF" '
                    f'stroke-width="1.3" stroke-linecap="round" opacity="0.8"/>')
    for x in (38, 82):
        out += f'<ellipse cx="{x}" cy="74" rx="4.6" ry="2.6" fill="{PINK}" opacity="0.75"/>'
    out += cstick(paws, CAT_BODY)
    for x in (45, 75):
        for dx in (-3.5, 3.5):
            out += (f'<path d="M{x+dx},{91.5} L{x+dx},{95}" stroke="{CAT_LINE}" stroke-width="1.2" '
                    f'stroke-linecap="round" opacity="0.7"/>')
    return stage(out)


def cat_blink():
    """Drawn over the open eyes for a moment."""
    out = ""
    for x, y in EYES:
        out += f'<ellipse cx="{x}" cy="{y}" rx="8.6" ry="9.4" fill="{CAT_BODY}"/>'
        out += (f'<path d="M{x-6},{y+1} Q{x},{y+6} {x+6},{y+1}" fill="none" stroke="{CAT_LINE}" '
                f'stroke-width="2" stroke-linecap="round"/>')
    return stage(out)


# --- hats ---------------------------------------------------------------
def hat_witch():
    g = 'transform="rotate(-9 60 41)"'
    cone = ['<path d="M43,41 Q50,24 55,12 Q58,4 67,3 Q61,9 63,16 Q68,28 77,41 Z"/>']
    brim = ['<ellipse cx="60" cy="42" rx="29" ry="5.5"/>']
    out = cstick(cone + brim, "#3B2F5C", extra=g)
    out += f'<g {g}><path d="M46.5,34 L73.5,34 L76,40 L44,40 Z" fill="#9B6BFF"/>'
    out += f'<rect x="56" y="33.5" width="8" height="7" rx="1.2" fill="none" stroke="{CAT_EYE}" stroke-width="2"/></g>'
    out += f'<g {g}><circle cx="66" cy="4" r="1.6" fill="{CAT_EYE}"/></g>'
    return stage(out)


def hat_santa():
    body = ['<path d="M37,44 Q40,22 60,15 Q80,10 92,22 Q96,27 94,34 Q88,24 78,24 Q84,32 83,44 Z"/>']
    out = cstick(body, "#E8434F")
    out += cstick(['<rect x="34" y="37" width="52" height="11" rx="5.5"/>'], "#FFFFFF")
    out += cstick(['<circle cx="94" cy="36" r="6"/>'], "#FFFFFF")
    return stage(out)


def hat_beanie():
    dome = ['<path d="M39,45 Q39,24 60,22 Q81,24 81,45 Z"/>']
    out = cstick(dome, "#FF7EB3")
    for x in (49, 60, 71):
        out += f'<path d="M{x},26 Q{x+0.5},35 {x},42" stroke="#E0457B" stroke-width="1.6" fill="none" opacity="0.7"/>'
    out += cstick(['<rect x="37" y="39" width="46" height="10" rx="4.5"/>'], "#FFB3CF")
    out += cstick(['<circle cx="60" cy="19" r="6.5"/>'], "#FFFFFF")
    return stage(out)


def hat_rain():
    crown = ['<path d="M44,44 Q44,24 60,23 Q76,24 76,44 Z"/>']
    brim = ['<path d="M27,46 Q60,34 93,46 Q94,52 88,51 Q60,44 32,51 Q26,52 27,46 Z"/>']
    out = cstick(crown + brim, "#FFD23F")
    out += f'<path d="M45,38 Q60,34 75,38" stroke="#E8B500" stroke-width="1.6" fill="none"/>'
    return stage(out)


# --- around the neck ------------------------------------------------------
def _scarf(c1, c2):
    band = ['<path d="M33,82 Q60,95 87,82 L88,88 Q60,102 32,88 Z"/>']
    tail = ['<path d="M70,88 L82,88 L86,100 L73,100 Z"/>']
    out = cstick(tail + band, c1)
    for x in (42, 54, 66, 78):
        out += f'<path d="M{x},{84.5 + (3 if 48 < x < 72 else 1.5)} l0,6" stroke="{c2}" stroke-width="3.4"/>'
    out += f'<path d="M74.5,94 L84.5,94" stroke="{c2}" stroke-width="2.6"/>'
    return stage(out)


def neck_scarf():
    return _scarf("#2EC4B6", "#FFFFFF")


def neck_scarf_blue():
    return _scarf("#3D7FE0", "#FFFFFF")


def neck_cape():
    collar = ['<path d="M30,80 L46,90 L60,86 L74,90 L90,80 L84,96 L60,92 L36,96 Z"/>']
    out = cstick(collar, "#8E5CF7")
    bow = ['<path d="M60,89 L50,84 L50,95 Z"/>', '<path d="M60,89 L70,84 L70,95 Z"/>',
           '<circle cx="60" cy="89.5" r="3"/>']
    out += cstick(bow, "#FF9A3C")
    return stage(out)


# --- on the face -----------------------------------------------------------
def heart(cx, cy, s):
    return (f'<path d="M{cx},{cy+s*0.9} C{cx-s*1.3},{cy+s*0.1} {cx-s*1.1},{cy-s*1.0} {cx},{cy-s*0.35} '
            f'C{cx+s*1.1},{cy-s*1.0} {cx+s*1.3},{cy+s*0.1} {cx},{cy+s*0.9} Z"/>')


def face_sunglasses():
    lenses = [heart(x, y + 1, 8.5) for x, y in EYES]
    out = cstick(lenses, "#FF5FA2", sw=1.8)
    out += f'<path d="M55.5,61 Q60,58.5 64.5,61" stroke="{O}" stroke-width="2.4" fill="none" stroke-linecap="round"/>'
    for x, y in EYES:
        out += f'<ellipse cx="{x-3}" cy="{y-2}" rx="2.2" ry="1.4" fill="#fff" opacity="0.85"/>'
    return stage(out)


# --- props, sitting on the card edge to the cat's right ----------------------
def prop_pumpkin():
    cx, cy = 146, 81
    ribs = [f'<ellipse cx="{cx-11}" cy="{cy}" rx="12" ry="14"/>', f'<ellipse cx="{cx+11}" cy="{cy}" rx="12" ry="14"/>',
            f'<ellipse cx="{cx}" cy="{cy}" rx="13" ry="15"/>']
    out = cstick([f'<path d="M{cx-2},{cy-14} Q{cx-1},{cy-22} {cx+5},{cy-24} L{cx+4},{cy-14} Z"/>'], "#5CB85C")
    out += cstick(ribs, "#FF9A3C")
    out += f'<ellipse cx="{cx}" cy="{cy}" rx="13" ry="15" fill="#FFAE5C"/>'
    out += (f'<path d="M{cx-9},{cy-3} l4,-6 l4,6 Z M{cx+1},{cy-3} l4,-6 l4,6 Z" fill="{O}"/>')
    out += (f'<path d="M{cx-11},{cy+3} Q{cx},{cy+13} {cx+11},{cy+3} L{cx+6},{cy+5} L{cx+3},{cy+3} L{cx},{cy+6} '
            f'L{cx-3},{cy+3} L{cx-6},{cy+5} Z" fill="{O}"/>')
    return stage(out)


def prop_present():
    out = cstick(['<rect x="126" y="66" width="40" height="29" rx="3"/>'], "#3DBE6E")
    out += cstick(['<rect x="123" y="60" width="46" height="9" rx="2.5"/>'], "#52D485")
    out += '<rect x="142" y="60" width="8" height="35" fill="#E8434F"/>'
    out += cstick(['<path d="M146,60 Q134,46 131,53 Q130,59 146,60 Z"/>',
                   '<path d="M146,60 Q158,46 161,53 Q162,59 146,60 Z"/>'], "#E8434F")
    return stage(out)


MENORAH_X0, MENORAH_DX = 104, 9      # candle i (0..8) sits at x0 + i*dx; i = 4 is the shamash
MENORAH_TOP = 60                        # candle top (shamash 6 px higher)


def menorah_candle_x(i):
    return MENORAH_X0 + i * MENORAH_DX


def prop_menorah():
    gold, gold2 = "#F5C542", "#D9A520"
    x0, x8 = menorah_candle_x(0), menorah_candle_x(8)
    mid = menorah_candle_x(4)
    parts = [f'<rect x="{x0-3}" y="69" width="{x8-x0+6}" height="5" rx="2.5"/>',
             f'<rect x="{mid-2.5}" y="69" width="5" height="22" rx="1.5"/>',
             f'<path d="M{mid-14},95 Q{mid},84 {mid+14},95 Z"/>']
    for i in range(9):
        x = menorah_candle_x(i)
        top = 63 if i != 4 else 57
        parts.append(f'<rect x="{x-3}" y="{top+5}" width="6" height="4" rx="1.5"/>')
    out = cstick(parts, gold, sw=1.8)
    out += f'<rect x="{x0-1}" y="70" width="{x8-x0+2}" height="1.5" fill="#FFF0B3" opacity="0.8"/>'
    for i in range(9):
        x = menorah_candle_x(i)
        top = MENORAH_TOP if i != 4 else MENORAH_TOP - 6
        col = "#FFFFFF" if i == 4 else ("#7FB8FF" if i % 2 else "#FFFFFF")
        out += (f'<rect x="{x-2}" y="{top}" width="4" height="{68-top}" rx="1" fill="{col}" '
                f'stroke="{O}" stroke-width="1.2"/>')
    return stage(out)


def flame():
    """One candle flame, 10 x 14 box; tip at the top, base at the bottom."""
    return svg(f'<path d="M5,1 Q9.5,7 8,10.5 Q6.5,13.5 5,13.5 Q3.5,13.5 2,10.5 Q0.5,7 5,1 Z" fill="#FF9A3C"/>'
               f'<path d="M5,5.5 Q7.3,9 6.4,11 Q5.8,12.3 5,12.3 Q4.2,12.3 3.6,11 Q2.7,9 5,5.5 Z" fill="#FFE38A"/>',
               "0 0 10 14")


def snowflake():
    """A falling flake, 16 x 16 box."""
    d = ""
    for a in range(0, 180, 60):
        r = math.radians(a)
        dx, dy = 5.5 * math.cos(r), 5.5 * math.sin(r)
        d += f"M{8-dx:.2f},{8-dy:.2f} L{8+dx:.2f},{8+dy:.2f} "
    return svg(f'<path d="{d}" stroke="{O}" stroke-width="4.2" stroke-linecap="round"/>'
               f'<path d="{d}" stroke="#FFFFFF" stroke-width="2.2" stroke-linecap="round"/>', "0 0 16 16")


# Stacking order in the firmware is base, neck, face, hat (props behind the cat).
CAT_LAYERS = {
    "base": cat_base, "blink": cat_blink,
    "hat_witch": hat_witch, "hat_santa": hat_santa, "hat_beanie": hat_beanie, "hat_rain": hat_rain,
    "neck_scarf": neck_scarf, "neck_scarf_blue": neck_scarf_blue, "neck_cape": neck_cape,
    "face_sunglasses": face_sunglasses,
    "prop_pumpkin": prop_pumpkin, "prop_present": prop_present, "prop_menorah": prop_menorah,
}


# ---------------------------------------------------------------- weather effects (falling things)
# Small sprites the today page animates behind everything else. Each entry is
# (svg, width, height) at the exact pixel size it's drawn on screen.
def fx_raindrop(w=8, h=20):
    """A short slanted streak: falling rain."""
    return svg(f'<path d="M{w*0.7:.1f},{1.5} L{w*0.3:.1f},{h-2.5}" stroke="{O}" stroke-width="4.4" stroke-linecap="round" opacity="0.55"/>'
               f'<path d="M{w*0.7:.1f},{1.5} L{w*0.3:.1f},{h-2.5}" stroke="#8ED0FF" stroke-width="2.4" stroke-linecap="round"/>',
               f"0 0 {w} {h}")


def fx_leaf(color, angle):
    """An autumn leaf, drawn at four angles so swapping them looks like tumbling."""
    body = ('<path d="M12,2.5 C18,6 20,12 12,21.5 C4,12 6,6 12,2.5 Z"/>')
    vein = '<path d="M12,4.5 L12,19.5 M12,10 L15.5,7.5 M12,13.5 L8.5,11 M12,16.5 L15,14.5" stroke="#00000033" stroke-width="1.2" fill="none" stroke-linecap="round"/>'
    stem = f'<path d="M12,21 L12,23.2" stroke="{O}" stroke-width="1.8" stroke-linecap="round"/>'
    g = f'transform="rotate({angle} 12 12)"'
    return svg(f'<g {g}>' + stick([body], color, sw=1.5) + vein + stem + '</g>', "0 0 24 24")


def fx_star():
    return svg(sparkle(8, 8, 6.2, "#FFE38A"), "0 0 16 16")


def fx_bolt():
    pts = "22,2 9,28 19,28 12,50 34,20 23,20 30,2"
    return svg(stick([f'<polygon points="{pts}"/>'], "#FFD43B", sw=2.2), "0 0 40 52")


LEAF_COLORS = {"orange": "#FF9A3C", "red": "#E8553C", "yellow": "#F5C542"}
LEAF_ANGLES = (0, 50, 100, 150)

FX = {
    "drop": (fx_raindrop(), 8, 20),
    "drop_s": (fx_raindrop(6, 13), 6, 13),
    "flake": (snowflake(), 16, 16),
    "flake_s": (snowflake(), 11, 11),
    "star": (fx_star(), 16, 16),
    "bolt": (fx_bolt(), 40, 52),
}
for _name, _col in LEAF_COLORS.items():
    for _i, _a in enumerate(LEAF_ANGLES):
        FX[f"leaf_{_name}_{_i}"] = (fx_leaf(_col, _a), 22, 22)


# ---------------------------------------------------------------- Weather Besties 2.0: full-body cat
# One shared stage, CAT2_W x CAT2_H px (1 unit = 1 px), cat centred at x=150 so
# seasonal props fit at x 0..70. Every layer is an SVG on the full stage;
# make_assets.py crops each to its pixels and records the stage offset, the
# same convention as the old CAT_LAYERS above. Outline width 2.6 at stage scale.
CAT2_W, CAT2_H = 260, 252
C2SW = 2.6
DENIM = "#4A7BD9"


def c2svg(body, scaled=True):
    if scaled:
        # The cat underfilled her stage: grow the whole character about her
        # feet (150, 250) so she reads as the centerpiece, not a sidebar.
        # Props pass scaled=False: they are ground furniture at stage scale.
        body = ('<g transform="translate(150 250) scale(1.10 1.03) translate(-150 -250)">'
                + body + '</g>')
    return svg(body, f"0 0 {CAT2_W} {CAT2_H}")


def c2stick(shapes, fill, sw=C2SW, extra=""):
    return stick(shapes, fill, sw=sw, extra=extra)


def capsule(x1, y1, x2, y2, r, fill, sw=C2SW):
    """A rounded limb: navy outline stroke under a fill stroke."""
    return (f'<path d="M{x1},{y1} L{x2},{y2}" stroke="{O}" stroke-width="{2*r+2*sw:.1f}" stroke-linecap="round"/>'
            f'<path d="M{x1},{y1} L{x2},{y2}" stroke="{fill}" stroke-width="{2*r:.1f}" stroke-linecap="round"/>')


def rrect(x, y, w, h, rx, fill, sw=C2SW):
    return c2stick([f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}"/>'], fill, sw=sw)


def _cat2_face(eye, whisker, line):
    out = ""
    for x in (128, 172):
        out += f'<ellipse cx="{x}" cy="76" rx="9" ry="11" fill="{eye}" stroke="{O}" stroke-width="1.6"/>'
        out += f'<ellipse cx="{x}" cy="77" rx="3.8" ry="6.8" fill="{O}"/>'
        out += f'<circle cx="{x-3}" cy="72" r="2" fill="#fff"/>'
    out += f'<path d="M144,89 L156,89 L150,96 Z" fill="#FF7EB3" stroke="{O}" stroke-width="1.4" stroke-linejoin="round"/>'
    out += (f'<path d="M150,96 Q146,102 141,99 M150,96 Q154,102 159,99" fill="none" stroke="{line}" '
            f'stroke-width="2" stroke-linecap="round"/>')
    for x in (112, 188):
        out += f'<ellipse cx="{x}" cy="93" rx="7" ry="4" fill="{PINK}" opacity="0.8"/>'
    for sgn in (-1, 1):
        out += (f'<path d="M{150+sgn*24},92 L{150+sgn*46},88 M{150+sgn*24},99 L{150+sgn*46},101" '
                f'stroke="{whisker}" stroke-width="1.8" stroke-linecap="round" opacity="0.85"/>')
    return out


def _cat2_base(body, eye, whisker, line):
    out = ""
    # soft ground shadow so she stands on something instead of floating
    out += '<ellipse cx="150" cy="247" rx="100" ry="9" fill="#1E2340" opacity="0.10"/>'
    # tail, behind everything: thick curve from the hip out to the right
    out += (f'<path d="M192,206 Q232,198 238,150" fill="none" stroke="{O}" stroke-width="{18+2*C2SW:.1f}" stroke-linecap="round"/>'
            f'<path d="M192,206 Q232,198 238,150" fill="none" stroke="{body}" stroke-width="18" stroke-linecap="round"/>')
    # legs and feet
    out += capsule(134, 196, 134, 234, 14, body)
    out += capsule(166, 196, 166, 234, 14, body)
    out += c2stick(['<ellipse cx="134" cy="240" rx="18" ry="10"/>',
                    '<ellipse cx="166" cy="240" rx="18" ry="10"/>'], body)
    # torso, arms, paws
    out += rrect(110, 122, 80, 78, 20, body)
    out += capsule(116, 134, 100, 190, 13, body)
    out += capsule(184, 134, 200, 190, 13, body)
    out += c2stick(['<circle cx="100" cy="190" r="14"/>', '<circle cx="200" cy="190" r="14"/>'], body)
    # head and ears
    head = ['<ellipse cx="150" cy="74" rx="60" ry="50"/>',
            '<path d="M97,54 L106,20 L140,34 Z"/>', '<path d="M203,54 L194,20 L160,34 Z"/>']
    out += c2stick(head, body)
    out += '<path d="M107,44 L110,29 L129,37 Z" fill="#FF9EC4"/><path d="M193,44 L190,29 L171,37 Z" fill="#FF9EC4"/>'
    out += _cat2_face(eye, whisker, line)
    return c2svg(out)


def _cat2_cover_eyes(body):
    return "".join(f'<ellipse cx="{x}" cy="76" rx="12" ry="13" fill="{body}"/>' for x in (128, 172))


def _cat2_blink(body, line):
    out = _cat2_cover_eyes(body)
    for x in (128, 172):
        out += (f'<path d="M{x-7},78 Q{x},84 {x+7},78" fill="none" stroke="{line}" '
                f'stroke-width="2.6" stroke-linecap="round"/>')
    return c2svg(out)


def _cat2_happy(body, line):
    out = _cat2_cover_eyes(body)
    for x in (128, 172):
        out += (f'<path d="M{x-8},80 Q{x},67 {x+8},80" fill="none" stroke="{line}" '
                f'stroke-width="3" stroke-linecap="round"/>')
    out += '<path d="M141,95 Q150,112 159,95 Q150,101 141,95 Z" fill="#2B3450"/>'
    out += '<ellipse cx="150" cy="103" rx="3.5" ry="2.4" fill="#FF8FB1"/>'
    return c2svg(out)


def _cat2_puzzled(body, line):
    out = (f'<path d="M117,60 L139,55" stroke="{line}" stroke-width="3" stroke-linecap="round"/>'
           f'<path d="M161,62 L183,62" stroke="{line}" stroke-width="2.6" stroke-linecap="round"/>')
    out += '<circle cx="190" cy="57" r="4" fill="#FFFFFF" stroke="#2B3450" stroke-width="1.8"/>'
    out += '<circle cx="198" cy="47" r="7" fill="#FFFFFF" stroke="#2B3450" stroke-width="1.8"/>'
    out += '<ellipse cx="208" cy="44" rx="26" ry="20" fill="#FFFFFF" stroke="#2B3450" stroke-width="2.2"/>'
    out += (f'<path d="M201,37 Q200,29 208,29 Q216,29 215,36 Q214,41 208,42 L208,45" fill="none" '
            f'stroke="{O}" stroke-width="3" stroke-linecap="round"/>')
    out += f'<circle cx="208" cy="50" r="2.1" fill="{O}"/>'
    return c2svg(out)


_CAT2_BLACK = ("#2E2E3A", "#FFD23F", "#FFFFFF", "#C9C9DA")
_CAT2_PINK = ("#FFB3CF", "#7FD7FF", O, O)

CAT2_PER_COLOR = {
    "black": {
        "base": lambda: _cat2_base(*_CAT2_BLACK),
        "blink": lambda: _cat2_blink(_CAT2_BLACK[0], _CAT2_BLACK[3]),
        "happy": lambda: _cat2_happy(_CAT2_BLACK[0], _CAT2_BLACK[3]),
        "puzzled": lambda: _cat2_puzzled(_CAT2_BLACK[0], _CAT2_BLACK[3]),
    },
    "pink": {
        "base": lambda: _cat2_base(*_CAT2_PINK),
        "blink": lambda: _cat2_blink(_CAT2_PINK[0], _CAT2_PINK[3]),
        "happy": lambda: _cat2_happy(_CAT2_PINK[0], _CAT2_PINK[3]),
        "puzzled": lambda: _cat2_puzzled(_CAT2_PINK[0], _CAT2_PINK[3]),
    },
}


# --- cat2 clothing layers (shared by both cat colours) --------------------
def cat2_top_short():
    out = capsule(116, 134, 107, 160, 15, "#FF8FB1")
    out += capsule(184, 134, 193, 160, 15, "#FF8FB1")
    out += rrect(108, 120, 84, 60, 18, "#FF8FB1")
    out += f'<g fill="#FFFFFF">{heart(150, 148, 11)}</g>'
    out += f'<path d="M136,121 Q150,132 164,121" fill="none" stroke="{O}" stroke-width="2.4"/>'
    return c2svg(out)


def cat2_top_long():
    out = capsule(116, 134, 100, 190, 14, "#7FD1AE")
    out += capsule(184, 134, 200, 190, 14, "#7FD1AE")
    out += rrect(108, 120, 84, 82, 20, "#7FD1AE")
    out += '<g stroke="#FFFFFF" stroke-width="5">'
    for y in (140, 156, 172, 188):
        out += f'<path d="M112,{y} L188,{y}"/>'
    out += '<path d="M103,160 L119,164 M99,178 L115,182 M181,164 L197,160 M185,182 L201,178"/>'
    out += '</g>'
    out += f'<path d="M136,121 Q150,132 164,121" fill="none" stroke="{O}" stroke-width="2.4"/>'
    return c2svg(out)


def cat2_bottom_shorts():
    out = rrect(108, 170, 84, 34, 10, DENIM)
    out += rrect(119, 198, 30, 18, 7, DENIM)
    out += rrect(151, 198, 30, 18, 7, DENIM)
    out += f'<path d="M110,181 L190,181" stroke="{O}" stroke-width="2.2" opacity="0.6"/>'
    return c2svg(out)


def cat2_bottom_pants():
    out = rrect(108, 170, 84, 34, 10, DENIM)
    out += capsule(134, 196, 134, 232, 15, DENIM)
    out += capsule(166, 196, 166, 232, 15, DENIM)
    out += f'<path d="M110,181 L190,181" stroke="{O}" stroke-width="2.2" opacity="0.6"/>'
    out += '<g stroke="#DCE8FF" stroke-width="3"><path d="M121,226 L147,226 M153,226 L179,226"/></g>'
    return c2svg(out)


def cat2_feet_sneakers():
    out = ""
    for cx in (134, 166):
        out += c2stick([f'<ellipse cx="{cx}" cy="240" rx="20" ry="11"/>'], "#FFFFFF")
        out += f'<path d="M{cx-13},237 L{cx+13},237" stroke="#FF5FA2" stroke-width="4" stroke-linecap="round"/>'
        out += f'<path d="M{cx-18},247 Q{cx},252 {cx+18},247" fill="none" stroke="{O}" stroke-width="3"/>'
    return c2svg(out)


def _boot(cx, sgn, fill, cuff=None):
    # One simple (non-self-intersecting) outline per boot: shaft, a step out
    # to the toe, a flat sole back to the heel. The old path crossed its own
    # edge, so the fill rule left the toe hollow and the pants showed through.
    if sgn > 0:
        d = (f'M{cx-13},196 L{cx+13},196 L{cx+13},228 L{cx+21},234 '
             f'Q{cx+21},244 {cx+10},244 L{cx-13},244 Z')
    else:
        d = (f'M{cx+13},196 L{cx-13},196 L{cx-13},228 L{cx-21},234 '
             f'Q{cx-21},244 {cx-10},244 L{cx+13},244 Z')
    out = c2stick([f'<path d="{d}"/>'], fill)
    if cuff:
        out += rrect(cx - 14, 192, 28, 12, 5, cuff)
    return out


def cat2_feet_rain():
    return c2svg(_boot(134, -1, "#FFD23F") + _boot(166, 1, "#FFD23F"))


def cat2_feet_snow():
    return c2svg(_boot(134, -1, "#8B5E3C", "#F4F1EA") + _boot(166, 1, "#8B5E3C", "#F4F1EA"))


def _outer(fill, h=86):
    out = capsule(116, 134, 100, 190, 15, fill)
    out += capsule(184, 134, 200, 190, 15, fill)
    out += rrect(104, 118, 92, h, 22, fill)
    return out


def _hood_collar(fill):
    return (f'<path d="M122,128 Q150,110 178,128" fill="none" stroke="{O}" stroke-width="15" stroke-linecap="round"/>'
            f'<path d="M122,128 Q150,110 178,128" fill="none" stroke="{fill}" stroke-width="9" stroke-linecap="round"/>')


def cat2_layer_hoodie():
    out = _outer("#9C8CFF")
    out += f'<rect x="131" y="166" width="38" height="24" rx="7" fill="none" stroke="{O}" stroke-width="2.4"/>'
    out += _hood_collar("#9C8CFF")
    return c2svg(out)


def cat2_layer_light():
    out = _outer("#FFB454", 84)
    out += f'<path d="M150,126 L150,200" stroke="{O}" stroke-width="2.6"/>'
    out += c2stick(['<path d="M134,119 L150,137 L141,119 Z"/>', '<path d="M166,119 L150,137 L159,119 Z"/>'], "#FFD9A0", sw=2.0)
    out += (f'<rect x="117" y="163" width="17" height="15" rx="2" fill="none" stroke="{O}" stroke-width="2.2"/>'
            f'<rect x="166" y="163" width="17" height="15" rx="2" fill="none" stroke="{O}" stroke-width="2.2"/>')
    return c2svg(out)


def cat2_layer_rain():
    # Shorter than the old dress-length coat (hem 198, not 208) so shorts
    # and legs show above the boots, plus a placket line and hem so it
    # reads as a jacket, not a bathrobe.
    out = _outer("#FFD23F", 80)
    out += f'<path d="M150,124 L150,196" stroke="{O}" stroke-width="2.4"/>'
    out += '<g stroke="#2B3450" stroke-width="3" stroke-linecap="round">'
    for y in (142, 162, 182):
        out += f'<path d="M143,{y} L157,{y}"/>'
    out += '</g>'
    out += f'<path d="M110,190 Q150,196 190,190" fill="none" stroke="{O}" stroke-width="2.2" opacity="0.8"/>'
    out += _hood_collar("#FFD23F")
    return c2svg(out)


def cat2_layer_warm():
    out = _outer("#6F8CFF")
    for y in (146, 168, 190):
        out += f'<path d="M108,{y} Q150,{y+6} 192,{y}" fill="none" stroke="{O}" stroke-width="2.2" opacity="0.8"/>'
    out += f'<path d="M150,124 L150,202" stroke="{O}" stroke-width="2.4"/>'
    return c2svg(out)


def cat2_layer_winter():
    out = _outer("#3D7FE0", 98)
    out += f'<path d="M108,176 Q150,182 192,176" fill="none" stroke="{O}" stroke-width="2.2" opacity="0.8"/>'
    out += f'<path d="M150,124 L150,214" stroke="{O}" stroke-width="2.4"/>'
    out += (f'<path d="M120,126 Q150,108 180,126" fill="none" stroke="{O}" stroke-width="17" stroke-linecap="round"/>'
            f'<path d="M120,126 Q150,108 180,126" fill="none" stroke="#F4F1EA" stroke-width="12" stroke-linecap="round"/>')
    return c2svg(out)


def cat2_scarf():
    shapes = ['<path d="M110,122 Q150,142 190,122 L190,138 Q150,158 110,138 Z"/>',
              '<path d="M166,136 L184,136 L188,176 L170,174 Z"/>']
    out = c2stick(shapes, "#2EC4B6")
    out += '<g stroke="#FFFFFF" stroke-width="3.4"><path d="M172,148 L186,149 M173,160 L187,161"/></g>'
    return c2svg(out)


def cat2_gloves():
    # Solid mittens a child can name: round body, a thumb on the inner side,
    # a darker cuff band at the wrist, one soft highlight. (The old art drew
    # white sticks across the paws; they read as drumsticks, not gloves.)
    out = ""
    for cx, sgn in ((100, -1), (200, 1)):
        out += c2stick([f'<circle cx="{cx}" cy="190" r="17"/>',
                        f'<ellipse cx="{cx - sgn*13}" cy="182" rx="6.5" ry="9.5" '
                        f'transform="rotate({sgn*28} {cx - sgn*13} 182)"/>'], "#9C8CFF")
        out += (f'<path d="M{cx-12},178 Q{cx},171 {cx+12},178" fill="none" stroke="#7E6BE0" '
                f'stroke-width="5" stroke-linecap="round"/>')
        out += (f'<ellipse cx="{cx-6}" cy="185" rx="4.5" ry="3" fill="#FFFFFF" opacity="0.35" '
                f'transform="rotate(-24 {cx-6} 185)"/>')
    return c2svg(out)


def cat2_held_umbrella():
    # Shifted left from the old art: the canopy's right edge used to sit 2 px
    # from the tile grid. It now clears the tiles and the cat's face.
    out = (f'<path d="M195,24 L187,184" stroke="{O}" stroke-width="6" stroke-linecap="round"/>'
           f'<path d="M195,24 L187,184" stroke="#8A93A6" stroke-width="3" stroke-linecap="round"/>'
           f'<path d="M187,184 Q187,196 196,194" fill="none" stroke="{O}" stroke-width="4" stroke-linecap="round"/>')
    canopy = ('<path d="M146,64 Q150,18 195,16 Q240,18 244,64 Q233,55 222,64 Q211,55 200,64 '
              'Q189,55 178,64 Q167,55 156,64 Q150,58 146,64 Z"/>')
    out += c2stick([canopy], "#FF5C8A")
    out += (f'<path d="M195,19 L195,62 M195,19 L164,60 M195,19 L226,60" stroke="{O}" '
            f'stroke-width="2" opacity="0.55" fill="none"/>')
    return c2svg(out)


def cat2_held_water():
    out = rrect(84, 146, 32, 54, 10, "#BDEBFF")
    out += '<path d="M86,168 L114,168 L114,190 Q114,198 106,198 L94,198 Q86,198 86,190 Z" fill="#4DC3FF"/>'
    out += rrect(92, 132, 16, 14, 4, "#FF8FB1")
    out += '<path d="M92,158 L92,190" stroke="#FFFFFF" stroke-width="3.4" stroke-linecap="round" opacity="0.8"/>'
    return c2svg(out)


def cat2_face_sun():
    # The plan's heart sunglasses, drawn as eyewear, not as eyes: dark
    # heart lenses with a bridge and temple arms, a sweat drop on the
    # temple, tongue out. Pink heart EYES read lovestruck; dark heart
    # LENSES on a frame read as sunglasses on a hot day.
    out = c2stick([heart(128, 78, 13), heart(172, 78, 13)], "#333A52", sw=2.0)
    out += f'<path d="M144,70 Q150,66 156,70" fill="none" stroke="{O}" stroke-width="3" stroke-linecap="round"/>'
    out += f'<path d="M112,70 L98,62 M188,70 L202,62" stroke="{O}" stroke-width="3.4" stroke-linecap="round"/>'
    for x in (121, 165):
        out += f'<path d="M{x},72 L{x+7},72" stroke="#FFFFFF" stroke-width="3" stroke-linecap="round" opacity="0.8"/>'
    out += c2stick(['<path d="M214,52 Q222,66 222,73 Q222,81 214,81 Q206,81 206,73 Q206,66 214,52 Z"/>'],
                   "#4DA6FF", sw=1.8)
    out += c2stick(['<path d="M143,100 L157,100 L157,106 Q150,112 143,106 Z"/>'], "#FF8FB1", sw=1.6)
    return c2svg(out)


def cat2_hat_warm():
    # Deep violet, not pink: the pink beanie dissolved into the pink cat's
    # fur. Violet pops on both cats.
    out = c2stick(['<path d="M108,58 Q104,18 150,16 Q196,18 192,58 Q150,46 108,58 Z"/>'], "#6C4BD8")
    out += rrect(106, 50, 88, 15, 7, "#8F74E8")
    out += c2stick(['<circle cx="150" cy="19" r="10"/>'], "#FFFFFF")
    return c2svg(out)


def cat2_hat_witch():
    g = 'transform="rotate(-8 150 52)"'
    cone = ['<path d="M114,52 Q133,28 144,13 Q148,7 155,8 Q151,13 153,21 Q159,34 186,52 Z"/>']
    brim = ['<ellipse cx="150" cy="54" rx="58" ry="11"/>']
    out = c2stick(cone + brim, "#3B2F5C", extra=g)
    out += f'<g {g}><path d="M121,40 L181,44 L183,52 L119,48 Z" fill="#9B6BFF"/>'
    out += f'<rect x="146" y="40" width="9" height="8" rx="1.2" fill="none" stroke="{CAT_EYE}" stroke-width="2"/></g>'
    return c2svg(out)


def cat2_hat_santa():
    body = ['<path d="M106,50 Q108,20 146,18 Q184,16 196,34 Q202,44 198,52 Q186,38 168,40 Q182,44 184,52 L106,52 Z"/>']
    out = c2stick(body, "#E8434F")
    out += rrect(102, 44, 96, 15, 7, "#FFFFFF")
    out += c2stick(['<circle cx="200" cy="48" r="10"/>'], "#FFFFFF")
    return c2svg(out)


# --- cat2 seasonal props (stage x 0..70, y 180..252) ------------------------
def cat2_prop_pumpkin():
    # Ground corner at the cat's right (was bottom-left, where it stacked
    # up with the advice-row icon and read as part of the instructions).
    cx, cy = 222, 222
    ribs = [f'<ellipse cx="{cx-11}" cy="{cy}" rx="17" ry="22"/>', f'<ellipse cx="{cx+11}" cy="{cy}" rx="17" ry="22"/>',
            f'<ellipse cx="{cx}" cy="{cy}" rx="18" ry="23"/>']
    out = c2stick([f'<path d="M{cx-2},{cy-21} Q{cx-1},{cy-29} {cx+5},{cy-31} L{cx+4},{cy-21} Z"/>'], "#5CB85C")
    out += c2stick(ribs, "#FF9A3C")
    out += f'<ellipse cx="{cx}" cy="{cy}" rx="18" ry="23" fill="#FFAE5C"/>'
    out += f'<path d="M{cx-11},{cy-4} l5,-7 l5,7 Z M{cx+1},{cy-4} l5,-7 l5,7 Z" fill="{O}"/>'
    out += (f'<path d="M{cx-13},{cy+5} Q{cx},{cy+16} {cx+13},{cy+5} L{cx+7},{cy+7} L{cx+3},{cy+4} '
            f'L{cx},{cy+8} L{cx-3},{cy+4} L{cx-7},{cy+7} Z" fill="{O}"/>')
    return c2svg(out, scaled=False)


MENORAH2_TOP = 196          # candle top; the shamash (i=4) sits 8 px higher
MENORAH2_CUP_Y = 228        # candle bottom / holder bar


def menorah2_candle_x(i):
    return 7 + 7 * i


def menorah2_candle_top(i):
    return MENORAH2_TOP - 8 if i == 4 else MENORAH2_TOP


def menorah2_flame_pos(i):
    """Top-left of the 10x14 flame image on candle i (left to right)."""
    return (menorah2_candle_x(i) - 5, menorah2_candle_top(i) - 13)


def cat2_prop_menorah():
    gold = "#F5C542"
    parts = [f'<rect x="4" y="230" width="62" height="5" rx="2.5"/>',
             '<rect x="33" y="230" width="4" height="12" rx="1.5"/>',
             '<path d="M20,250 Q35,240 50,250 Z"/>']
    for i in range(9):
        x = menorah2_candle_x(i)
        parts.append(f'<rect x="{x-3}" y="224" width="6" height="5" rx="1.5"/>')
    out = c2stick(parts, gold, sw=1.8)
    for i in range(9):
        x = menorah2_candle_x(i)
        top = menorah2_candle_top(i)
        col = "#FFFFFF" if i == 4 else ("#7FB8FF" if i % 2 else "#FFFFFF")
        out += (f'<rect x="{x-2}" y="{top}" width="4" height="{MENORAH2_CUP_Y-top}" rx="1" '
                f'fill="{col}" stroke="{O}" stroke-width="1.2"/>')
    return c2svg(out, scaled=False)


def cat2_prop_present():
    # Same ground corner as the pumpkin (right of the cat's feet).
    out = rrect(197, 204, 54, 44, 4, "#3DBE6E")
    out += rrect(194, 196, 60, 11, 3, "#52D485")
    out += '<rect x="220" y="196" width="8" height="52" fill="#E8434F"/>'
    out += c2stick(['<path d="M224,196 Q212,182 209,189 Q208,195 224,196 Z"/>',
                    '<path d="M224,196 Q236,182 239,189 Q240,195 224,196 Z"/>'], "#E8434F")
    return c2svg(out, scaled=False)


CAT2_SHARED = {
    "bottom_shorts": cat2_bottom_shorts, "bottom_pants": cat2_bottom_pants,
    "feet_sneakers": cat2_feet_sneakers, "feet_rain": cat2_feet_rain, "feet_snow": cat2_feet_snow,
    "top_short": cat2_top_short, "top_long": cat2_top_long,
    "layer_hoodie": cat2_layer_hoodie, "layer_light": cat2_layer_light, "layer_rain": cat2_layer_rain,
    "layer_warm": cat2_layer_warm, "layer_winter": cat2_layer_winter,
    "scarf": cat2_scarf, "gloves": cat2_gloves,
    "held_umbrella": cat2_held_umbrella, "held_water": cat2_held_water,
    "face_sun": cat2_face_sun,
    "hat_warm": cat2_hat_warm, "hat_witch": cat2_hat_witch, "hat_santa": cat2_hat_santa,
    "prop_pumpkin": cat2_prop_pumpkin, "prop_menorah": cat2_prop_menorah, "prop_present": cat2_prop_present,
}

# Stacking order, bottom to top (props sit on the ground beside the cat;
# expression overlays go last so they always win over the face).
CAT2_LAYER_ORDER = [
    "base",
    "bottom_shorts", "bottom_pants",
    "feet_sneakers", "feet_rain", "feet_snow",
    "top_short", "top_long",
    "layer_hoodie", "layer_light", "layer_rain", "layer_warm", "layer_winter",
    "scarf",
    "held_umbrella", "held_water",
    "gloves",
    "face_sun",
    "hat_warm", "hat_witch", "hat_santa",
    "prop_pumpkin", "prop_menorah", "prop_present",
    "blink", "happy", "puzzled",
]


# ---------------------------------------------------------------- garment tile icons + extra icons (2.0)
# Flat-lay versions of the exact garments the cat wears (same colours and
# details), in a 100 x 100 box. GARMENTS is in garment_t order (spec 4.1).
def g_long_sleeves():
    ex = ('<g stroke="#FFFFFF" stroke-width="5"><path d="M31,42 L69,42 M30,58 L70,58 M30,74 L70,74"/></g>'
          f'<path d="M43,16 Q50,26 57,16" fill="none" stroke="{O}" stroke-width="2.4"/>')
    return svg(sleeves_body("#7FD1AE", ex))


def g_shorts():
    body = '<path d="M28,24 L72,24 L77,76 L58,76 L50,54 L42,76 L23,76 Z"/>'
    out = stick([body], DENIM)
    out += (f'<path d="M27,36 L73,36" stroke="{O}" stroke-width="2.4" opacity="0.7"/>'
            f'<path d="M33,44 Q38,52 44,45 M56,45 Q62,52 67,44" fill="none" stroke="{O}" stroke-width="2.2"/>')
    return svg(out)


def g_long_pants():
    body = '<path d="M30,12 L70,12 L75,88 L57,88 L50,42 L43,88 L25,88 Z"/>'
    out = stick([body], DENIM)
    out += (f'<path d="M29,24 L71,24" stroke="{O}" stroke-width="2.4" opacity="0.7"/>'
            '<path d="M28,80 L44,80 M56,80 L72,80" stroke="#DCE8FF" stroke-width="3"/>')
    return svg(out)


def g_hoodie():
    hood = stick(['<path d="M32,26 Q50,6 68,26 Q50,36 32,26 Z"/>'], "#9C8CFF")
    ex = (f'<rect x="38" y="56" width="24" height="17" rx="4" fill="none" stroke="{O}" stroke-width="2.4"/>'
          f'<path d="M43,16 Q50,26 57,16" fill="none" stroke="{O}" stroke-width="2.4"/>')
    return svg(hood + sleeves_body("#9C8CFF", ex))


def g_rain_jacket():
    hood = stick(['<path d="M32,26 Q50,6 68,26 Q50,36 32,26 Z"/>'], "#FFD23F")
    ex = ('<g stroke="#2B3450" stroke-width="3" stroke-linecap="round">'
          '<path d="M44,40 L56,40 M44,56 L56,56 M44,72 L56,72"/></g>')
    return svg(hood + sleeves_body("#FFD23F", ex))


def g_winter_coat():
    ex = (f'<path d="M31,52 Q50,56 69,52" fill="none" stroke="{O}" stroke-width="2.2" opacity="0.8"/>'
          f'<path d="M50,24 L50,86" stroke="{O}" stroke-width="2.6"/>')
    out = sleeves_body("#3D7FE0", ex)
    out += stick(['<path d="M33,25 Q50,12 67,25 Q50,34 33,25 Z"/>'], "#F4F1EA", sw=2.6)
    return svg(out)


def _sneaker(x, y):
    """One profile sneaker facing right: rounded toe, thick navy sole,
    pink side stripe, lace ticks. (The old icon drew two boot shafts and
    read as socks.)"""
    body = (f'M{x+2},{y+24} L{x+2},{y+14} Q{x+2},{y+6} {x+11},{y+6} L{x+21},{y+6} '
            f'Q{x+34},{y+7} {x+40},{y+15} L{x+44},{y+20} Q{x+46},{y+22} {x+46},{y+24} Z')
    out = stick([f'<path d="{body}"/>'], "#FFFFFF")
    out += stick([f'<rect x="{x}" y="{y+23}" width="48" height="8" rx="3.5"/>'], "#2B3450", sw=1.2)
    out += f'<path d="M{x+5},{y+18} L{x+41},{y+18}" stroke="#FF5FA2" stroke-width="3.4"/>'
    out += (f'<g stroke="{O}" stroke-width="2.1" stroke-linecap="round">'
            f'<path d="M{x+23},{y+9} L{x+27},{y+13} M{x+28},{y+9} L{x+32},{y+13}"/></g>')
    return out


def g_sneakers():
    # A staggered pair, like shoes set out by the door.
    return svg(_sneaker(4, 12) + _sneaker(48, 46))


def _boot_pair(fill, cuff):
    """Two front-view boots, toes pointing out, like the pair on the cat."""
    out = ""
    for cx, sgn in ((33, -1), (67, 1)):
        if sgn > 0:
            d = (f'M{cx-12},16 L{cx+12},16 L{cx+12},54 L{cx+19},60 '
                 f'Q{cx+19},70 {cx+10},70 L{cx-12},70 Z')
        else:
            d = (f'M{cx+12},16 L{cx-12},16 L{cx-12},54 L{cx-19},60 '
                 f'Q{cx-19},70 {cx-10},70 L{cx+12},70 Z')
        out += stick([f'<path d="{d}"/>'], fill)
        out += stick([f'<rect x="{cx-13}" y="{66}" width="28" height="8" rx="3.5"/>'], "#2B3450", sw=1.2)
        out += stick([f'<rect x="{cx-14}" y="{10}" width="28" height="12" rx="5"/>'], cuff)
    return out


def g_rain_boots():
    return svg(_boot_pair("#FFD23F", "#FFE98A"))


def g_snow_boots():
    return svg(_boot_pair("#8B5E3C", "#F4F1EA"))


def _beanie100(dome, band, pom):
    dome_s = '<path d="M22,66 Q22,24 50,24 Q78,24 78,66 Z"/>'
    out = stick([dome_s], dome)
    for x in (36, 50, 64):
        out += f'<path d="M{x},30 L{x},64" stroke="{O}" stroke-width="2.4" opacity="0.25" stroke-linecap="round"/>'
    out += stick(['<rect x="17" y="60" width="66" height="18" rx="7"/>'], band)
    out += stick(['<circle cx="50" cy="20" r="10"/>'], pom)
    return out


def g_warm_hat():
    return svg(_beanie100("#6C4BD8", "#8F74E8", "#FFFFFF"))


def g_sunglasses():
    # Dark heart lenses on a frame, the same glasses the cat wears.
    lens = [heart(28, 52, 15), heart(72, 52, 15)]
    out = (f'<path d="M45,46 Q50,40 55,46" fill="none" stroke="{O}" stroke-width="4.5" stroke-linecap="round"/>'
           f'<path d="M10,44 L4,36 M90,44 L96,36" stroke="{O}" stroke-width="4.5" stroke-linecap="round"/>')
    out += stick(lens, "#3E4A6B")
    out += ('<path d="M17,46 L26,46 M62,46 L71,46" stroke="#ffffff" stroke-width="3" stroke-linecap="round" '
            'opacity="0.75"/>')
    return svg(out)


def g_scarf():
    shapes = ['<path d="M20,26 Q50,44 80,26 L80,40 Q50,58 20,40 Z"/>',
              '<rect x="58" y="40" width="17" height="44" rx="6"/>',
              '<rect x="36" y="42" width="15" height="34" rx="6"/>']
    out = stick(shapes, "#2EC4B6")
    out += ('<g stroke="#FFFFFF" stroke-width="3.4"><path d="M60,54 L73,54 M60,66 L73,66 '
            'M38,54 L49,54 M38,64 L49,64"/></g>')
    return svg(out)


def g_dress():
    # Fallback only: the real tile icon is the wardrobe sheet's dress,
    # cropped by tools/crop_garment_sheet.py. A simple pink A-line dress
    # in the same stick style, in case the crop ever goes missing.
    shapes = ['<path d="M38,12 L62,12 L58,34 L86,84 Q50,94 14,84 L42,34 Z"/>']
    out = stick(shapes, "#F49AC1")
    out += '<path d="M42,34 L58,34" stroke="#E0457B" stroke-width="4" stroke-linecap="round"/>'
    return svg(out)


def g_tights():
    # Fallback only: the real icon is a crop of the dress sprite's legs
    # (tools/crop_garment_sheet.py). Two dark tight legs under a hem.
    shapes = ['<path d="M24,26 L76,26 L80,90 L66,90 L50,50 L34,90 L20,90 Z"/>']
    out = stick(shapes, "#2E3242")
    out += '<path d="M22,26 L78,26" stroke="#F49AC1" stroke-width="7" stroke-linecap="round"/>'
    return svg(out)


def g_dress_shoes():
    # Fallback only: the real icon is a crop of the dress sprite's feet
    # (tools/crop_garment_sheet.py). Two pink Mary Janes with straps.
    shapes = ['<path d="M8,58 Q8,38 26,38 Q44,38 44,58 L44,72 Q26,80 8,72 Z"/>',
              '<path d="M56,58 Q56,38 74,38 Q92,38 92,58 L92,72 Q74,80 56,72 Z"/>']
    out = stick(shapes, "#F06292")
    out += ('<path d="M10,54 L42,54 M58,54 L90,54" stroke="#2B3450" stroke-width="3.4" stroke-linecap="round"/>')
    return svg(out)


GARMENTS = {
    "short_sleeves": c_tshirt, "long_sleeves": g_long_sleeves,
    "shorts": g_shorts, "long_pants": g_long_pants,
    "hoodie": g_hoodie, "light_jacket": c_jacket, "rain_jacket": g_rain_jacket,
    "warm_coat": c_coat, "winter_coat": g_winter_coat,
    "sneakers": g_sneakers, "rain_boots": g_rain_boots, "snow_boots": g_snow_boots,
    "warm_hat": g_warm_hat, "gloves": c_mittens, "umbrella": c_umbrella,
    "sunglasses": g_sunglasses, "water_bottle": c_water, "scarf": g_scarf,
    "dress": g_dress, "tights": g_tights, "dress_shoes": g_dress_shoes,
}


def x_bolt():
    return svg(stick(['<polygon points="56,6 26,56 42,56 36,94 74,40 57,40 68,6"/>'], "#FFD43B"))


def x_hat_gloves():
    # ONE icon, two items side by side at full size (the old version stacked
    # a small hat over a smaller mitten and neither could be read). The pair
    # is wider than the 100 box at full size, so the composition is scaled
    # to fit inside it (iter-08): drawn past the box edge, the beanie's brim
    # and the mitten's edge were clipped by the canvas and the glyph read
    # as sliding out of the advice row's frame.
    out = f'<g transform="translate(-8,14) scale(0.68)">{_beanie100("#6C4BD8", "#8F74E8", "#FFFFFF")}</g>'
    out += f'<g transform="translate(52,28) scale(0.62)">{mitten(0, -8, "#9C8CFF")}</g>'
    return svg(f'<g transform="translate(50 50) scale(0.84) translate(-50 -50)">{out}</g>')


def x_sun_up():
    out = stick(['<circle cx="36" cy="64" r="15"/>'], "#FFD54A")
    out += line_stick("M36,38 L36,44 M14,50 L19,54 M58,50 L53,54", "#FFB020", 4.5, sw=2.2)
    out += line_stick("M72,88 L72,36", "#2FA866", 7.0, sw=2.4)
    out += stick(['<polygon points="72,16 59,38 85,38"/>'], "#2FA866", sw=2.4)
    return svg(out)


def x_wind():
    out = line_stick("M12,32 Q44,20 74,30 Q88,34 84,44 Q81,51 72,47", "#7FB8FF", 6.0, sw=2.6)
    out += line_stick("M10,56 Q42,46 88,56", "#7FB8FF", 6.0, sw=2.6)
    out += line_stick("M16,78 Q44,70 70,78", "#7FB8FF", 6.0, sw=2.6)
    return svg(out)


def x_heart():
    return svg(stick([heart(50, 53, 36)], "#E0457B", sw=2.4))


def x_sparkle():
    return svg(sparkle(24, 24, 15, "#FFC93C"), "0 0 48 48")


EXTRA_ICONS = {
    # iter-09: the advice-row glyph (bolt/umbrella/hat_gloves/gloves/sun_up/
    # water/sunglasses/wind/heart) is gone from the UI, so only the all-done
    # sparkle burst still needs an extra icon asset.
    "sparkle": x_sparkle,
}
