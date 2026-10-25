#!/usr/bin/env python3
"""Builds the Pippa character assets into components/wb_ui/assets.

Pippa's artwork comes from external packages (flattened full-character
sprites, 260x252, transparent PNGs; PNG transparency is authoritative).
This script converts the pieces the UI uses into the same LVGL image
format as every other asset (RGB565A8, RLE compressed, via
components/lvgl/scripts/LVGLImage.py) and generates:

  assets/img_pippa_<variant>.c    (13 sprites: six outfits x two fur
                                   colourways, plus the undressed base)
  assets/img_pippa_heart_0..5.c   (the heart animation, black dress only)
  assets/img_pippa_head.c         (head crop of the dress sprite, for
                                   the boot / setup / wifi-error screens)
  assets/img_pippa_mini_*.c       (forecast-card minis of the twelve
                                   outfit sprites, downscaled at build
                                   time; see MINI_H below)
  assets/img_pippa_head_*_thumb.c (Settings cat-pill head thumbnails,
                                   black + pink; see THUMB_H below)
  assets/img_pippa_heart_overlay.c (the all-done heart gesture from
                                   heart frame 04, for the outfits the
                                   heart animation does not cover)
  assets/pippa_sets.h / pippa_sets.c   (descriptors + stage positions)
  assets/pippa_assets_list.cmake     (firmware source list fragment)

Sources: the wardrobe expansion package (WB_WARDROBE_DIR, default
../../pippa-complete/Pippa/wardrobe-expansion) holds the twelve outfit
sprites in sprites/black/ and sprites/pink/. The original package
(WB_PIPPA_DIR, default ../../pippa-incoming/Pippa) still supplies the
base sprite and the heart frames: the wardrobe package has neither
(the heart animation exists for the black dress outfit only, and the
UI plays it only on that sprite).

Unlike the cat2 layers, Pippa sprites are NOT swappable garment layers:
each is a complete character. The UI draws exactly one sprite (or one
heart frame) in the 260x252 stage slot. Sprites are cropped to their
alpha bounding box and the crop offset is recorded, exactly like the
cat2 pipeline, so positions stay honest.

Run from anywhere:  ~/artenv/bin/python tools/make_pippa_assets.py
Needs: pillow. NOTE: tools/make_assets.py regenerates assets_list.cmake
and assets.h from scratch; it does not touch the pippa_* files, and
pippa_sets.h is included by ui.c directly, not via assets.h.
"""
import os
import subprocess
import sys
from collections import deque

from PIL import Image, ImageChops

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
PKG = os.environ.get("WB_PIPPA_DIR",
                     os.path.join(os.path.dirname(ROOT), "pippa-incoming", "Pippa"))
WARDROBE = os.environ.get(
    "WB_WARDROBE_DIR",
    os.path.join(os.path.dirname(ROOT), "pippa-complete", "Pippa", "wardrobe-expansion"))
OUT = os.path.join(ROOT, "components", "wb_ui", "assets")
PNG = os.path.join(HERE, "build", "png")
LVGLIMAGE = os.path.join(ROOT, "components", "lvgl", "scripts", "LVGLImage.py")

os.makedirs(OUT, exist_ok=True)
os.makedirs(PNG, exist_ok=True)

# (enum name, symbol, source PNG) in pippa_variant_t order (pippa_sets.h).
# Outfit variants come in black/pink pairs: the pink variant is always
# the black variant's enum value + 1 (ui.c relies on this). PIPPA_BASE
# stands alone at the end: one undressed sprite for the unavailable
# screen, whatever the colourway setting.
VARIANTS = []
for _outfit in ("dress", "pants", "snow", "rain", "hot", "fall"):
    for _color in ("black", "pink"):
        _enum = "PIPPA_" + (_outfit if _color == "black" else f"PINK_{_outfit}").upper()
        _sym = "img_pippa_" + (_outfit if _color == "black" else f"pink_{_outfit}")
        VARIANTS.append((_enum, _sym, os.path.join(
            WARDROBE, "sprites", _color, f"pippa-{_outfit}.png")))
