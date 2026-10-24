#!/usr/bin/env python3
"""Builds every image and font the firmware uses into components/wb_ui/assets.

Run from anywhere:  ~/artenv/bin/python tools/make_assets.py
Needs: resvg_py, pillow, pypng (pip), lv_font_conv (npm, installed in tools/).

Weather Besties 2.0: the old head-only kitty set, the img_wear_* clothing
icons and the img_fx_* weather-effect sprites are gone. In their place:
full-body cat layers (assets/cat2_sets.c + cat2_layers.h + cat2_sets.h),
flat-lay garment tiles (img_g_<name>_96 / _56) and extra icons (img_x_<name>).
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
CAT_SIZE = 110
GARMENT_SIZES = (96, 56)
EXTRA_SIZE = 48

# Generated files from the 1.x pipeline that 2.0 no longer produces.
STALE_PATTERNS = ("img_kitty_", "img_wear_", "img_fx_", "font_temp_xl")
STALE_FILES = ("kitty_sets.c",)


def render(svg_text, size, path, height=None):
    data = bytes(resvg_py.svg_to_bytes(svg_string=svg_text, width=size, height=height or size))
    Image.open(io.BytesIO(data)).convert("RGBA").save(path)


def render_cropped(svg_text, w, h, path):
    """Renders on the full cat stage, then trims to the drawn pixels. Returns the trim offset."""
    data = bytes(resvg_py.svg_to_bytes(svg_string=svg_text, width=w, height=h))
    im = Image.open(io.BytesIO(data)).convert("RGBA")
    bb = im.getchannel("A").getbbox()
    if bb is None:
        raise ValueError(f"empty layer rendered for {path}")
    im.crop(bb).save(path)
    return bb[0], bb[1]


def to_c(png_path, name):
    subprocess.run([sys.executable, LVGLIMAGE, "--ofmt", "C", "--cf", "RGB565A8", "--compress", "RLE", "-o", OUT,
                    "--name", name, png_path], check=True, stdout=subprocess.DEVNULL)
    # LVGLImage names the file after the PNG; rename to the symbol name.
    produced = os.path.join(OUT, os.path.splitext(os.path.basename(png_path))[0] + ".c")
    target = os.path.join(OUT, name + ".c")
    if produced != target:
        os.replace(produced, target)


# Drop stale 1.x generated files so removed assets really leave the build.
for fn in os.listdir(OUT):
    if fn.endswith(".c") and (fn.startswith(STALE_PATTERNS) or fn in STALE_FILES):
        os.remove(os.path.join(OUT, fn))

images = []

for key, fn in icons.WEATHER.items():
    for sz, px in WX_SIZES.items():
        name = f"img_wx_{key}_{sz}"
        p = os.path.join(PNG, name + ".png")
        render(fn(), px, p)
        to_c(p, name)
        images.append(name)

for key, fn in icons.CATS.items():
    name = f"img_{key}"
    p = os.path.join(PNG, name + ".png")
    render(fn(), CAT_SIZE, p)
    to_c(p, name)
    images.append(name)

p = os.path.join(PNG, "img_drop.png")
render(icons.tiny_drop(), 22, p)
to_c(p, "img_drop")
images.append("img_drop")

# ------------------------------------------------------- garment tile icons
for key, fn in icons.GARMENTS.items():
    for px in GARMENT_SIZES:
        name = f"img_g_{key}_{px}"
        p = os.path.join(PNG, name + ".png")
        render(fn(), px, p)
        to_c(p, name)
        images.append(name)

# ------------------------------------------------------- extra icons (48 px)
for key, fn in icons.EXTRA_ICONS.items():
    name = f"img_x_{key}"
    p = os.path.join(PNG, name + ".png")
    render(fn(), EXTRA_SIZE, p)
    to_c(p, name)
    images.append(name)

# ------------------------------------------------------- full-body cat (cat2)
# Each layer is drawn on the shared 260x252 stage, cropped to its pixels, and
# its stage offset recorded. Clothing layers are shared between the two cat
# colours; only base/blink/happy/puzzled are rendered per colour.
cat2_pos = {}   # symbol name -> (layer_id, x, y)
for layer in icons.CAT2_LAYER_ORDER:
    if layer in icons.CAT2_SHARED:
        name = f"img_cat2_{layer}"
        p = os.path.join(PNG, name + ".png")
        x, y = render_cropped(icons.CAT2_SHARED[layer](), icons.CAT2_W, icons.CAT2_H, p)
        to_c(p, name)
        images.append(name)
        cat2_pos[name] = (layer, x, y)
for color, layers in icons.CAT2_PER_COLOR.items():
    for layer, fn in layers.items():
        name = f"img_cat2_{color}_{layer}"
        p = os.path.join(PNG, name + ".png")
        x, y = render_cropped(fn(), icons.CAT2_W, icons.CAT2_H, p)
        to_c(p, name)
        images.append(name)
        cat2_pos[name] = (layer, x, y)

# One shared menorah flame (the menorah layer itself is drawn flameless).
p = os.path.join(PNG, "img_cat2_flame.png")
render(icons.flame(), 10, p, 14)
to_c(p, "img_cat2_flame")
images.append("img_cat2_flame")
cat2_flames = [icons.menorah2_flame_pos(i) for i in range(9)]


def layer_enum(layer):
    return "WB_C2_" + layer.upper()


with open(os.path.join(OUT, "cat2_layers.h"), "w") as f:
    f.write("/* Generated by tools/make_assets.py. Do not edit. */\n#pragma once\n\n")
    f.write("/* Full-body cat layer ids, in stacking order (bottom to top). */\n")
    f.write("enum {\n")
    for i, layer in enumerate(icons.CAT2_LAYER_ORDER):
        f.write(f"    {layer_enum(layer)} = {i},\n")
    f.write(f"    WB_C2_COUNT = {len(icons.CAT2_LAYER_ORDER)},\n}};\n")

with open(os.path.join(OUT, "cat2_sets.h"), "w") as f:
    f.write("/* Generated by tools/make_assets.py. Do not edit. */\n#pragma once\n")
    f.write('#include "lvgl.h"\n#include "cat2_layers.h"\n\n')
    f.write(f"#define WB_CAT2_W {icons.CAT2_W}\n#define WB_CAT2_H {icons.CAT2_H}\n\n")
    f.write("""/* One dressed-cat set: for every layer id (cat2_layers.h), the cropped
 * image and where its top-left goes on the WB_CAT2_W x WB_CAT2_H stage.
 * Layers an outfit does not use are simply not drawn; there are no NULLs,
 * every slot holds a valid image. The black and pink sets share the same
 * clothing/prop images; only base/blink/happy/puzzled differ. */
