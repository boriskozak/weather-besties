#!/usr/bin/env python3
"""Builds every image and font the firmware uses into components/wb_ui/assets.

Run from anywhere:  python3 tools/make_assets.py
Needs: resvg_py, pillow, pypng (pip), lv_font_conv (npm, installed in tools/).
"""
import io
import os
import subprocess
import sys

import resvg_py
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import icons  # noqa: E402

OUT = os.path.join(ROOT, "components", "wb_ui", "assets")
PNG = os.path.join(HERE, "build", "png")
LVGLIMAGE = os.path.join(ROOT, "components", "lvgl", "scripts", "LVGLImage.py")
FONTS = os.path.join(HERE, "fonts")
FONTCONV = os.path.join(HERE, "node_modules", ".bin", "lv_font_conv")

os.makedirs(OUT, exist_ok=True)
os.makedirs(PNG, exist_ok=True)

WX_SIZES = {"m": 110, "s": 72}
CLOTH_SIZE = 60
CAT_SIZE = 110


def render(svg_text, size, path, height=None):
    data = bytes(resvg_py.svg_to_bytes(svg_string=svg_text, width=size, height=height or size))
    Image.open(io.BytesIO(data)).convert("RGBA").save(path)


def render_cropped(svg_text, w, h, path):
    """Renders on the full cat stage, then trims to the drawn pixels. Returns the trim offset."""
    data = bytes(resvg_py.svg_to_bytes(svg_string=svg_text, width=w, height=h))
    im = Image.open(io.BytesIO(data)).convert("RGBA")
    bb = im.getchannel("A").getbbox()
    im.crop(bb).save(path)
    return bb[0], bb[1]


def to_c(png_path, name):
    subprocess.run([sys.executable, LVGLIMAGE, "--ofmt", "C", "--cf", "RGB565A8", "-o", OUT,
                    "--name", name, png_path], check=True, stdout=subprocess.DEVNULL)
    # LVGLImage names the file after the PNG; rename to the symbol name.
    produced = os.path.join(OUT, os.path.splitext(os.path.basename(png_path))[0] + ".c")
    target = os.path.join(OUT, name + ".c")
    if produced != target:
        os.replace(produced, target)


images = []

for key, fn in icons.WEATHER.items():
    for sz, px in WX_SIZES.items():
        name = f"img_wx_{key}_{sz}"
        p = os.path.join(PNG, name + ".png")
        render(fn(), px, p)
        to_c(p, name)
        images.append(name)

for key, fn in icons.CLOTHES.items():
    name = f"img_wear_{key}"
    p = os.path.join(PNG, name + ".png")
    render(fn(), CLOTH_SIZE, p)
    to_c(p, name)
    images.append(name)

for key, fn in icons.CATS.items():
    name = f"img_{key}"
    p = os.path.join(PNG, name + ".png")
    render(fn(), CAT_SIZE, p)
    to_c(p, name)
    images.append(name)

# The yellow-eyed cat: layers cropped from one shared stage, plus where each goes,
# drawn at each size a layout might use (vector art, so every size is crisp).
KITTY_SETS = {"": 1.5}   # one size; add more here if a layout needs another
kitty_sets = {}
for tag, sc in KITTY_SETS.items():
    prefix = f"img_kitty{tag}"
    w, h = round(icons.CAT_W * sc), round(icons.CAT_H * sc)
    pos = {}
    for key, fn in icons.CAT_LAYERS.items():
        name = f"{prefix}_{key}"
        p = os.path.join(PNG, name + ".png")
        pos[key] = (name,) + render_cropped(fn(), w, h, p)
        to_c(p, name)
        images.append(name)
    name = f"{prefix}_flame"
    p = os.path.join(PNG, name + ".png")
    render(icons.flame(), round(10 * sc), p, round(14 * sc))
    to_c(p, name)
    images.append(name)
    flames = []
    for i in range(9):
        cx = icons.menorah_candle_x(i)
        top = icons.MENORAH_TOP - (6 if i == 4 else 0)
        flames.append((round((cx - 5) * sc), round((top - 13) * sc)))
    kitty_sets[tag] = dict(w=w, h=h, edge=round(icons.CAT_EDGE * sc), pos=pos, flame=name, flames=flames)

for name, fn, w, h in (("img_kitty_flake", icons.snowflake, 16, 16),):
    p = os.path.join(PNG, name + ".png")
    render(fn(), w, p, h)
    to_c(p, name)
    images.append(name)

# Weather effects: rain, snow, leaves, stars, lightning (ui.c animates them).
fx_names = []
for key, (svg_text, w, h) in icons.FX.items():
    name = f"img_fx_{key}"
    p = os.path.join(PNG, name + ".png")
    render(svg_text, w, p, h)
    to_c(p, name)
    images.append(name)
    fx_names.append(name)

p = os.path.join(PNG, "img_drop.png")
render(icons.tiny_drop(), 22, p)
to_c(p, "img_drop")
images.append("img_drop")

# ------------------------------------------------------------------ fonts
TEXT = "0x20-0x7E,0xB0,0xB7,0x2019,0x201C,0x201D,0x2022,0x2026"
SYMBOLS = ",".join(hex(c) for c in (
    0xF1EB,  # wifi
    0xF013,  # settings
    0xF021,  # refresh
    0xF071,  # warning
    0xF00C,  # ok
    0xF00D,  # close
    0xF053,  # left
    0xF054,  # right
    0xF124,  # gps / location
))
FA = os.path.join(FONTS, "FontAwesome5-Solid+Brands+Regular.woff")