VARIANTS.append(("PIPPA_BASE", "img_pippa_base",
                 os.path.join(PKG, "sprites", "pippa_base-260x252.png")))
SPRITES = [(sym, src) for _, sym, src in VARIANTS]

HEART_MS = [260, 150, 150, 240, 500, 400]   # animations/manifest.json
HEART = [(f"img_pippa_heart_{i}", os.path.join(PKG, "animations", "frames", "heart", f"0{i}.png"))
         for i in range(6)]

# Pippa's head, for the boot / setup / wifi-error screens: a crop of the
# dress sprite (ears to chin), NOT a redrawn mark. The window is in
# sprite coordinates (260x252). It stays above the neck (the alpha rows
# narrow at y ~104-120) and left of the tail, which only rises at
# x > 200 / y > 120, so neither leaks into the crop. The wardrobe's
# black dress is pixel-identical to the original package's dress
# sprite (verified), so the crop is unchanged.
HEAD_NAME = "img_pippa_head"
HEAD_SRC = os.path.join(WARDROBE, "sprites", "black", "pippa-dress.png")
HEAD_WINDOW = (48, 4, 198, 120)

# Settings cat-pill thumbnails (iter-11, panel round 9): the Cat
# chooser was three text pills on the one screen about the cat that
# showed no cat. Each pill now carries a small head of the cat it
# selects, cropped from the dress sprites with the head window above
# and downscaled at build time (like the forecast minis: this LVGL
# build cannot be trusted to downscale at runtime), one per
# colourway; "Take turns" shows both.
THUMB_H = 32
THUMBS = [("img_pippa_head_thumb", HEAD_SRC),
          ("img_pippa_head_pink_thumb",
           os.path.join(WARDROBE, "sprites", "pink", "pippa-dress.png"))]

# All-done heart overlay (iter-11, panel round 9): the heart
# animation exists only for the black dress sprite, so on every
# other outfit the all-done celebration used to leave the cat
# pixel-identical to her unchecked state. The overlay is the heart
# gesture itself, extracted from heart frame 04: within the window
# below, the pixels that differ from the static dress sprite AND
# read as the gesture (near-black paw outline or the heart's pink
# centre), flood-filled from the centre so stray dress/bow pixels
# the paws vacated stay out, with the centre's enclosed highlight
# holes filled in the centre's median pink. Drawn over the current
# sprite's chest (all sprites share the 260x252 stage space) with
# the sparkles, so all-done celebrates the same everywhere.
HEART_OVERLAY_NAME = "img_pippa_heart_overlay"
HEART_OVERLAY_FRAME = os.path.join(PKG, "animations", "frames", "heart", "04.png")
HEART_OVERLAY_WINDOW = (99, 106, 165, 152)   # x0, y0, x1, y1 in stage space
HEART_OVERLAY_SEED = (126, 131)              # (y, x) inside the pink centre
HEART_CENTRE_BOX = (112, 114, 151, 143)      # hole filling stays in here

# Forecast-card minis (iter-07): one small copy of each outfit sprite for
# the Next Days cards. They are downscaled HERE, at asset-build time,
# because this LVGL build cannot downscale the RLE sprites at runtime (a
# runtime-scaled RLE image draws nothing; an unscaled one draws fine).
# Resized in stage space (252 -> MINI_H px tall) and cropped to the alpha
# bbox like every other piece, so a card shows the whole cat, small.
# PIPPA_BASE gets no mini: the forecast never shows the undressed sprite.
MINI_H = 104   # iter-08 (panel round 6): at 50 the minis read as a logo,
               # not the star. The forecast cards were redesigned around
               # the bigger cat (72 px weather icon, the garment icon +
               # label stacked beside her instead of one row of three),
               # so ~104 px tall fits the 236x344 card with the day,
               # weather, temps and key garment all legible.