typedef struct {
    const lv_image_dsc_t *img[WB_C2_COUNT];
    int16_t x[WB_C2_COUNT];
    int16_t y[WB_C2_COUNT];
} wb_cat2_t;

extern const wb_cat2_t wb_cat2_black;
extern const wb_cat2_t wb_cat2_pink;

/* Menorah flames: the prop_menorah layer is drawn without flames. Draw
 * img_cat2_flame at these stage positions for candles 0..n-1 (left to
 * right; index 4 is the shamash). */
LV_IMAGE_DECLARE(img_cat2_flame);
""")
    f.write("static const lv_point_t WB_CAT2_MENORAH_FLAMES[9] = {"
            + ", ".join(f"{{{x}, {y}}}" for x, y in cat2_flames) + "};\n")

with open(os.path.join(OUT, "cat2_sets.c"), "w") as f:
    f.write("/* Generated by tools/make_assets.py. Do not edit. */\n#include \"assets.h\"\n")
    for color in icons.CAT2_PER_COLOR:
        f.write(f"\nconst wb_cat2_t wb_cat2_{color} = {{\n")
        for field in ("img", "x", "y"):
            f.write(f"    .{field} = {{\n")
            for layer in icons.CAT2_LAYER_ORDER:
                if layer in icons.CAT2_PER_COLOR[color]:
                    name = f"img_cat2_{color}_{layer}"
                else:
                    name = f"img_cat2_{layer}"
                _, x, y = cat2_pos[name]
                val = f"&{name}" if field == "img" else str(x if field == "x" else y)
                f.write(f"        [{layer_enum(layer)}] = {val},\n")
            f.write("    },\n")
        f.write("};\n")

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
    ("font_h1",        "Fredoka-SemiBold.ttf", 40,  TEXT,                  False),
    ("font_temp_l",    "Fredoka-SemiBold.ttf", 40,  TEXT,                  False),
    ("font_label",     "Fredoka-SemiBold.ttf", 30,  TEXT,                  False),
    ("font_title",     "Fredoka-SemiBold.ttf", 32,  TEXT,                  True),
    ("font_body",      "Fredoka-Medium.ttf",   26,  TEXT,                  True),
    ("font_chip",      "Fredoka-SemiBold.ttf", 24,  TEXT,                  True),
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
    f.write("/* Generated by tools/make_assets.py. Do not edit. */\n#pragma once\n#include \"lvgl.h\"\n")
    f.write('#include "cat2_sets.h"\n\n')
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
    f.write("\n/* Garment tile icons, in garment_t order (see outfit.h). */\n")
    for px in GARMENT_SIZES:
        arr = ", ".join(f"&img_g_{k}_{px}" for k in icons.GARMENTS)
        f.write(f"#define WB_GARMENT_ICONS_{px} {{ {arr} }}\n")

with open(os.path.join(OUT, "assets_list.cmake"), "w") as f:
    srcs = [n + ".c" for n in images] + [j[0] + ".c" for j in FONT_JOBS] + ["cat2_sets.c"]
    f.write("set(WB_ASSET_SRCS\n" + "\n".join(f"    assets/{s}" for s in srcs) + "\n)\n")

gen_files = [n + ".c" for n in images] + [j[0] + ".c" for j in FONT_JOBS] + [
    "cat2_sets.c", "cat2_sets.h", "cat2_layers.h", "assets.h"]
total = sum(os.path.getsize(os.path.join(OUT, fn)) for fn in gen_files)
img_bytes = 0
for n in images:
    with Image.open(os.path.join(PNG, n + ".png")) as im:
        img_bytes += im.width * im.height * 3 + 32  # RGB565A8 + descriptor/header slack
print(f"{len(images)} images, {len(FONT_JOBS)} fonts -> {OUT}")
print(f"total generated source bytes (.c/.h, generated files only): {total} ({total / 1024:.0f} KiB)")
print(f"image binary estimate (RGB565A8, w*h*3+32 per image): {img_bytes} ({img_bytes / 1024:.0f} KiB)")
print("weather order:", ", ".join(icons.WEATHER.keys()))
print("garment order:", ", ".join(icons.GARMENTS.keys()))
print("cat2 layers:", ", ".join(icons.CAT2_LAYER_ORDER))
