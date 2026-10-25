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

    # Wardrobe mapping (iter-05) + fidelity canonicalisation (iter-06):
    # each scenario family shows its own Pippa sprite, proven by the
    # family's signature colour in the stage, AND the prescription was
    # rewritten to the outfit that sprite wears. A blank stage, the old
    # dress-for-everything mapping, or the retired layered cat fails
    # these.
    #
    # Snow family: the snow day and the genuinely cold dry days
    # (halloween, hanukkah, bike bus) wear the puffer sprite: green
    # striped shirt under the coat, golden eyes, no denim (her pants
    # there are dark navy, outside the denim range). Since iter-07
    # the coat-off-later warm-up days (the brief morning, the long
    # text day) are NOT in this family any more: they wear the dress
    # (next block), and days in this family name their boots "Warm
    # boots" unless snow is forecast (boots-naming block below).
    for name in ("28_snow.png", "31_halloween.png",
                 "35_hanukkah.png", "36_bike_bus.png"):
        img = Image.open(os.path.join(OUT, name))
        green = count_where(img, STAGE, is_snow_green)
        eyes = count_where(img, EYE_BOX, is_eye_yellow)
        # Denim is counted over the cat's own ground (left of the
        # seasonal props). 35_hanukkah is exempt: its blue candles
        # match the denim predicate almost exactly, and the green
        # shirt + eyes already prove the family there.
        denim = count_where(img, (24, 100, 190, 352), is_denim)
        check(green > 120, f"{name}: snow sprite's green shirt in stage ({green} px, need > 120)")
        check(eyes > 150, f"{name}: Pippa golden eyes in stage ({eyes} px, need > 150)")
        if name != "35_hanukkah.png":
            check(denim < 200, f"{name}: snow sprite on stage, no denim ({denim} px, need < 200)")
    img = Image.open(os.path.join(OUT, "20_morning.png"))
    fur = count_where(img, HEAD_BOX, is_pink_fur)
    check(fur < 600, f"20_morning: black fur on the head, not the pink colourway ({fur} px, need < 600)")

    # Dress family (iter-06, widened iter-07): a warm-coat day with no
    # hat/gloves need wears the dress, and so do warm-coat days whose
    # hat/gloves need comes with a big warm-up and no snow (the brief
    # morning and the long-text day: cold at the bus stop, 60s by
    # lunch, so the dress + warm coat is the honest outfit and the
    # winter kit stays home). Pink garment, golden eyes, no denim
    # (tights, not jeans).
    for name in ("20_morning.png", "34_long_text.png", "37_dress_day.png"):
        img = Image.open(os.path.join(OUT, name))
        pink = count_where(img, STAGE, is_garment_pink)
        eyes = count_where(img, EYE_BOX, is_eye_yellow)
        denim = count_where(img, STAGE, is_denim)
        check(pink > 1000, f"{name}: Pippa pink dress in stage ({pink} px, need > 1000)")
        check(eyes > 150, f"{name}: Pippa golden eyes in stage ({eyes} px, need > 150)")
        check(denim < 200, f"{name}: dress variant on stage, no denim ({denim} px, need < 200)")

    # Pants family (iter-06): the warm day wears the pants sprite:
    # denim jeans and the pink tee. Since iter-07 she also wears her
    # open blue jacket there, and it is prescribed: tile 3 (bottom-left
    # cell, icon box (294,220)-(390,316)) is the light-jacket icon,
    # whose blue matches the jacket on her body.
    img = Image.open(os.path.join(OUT, "38_warm_day.png"))
    denim = count_where(img, STAGE, is_denim)
    pink = count_where(img, STAGE, is_garment_pink)
    eyes = count_where(img, EYE_BOX, is_eye_yellow)
    check(denim > 800, f"38_warm_day: pants sprite's denim jeans in stage ({denim} px, need > 800)")
    check(pink > 500, f"38_warm_day: pants sprite's pink tee in stage ({pink} px, need > 500)")
    check(eyes > 150, f"38_warm_day: Pippa golden eyes in stage ({eyes} px, need > 150)")

    def is_jacket_blue(r, g, b):
        return b > r + 25 and b > 110 and r < 170

    jblue = count_where(img, (294, 220, 390, 316), is_jacket_blue)
    check(jblue > 500, f"38_warm_day: tile 3 icon is the blue light jacket she wears ({jblue} px, need > 500)")

    # Rain family (evening rain, all-done rain): the yellow raincoat
    # fills the torso band. On 23 the coat also proves the all-done
    # state holds the STATIC rain sprite: the heart frames exist only
    # for the black dress and contain no yellow coat, so a stretched
    # heart animation fails here. The storm screen left this family
    # in iter-07 (next block): at 88/90F it wears the hot outfit.
    for name in ("21_evening.png", "23_evening_all_done.png"):
        img = Image.open(os.path.join(OUT, name))
        coat = count_where(img, STAGE_TORSO, is_rain_yellow)
        green = count_where(img, STAGE, is_snow_green)
        check(coat > 700, f"{name}: rain sprite's yellow coat in stage torso ({coat} px, need > 700)")
        check(green < 30, f"{name}: rain sprite, no snow-shirt green ({green} px, need < 30)")

    # Hot storm (iter-07): at hot temperatures the storm screen
    # dresses for the heat, not the rain (round 5: 88/90F prescribed
    # long sleeves, long pants and a rain jacket while the plain hot
    # day at 89F prescribed short sleeves and shorts). The sprite is
    # the hot one (heart sunglasses over the eyes, no yellow coat in
    # the torso), and tile 2 (top-right cell, icon box
    # (544,108)-(640,204)) is the shorts icon: lavender, with only
    # outline denim (~54 px here vs ~480 for the jeans icon).
    img = Image.open(os.path.join(OUT, "30_storm.png"))
    coat = count_where(img, STAGE_TORSO, is_rain_yellow)
    frames = count_where(img, EYE_BAND, is_hot_frames)
    check(coat < 30, f"30_storm: no rain coat on the hot storm sprite ({coat} px, need < 30)")
    check(frames > 200, f"30_storm: hot sprite's heart sunglasses in stage ({frames} px, need > 200)")

    def is_shorts_lavender(r, g, b):
        return 130 < r < 195 and 120 < g < 185 and b > 185

    t2lav = count_where(img, (544, 108, 640, 204), is_shorts_lavender)
    t2den = count_where(img, (544, 108, 640, 204), is_denim)
    check(t2lav > 120, f"30_storm: tile 2 icon is the lavender shorts ({t2lav} px, need > 120)")
    check(t2den < 120, f"30_storm: tile 2 icon is not the jeans ({t2den} denim px, need < 120)")

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

    # Sunglasses chip icon (iter-07): the chip in the strip must read
    # as sunglasses, not a dark smudge. The wide-aspect icon is fitted
    # by height now, so its pink frames actually land in the chip's
    # icon zone (284,322)-(340,356): frame pink there (the iter-06
    # square-fit rendering left 18 px; the fitted one leaves ~60).
    def is_frame_pink(r, g, b):
        return r > 200 and 90 < g < 170 and b > 140

    fpink = count_where(img, (284, 322, 340, 356), is_frame_pink)
    check(fpink > 30, f"29_hot: sunglasses chip icon reads (frame pink {fpink} px, need > 30)")

    # Pink colourway: scenario 32 resolves the Settings choice to
    # pink, so the stage must show Pippa's pink fur and golden eyes.
    # The retired layered pink cat had navy dot eyes and flat #FF9EC4
    # fur, so the golden-eye count is what proves she is gone. Since
    # iter-07 the brief morning is a dress day (coat-off-later rule),
    # so the pink colourway shows the pink DRESS sprite: pink garment
    # everywhere, no denim, no snow-shirt green.
    img = Image.open(os.path.join(OUT, "32_pink_cat.png"))
    fur = count_where(img, STAGE, is_pink_fur)
    head = count_where(img, HEAD_BOX, is_pink_fur)
    eyes = count_where(img, EYE_BOX, is_eye_yellow)
    pink = count_where(img, STAGE, is_garment_pink)
    denim = count_where(img, STAGE, is_denim)
    green = count_where(img, STAGE, is_snow_green)
    check(fur > 3000, f"32_pink_cat: Pippa pink fur in stage ({fur} px, need > 3000)")
    check(head > 2000, f"32_pink_cat: pink fur on the head, not just a pink garment ({head} px, need > 2000)")
    check(eyes > 150, f"32_pink_cat: Pippa golden eyes in stage ({eyes} px, need > 150)")
    check(pink > 1000, f"32_pink_cat: pink dress sprite's dress in stage ({pink} px, need > 1000)")
    check(denim < 200, f"32_pink_cat: dress variant on stage, no denim ({denim} px, need < 200)")
    check(green < 30, f"32_pink_cat: dress variant, no snow-shirt green ({green} px, need < 30)")

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

    # The heart proof still: the dress scenario's outfit (black dress
    # sprite) all laid out, heart animation pinned at frame 4, the
    # eyes-closed heart hold. Golden eyes nearly vanish (they are
    # closed), which together with the stage hash against the static
    # dress day (37) proves a heart frame, not the static dress, is
    # on stage.
    img = Image.open(os.path.join(OUT, "23b_morning_all_done_heart.png"))
    pink = count_where(img, STAGE, is_garment_pink)
    eyes = count_where(img, EYE_BOX, is_eye_yellow)
    check(pink > 1000, f"23b_morning_all_done_heart: Pippa pink dress in stage ({pink} px, need > 1000)")
    check(eyes < 120, f"23b_morning_all_done_heart: eyes closed in the heart hold ({eyes} px, need < 120)")
    check(stage_sha("23b_morning_all_done_heart.png") != stage_sha("37_dress_day.png"),
          "23b heart frame stage differs from the static dress stage")

    # Fix 1: the extra-layer tile names itself in words. The "Extra
    # layer" caption is COL_WARN_TX (138,75,0); three panel rounds
    # failed the layer question while it was marked by border+star
    # alone. The canonical layer tiles carry it, each in its cell:
    # 28_snow tile 3 (winter coat, bottom-left (284,216)-(522,320)),
    # 37_dress_day tile 2 (warm coat, top-right (534,104)-(772,208)),
    # 38_warm_day tile 3 (the light jacket she wears, iter-07,
    # presented exactly like the dress look's coat). A non-layer tile
    # (28 tile 1) must NOT carry it.
    def is_caption(r, g, b):
        return 105 < r < 170 and 50 < g < 100 and b < 45

    img = Image.open(os.path.join(OUT, "28_snow.png"))
    cap = count_where(img, (284, 216, 522, 320), is_caption)
    check(cap > 40, f"28_snow: 'Extra layer' caption pixels on the layer tile ({cap}, need > 40)")
    cap1 = count_where(img, (284, 104, 522, 208), is_caption)
    check(cap1 < 10, f"28_snow: no caption on a non-layer tile ({cap1}, need < 10)")
    img = Image.open(os.path.join(OUT, "37_dress_day.png"))
    cap = count_where(img, (534, 104, 772, 208), is_caption)
    check(cap > 40, f"37_dress_day: 'Extra layer' caption pixels on the warm coat tile ({cap}, need > 40)")
    img = Image.open(os.path.join(OUT, "38_warm_day.png"))
    cap = count_where(img, (284, 216, 522, 320), is_caption)
    check(cap > 40, f"38_warm_day: 'Extra layer' caption pixels on the light jacket tile ({cap}, need > 40)")

    # Fix 2: advised extras are checkable chips in the strip under the
    # grid (284,322)-(772,356). Chip bodies are pure white (255,255,255)
    # on the cream page, a count RGB565 preserves exactly, so white
    # pixels in the strip measure chip presence without calibration:
    # ~1700+ per chip, ~0 for an empty strip. Since iter-06 the sets
    # are canonical: 28_snow advises hat + gloves (the scarf is gone:
    # the snow sprite wears none, the fall sprite owns the scarf now),
    # 25_stale's fall outfit advises exactly the scarf, and the dress
    # and pants days advise nothing.
    def is_pure_white(r, g, b):
        return r > 250 and g > 250 and b > 250

    for name, need in (("28_snow.png", 3000), ("25_stale.png", 1500)):
        img = Image.open(os.path.join(OUT, name))
        w = count_where(img, (284, 322, 772, 356), is_pure_white)
        check(w > need, f"{name}: extras chips present in strip ({w} white px, need > {need})")
    for name in ("37_dress_day.png", "38_warm_day.png"):
        img = Image.open(os.path.join(OUT, name))
        w = count_where(img, (284, 322, 772, 356), is_pure_white)
        check(w < 200, f"{name}: no extras advised, chip strip empty ({w} white px, need < 200)")

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

    # Fix 1 (iter-06): no label text under the check badge. The badge
    # moved onto the garment icon's bottom-right corner, out of the
    # label column entirely (set pos (68,62) in a tile whose 3 px
    # border shifts children to (71,65); centre at tile + (89,83)).
    # Two assertions per tile. (a) The disc is present and clean at
    # its home: the centre patch may only be badge colours, the white
    # fill, the COL_OK green fill, or white/green anti-alias blends
    # of the tick (green channel dominant, never dark, never ink).
    # (b) The disc actually lives at that home: counting its
    # saturated green (checked) or ring colour (unchecked) over the
    # whole tile, at most a few anti-alias strays (< 60 px, measured
    # worst case 23) may fall outside the disc's home box (rel. tile
    # (66,60)-(112,107)). The round-4 failure, the disc at the tile's
    # top-right inside the label text, would leave its whole ring or
    # fill (110-800 px) outside the box.
    def is_badge_blend(r, g, b):
        return g >= r and g >= b and max(r, g, b) > 120

    def is_ring(r, g, b):
        return abs(r - 207) <= 14 and abs(g - 199) <= 14 and abs(b - 184) <= 16

    def is_disc_green(r, g, b):
        return 30 < r < 70 and 150 < g < 190 and 80 < b < 125

    def disc_counts(img, box):
        x0, y0, x1, y1 = box
        crop = img.crop(box).convert("RGB")
        inside = outside = 0
        for y in range(y1 - y0):
            for x in range(x1 - x0):
                if is_disc_green(*crop.getpixel((x, y))) or is_ring(*crop.getpixel((x, y))):
                    if 66 <= x <= 112 and 60 <= y <= 107:
                        inside += 1
                    else:
                        outside += 1
        return inside, outside

    TILE_CELLS = [(284, 104), (534, 104), (284, 216), (534, 216)]
    BADGED = {"20_morning.png": 2, "21_evening.png": 4, "22_evening_2_checked.png": 4,
              "23_evening_all_done.png": 4, "23b_morning_all_done_heart.png": 2,
              "25_stale.png": 4, "27_rollover.png": 4, "28_snow.png": 4,
              "29_hot.png": 3, "30_storm.png": 3, "31_halloween.png": 4,
              "32_pink_cat.png": 2, "34_long_text.png": 2, "35_hanukkah.png": 4,
              "36_bike_bus.png": 4, "37_dress_day.png": 2, "38_warm_day.png": 4}
    for name, ntiles in BADGED.items():
        img = Image.open(os.path.join(OUT, name))
        for i in range(ntiles):
            tx, ty = TILE_CELLS[i]
            cx, cy = tx + 89, ty + 83
            bad = count_where(img, (cx - 7, cy - 7, cx + 8, cy + 8),
                              lambda r, g, b: not is_badge_blend(r, g, b))
            check(bad == 0, f"{name} tile {i + 1}: badge centre is pure badge colours "
                            f"({bad} foreign px, need 0)")
            inside, outside = disc_counts(img, (tx, ty, tx + 238, ty + 104))
            check(inside >= 20 and outside < 60,
                  f"{name} tile {i + 1}: badge lives on the icon "
                  f"({inside} disc px at home, {outside} outside, need >= 20 and < 60)")

    # Settings (iter-06): the sheet sits in the app system now. The
    # backdrop dim is warm (red >= blue), not the old debug navy, and
    # the panel itself is the morning cream.
    img = Image.open(os.path.join(OUT, "33_settings.png")).convert("RGB")
    bd = img.getpixel((10, 10))
    check(bd[0] >= bd[2], f"33_settings: backdrop dim is warm {bd} (need R >= B)")
    pn = img.getpixel((100, 30))
    check(all(abs(pn[k] - (251, 246, 238)[k]) <= 14 for k in range(3)),
          f"33_settings: panel is morning cream {pn} (need ~ (251,246,238))")

    # Brand mark (iter-06): boot, setup and wifi-error show Pippa's
    # head inside the round white badge. The badge's top inside edge
    # (12 px below its top) is badge white, and her golden eyes are
    # inside it. Boot/error badge: centre (400,124); setup: (460,96).
    for name, cx, cy in (("01_boot.png", 400, 124), ("10_wifi_error.png", 400, 124),
                         ("02_setup.png", 460, 96)):
        img = Image.open(os.path.join(OUT, name))
        top = img.convert("RGB").getpixel((cx, cy - 68))
        check(top[0] > 240 and top[1] > 240 and top[2] > 240,
              f"{name}: badge top inside edge is white {top}")
        eyes = count_where(img, (cx - 55, cy - 40, cx + 55, cy + 40), is_eye_yellow)
        check(eyes > 40, f"{name}: Pippa golden eyes inside the badge ({eyes} px, need > 40)")

    # Boots naming (iter-07): the winter boots tile (tile 4, bottom-
    # right cell; its label sits in the crop (648,242)-(768,294)) is
    # "Snow boots" only when snow is forecast. 28_snow has real snow;
    # 31/35/36 are dry cold days and must all carry the SAME other
    # label ("Warm boots"): their label ink patterns are identical to
    # each other and different from the snow day's. (Ink only: the
    # morning/evening tile border colours differ, so the crop keeps
    # to the label's text rows and the ink predicate ignores borders.)
    def is_label_ink(r, g, b):
        return r < 95 and g < 105 and b < 130

    def label_ink(name):
        img = Image.open(os.path.join(OUT, name)).convert("RGB")
        crop = img.crop((648, 242, 768, 294))
        return {(x, y) for y in range(crop.height) for x in range(crop.width)
                if is_label_ink(*crop.getpixel((x, y)))}

    snow_ink = label_ink("28_snow.png")
    warm_ink = label_ink("31_halloween.png")
    check(len(snow_ink) > 200 and len(warm_ink) > 200,
          f"boots labels render (snow {len(snow_ink)} ink px, warm {len(warm_ink)}, need > 200)")
    check(snow_ink != warm_ink,
          "boots tile label differs: snow day says Snow boots, dry day says Warm boots")
    check(label_ink("35_hanukkah.png") == warm_ink and label_ink("36_bike_bus.png") == warm_ink,
          "dry cold days (35_hanukkah, 36_bike_bus) share the Warm boots label")

    # Prop restraint (iter-07): the seasonal ground prop belongs to
    # the outfit pages only. Pumpkin orange (saturated, b < 70 so the
    # amber tile frames and menorah gold stay out) in the stage's
    # lower-right zone (190,260)-(280,380), left of the tile column:
    # present on the fresh outfit pages (21_evening, 31_halloween),
    # absent on the stale page and the offline state.
    def is_pumpkin(r, g, b):
        return r > 215 and 100 < g < 165 and b < 70

    for name in ("21_evening.png", "31_halloween.png"):
        img = Image.open(os.path.join(OUT, name))
        n = count_where(img, (190, 260, 280, 380), is_pumpkin)
        check(n > 400, f"{name}: pumpkin on the fresh outfit page ({n} px, need > 400)")
    for name in ("25_stale.png", "26_unavailable.png"):
        img = Image.open(os.path.join(OUT, name))
        n = count_where(img, (190, 260, 280, 380), is_pumpkin)
        check(n < 40, f"{name}: no seasonal prop off the outfit pages ({n} px, need < 40)")

    # Next days (iter-07): every forecast card carries a mini Pippa
    # in that day's look, at the left of the garment row. Her dark
    # fur fills the left of the card's bottom band (nothing else dark
    # sits there: the garment icon starts further right), and her
    # golden eyes land somewhere in the band.
    def is_dark_fur(r, g, b):
        return r < 70 and g < 70 and b < 90

    img = Image.open(os.path.join(OUT, "24_next_days.png"))
    for i in range(3):
        cx = 28 + i * 252
        fur = count_where(img, (cx + 6, 348, cx + 96, 420), is_dark_fur)
        eyes = count_where(img, (cx, 348, cx + 236, 420), is_eye_yellow)
        check(fur > 150, f"24_next_days card {i + 1}: mini Pippa's dark fur in the garment row ({fur} px, need > 150)")
        check(eyes > 5, f"24_next_days card {i + 1}: mini Pippa's golden eyes in the garment row ({eyes} px, need > 5)")

    # Settings bands (iter-07): the Speed test pill and the bottom
    # button row must not overlap. Geometry read off this render: the
    # button row's ink fill (24,32,65) forms rows at least 200 px
    # wide in (104,345)-(500,475) (the pill's own label text is ink
    # too, but never 200 px wide in a row), and the pill's light body
    # (238,242,246) shows above it. In the iter-06 layout the button
    # drew OVER the pill: the pill's visible light ended at the
    # button's top edge and the gap computed here was 0.
    img = Image.open(os.path.join(OUT, "33_settings.png")).convert("RGB")

    def is_pill_light(r, g, b):
        return r > 225 and g > 228 and b > 235 and b >= r

    def is_button_ink(r, g, b):
        return abs(r - 24) <= 12 and abs(g - 32) <= 12 and abs(b - 65) <= 14

    ink_rows = [y for y in range(345, 475)
                if count_where(img, (104, y, 500, y + 1), is_button_ink) >= 200]
    check(bool(ink_rows), "33_settings: bottom button row found")
    if ink_rows:
        button_top = min(ink_rows)
        light_rows = [y for y in range(300, button_top)
                      if count_where(img, (104, y, 300, y + 1), is_pill_light) >= 10]
        check(bool(light_rows), "33_settings: speed test pill found above the button row")
        if light_rows:
            gap = button_top - max(light_rows) - 1
            check(gap >= 3, f"33_settings: speed pill and button row do not overlap ({gap} clear px, need >= 3)")

    if failures:
        print(f"\n{len(failures)} render check(s) FAILED")
        return 1
    print("\nAll render checks passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
