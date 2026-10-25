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
# markers, measured on the wardrobe source sprites: golden-yellow
# eyes, the pink dress/top, denim that appears only in the pants
# variant, and one signature colour per wardrobe family (snow-shirt
# green, raincoat yellow, hot-day sunglass frames, fall-jacket
# periwinkle, pink-#FFB3CF fur). Sprite coordinates are stage
# coordinates: the pipeline crops each sprite to its alpha box and
# the UI draws it back at that offset.
STAGE = (24, 100, 284, 352)
STAGE_TORSO = (24, 210, 284, 352)   # stage below the face: coats live here
EYE_BAND = (64, 145, 244, 200)      # where the hot sprite's sunglasses sit
EYE_BOX = (107, 142, 199, 188)      # just the eyes: stage-wide counts also
                                    # catch the pumpkin's yellow highlights
HEAD_BOX = (94, 105, 214, 205)      # the head: fur colour shows here, the
                                    # dress colour does not


def count_where(img, box, pred):
    crop = img.crop(box).convert("RGB")
    return sum(1 for px in crop.getdata() if pred(*px))


def is_eye_yellow(r, g, b):
    return r > 200 and 150 < g < 235 and b < 130


def is_garment_pink(r, g, b):
    return r > 220 and 120 < g < 190 and b > 160


def is_denim(r, g, b):
    return 40 < r < 110 and 80 < g < 140 and b > 140


def is_snow_green(r, g, b):
    return 30 < r < 95 and 125 < g < 195 and 45 < b < 115


def is_rain_yellow(r, g, b):
    return r > 235 and 180 < g < 230 and b < 110


def is_hot_frames(r, g, b):
    return r > 225 and 55 < g < 130 and 110 < b < 175


def is_fall_periwinkle(r, g, b):
    return 95 < r < 140 and 140 < g < 185 and b > 185


