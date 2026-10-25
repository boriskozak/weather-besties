#!/usr/bin/env python3
"""Pixel-level guard for `make shots`.

The 2.0 build once shipped renders in which every bitmap image decoded to
nothing (LVGL's bin decoder refused the RLE-compressed assets because
LV_BIN_DECODER_RAM_LOAD was off) while text and shapes rendered fine, and
nothing in the pipeline noticed. This check makes that failure loud: it
asserts, on the actual PNGs, that the cat is visibly present on the outfit
pages, that the garment tiles carry icon pixels, and that scenarios which
must differ do differ.

Usage: python3 check_renders.py [outdir]   (default: out)
Exit code 0 = all assertions passed.
"""
import hashlib
import os
import sys

from PIL import Image

OUT = sys.argv[1] if len(sys.argv) > 1 else "out"

# Page backgrounds (plan section 2). Tolerance covers anti-aliasing noise.
BGS = [(251, 246, 238), (242, 239, 248), (244, 248, 252), (255, 255, 255)]


def non_bg_fraction(img, box):
    x0, y0, x1, y1 = box
    crop = img.crop(box).convert("RGB")
    px = list(crop.getdata())
    n = 0
    for r, g, b in px:
        if all(abs(r - br) + abs(g - bg) + abs(b - bb) > 36 for br, bg, bb in BGS):
            n += 1
    return n / len(px)


def sha(name):
    with open(os.path.join(OUT, name), "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


# Pippa (the flattened-sprite cat) draws in the same stage box. Her
# markers, measured on the source sprites: golden-yellow eyes, the pink
# dress/top, and denim that appears only in the pants variant.
STAGE = (24, 100, 284, 352)


def count_where(img, box, pred):
    crop = img.crop(box).convert("RGB")
    return sum(1 for px in crop.getdata() if pred(*px))


def is_eye_yellow(r, g, b):
    return r > 200 and 150 < g < 235 and b < 130


def is_garment_pink(r, g, b):
    return r > 220 and 120 < g < 190 and b > 160


def is_denim(r, g, b):
    return 40 < r < 110 and 80 < g < 140 and b > 140


def stage_sha(name):
    img = Image.open(os.path.join(OUT, name)).crop(STAGE)
    return hashlib.sha256(img.tobytes()).hexdigest()


def main():
    failures = []

    def check(cond, msg):
        print(("PASS " if cond else "FAIL ") + msg)
        if not cond:
            failures.append(msg)

    # The dressed cat occupies the stage box (24,100)-(284,352) on every
    # outfit page. Background alone there means the images drew blank.
    for name in ("20_morning.png", "21_evening.png", "23_evening_all_done.png",
                 "28_snow.png", "32_pink_cat.png", "26_unavailable.png"):
        img = Image.open(os.path.join(OUT, name))
        frac = non_bg_fraction(img, (24, 100, 284, 352))
        check(frac > 0.10, f"{name}: cat stage is {frac:.1%} non-background (need > 10%)")

    # Tile icons: tile 1 of the morning page must carry garment pixels, not
    # just white tile.
    img = Image.open(os.path.join(OUT, "20_morning.png"))
    frac = non_bg_fraction(img, (294, 118, 390, 214))
    check(frac > 0.10, f"20_morning: tile 1 icon box is {frac:.1%} non-white (need > 10%)")

    # Scenarios that must differ, differ.
    check(sha("32_pink_cat.png") != sha("20_morning.png"),
          "32_pink_cat differs from 20_morning (pink cat is not the black cat)")
    check(sha("23_evening_all_done.png") != sha("21_evening.png"),
          "23_evening_all_done differs from 21_evening (checks change the page)")

    # Pippa is the cat on the black-cat outfit pages: golden eyes and
    # the pink dress in the stage, and no denim (that is the pants
    # variant). A blank or old-cat stage fails these, not just the
    # non-background fraction above.
    for name in ("20_morning.png", "21_evening.png", "23_evening_all_done.png", "28_snow.png"):
        img = Image.open(os.path.join(OUT, name))
        eyes = count_where(img, STAGE, is_eye_yellow)
        pink = count_where(img, STAGE, is_garment_pink)
        denim = count_where(img, STAGE, is_denim)
        check(eyes > 150, f"{name}: Pippa golden eyes in stage ({eyes} px, need > 150)")
        check(pink > 1000, f"{name}: Pippa pink dress in stage ({pink} px, need > 1000)")
        check(denim < 200, f"{name}: dress variant on stage, no denim ({denim} px, need < 200)")

    # The hot day maps to the pants variant: denim in the stage.
    img = Image.open(os.path.join(OUT, "29_hot.png"))
    eyes = count_where(img, STAGE, is_eye_yellow)
    denim = count_where(img, STAGE, is_denim)
    check(eyes > 150, f"29_hot: Pippa golden eyes in stage ({eyes} px, need > 150)")
    check(denim > 500, f"29_hot: Pippa denim pants in stage ({denim} px, need > 500)")
    check(stage_sha("29_hot.png") != stage_sha("20_morning.png"),
          "stage crops differ: pants variant is not the dress variant")

    # Unavailable shows the undressed base: eyes, but no pink garment
    # and no denim.
    img = Image.open(os.path.join(OUT, "26_unavailable.png"))
    eyes = count_where(img, STAGE, is_eye_yellow)
    pink = count_where(img, STAGE, is_garment_pink)
    denim = count_where(img, STAGE, is_denim)
    check(eyes > 150, f"26_unavailable: Pippa base golden eyes in stage ({eyes} px, need > 150)")
    check(pink < 800, f"26_unavailable: base variant, no pink garment ({pink} px, need < 800)")
    check(denim < 200, f"26_unavailable: base variant, no denim ({denim} px, need < 200)")

    # The heart proof still: Pippa mid-gesture, not the static sprite.
    img = Image.open(os.path.join(OUT, "23b_evening_all_done_heart.png"))
    pink = count_where(img, STAGE, is_garment_pink)
    check(pink > 1000, f"23b_evening_all_done_heart: Pippa pink dress in stage ({pink} px, need > 1000)")
    check(stage_sha("23b_evening_all_done_heart.png") != stage_sha("23_evening_all_done.png"),
          "23b heart frame stage differs from the static all-done stage")

    if failures:
        print(f"\n{len(failures)} render check(s) FAILED")
        return 1
    print("\nAll render checks passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