FONT_JOBS = [
    # name,             ttf,                  size, ranges,               add symbols
    ("font_temp_xl",   "Fredoka-SemiBold.ttf", 140, "0x30-0x39,0x2D,0xB0", False),
    ("font_temp_l",    "Fredoka-SemiBold.ttf", 40,  TEXT,                  False),
    ("font_title",     "Fredoka-SemiBold.ttf", 32,  TEXT,                  True),
    ("font_body",      "Fredoka-Medium.ttf",   26,  TEXT,                  True),
    ("font_small",     "Fredoka-Medium.ttf",   22,  TEXT,                  True),
    ("font_tiny",      "Fredoka-Medium.ttf",   18,  TEXT,                  True),
]

for name, ttf, size, rng, sym in FONT_JOBS:
    cmd = [FONTCONV, "--bpp", "4", "--size", str(size), "--no-compress", "--format", "lvgl",
           "--lv-include", "lvgl.h", "--lv-font-name", name,
           "--font", os.path.join(FONTS, ttf), "--range", rng]
    if sym:
        cmd += ["--font", FA, "--range", SYMBOLS]
    cmd += ["-o", os.path.join(OUT, name + ".c")]
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL)

# ------------------------------------------------------------------ header
with open(os.path.join(OUT, "assets.h"), "w") as f:
    f.write("/* Generated by tools/make_assets.py. Do not edit. */\n#pragma once\n#include \"lvgl.h\"\n\n")
    for n in images:
        f.write(f"LV_IMAGE_DECLARE({n});\n")
    f.write("\n")
    for name, *_ in FONT_JOBS:
        f.write(f"LV_FONT_DECLARE({name});\n")
    f.write("\n/* Weather icon tables, indexed by wx_icon_t (see wx.h). */\n")
    keys = list(icons.WEATHER.keys())
    for sz in WX_SIZES:
        arr = ", ".join(f"&img_wx_{k}_{sz}" for k in keys)
        f.write(f"#define WB_WX_ICONS_{sz.upper()} {{ {arr} }}\n")
    f.write("\n/* Clothing icon table, indexed by wear_t (see advice.h). */\n")
    arr = ", ".join(f"&img_wear_{k}" for k in icons.CLOTHES)
    f.write(f"#define WB_WEAR_ICONS {{ {arr} }}\n")
    f.write("""
/* The yellow-eyed cat, one table per size (kitty_sets.c). Every layer is cropped
 * from one shared stage; x/y is where it goes on that stage. */
typedef struct {
    const lv_image_dsc_t *img;
    int16_t x, y;
} wb_layer_t;

typedef struct {
    int16_t w, h, edge;   /* stage size; edge = where the card top sits */
    wb_layer_t base, blink, hat_witch, hat_santa, hat_beanie, hat_rain, neck_scarf, neck_scarf_blue, neck_cape,
        face_sunglasses, prop_pumpkin, prop_present, prop_menorah;
    const lv_image_dsc_t *flame;
    lv_point_t flames[9]; /* flame top-left on each menorah candle, left to right; [4] is the shamash */
} wb_kitty_set_t;

""")
    for tag in kitty_sets:
        f.write(f"extern const wb_kitty_set_t wb_kitty{tag};\n")

    f.write("\n/* Autumn leaves: [color][angle] (orange, red, yellow x 4 tumble frames). */\n")
    rows = []
    for cname in icons.LEAF_COLORS:
        rows.append("{ " + ", ".join(f"&img_fx_leaf_{cname}_{i}" for i in range(len(icons.LEAF_ANGLES))) + " }")
    f.write(f"#define WB_FX_LEAVES {{ {', '.join(rows)} }}\n")
    f.write(f"#define WB_FX_LEAF_COLORS {len(icons.LEAF_COLORS)}\n#define WB_FX_LEAF_FRAMES {len(icons.LEAF_ANGLES)}\n")

with open(os.path.join(OUT, "kitty_sets.c"), "w") as f:
    f.write("/* Generated by tools/make_assets.py. Do not edit. */\n#include \"assets.h\"\n")
    for tag, ks in kitty_sets.items():
        f.write(f"\nconst wb_kitty_set_t wb_kitty{tag} = {{\n")
        f.write(f"    .w = {ks['w']}, .h = {ks['h']}, .edge = {ks['edge']},\n")
        for key, (name, x, y) in ks["pos"].items():
            f.write(f"    .{key} = {{&{name}, {x}, {y}}},\n")
        f.write(f"    .flame = &{ks['flame']},\n")
        f.write("    .flames = {" + ", ".join(f"{{{x}, {y}}}" for x, y in ks["flames"]) + "},\n};\n")

with open(os.path.join(OUT, "assets_list.cmake"), "w") as f:
    srcs = [n + ".c" for n in images] + [j[0] + ".c" for j in FONT_JOBS] + ["kitty_sets.c"]
    f.write("set(WB_ASSET_SRCS\n" + "\n".join(f"    assets/{s}" for s in srcs) + "\n)\n")

print(f"{len(images)} images, {len(FONT_JOBS)} fonts -> {OUT}")
print("weather order:", ", ".join(icons.WEATHER.keys()))
print("wear order:", ", ".join(icons.CLOTHES.keys()))