def is_pink_fur(r, g, b):
    # #FFB3CF fur, widened for the RGB565 round trip (its green and
    # blue come back a few steps brighter than the source PNG).
    return r > 230 and 148 < g < 182 and 178 < b < 215


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
    # just white tile. (Tile grid: cells at (284,104) and (534,216),
    # every tile 238x104, icon at tile + (10,4), 96 px.)
    img = Image.open(os.path.join(OUT, "20_morning.png"))
    frac = non_bg_fraction(img, (294, 108, 390, 204))
    check(frac > 0.10, f"20_morning: tile 1 icon box is {frac:.1%} non-white (need > 10%)")

    # Scenarios that must differ, differ.
    check(sha("32_pink_cat.png") != sha("20_morning.png"),
          "32_pink_cat differs from 20_morning (pink cat is not the black cat)")
    check(sha("23_evening_all_done.png") != sha("21_evening.png"),
          "23_evening_all_done differs from 21_evening (checks change the page)")

    # Wardrobe mapping (iter-05): each scenario family shows its own
    # Pippa sprite, proven by the family's signature colour in the
    # stage. A blank stage, the old dress-for-everything mapping, or
    # the retired layered cat fails these.
    img = Image.open(os.path.join(OUT, "20_morning.png"))
    eyes = count_where(img, EYE_BOX, is_eye_yellow)
    pink = count_where(img, STAGE, is_garment_pink)
    denim = count_where(img, STAGE, is_denim)
    fur = count_where(img, HEAD_BOX, is_pink_fur)
    check(eyes > 150, f"20_morning: Pippa golden eyes in stage ({eyes} px, need > 150)")
    check(pink > 1000, f"20_morning: Pippa pink dress in stage ({pink} px, need > 1000)")
    check(denim < 200, f"20_morning: dress variant on stage, no denim ({denim} px, need < 200)")
    check(fur < 600, f"20_morning: black fur on the head, not the pink colourway ({fur} px, need < 600)")

    # Rain family (evening rain, all-done rain, storm): the yellow
    # raincoat fills the torso band. On 23 the coat also proves the
    # all-done state holds the STATIC rain sprite: the heart frames
    # exist only for the black dress and contain no yellow coat, so
    # a stretched heart animation fails here.
    for name in ("21_evening.png", "23_evening_all_done.png", "30_storm.png"):
        img = Image.open(os.path.join(OUT, name))
        coat = count_where(img, STAGE_TORSO, is_rain_yellow)
        green = count_where(img, STAGE, is_snow_green)
        check(coat > 700, f"{name}: rain sprite's yellow coat in stage torso ({coat} px, need > 700)")
        check(green < 30, f"{name}: rain sprite, no snow-shirt green ({green} px, need < 30)")

    # Snow family: the green-and-white striped shirt under the puffer.
    img = Image.open(os.path.join(OUT, "28_snow.png"))
    green = count_where(img, STAGE, is_snow_green)
    eyes = count_where(img, EYE_BOX, is_eye_yellow)
    check(green > 120, f"28_snow: snow sprite's green shirt in stage ({green} px, need > 120)")
    check(eyes > 150, f"28_snow: Pippa golden eyes in stage ({eyes} px, need > 150)")

    # Fall family: the stale day wears a hoodie, which maps to the
    # fall sprite (periwinkle jacket; the dress sprite has none).
    img = Image.open(os.path.join(OUT, "25_stale.png"))
    peri = count_where(img, STAGE, is_fall_periwinkle)
    green = count_where(img, STAGE, is_snow_green)
    check(peri > 120, f"25_stale: fall sprite's periwinkle jacket in stage ({peri} px, need > 120)")
    check(green < 30, f"25_stale: fall sprite, no snow-shirt green ({green} px, need < 30)")

    # Hot family: heart sunglasses over the eyes (so no golden-eye
    # assertion here: the lenses cover them), lavender shorts, and
    # no denim (the hot sprite replaced the old pants mapping).
    img = Image.open(os.path.join(OUT, "29_hot.png"))
    frames = count_where(img, EYE_BAND, is_hot_frames)
    denim = count_where(img, STAGE, is_denim)
    check(frames > 200, f"29_hot: hot sprite's heart sunglasses in stage ({frames} px, need > 200)")
    check(denim < 200, f"29_hot: hot variant on stage, no denim ({denim} px, need < 200)")
    check(stage_sha("29_hot.png") != stage_sha("20_morning.png"),
          "stage crops differ: hot variant is not the dress variant")

    # Pink colourway: scenario 32 resolves the Settings choice to
    # pink, so the stage must show Pippa's pink fur and golden eyes.
    # The retired layered pink cat had navy dot eyes and flat #FF9EC4
    # fur, so the golden-eye count is what proves she is gone.
    img = Image.open(os.path.join(OUT, "32_pink_cat.png"))
    fur = count_where(img, STAGE, is_pink_fur)
    head = count_where(img, HEAD_BOX, is_pink_fur)
    eyes = count_where(img, EYE_BOX, is_eye_yellow)
    check(fur > 3000, f"32_pink_cat: Pippa pink fur in stage ({fur} px, need > 3000)")
    check(head > 2000, f"32_pink_cat: pink fur on the head, not just a pink garment ({head} px, need > 2000)")
    check(eyes > 150, f"32_pink_cat: Pippa golden eyes in stage ({eyes} px, need > 150)")

    # Tile icons come from the wardrobe sheet now: its sneakers have
    # broad pink soles (g ~128), where the retired code-drawn icon
    # had a navy sole and a thin pink stripe (g ~95). Tile 3 of
    # 29_hot is the sneakers tile, icon box (294,220)-(390,316).
    def is_sole_pink(r, g, b):
        return r > 225 and 108 < g < 152 and 158 < b < 198

    img = Image.open(os.path.join(OUT, "29_hot.png"))
    sole = count_where(img, (294, 220, 390, 316), is_sole_pink)
    check(sole > 120, f"29_hot: sheet sneakers' pink soles in tile 3 icon ({sole} px, need > 120)")

    # Unavailable shows the undressed base: eyes, but no pink garment
    # and no denim.
    img = Image.open(os.path.join(OUT, "26_unavailable.png"))
    eyes = count_where(img, EYE_BOX, is_eye_yellow)
    pink = count_where(img, STAGE, is_garment_pink)
    denim = count_where(img, STAGE, is_denim)
    check(eyes > 150, f"26_unavailable: Pippa base golden eyes in stage ({eyes} px, need > 150)")
    check(pink < 800, f"26_unavailable: base variant, no pink garment ({pink} px, need < 800)")
    check(denim < 200, f"26_unavailable: base variant, no denim ({denim} px, need < 200)")

    # The heart proof still: the morning outfit (black dress sprite)
    # all laid out, heart animation pinned at frame 4, the eyes-closed
    # heart hold. Golden eyes nearly vanish (they are closed), which
    # together with the stage hash proves a heart frame, not the
    # static dress, is on stage.
    img = Image.open(os.path.join(OUT, "23b_morning_all_done_heart.png"))
    pink = count_where(img, STAGE, is_garment_pink)
    eyes = count_where(img, EYE_BOX, is_eye_yellow)
    check(pink > 1000, f"23b_morning_all_done_heart: Pippa pink dress in stage ({pink} px, need > 1000)")
    check(eyes < 120, f"23b_morning_all_done_heart: eyes closed in the heart hold ({eyes} px, need < 120)")
    check(stage_sha("23b_morning_all_done_heart.png") != stage_sha("20_morning.png"),
          "23b heart frame stage differs from the static dress stage")

    # Fix 1: the extra-layer tile names itself in words. The "Extra
    # layer" caption is COL_WARN_TX (138,75,0); three panel rounds
    # failed the layer question while it was marked by border+star
    # alone. 20_morning's layer tile is tile 3 (Warm coat), the
    # bottom-left cell (284,216)-(522,320); tile 1 must NOT carry it.
    def is_caption(r, g, b):
        return 105 < r < 170 and 50 < g < 100 and b < 45

    img = Image.open(os.path.join(OUT, "20_morning.png"))
    cap = count_where(img, (284, 216, 522, 320), is_caption)
    check(cap > 40, f"20_morning: 'Extra layer' caption pixels on the layer tile ({cap}, need > 40)")
    cap1 = count_where(img, (284, 104, 522, 208), is_caption)
    check(cap1 < 10, f"20_morning: no caption on a non-layer tile ({cap1}, need < 10)")

    # Fix 2: advised extras are checkable chips in the strip under the
    # grid (284,322)-(772,356). 28_snow advises hat + gloves + scarf,
    # so the strip must carry chip content; 25_stale advises no extras,
    # so the strip must be empty there.
    img = Image.open(os.path.join(OUT, "28_snow.png"))
    frac = non_bg_fraction(img, (284, 322, 772, 356))
    check(frac > 0.08, f"28_snow: extras chip strip is {frac:.1%} filled (need > 8%)")
    img = Image.open(os.path.join(OUT, "25_stale.png"))
    frac = non_bg_fraction(img, (284, 322, 772, 356))
    check(frac < 0.02, f"25_stale: no extras advised, chip strip empty ({frac:.1%}, need < 2%)")

    # Fix 2, gating: 23_evening_all_done could only reach all-done by
    # checking the umbrella chip too (the sim checks tiles + chip), so
    # the chip's green check disc (COL_OK 47,168,102) must show in the
    # strip; and 21_evening (nothing checked) must not show one.
    def is_ok_green(r, g, b):
        return 30 < r < 70 and 150 < g < 190 and 80 < b < 125

    img = Image.open(os.path.join(OUT, "23_evening_all_done.png"))
    greens = count_where(img, (284, 322, 772, 356), is_ok_green)
    check(greens > 30, f"23_evening_all_done: checked umbrella chip disc in strip ({greens} px, need > 30)")
    img = Image.open(os.path.join(OUT, "21_evening.png"))
    greens = count_where(img, (284, 322, 772, 356), is_ok_green)
    check(greens < 10, f"21_evening: umbrella chip present but unchecked ({greens} green px, need < 10)")

    # Fix 3: 29_hot has 3 garments and must keep the 2x2 grid: the
    # bottom-right cell stays page background (the third tile used to
    # stretch full-width across it), and tile 3 keeps its icon in the
    # bottom-left cell at the standard size.
    def is_page_cream(r, g, b):
        return abs(r - 251) <= 8 and abs(g - 246) <= 8 and abs(b - 238) <= 8

    img = Image.open(os.path.join(OUT, "29_hot.png"))
    cell = img.crop((538, 220, 768, 316)).convert("RGB")
    px = list(cell.getdata())
    cream = sum(1 for p in px if is_page_cream(*p)) / len(px)
    check(cream > 0.95, f"29_hot: empty grid cell is {cream:.1%} page background (need > 95%)")
    frac = non_bg_fraction(img, (294, 220, 390, 316))
    check(frac > 0.10, f"29_hot: tile 3 icon box is {frac:.1%} non-white (need > 10%)")

    if failures:
        print(f"\n{len(failures)} render check(s) FAILED")
        return 1
    print("\nAll render checks passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