MINIS = [(sym.replace("img_pippa_", "img_pippa_mini_"), src)
         for _, sym, src in VARIANTS if sym != "img_pippa_base"]


def convert_mini(png_path, name):
    """Downscale a sprite to MINI_H px tall (premultiplied, so the
    transparent black around the art cannot bleed into her outline),
    crop to the alpha bbox, convert like `convert`."""
    im = Image.open(png_path).convert("RGBA")
    w = round(im.width * MINI_H / im.height)
    r, g, b, a = im.split()
    pm = Image.merge("RGBA", (ImageChops.multiply(r, a),
                               ImageChops.multiply(g, a),
                               ImageChops.multiply(b, a), a))
    pm = pm.resize((w, MINI_H), Image.LANCZOS)
    r, g, b, a = pm.split()
    px = [(min(255, rv * 255 // av) if av else 0,
           min(255, gv * 255 // av) if av else 0,
           min(255, bv * 255 // av) if av else 0, av)
          for rv, gv, bv, av in zip(r.getdata(), g.getdata(),
                                    b.getdata(), a.getdata())]
    im = Image.new("RGBA", pm.size)
    im.putdata(px)
    bb = im.getchannel("A").getbbox()
    cropped = os.path.join(PNG, name + ".png")
    im.crop(bb).save(cropped)
    subprocess.run([sys.executable, LVGLIMAGE, "--ofmt", "C", "--cf", "RGB565A8",
                    "--compress", "RLE", "-o", OUT, "--name", name, cropped],
                   check=True, stdout=subprocess.DEVNULL)
    produced = os.path.join(OUT, name + ".png.c")
    if not os.path.exists(produced):
        produced = os.path.join(OUT, os.path.splitext(os.path.basename(cropped))[0] + ".c")
    target = os.path.join(OUT, name + ".c")
    if produced != target:
        os.replace(produced, target)
    return bb[2] - bb[0], bb[3] - bb[1]


def _write_lvimg(png_path, name):
    """Convert one prepared PNG to an LVGL C image in OUT (the tail
    of convert()/convert_mini(), shared by the iter-11 assets)."""
    subprocess.run([sys.executable, LVGLIMAGE, "--ofmt", "C", "--cf", "RGB565A8",
                    "--compress", "RLE", "-o", OUT, "--name", name, png_path],
                   check=True, stdout=subprocess.DEVNULL)
    produced = os.path.join(OUT, name + ".png.c")
    if not os.path.exists(produced):
        produced = os.path.join(OUT, os.path.splitext(os.path.basename(png_path))[0] + ".c")
    target = os.path.join(OUT, name + ".c")
    if produced != target:
        os.replace(produced, target)


def convert_thumb(src_png, name):
    """Head thumbnail for the Settings pills: the head window of a
    dress sprite, downscaled at build time to THUMB_H px tall
    (premultiplied, like the minis), cropped to its alpha bbox."""
    im = Image.open(src_png).convert("RGBA").crop(HEAD_WINDOW)
    w = round(im.width * THUMB_H / im.height)
    r, g, b, a = im.split()
    pm = Image.merge("RGBA", (ImageChops.multiply(r, a),
                               ImageChops.multiply(g, a),
                               ImageChops.multiply(b, a), a))
    pm = pm.resize((w, THUMB_H), Image.LANCZOS)
    r, g, b, a = pm.split()
    px = [(min(255, rv * 255 // av) if av else 0,
           min(255, gv * 255 // av) if av else 0,
           min(255, bv * 255 // av) if av else 0, av)
          for rv, gv, bv, av in zip(r.getdata(), g.getdata(),
                                    b.getdata(), a.getdata())]
    im = Image.new("RGBA", pm.size)
    im.putdata(px)
    bb = im.getchannel("A").getbbox()
    cropped = os.path.join(PNG, name + ".png")
    im.crop(bb).save(cropped)
    _write_lvimg(cropped, name)
    return bb[2] - bb[0], bb[3] - bb[1]


def _flood(mask, w, h, seeds):
    """4-connected flood fill over a set of (x, y) in a w x h mask;
    a seed outside the mask snaps to the nearest mask pixel."""
    keep = set()
    dq = deque()
    for sx, sy in seeds:
        if (sx, sy) not in mask:
            if not mask:
                return keep
            sx, sy = min(mask, key=lambda p: (p[0] - sx) ** 2 + (p[1] - sy) ** 2)
        if (sx, sy) not in keep:
            keep.add((sx, sy))
            dq.append((sx, sy))
    while dq:
        x, y = dq.popleft()
        for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if (nx, ny) in mask and (nx, ny) not in keep:
                keep.add((nx, ny))
                dq.append((nx, ny))
    return keep


def convert_heart_overlay():
    """Extract the heart gesture from heart frame 04 (see the
    HEART_OVERLAY note above). Returns (stage x, stage y, w, h)."""
    frame = Image.open(HEART_OVERLAY_FRAME).convert("RGBA")
    static = Image.open(HEAD_SRC).convert("RGBA")
    fp, sp = frame.load(), static.load()
    x0, y0, x1, y1 = HEART_OVERLAY_WINDOW
    mask = set()
    for y in range(y0, y1):
        for x in range(x0, x1):
            r, g, b, a = fp[x, y]
            if a <= 200:
                continue
            sr, sg, sb, _ = sp[x, y]
            if max(abs(r - sr), abs(g - sg), abs(b - sb)) <= 40:
                continue
            dark = r < 95 and g < 95 and b < 95
            pink = r > 215 and 100 < g < 175 and 140 < b < 215
            if dark or pink:
                mask.add((x, y))
    seed = (HEART_OVERLAY_SEED[1], HEART_OVERLAY_SEED[0])   # stored (y, x)
    keep = _flood(mask, frame.width, frame.height, [seed])
    # Enclosed highlight holes inside the pink centre only: flood the
    # centre box's complement from its border; what it cannot reach
    # is a hole and takes the centre's median pink.
    cx0, cy0, cx1, cy1 = HEART_CENTRE_BOX
    cbox = {(x, y) for y in range(cy0, cy1) for x in range(cx0, cx1)}
    border = ([(x, cy0) for x in range(cx0, cx1)] + [(x, cy1 - 1) for x in range(cx0, cx1)]
              + [(cx0, y) for y in range(cy0, cy1)] + [(cx1 - 1, y) for y in range(cy0, cy1)])
    reach = _flood(cbox - keep, frame.width, frame.height, border)
    holes = cbox - keep - reach
    pinks = sorted(fp[p] for p in keep
                   if fp[p][0] > 215 and 100 < fp[p][1] < 175 and 140 < fp[p][2] < 215)
    med = pinks[len(pinks) // 2][:3] if pinks else (252, 155, 183)
    full = keep | holes
    # Below the heart's point only the point's own columns survive:
    # the connected strays there are dress, not gesture.
    full = {(x, y) for (x, y) in full if y < 147 or 120 <= x <= 142}
    xs = [p[0] for p in full]
    ys = [p[1] for p in full]
    bb = (min(xs), min(ys), max(xs) + 1, max(ys) + 1)
    out = Image.new("RGBA", frame.size)
    op = out.load()
    for (x, y) in full:
        op[x, y] = fp[x, y] if (x, y) in keep else (med[0], med[1], med[2], 255)
    cropped = os.path.join(PNG, HEART_OVERLAY_NAME + ".png")
    out.crop(bb).save(cropped)
    _write_lvimg(cropped, HEART_OVERLAY_NAME)
    return bb[0], bb[1], bb[2] - bb[0], bb[3] - bb[1]


def convert(png_path, name, window=None):
    """Crop (optionally to `window` first) to the alpha bbox, convert to
    an LVGL C image, return (x, y, w, h) of the alpha bbox."""
    im = Image.open(png_path).convert("RGBA")
    if window is not None:
        im = im.crop(window)
    bb = im.getchannel("A").getbbox()
    if bb is None:
        raise ValueError(f"empty sprite: {png_path}")
    cropped = os.path.join(PNG, name + ".png")
    im.crop(bb).save(cropped)
    subprocess.run([sys.executable, LVGLIMAGE, "--ofmt", "C", "--cf", "RGB565A8",
                    "--compress", "RLE", "-o", OUT, "--name", name, cropped],
                   check=True, stdout=subprocess.DEVNULL)
    produced = os.path.join(OUT, name + ".png.c")
    if not os.path.exists(produced):
        produced = os.path.join(OUT, os.path.splitext(os.path.basename(cropped))[0] + ".c")
    target = os.path.join(OUT, name + ".c")
    if produced != target:
        os.replace(produced, target)
    return bb[0], bb[1], bb[2] - bb[0], bb[3] - bb[1]


def main():
    if not os.path.isdir(PKG):
        raise SystemExit(f"Pippa package not found: {PKG} (set WB_PIPPA_DIR)")
    if not os.path.isdir(WARDROBE):
        raise SystemExit(f"Wardrobe package not found: {WARDROBE} (set WB_WARDROBE_DIR)")
    jobs = SPRITES + HEART
    info = {}
    for name, src in jobs:
        info[name] = convert(src, name)
        x, y, w, h = info[name]
        print(f"{name}: cropped {w}x{h} at stage ({x}, {y})")
    hx, hy, hw, hh = convert(HEAD_SRC, HEAD_NAME, HEAD_WINDOW)
    print(f"{HEAD_NAME}: cropped {hw}x{hh} from head window {HEAD_WINDOW} at ({hx}, {hy})")
    for name, src in MINIS:
        mw, mh = convert_mini(src, name)
        print(f"{name}: mini {mw}x{mh}")
    for name, src in THUMBS:
        tw, th = convert_thumb(src, name)
        print(f"{name}: thumb {tw}x{th}")
    ox, oy, ow, oh = convert_heart_overlay()
    print(f"{HEART_OVERLAY_NAME}: overlay {ow}x{oh} at stage ({ox}, {oy})")

    with open(os.path.join(OUT, "pippa_sets.h"), "w") as f:
        f.write("""/* Pippa character assets: generated by tools/make_pippa_assets.py.
 * Do not edit by hand; rerun that script instead.
 *
 * Pippa sprites are FLATTENED full-character images for the 260x252
 * stage, not interchangeable garment layers (see the wardrobe
 * package README). Exactly one sprite, or one heart-animation frame,
 * is drawn in the stage slot at its recorded position. The heart
 * frames exist for the black dress outfit only. */
#pragma once
#include "lvgl.h"

""")
        for name, _ in jobs:
            f.write(f"LV_IMAGE_DECLARE({name});\n")
        f.write(f"LV_IMAGE_DECLARE({HEAD_NAME});   /* head crop for boot/setup/error */\n")
        for name, _ in MINIS:
            f.write(f"LV_IMAGE_DECLARE({name});   /* forecast-card mini */\n")
        for name, _ in THUMBS:
            f.write(f"LV_IMAGE_DECLARE({name});   /* Settings cat-pill thumbnail */\n")
        f.write(f"LV_IMAGE_DECLARE({HEART_OVERLAY_NAME});   /* all-done heart overlay */\n")
        f.write("""
/* Static variants, in draw order. Outfit variants come in black/pink
 * pairs (pink = black + 1); PIPPA_BASE is the undressed sprite for
 * the unavailable screen and has no pair. PIPPA_NONE means "no Pippa
 * on stage" (kept for the stage code; the outfit pages now always
 * show Pippa, in the colourway the Settings choice resolves to). */
typedef enum {
""")
        for i, (enum, _, _) in enumerate(VARIANTS):
            f.write(f"    {enum} = {i},\n")
        f.write(f"""    PIPPA_COUNT = {len(VARIANTS)}
}} pippa_variant_t;
#define PIPPA_NONE (-1)

extern const lv_image_dsc_t *const WB_PIPPA_SPRITES[PIPPA_COUNT];
extern const lv_point_t WB_PIPPA_SPRITE_POS[PIPPA_COUNT];

/* Forecast-card minis, index-aligned with WB_PIPPA_SPRITES (NULL for
 * PIPPA_BASE): build-time downscales of the outfit sprites, drawn
 * unscaled on the Next Days cards (iter-07). */
extern const lv_image_dsc_t *const WB_PIPPA_MINIS[PIPPA_COUNT];

/* Heart animation (all-done celebration): black dress variant only,
 * frames in manifest order, played once and holding the last frame
 * (the desktop renderer loops it while all-done lasts, so the motion
 * is visible in the emulator and in stills). On any other outfit or
 * colourway, all-done holds the static sprite. */
extern const lv_image_dsc_t *const WB_PIPPA_HEART_FRAMES[6];
extern const lv_point_t WB_PIPPA_HEART_POS[6];

/* All-done heart overlay (iter-11): the heart gesture extracted
 * from heart frame 04, drawn over any other sprite's chest at this
 * stage position when all-done shows it instead of the animation,
 * so the celebration is the same on every outfit. */
extern const lv_point_t WB_PIPPA_HEART_OVERLAY_POS;
""")
        f.write("static const uint16_t WB_PIPPA_HEART_MS[6] = {"
                + ", ".join(str(m) for m in HEART_MS) + "};\n")
        f.write(f"#define WB_PIPPA_HEART_TOTAL_MS {sum(HEART_MS)}\n")

    with open(os.path.join(OUT, "pippa_sets.c"), "w") as f:
        f.write("/* Generated by tools/make_pippa_assets.py. Do not edit. */\n")
        f.write('#include "pippa_sets.h"\n\n')
        f.write("const lv_image_dsc_t *const WB_PIPPA_SPRITES[PIPPA_COUNT] = {"
                + ", ".join(f"&{n}" for n, _ in SPRITES) + "};\n")
        f.write("const lv_point_t WB_PIPPA_SPRITE_POS[PIPPA_COUNT] = {"
                + ", ".join(f"{{{info[n][0]}, {info[n][1]}}}" for n, _ in SPRITES) + "};\n")
        mini_by_variant = {sym: mini for mini, (_, sym, _) in
                             zip([m for m, _ in MINIS],
                                 [v for v in VARIANTS if v[1] != "img_pippa_base"])}
        f.write("const lv_image_dsc_t *const WB_PIPPA_MINIS[PIPPA_COUNT] = {"
                + ", ".join(("&" + mini_by_variant[sym]) if sym in mini_by_variant else "NULL"
                            for _, sym, _ in VARIANTS) + "};\n\n")
        f.write("const lv_image_dsc_t *const WB_PIPPA_HEART_FRAMES[6] = {"
                + ", ".join(f"&{n}" for n, _ in HEART) + "};\n")
        f.write("const lv_point_t WB_PIPPA_HEART_POS[6] = {"
                + ", ".join(f"{{{info[n][0]}, {info[n][1]}}}" for n, _ in HEART) + "};\n")
        f.write(f"const lv_point_t WB_PIPPA_HEART_OVERLAY_POS = {{{ox}, {oy}}};\n")

    with open(os.path.join(OUT, "pippa_assets_list.cmake"), "w") as f:
        srcs = ([n + ".c" for n, _ in jobs] + [HEAD_NAME + ".c"]
                + [n + ".c" for n, _ in MINIS] + [n + ".c" for n, _ in THUMBS]
                + [HEART_OVERLAY_NAME + ".c"] + ["pippa_sets.c"])
        f.write("# Generated by tools/make_pippa_assets.py. Do not edit.\n")
        f.write("set(WB_PIPPA_ASSET_SRCS\n"
                + "\n".join(f"    assets/{s}" for s in srcs) + "\n)\n")
    print(f"{len(jobs)} Pippa images -> {OUT}")


if __name__ == "__main__":
    main()
