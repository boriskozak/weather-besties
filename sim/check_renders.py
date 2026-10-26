#!/usr/bin/env python3
"""Pixel-level guard for `make shots`.

The 2.0 build once shipped renders in which every bitmap image decoded to
nothing (LVGL's bin decoder refused the RLE-compressed assets because
LV_BIN_DECODER_RAM_LOAD was off) while text and shapes rendered fine, and
nothing in the pipeline noticed. This check makes that failure loud: it
asserts, on the actual PNGs, that the cat is visibly present on the outfit
pages, that the garment tiles carry icon pixels, and that scenarios which
must differ do differ.

Since iter-08 it also reads the sim's per-scenario text dumps
(<name>.txt, written by ui_test_dump): words are asserted as words
(the tier a scenario landed in, the tiles and chips it prescribed,
whether the advice sentence names every chip, the stale header's
wording, the Settings info lines), because pixels alone cannot prove
a sentence kept its promise.

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
    # Iter-35: 32 does NOT differ from 20 any more (the alias
    # block below pins why); it left this list.
    check(sha("32_pink_cat.png") == sha("20_morning.png"),
          "32_pink_cat is pixel-identical to 20_morning (the pink choice aliases the black sprite on a black-only look)")
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
    # (halloween, hanukkah: departure feels-like in the low 30s or
    # below) wear the puffer sprite: green striped shirt under the
    # coat, golden eyes, no denim (her pants there are dark navy,
    # outside the denim range). Since iter-09 the family is bigger:
    # EVERY warm-coat departure joins it (the brief morning and the
    # long-text day, 33F departures with big warm-ups, and the
    # bike-bus Friday), because the winter kit is the only sprite
    # that wears a real warm coat and the dress look is bounded to
    # the light-jacket tier (iter-09, panel round 7). Days in this
    # family name their boots "Warm boots" unless snow is forecast
    # (boots-naming block below).
    for name in ("28_snow.png", "31_halloween.png", "35_hanukkah.png",
                 "34_long_text.png", "36_bike_bus.png"):
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
    # Iter-35: Oct 27's hat-band pick is the red-coat navy SOLID
    # (band index 1 after the reorder), not a striped kit: the
    # stage carries the navy shirt and the red coat, no green
    # stripe, golden eyes, no denim.
    def is_navy_shirt(r, g, b):
        return r < 40 and g > r + 20 and b > 90 and b > r + 60
    def is_red_coat(r, g, b):
        return r > 170 and g < 70 and b < 90
    navy = count_where(img, STAGE, is_navy_shirt)
    red = count_where(img, STAGE, is_red_coat)
    green = count_where(img, STAGE, is_snow_green)
    eyes = count_where(img, EYE_BOX, is_eye_yellow)
    denim = count_where(img, (24, 100, 190, 352), is_denim)
    check(navy > 800, f"20_morning: navy solid shirt in stage ({navy} px, need > 800)")
    check(red > 1000, f"20_morning: red coat in stage ({red} px, need > 1000)")
    check(green < 120, f"20_morning: no green stripe shirt any more ({green} px, need < 120)")
    check(eyes > 150, f"20_morning: Pippa golden eyes in stage ({eyes} px, need > 150)")
    check(denim < 200, f"20_morning: snow-family sprite on stage, no denim ({denim} px, need < 200)")

    # Dress family (iter-06; bounded iter-09): since panel round 7
    # the dress look may only be chosen when a LIGHT JACKET is the
    # truthful departure layer, because the jacket on the dress
    # sprite is short, light and open, the identical garment the
    # pants look honestly tiles as "Light jacket". The old warm-coat
    # dress days (brief morning, long-text day, bike-bus Friday) are
    # winter-kit days now (previous block). What remains is the one
    # genuine light-jacket-band scenario: pink garment, golden eyes,
    # no denim (tights, not jeans).
    for name in ("37_dress_day.png",):
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
    # the torso), and tile 2 (middle tile of the iter-09 three-across
    # row, glyph box (473,115)-(583,225)) is the shorts icon:
    # lavender, with only outline denim (vs ~480 for the jeans icon).
    img = Image.open(os.path.join(OUT, "30_storm.png"))
    coat = count_where(img, STAGE_TORSO, is_rain_yellow)
    frames = count_where(img, EYE_BAND, is_hot_frames)
    check(coat < 30, f"30_storm: no rain coat on the hot storm sprite ({coat} px, need < 30)")
    check(frames > 200, f"30_storm: hot sprite's heart sunglasses in stage ({frames} px, need > 200)")

    def is_shorts_lavender(r, g, b):
        return 130 < r < 195 and 120 < g < 185 and b > 185

    t2lav = count_where(img, (473, 115, 583, 225), is_shorts_lavender)
    t2den = count_where(img, (473, 115, 583, 225), is_denim)
    check(t2lav > 120, f"30_storm: tile 2 icon is the lavender shorts ({t2lav} px, need > 120)")
    check(t2den < 120, f"30_storm: tile 2 icon is not the jeans ({t2den} denim px, need < 120)")

    # Fall family + the stale treatment (iter-09): the stale day
    # wears a hoodie (the fall sprite), but the whole stage is faded
    # back, so the family colours are asserted in their FADED form:
    # the periwinkle jacket blended toward the page cream (measured
    # predicate below), the black fur gone grey (a fresh sprite's
    # head carries thousands of near-black fur pixels; the faded
    # one carries ~0 and thousands of mid-grey ones instead). And
    # the kid-sized signal itself: the big "From earlier today"
    # banner pill (COL_WARN_BG) in the header slot (284,56)-(560,100)
    # (measured ~8800 px; a fresh page has none there).
    def is_faded_periwinkle(r, g, b):
        return 145 < r < 205 and 170 < g < 220 and b > 205

    def is_dark_fur(r, g, b):
        return r < 70 and g < 70 and b < 90

    def is_grey_fur(r, g, b):
        return 95 < r < 175 and 95 < g < 175 and 95 < b < 180

    def is_warn_bg(r, g, b):
        return abs(r - 255) <= 12 and abs(g - 235) <= 12 and abs(b - 199) <= 14

    img = Image.open(os.path.join(OUT, "25_stale.png"))
    peri = count_where(img, STAGE, is_faded_periwinkle)
    green = count_where(img, STAGE, is_snow_green)
    dark = count_where(img, HEAD_BOX, is_dark_fur)
    grey = count_where(img, HEAD_BOX, is_grey_fur)
    banner = count_where(img, (284, 56, 560, 100), is_warn_bg)
    check(peri > 300, f"25_stale: fall sprite's periwinkle jacket in stage, faded ({peri} px, need > 300)")
    check(green < 30, f"25_stale: fall sprite, no snow-shirt green ({green} px, need < 30)")
    check(dark < 100, f"25_stale: the cat is faded, no full-weight black fur ({dark} px, need < 100)")
    check(grey > 1500, f"25_stale: the cat is greyed by the stale fade ({grey} px, need > 1500)")
    check(banner > 2500, f"25_stale: big stale banner pill in the header ({banner} px, need > 2500)")

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

    # Sunglasses chip icon (iter-08, third attempt): the icon is now
    # Pippa's own heart sunglasses masked out of her hot sprite, not
    # the sheet's thin-framed drawing (which read as a smudge in two
    # rounds). The chip's icon zone (284,322)-(340,356) must carry
    # BOTH halves of that glyph: the bright-pink heart frames AND the
    # dark saturated-purple lens fill (the sheet icon's lenses were
    # empty, so purple there is what proves the new source). Measured
    # on the iter-08 render: 44 frame px + 43 lens px. Iter-10 zoom
    # check: at the iter-08/09 presentation (scale 200) the profile
    # still measured only that same 44 + 43, marginal against the
    # "smudge" complaint, so the presentation grew to scale 248
    # (~51x26 glyph, label shifted, chip widened): the same zone now
    # measures 53 frame px + 70 lens px, and both halves must stay
    # clearly above the old profile.
    def is_frame_pink(r, g, b):
        return r > 200 and 90 < g < 170 and b > 140

    def is_lens_purple(r, g, b):
        return 55 < r < 150 and 55 < b < 160 and max(r, b) - g > 30

    fpink = count_where(img, (284, 322, 340, 356), is_frame_pink)
    flens = count_where(img, (284, 322, 340, 356), is_lens_purple)
    check(fpink > 48, f"29_hot: sunglasses chip has Pippa's pink heart frames ({fpink} px, need > 48; was 44)")
    check(flens > 60, f"29_hot: sunglasses chip has the filled purple lenses ({flens} px, need > 60; was 43)")

    # Pink colourway: scenario 32 resolves the Settings choice to
    # pink, so the stage must show Pippa's pink fur and golden eyes.
    # The retired layered pink cat had navy dot eyes and flat #FF9EC4
    # fur, so the golden-eye count is what proves she is gone. Since
    # iter-09 the brief morning is a winter-kit day (33F departure
    # needs a warm coat; the dress look is light-jacket tier only),
    # so the pink colourway shows the pink SNOW sprite: pink fur, the
    # snow family's green striped shirt, no denim.
    img = Image.open(os.path.join(OUT, "32_pink_cat.png"))
    fur = count_where(img, STAGE, is_pink_fur)
    eyes = count_where(img, EYE_BOX, is_eye_yellow)
    # Iter-35: the brief morning's pick is a black-fur-only
    # variant (kitr_navy_solid), so the pink choice has no pink
    # sprite to show and the batch's rule applies: the pink slot
    # aliases the black sprite, and the render is the black cat's
    # own (the sha check above pins them identical). The pink
    # colourway's positive proof rides 61_kit2_pink, whose date
    # picks the kit (a look with pink art).
    check(fur < 600, f"32_pink_cat: no pink fur on a black-only look ({fur} px, need < 600)")
    check(eyes > 150, f"32_pink_cat: Pippa golden eyes in stage ({eyes} px, need > 150)")

    # Tile icons come from the wardrobe sheet now: its sneakers have
    # broad pink soles (g ~128), where the retired code-drawn icon
    # had a navy sole and a thin pink stripe (g ~95). Tile 3 of
    # 29_hot is the sneakers tile (right tile of the three-across
    # row, glyph box (639,115)-(749,225)).
    def is_sole_pink(r, g, b):
        return r > 225 and 108 < g < 152 and 158 < b < 198

    img = Image.open(os.path.join(OUT, "29_hot.png"))
    sole = count_where(img, (639, 115, 749, 225), is_sole_pink)
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
    # 20_morning tile 3 (the same winter coat on the new kit
    # morning), 37_dress_day tile 2 (the LIGHT jacket, top-right
    # (534,104)-(772,208); iter-09: the dress look's layer is the
    # light jacket now), 38_warm_day tile 3 (the pants look's light
    # jacket, presented exactly like the dress look's). A non-layer
    # tile (28 tile 1) must NOT carry it.
    def is_caption(r, g, b):
        return 105 < r < 170 and 50 < g < 100 and b < 45

    img = Image.open(os.path.join(OUT, "28_snow.png"))
    cap = count_where(img, (284, 216, 522, 320), is_caption)
    check(cap > 40, f"28_snow: 'Extra layer' caption pixels on the layer tile ({cap}, need > 40)")
    cap1 = count_where(img, (284, 104, 522, 208), is_caption)
    check(cap1 < 10, f"28_snow: no caption on a non-layer tile ({cap1}, need < 10)")
    img = Image.open(os.path.join(OUT, "20_morning.png"))
    cap = count_where(img, (284, 216, 522, 320), is_caption)
    check(cap > 40, f"20_morning: 'Extra layer' caption pixels on the winter coat tile ({cap}, need > 40)")
    img = Image.open(os.path.join(OUT, "37_dress_day.png"))
    cap = count_where(img, (534, 104, 772, 208), is_caption)
    check(cap > 40, f"37_dress_day: 'Extra layer' caption pixels on the light jacket tile ({cap}, need > 40)")
    img = Image.open(os.path.join(OUT, "38_warm_day.png"))
    cap = count_where(img, (284, 216, 522, 320), is_caption)
    check(cap > 40, f"38_warm_day: 'Extra layer' caption pixels on the light jacket tile ({cap}, need > 40)")

    # Fix 2: advised extras are checkable chips in the strip under the
    # grid (284,322)-(772,356). Chip bodies are pure white (255,255,255)
    # on the cream page, a count RGB565 preserves exactly, so white
    # pixels in the strip measure chip presence without calibration:
    # ~1700+ per chip. Since iter-06 the sets are canonical: 28_snow
    # advises hat + gloves (the scarf is gone: the snow sprite wears
    # none, the fall sprite owns the scarf now), and since iter-09 so
    # does 20_morning (a winter-kit morning now); 25_stale's fall
    # outfit advises exactly the scarf, and the dress and pants days
    # advise nothing. Two iter-09 wrinkles: the corner badges' outer
    # halves reach into the strip's top rows (their white interiors
    # add ~400 px to an "empty" strip, hence the looser empty bound),
    # and on the stale page the chips are faded, so their bodies read
    # (255,250,246) instead of pure white and get their own predicate.
    def is_pure_white(r, g, b):
        return r > 250 and g > 250 and b > 250

    def is_faded_white(r, g, b):
        return r > 250 and g > 245 and b > 240

    for name, need in (("28_snow.png", 3000), ("20_morning.png", 3000)):
        img = Image.open(os.path.join(OUT, name))
        w = count_where(img, (284, 322, 772, 356), is_pure_white)
        check(w > need, f"{name}: extras chips present in strip ({w} white px, need > {need})")
    img = Image.open(os.path.join(OUT, "25_stale.png"))
    w = count_where(img, (284, 322, 772, 356), is_faded_white)
    check(w > 1200, f"25_stale: the scarf chip is present in the strip, faded ({w} px, need > 1200)")
    for name in ("37_dress_day.png", "38_warm_day.png"):
        img = Image.open(os.path.join(OUT, name))
        w = count_where(img, (284, 322, 772, 356), is_pure_white)
        check(w < 700, f"{name}: no extras advised, chip strip empty ({w} white px, need < 700)")

    # Fix 2, gating: 23_evening_all_done could only reach all-done by
    # checking the umbrella chip too (the sim checks tiles + chip), so
    # the chip's green check disc (COL_OK 47,168,102) must show in the
    # strip; and 21_evening (nothing checked) must not show one. The
    # zone stops at x=500: the bottom-left tile's corner badge (iter-09)
    # reaches into the strip's top rows at x 504+, and its green must
    # not be mistaken for the chip's.
    def is_ok_green(r, g, b):
        return 30 < r < 70 and 150 < g < 190 and 80 < b < 125

    img = Image.open(os.path.join(OUT, "23_evening_all_done.png"))
    greens = count_where(img, (284, 322, 500, 356), is_ok_green)
    check(greens > 30, f"23_evening_all_done: checked umbrella chip disc in strip ({greens} px, need > 30)")
    img = Image.open(os.path.join(OUT, "21_evening.png"))
    greens = count_where(img, (284, 322, 500, 356), is_ok_green)
    check(greens < 10, f"21_evening: umbrella chip present but unchecked ({greens} green px, need < 10)")

    # Adaptive tile layouts (iter-09 fix 8): a 3-garment day no longer
    # keeps the 2x2 grid with a dead quadrant (the round-7 premium
    # complaint). 29_hot and 30_storm lay their three tiles out as one
    # row of tall tiles (284/450/616, y 110, 156x200): each tile's
    # glyph box (tile + (23,5), 110x110) carries its garment at the
    # bigger presentation, each label box (tile + (8,124), 140x76)
    # carries real label ink, and the old dead quadrant's centre
    # (660,262) is inside tile 3 now, not page background.
    def is_page_cream(r, g, b):
        return abs(r - 251) <= 8 and abs(g - 246) <= 8 and abs(b - 238) <= 8

    def is_label_ink(r, g, b):
        return r < 95 and g < 105 and b < 130

    ROW3 = [(284, 110, 156, 200), (450, 110, 156, 200), (616, 110, 156, 200)]
    for name in ("29_hot.png", "30_storm.png"):
        img = Image.open(os.path.join(OUT, name))
        for i, (tx, ty, tw, th) in enumerate(ROW3):
            frac = non_bg_fraction(img, (tx + 23, ty + 5, tx + 133, ty + 115))
            check(frac > 0.10, f"{name} tile {i + 1}: tall-layout glyph box is {frac:.1%} non-white (need > 10%)")
            ink = count_where(img, (tx + 8, ty + 124, tx + tw - 8, ty + th), is_label_ink)
            check(ink > 200, f"{name} tile {i + 1}: tall-layout label renders ({ink} ink px, need > 200)")
        cell = img.crop((620, 240, 700, 290)).convert("RGB")
        px = list(cell.getdata())
        cream = sum(1 for p in px if is_page_cream(*p)) / len(px)
        check(cream < 0.50, f"{name}: the old dead quadrant is tile now ({cream:.1%} page cream, need < 50%)")

    # Check badge (iter-09 fix 6): the disc straddles the tile's
    # bottom-right CORNER (centred on the corner point, half in the
    # tile's chrome, half outside), in every layout. It used to sit on
    # the garment icon, where round 7's premium judge read it as a
    # sticker on finished art. Two assertions per tile:
    # (a) findable: the badge box (corner +- 22) holds >= 80 disc
    #     pixels (ring colour unchecked, COL_OK green checked;
    #     measured 196-822).
    # (b) confinement: over the tile's whole neighbourhood (its rect
    #     grown by 24, clipped above the chip strip), disc-coloured
    #     pixels outside ALL badge boxes number < 90 (measured worst
    #     ~60: garment art shares disc colours, e.g. the snow shirt's
    #     green stripes, so art pixels are counted and allowed for;
    #     a disc displaced onto the art or the label, the failure
    #     this guards, would leave its whole ring or fill, 110-800
    #     px, outside the boxes).
    def is_ring(r, g, b):
        return abs(r - 207) <= 14 and abs(g - 199) <= 14 and abs(b - 184) <= 16

    def is_disc_green(r, g, b):
        return 30 < r < 70 and 150 < g < 190 and 80 < b < 125

    def is_disc(r, g, b):
        return is_disc_green(r, g, b) or is_ring(r, g, b)

    GRID4 = [(284, 104, 238, 104), (534, 104, 238, 104),
             (284, 216, 238, 104), (534, 216, 238, 104)]
    BADGED = {"20_morning.png": 4, "21_evening.png": 4, "22_evening_2_checked.png": 4,
              "23_evening_all_done.png": 4, "23b_morning_all_done_heart.png": 4,
              "25_stale.png": 4, "27_rollover.png": 4, "28_snow.png": 4,
              "29_hot.png": 3, "30_storm.png": 3, "31_halloween.png": 4,
              "32_pink_cat.png": 4, "34_long_text.png": 4, "35_hanukkah.png": 4,
              "36_bike_bus.png": 4, "37_dress_day.png": 4, "38_warm_day.png": 4}
    for name, ntiles in BADGED.items():
        img = Image.open(os.path.join(OUT, name))
        cells = ROW3 if ntiles == 3 else GRID4
        badge_boxes = [(tx + tw - 22, ty + th - 22, tx + tw + 22, ty + th + 22)
                         for (tx, ty, tw, th) in cells[:ntiles]]
        for i in range(ntiles):
            tx, ty, tw, th = cells[i]
            inside = count_where(img, badge_boxes[i], is_disc)
            check(inside >= 80, f"{name} tile {i + 1}: corner badge findable "
                                f"({inside} disc px in the badge box, need >= 80)")
            rect = (tx - 24, ty - 24, tx + tw + 24, min(ty + th + 24, 322))
            outside = count_where(img, rect, is_disc)
            for bb in badge_boxes:
                inter = (max(rect[0], bb[0]), max(rect[1], bb[1]),
                         min(rect[2], bb[2]), min(rect[3], bb[3]))
                if inter[2] > inter[0] and inter[3] > inter[1]:
                    outside -= count_where(img, inter, is_disc)
            check(outside < 90, f"{name} tile {i + 1}: badge confined to the corner "
                                f"({outside} disc px outside the badge boxes, need < 90)")

    # Settings (iter-08): a full opaque page in the app palette, not
    # a dimmed overlay. Both a top corner and the bottom-right corner
    # must be the morning cream itself: the iter-06/07 translucent
    # charcoal dim (58,50,42 at 150/255 over the pink page) fails
    # this, and so would any return of a see-through backdrop.
    # Iter-09: the panel card is GONE (the round-7 premium judge's
    # "panel-on-page double border"), so the points where its top
    # border ran (y = 12, the old panel's top edge) must now be page
    # cream too, not the tile-line border colour (231,217,190).
    img = Image.open(os.path.join(OUT, "33_settings.png")).convert("RGB")
    for px_at in ((10, 10), (790, 470), (100, 12), (400, 12), (700, 12)):
        bd = img.getpixel(px_at)
        check(all(abs(bd[k] - (251, 246, 238)[k]) <= 14 for k in range(3)),
              f"33_settings: page ground at {px_at} is opaque morning cream {bd} (need ~ (251,246,238))")

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
    # the dry cold days must all carry the SAME other label ("Warm
    # boots"): their label ink patterns are identical to each other
    # and different from the snow day's. Since iter-09 the dry kit
    # days are 31/35 plus the warm-up mornings (20_morning,
    # 34_long_text) and the bike-bus Friday (36_bike_bus), all winter
    # kit now. (Ink only: the morning/evening tile border colours
    # differ, so the crop keeps to the label's text rows and the ink
    # predicate ignores borders.)
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
    for name in ("35_hanukkah.png", "20_morning.png", "34_long_text.png", "36_bike_bus.png"):
        check(label_ink(name) == warm_ink,
              f"dry cold days share the Warm boots label ({name})")

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

    # Next days (iter-07, enlarged iter-08, relaid iter-11): every
    # forecast card carries a mini Pippa in that day's look at the
    # left of the middle band (card-local x 10, feet on the band's
    # base at card-local y 206). At ~100 px tall she is the card's
    # star, not a logo: her fur fills the band zone, her golden eyes
    # land in it, and her silhouette spans at least 85 rows there
    # (the iter-07 minis spanned ~45 and would fail this).
    def is_dark_fur(r, g, b):
        return r < 70 and g < 70 and b < 90

    def is_card_white(r, g, b):
        return r > 245 and g > 245 and b > 245

    img = Image.open(os.path.join(OUT, "24_next_days.png"))
    for i in range(3):
        cx = 28 + i * 252
        fur = count_where(img, (cx + 6, 172, cx + 118, 282), is_dark_fur) \
            + count_where(img, (cx + 6, 172, cx + 118, 282), is_pink_fur)
        eyes = count_where(img, (cx + 6, 172, cx + 118, 282), is_eye_yellow)
        check(fur > 300, f"24_next_days card {i + 1}: big mini Pippa's fur in the band ({fur} px, need > 300)")
        check(eyes > 20, f"24_next_days card {i + 1}: big mini Pippa's golden eyes in the band ({eyes} px, need > 20)")
        rows = [y for y in range(172, 282)
                if count_where(img, (cx + 6, y, cx + 118, y + 1),
                                lambda r, g, b: not is_card_white(r, g, b)) > 8]
        extent = (max(rows) - min(rows) + 1) if rows else 0
        check(extent >= 85, f"24_next_days card {i + 1}: mini Pippa spans {extent} rows (need >= 85; the 50 px minis spanned ~45)")

    # Forecast garment lists (iter-11): under the band, each card
    # carries the day's FULL outfit as a vertical icon+word list
    # (the tiles' own icons; the words are the tiles' exact names,
    # asserted as strings in the dump block below). Every row must
    # draw its icon and its word, the word on ONE line at chip-label
    # size, and no list ink may cross the card's inner right edge.
    for i in range(3):
        cx = 28 + i * 252
        for k in range(4):
            ry = 290 + k * 30   # card-local 218 + k*30, cards at y 72
            icon_ink = count_where(img, (cx + 8, ry, cx + 42, ry + 30),
                                   lambda r, g, b: not is_card_white(r, g, b))
            check(icon_ink > 40, f"24_next_days card {i + 1} row {k + 1}: garment icon drawn ({icon_ink} ink px, need > 40)")
            word_ink = count_where(img, (cx + 50, ry + 2, cx + 234, ry + 28), is_label_ink)
            check(word_ink > 150, f"24_next_days card {i + 1} row {k + 1}: the garment's name renders at readable size ({word_ink} ink px, need > 150)")
            # The word's own ink band (the probe starts below the
            # previous row's descender tail: rows are 30 px pitch,
            # each word's ink sits ~13..29 px into its row).
            rows = [y for y in range(ry + 8, ry + 30)
                    if count_where(img, (cx + 50, y, cx + 234, y + 1), is_label_ink) > 0]
            extent = (max(rows) - min(rows) + 1) if rows else 0
            check(0 < extent <= 22, f"24_next_days card {i + 1} row {k + 1}: the name sits on one line ({extent} ink rows, need 1..22)")
        ink_cols = [x for x in range(cx + 40, cx + 236) for y in range(288, 412)
                    if is_label_ink(*img.getpixel((x, y)))]
        check(ink_cols and max(ink_cols) <= cx + 232,
              f"24_next_days card {i + 1}: list ink stays inside the card (rightmost {max(ink_cols) if ink_cols else -1}, limit {cx + 232})")

    # Forecast ground (iter-10): the page's pale blue-grey third
    # tint is gone; the forecast page follows the outfit pages'
    # background rule. This set renders at 08:05 (morning), so the
    # ground is the morning cream, pixel-equal to the outfit page's
    # own corner in the same bucket; the stale set's forecast page
    # (25b, 13:12, also morning) matches its outfit page (25) too.
    img = Image.open(os.path.join(OUT, "24_next_days.png")).convert("RGB")
    img20 = Image.open(os.path.join(OUT, "20_morning.png")).convert("RGB")
    c24, c20 = img.getpixel((4, 4)), img20.getpixel((4, 4))
    check(all(abs(c24[k] - (251, 246, 238)[k]) <= 14 for k in range(3)),
          f"24_next_days: forecast ground is the morning cream {c24} (need ~ (251,246,238))")
    check(c24 == c20, f"24_next_days: forecast ground equals the outfit page's ground ({c24} vs {c20})")
    img25 = Image.open(os.path.join(OUT, "25_stale.png")).convert("RGB")
    img25b = Image.open(os.path.join(OUT, "25b_stale_next_days.png")).convert("RGB")
    check(img25b.getpixel((4, 4)) == img25.getpixel((4, 4)),
          f"25b_stale_next_days: stale forecast ground equals its outfit page's ({img25b.getpixel((4, 4))} vs {img25.getpixel((4, 4))})")

    # Settings bands (iter-07; geometry iter-09): the speed test
    # pill and the bottom button row must not overlap. The button
    # row's ink fill (24,32,65) forms rows at least 200 px wide in
    # (80,420)-(560,480) (a pill's own label text is ink too, but
    # never 200 px wide in a row), and the speed pill's light body
    # (237,241,245) shows above it. In the iter-06 layout the button
    # drew OVER the pill: the pill's visible light ended at the
    # button's top edge and the gap computed here was 0.
    img = Image.open(os.path.join(OUT, "33_settings.png")).convert("RGB")

    def is_pill_light(r, g, b):
        return r > 225 and g > 228 and b > 235 and b >= r

    def is_button_ink(r, g, b):
        return abs(r - 24) <= 12 and abs(g - 32) <= 12 and abs(b - 65) <= 14

    ink_rows = [y for y in range(420, 480)
                if count_where(img, (80, y, 560, y + 1), is_button_ink) >= 200]
    check(bool(ink_rows), "33_settings: bottom button row found")
    if ink_rows:
        button_top = min(ink_rows)
        light_rows = [y for y in range(356, button_top)
                      if count_where(img, (80, y, 300, y + 1), is_pill_light) >= 10]
        check(bool(light_rows), "33_settings: speed test pill found above the button row")
        if light_rows:
            gap = button_top - max(light_rows) - 1
            check(gap >= 3, f"33_settings: speed pill and button row do not overlap ({gap} clear px, need >= 3)")

    # Settings measure (iter-10): the content spans the page (the
    # old layout hugged the left ~70% and left a dead right column).
    # Non-cream ink/fill must start at the left measure (x <= 60;
    # measured 56, the title's gear) and reach the right measure
    # (x >= 740; measured 743, the Close button's right edge). The
    # tolerance is looser than the corner check's because the light
    # pill fill (238,241,247) sits only 27 from the cream.
    def is_non_cream(r, g, b):
        return abs(r - 251) + abs(g - 246) + abs(b - 238) > 20

    img33 = img
    occ_cols = [x for x in range(800) for y in range(480) if is_non_cream(*img33.getpixel((x, y)))]
    check(min(occ_cols) <= 60, f"33_settings: content starts at the left measure (min ink x {min(occ_cols)}, need <= 60)")
    check(max(occ_cols) >= 740, f"33_settings: content reaches the right measure (max ink x {max(occ_cols)}, need >= 740)")

    # Cat pill gaps (iter-10): the three pills ride a space-between
    # row, so the two gaps are equal by construction. At the pills'
    # mid-row (y 85) the fill colours are the pill light (238,241,
    # 247) and the ink of the selected pill (30,35,64).
    # Iter-11: each pill also carries a white head disc (see the
    # heads check below), so disc white and the heads' fur count as
    # fill too, or the discs would split their pills' runs.
    def is_pill_fill(r, g, b):
        return (abs(r - 238) <= 10 and abs(g - 241) <= 10 and abs(b - 247) <= 10) or \
               (abs(r - 30) <= 12 and abs(g - 35) <= 12 and abs(b - 64) <= 14) or \
               (r > 248 and g > 248 and b > 248) or is_dark_fur(r, g, b) or is_pink_fur(r, g, b)

    # Iter-11 amendment: the discs widened the pills until the
    # space-between gaps shrank to ~7 px of page cream, the same
    # size as the breaks the heads and labels cut in a pill's fill,
    # so fill-run clustering can no longer tell a gap from a break.
    # The gaps are the only PURE CREAM on the row, so cluster on
    # non-cream instead: three wide non-cream runs, cream between.
    def is_page_cream85(r, g, b):
        return abs(r - 251) <= 6 and abs(g - 246) <= 6 and abs(b - 238) <= 6

    nc_runs = []
    ncr = None
    for x in range(140, 745):
        if not is_page_cream85(*img33.getpixel((x, 85))[:3]):
            if ncr is None:
                ncr = [x, x]
            ncr[1] = x
        elif ncr is not None and x - ncr[1] > 3:
            nc_runs.append(tuple(ncr))
            ncr = None
    if ncr is not None:
        nc_runs.append(tuple(ncr))
    clusters = [list(r) for r in nc_runs if r[1] - r[0] >= 100]
    check(len(clusters) == 3, f"33_settings: three Cat pills found in their row ({clusters})")
    if len(clusters) == 3:
        gap1 = clusters[1][0] - clusters[0][1] - 1
        gap2 = clusters[2][0] - clusters[1][1] - 1
        check(abs(gap1 - gap2) <= 2, f"33_settings: Cat pill gaps are equal ({gap1} px and {gap2} px, need within 2)")
        check(clusters[0][0] <= 145 and clusters[2][1] >= 738,
              f"33_settings: Cat pill row spans the measure ({clusters[0][0]}..{clusters[2][1]}, need ~140..744)")

    # The cat is IN the Cat chooser (iter-11): the black head's dark
    # fur and the pink head's pink fur both appear in the pill row's
    # band. The selected pill's ink fill (30,35,64) is blue-dark, so
    # the black-head predicate is the tighter neutral dark (all
    # channels < 55, blue < 50), which the ink does not satisfy.
    def is_head_dark(r, g, b):
        return r < 55 and g < 55 and b < 50

    dark_heads = count_where(img33, (140, 62, 744, 108), is_head_dark)
    pink_heads = count_where(img33, (140, 62, 744, 108), is_pink_fur)
    check(dark_heads > 150, f"33_settings: the black cat's head rides its pill ({dark_heads} dark-fur px, need > 150)")
    check(pink_heads > 150, f"33_settings: the pink cat's head rides its pill ({pink_heads} pink-fur px, need > 150)")

    # The tools row (iter-11): the speed pill and the three action
    # buttons read as one group: the buttons share one height (48)
    # and are laid out at deliberate fixed widths (300/190/174) with
    # identical 12 px gaps across the measure, and the speed pill
    # sits directly above them on the same left edge, 12 px up.
    def is_btn_fill(r, g, b):
        return (abs(r - 24) <= 8 and abs(g - 32) <= 8 and abs(b - 66) <= 10) or \
               (abs(r - 238) <= 8 and abs(g - 241) <= 8 and abs(b - 247) <= 8) or \
               (r > 250 and g > 250 and b > 250)

    btn_runs = []
    run = None
    for x in range(50, 750):
        if is_btn_fill(*img33.getpixel((x, 448))[:3]):
            if run is None:
                run = [x, x]
            run[1] = x
        elif run is not None and x - run[1] > 6:
            btn_runs.append(tuple(run))
            run = None
    if run is not None:
        btn_runs.append(tuple(run))
    btn_runs = [r for r in btn_runs if r[1] - r[0] >= 100]
    check(len(btn_runs) == 3, f"33_settings: three tools buttons on the bottom row ({btn_runs})")
    if len(btn_runs) == 3:
        widths = [b - a + 1 for a, b in btn_runs]
        bgaps = [btn_runs[1][0] - btn_runs[0][1] - 1, btn_runs[2][0] - btn_runs[1][1] - 1]
        check(all(abs(w - e) <= 3 for w, e in zip(widths, (300, 190, 174))),
              f"33_settings: tools buttons at their deliberate widths ({widths}, need [300, 190, 174] within 3)")
        check(abs(bgaps[0] - bgaps[1]) <= 2 and 8 <= bgaps[0] <= 16,
              f"33_settings: tools button gaps identical ({bgaps} px, need equal within 2, ~12)")
        for (a, b) in btn_runs:
            cxm = (a + b) // 2
            check(is_btn_fill(*img33.getpixel((cxm, 430))[:3]) and is_btn_fill(*img33.getpixel((cxm, 466))[:3]),
                  f"33_settings: tools button at x {a}..{b} spans the shared height (fill at y 430 and y 466)")
    col100 = [y for y in range(340, 480) if is_btn_fill(*img33.getpixel((100, y))[:3])]
    if col100:
        check(min(col100) <= 368, f"33_settings: speed pill grouped above the row (its fill starts y {min(col100)}, need <= 368)")
        below = [y for y in col100 if y > 416]
        check(below and min(below) == 424, f"33_settings: button row top at y 424 under the speed pill (first fill {min(below) if below else None})")

    # Status pills out of the tab bar (iter-10): the stale/offline
    # pill (COL_WARN_BG) and the progress pill (COL_OK_BG) are
    # status, not tabs: neither fill may appear in the tab row zone
    # (y 424..480) on their screens, and each pill must be findable
    # in its new home: the stale pill as a toast above the tab bar
    # (25, 26) or in the forecast header band (25b), the progress
    # pill in the header band right of the day chip (22). A pill is
    # ~3000+ fill px; stray anti-aliasing is tens, so the tab-zone
    # bound is < 100, not 0 (the evening ground shows through the
    # transparent tabs and sits near COL_OK_BG at loose tolerance;
    # the tight +-8 predicate below is what keeps it out).
    def is_ok_bg_tight(r, g, b):
        return abs(r - 232) <= 8 and abs(g - 246) <= 8 and abs(b - 238) <= 8

    img = Image.open(os.path.join(OUT, "25_stale.png"))
    n = count_where(img, (0, 424, 800, 480), is_warn_bg)
    check(n < 100, f"25_stale: no status pill fill in the tab row ({n} px, need < 100)")
    n = count_where(img, (500, 388, 772, 424), is_warn_bg)
    check(n > 3000, f"25_stale: 'Weather from' pill is a toast above the tab bar ({n} px, need > 3000)")
    img = Image.open(os.path.join(OUT, "26_unavailable.png"))
    n = count_where(img, (0, 424, 800, 480), is_warn_bg)
    check(n < 100, f"26_unavailable: no status pill fill in the tab row ({n} px, need < 100)")
    n = count_where(img, (500, 388, 772, 424), is_warn_bg)
    check(n > 3000, f"26_unavailable: 'Offline since' pill is a toast above the tab bar ({n} px, need > 3000)")
    img = Image.open(os.path.join(OUT, "25b_stale_next_days.png"))
    n = count_where(img, (500, 40, 772, 76), is_warn_bg)
    check(n > 3000, f"25b_stale_next_days: stale pill sits in the forecast header band ({n} px, need > 3000)")
    img = Image.open(os.path.join(OUT, "22_evening_2_checked.png"))
    n = count_where(img, (270, 60, 486, 108), is_ok_bg_tight)
    check(n > 2000, f"22_evening_2_checked: progress pill sits in the header beside the day chip ({n} px, need > 2000)")
    n = count_where(img, (0, 424, 800, 480), is_ok_bg_tight)
    check(n < 100, f"22_evening_2_checked: no progress pill fill in the tab row ({n} px, need < 100)")
    img = Image.open(os.path.join(OUT, "21_evening.png"))
    n = count_where(img, (270, 60, 486, 108), is_ok_bg_tight)
    check(n < 100, f"21_evening: no progress pill before anything is checked ({n} px, need < 100)")

    # ------------------------------------------------------------ text
    # dumps (iter-08). The sim writes <name>.txt beside each PNG
    # (ui_test_dump): the outfit state the render actually produced.
    # Words are checked as words here, tiers as tiers.
    import json as _json

    def dump(name):
        d = {"info_lines": []}
        in_info = False
        with open(os.path.join(OUT, name + ".txt")) as f:
            for line in f.read().splitlines():
                if line == "info_begin":
                    in_info = True
                elif line == "info_end":
                    in_info = False
                elif in_info:
                    d["info_lines"].append(line)
                elif "=" in line:
                    k, v = line.split("=", 1)
                    d[k] = v
        return d

    def scenario_now_temp(fname):
        path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "scenarios", fname)
        with open(path) as f:
            return _json.load(f)["current"]["temperature_2m"]

    DUMPED = ("20_morning", "21_evening", "22_evening_2_checked", "23_evening_all_done",
              "25_stale", "25c_stale_tapped", "26_unavailable",
              "26b_unavailable_tomorrow", "27_rollover",
              "28_snow", "29_hot", "30_storm", "31_halloween", "32_pink_cat",
              "34_long_text", "35_hanukkah", "36_bike_bus", "37_dress_day", "38_warm_day",
              "39_no_yesterday",
              "50_rot_nov08", "50_rot_nov08b_tomorrow", "51_rot_nov09", "51_rot_nov09b_tomorrow",
              "52_rot_nov10", "52_rot_nov10b_tomorrow", "53_rot_nov11", "53_rot_nov11b_tomorrow",
              "54_rot_nov12", "54_rot_nov12b_tomorrow", "55_rot_nov13", "55_rot_nov13b_tomorrow",
              "56_warmcoat",
              "57_rot_jul13", "57_rot_jul13b_tomorrow", "58_rot_jul14",
              "59_kit_dec13", "59_kit_dec13b_tomorrow", "60_kit2_dec14", "61_kit2_pink",
              "62_hat_kitp_shirt", "62b_hat_kitp_shirt_tomorrow", "63_hat_kitr_shirt",
              "64_hat_kitp_dress", "64b_hat_kitr_dress_tomorrow", "65_hat_kitp_dress_pink")
    dumps = {n: dump(n) for n in DUMPED}

    # The dress bound (iter-09 fix 1, panel round 7's headline): the
    # dress look may only be chosen when the departure layer need is
    # the light-jacket tier, because the dress sprite's jacket IS a
    # light jacket. Where a warm coat is genuinely needed at
    # departure, the winter kit (the only sprite wearing one) takes
    # the morning whole: the 33F brief morning with its big warm-up
    # (the old warm-up carve-out is gone), the long-text day, and the
    # bike-bus Friday, whose gloves are now the kit's own chips.
    KIT_TILES = "Long sleeves|Long pants|Winter coat|Warm boots"
    # Iter-33: the kit family is the 16-look hat band (the kit, kit2,
    # the twelve shirt variants, the two dress variants); the dress
    # variants tile differently, so they are not in this list.
    KIT_FAMILY = ("snow", "kit2",
                  "kitp_navy_stripe", "kitp_rust_stripe", "kitp_mustard_stripe",
                  "kitp_navy_solid", "kitp_mustard_solid", "kitp_teal_solid",
                  "kitr_navy_stripe", "kitr_rust_stripe", "kitr_mustard_stripe",
                  "kitr_navy_solid", "kitr_mustard_solid", "kitr_teal_solid")
    for name in ("20_morning", "34_long_text", "36_bike_bus"):
        d = dumps[name]
        # Iter-30: on a warm-tier hat day the kit family resolves to
        # the kit or its red colourway by date; both are the winter
        # kit this bound protects (the failure mode is the dress).
        # Iter-33: the band is 16 looks now; any shirt-family pick is
        # still the winter kit this bound protects, with the kit's
        # tiles by name.
        check(d["look_name"] in KIT_FAMILY and d["tiles"] == KIT_TILES,
              f"{name}: warm-coat departure is the winter kit (look={d['look_name']}, tiles={d['tiles']})")
        check(d["chips"] == "Warm hat|Gloves",
              f"{name}: the kit's hat and gloves are checkable chips (chips={d['chips']})")
    d36 = dumps["36_bike_bus"]
    check("Bike bus today" in d36["advice"] and "gloves" in d36["advice"].lower(),
          f"36_bike_bus: sentence carries the bike-bus gloves ({d36['advice']})")
    check(dumps["28_snow"]["look_name"] == "snow",
          f"28_snow: the snow day IS the winter kit (look={dumps['28_snow']['look_name']})")
    check(dumps["31_halloween"]["look_name"] == "snow",
          f"31_halloween: 30F departure feels-like IS the winter kit (look={dumps['31_halloween']['look_name']})")
    # The one genuine dress scenario: the fixture was retuned into
    # the light-jacket band (50F now, ~48F at departure), and the
    # dress look's layer is the light jacket, tiled and captioned
    # exactly like the pants look's.
    check(scenario_now_temp("dress_morning.json") == 50.0,
          "dress fixture: retuned to the light-jacket band (50F now)")
    check(dumps["37_dress_day"]["look_name"] == "dress",
          f"37_dress_day: the light-jacket-band morning is the dress look (look={dumps['37_dress_day']['look_name']})")
    check(dumps["37_dress_day"]["tiles"] == "Dress|Light jacket|Tights|Dress shoes",
          f"37_dress_day: dress tiles are Dress|Light jacket|Tights|Dress shoes ({dumps['37_dress_day']['tiles']})")
    # The bound itself, over every dumped scenario: no dress look
    # anywhere prescribes a warm or winter coat, and every dress
    # look's layer tile is the light jacket. Iter-25: the bound
    # covers the whole dress band, not just the original dress.
    DRESS_LOOKS = {"dress", "dress_lavender", "dress_mint", "dress_mustard", "uniform_dress"}
    for name, d in dumps.items():
        if d["look_name"] in DRESS_LOOKS:
            check("Warm coat" not in d["tiles"] and "Winter coat" not in d["tiles"]
                  and "Light jacket" in d["tiles"],
                  f"{name}: dress look never tiles a heavy coat (tiles={d['tiles']})")

    # The sentence names every chip (iter-08 fix 2): for every dumped
    # scenario, each chip's word must appear in BOTH the full advice
    # and the sentence actually shown (the UI may swap to the short
    # fallback; the promise has to hold for what is on screen).
    CHIP_WORD = {"Warm hat": "hat", "Gloves": "glove", "Scarf": "scarf", "Umbrella": "umbrella",
                 "Sunglasses": "sunglasses", "Water bottle": "water bottle"}
    for name, d in dumps.items():
        chips = [c for c in d["chips"].split("|") if c]
        for chip in chips:
            word = CHIP_WORD[chip]
            check(word in d["advice"].lower(),
                  f"{name}: advice names the {chip} chip ({d['advice']})")
            # The shown-sentence half applies wherever the advice
            # sentence is what is on screen. The all-done dump (in
            # DUMPED since iter-11) replaces the sentence with the
            # "Nice job!" celebration by design, so it is exempt from
            # this half and asserted in the celebration block below.
            if name != "23_evening_all_done":
                check(word in d["advice_shown"].lower(),
                      f"{name}: shown sentence names the {chip} chip ({d['advice_shown']})")

    # Dress days itemise fully (iter-08 fix 3): exactly four tiles,
    # the dress, its layer, the tights and the dress shoes she
    # wears. Since iter-09 the only dress scenario is 37 (asserted
    # in the bound block above); the pink brief morning (32) wears
    # the same winter kit as its black colourway (20).
    check(dumps["32_pink_cat"]["tiles"] == dumps["20_morning"]["tiles"]
          and dumps["32_pink_cat"]["look_name"] == dumps["20_morning"]["look_name"],
          f"32_pink_cat: pink colourway wears the kit like 20_morning ({dumps['32_pink_cat']['tiles']})")

    # Stale header honesty (iter-08 fix 8): stale data (bucket 1)
    # must not say "now"; fresh data still may; the unavailable
    # screen keeps its "--".
    check(dumps["25_stale"]["bucket"] == "1" and "now" not in dumps["25_stale"]["wx1"],
          f"25_stale: stale header shows the temperature plainly ({dumps['25_stale']['wx1']})")
    check("now" in dumps["20_morning"]["wx1"],
          f"20_morning: fresh header still says now ({dumps['20_morning']['wx1']})")
    check(dumps["26_unavailable"]["wx1"] == "--",
          f"26_unavailable: header unchanged ({dumps['26_unavailable']['wx1']})")

    # Stale banner (iter-09 fix 5): the big kid-worded pill is on
    # screen exactly when the outfit is stale (its pixels are
    # asserted in the fall-family block above).
    check(dumps["25_stale"]["stale_banner"] == "1",
          f"25_stale: stale banner shown (stale_banner={dumps['25_stale']['stale_banner']})")
    check(dumps["20_morning"]["stale_banner"] == "0" and dumps["26_unavailable"]["stale_banner"] == "0",
          "fresh and unavailable screens show no stale banner")

    # Compare pill (iter-12): the shown day's high against the day
    # before the SHOWN date, in the kid's own terms. A kid remembers
    # whether yesterday was hot or cold; the numbers across days mean
    # nothing to them, so the product speaks in the relative signal.
    # Tiers: |delta| >= 12F "A lot ...", 4..11F plain, within +-3F
    # "Same as ...". The Today page says "than yesterday", the
    # Tomorrow page "than today". Every expectation below is the
    # fixture's own daily highs, differenced by hand:
    #   brief week: Mon 60, Tue 64, Wed 55 (yday is Monday)
    #   snow 38->31, hot 78->92, storm 88->90, halloween 55->48,
    #   hanukkah (renders today) 41 -> 39, bike-bus Thu 74 -> Fri 52,
    #   dress 62->62, warm 70->73, long-text 55->66.
    CMP = {
        "20_morning": "Warmer than yesterday",        # 64 vs 60, +4
        "21_evening": "Colder than today",            # 55 vs 64, -9
        "22_evening_2_checked": "Colder than today",
        "23_evening_all_done": "Colder than today",
        "25_stale": "Warmer than yesterday",          # same fetch, faded
        "25c_stale_tapped": "Warmer than yesterday",
        "26_unavailable": "",                         # no data, no pill
        "26b_unavailable_tomorrow": "",
        "27_rollover": "Colder than yesterday",       # shown Wed 55 vs Tue 64
        "28_snow": "Colder than yesterday",           # 31 vs 38, -7
        "29_hot": "A lot warmer than yesterday",      # 92 vs 78, +14
        "30_storm": "Same as yesterday",              # 90 vs 88, +2
        "31_halloween": "Colder than yesterday",      # 48 vs 55, -7
        "32_pink_cat": "Warmer than yesterday",
        "34_long_text": "Warmer than yesterday",      # 66 vs 55, +11: plain tier
        "35_hanukkah": "Same as yesterday",           # renders the today page: 39 vs 41, -2
        "36_bike_bus": "A lot colder than yesterday", # 52 vs 74, -22
        "37_dress_day": "Same as yesterday",          # 62 vs 62
        "38_warm_day": "Same as yesterday",           # 73 vs 70, +3
        "39_no_yesterday": "",                        # no yesterday in the payload
    }
    for name, want in CMP.items():
        check(dumps[name]["compare"] == want,
              f"{name}: compare pill says {want!r} (dump has {dumps[name]['compare']!r})")
    check(dump("24_next_days")["compare"] == "Warmer than yesterday",
          "24_next_days: the forecast page carries the morning's compare state")

    # Pill tints by tier, counted in the pill's own band under the
    # High/Low line (y78..103, right-aligned to x772; the widest pill
    # starts at x488). Colder is cool blue, warmer is warm amber,
    # same is quiet neutral; the stale pill wears the faded blend of
    # its tint over the page ground, like the rest of the stage.
    PILL_BAND = (480, 76, 772, 106)

    def pill_img(name):
        return Image.open(os.path.join(OUT, name + ".png"))

    def is_cmp_cold(r, g, b):
        return 205 < r < 230 and 225 < g < 245 and b > 238

    def is_cmp_warm(r, g, b):
        return r > 245 and 218 < g < 236 and 165 < b < 185

    def is_cmp_same(r, g, b):
        return 226 < r < 242 and 223 < g < 239 and 214 < b < 230

    def is_cmp_warm_faded(r, g, b):
        return r > 245 and 226 < g < 239 and 184 < b < 200

    check(count_where(pill_img("36_bike_bus"), PILL_BAND, is_cmp_cold) > 3000,
          "36_bike_bus: a-lot-colder pill wears the cool blue tint")
    check(count_where(pill_img("27_rollover"), PILL_BAND, is_cmp_cold) > 3000,
          "27_rollover: colder pill wears the cool blue tint")
    check(count_where(pill_img("20_morning"), PILL_BAND, is_cmp_warm) > 2500,
          "20_morning: warmer pill wears the warm amber tint")
    check(count_where(pill_img("29_hot"), PILL_BAND, is_cmp_warm) > 3000,
          "29_hot: a-lot-warmer pill wears the warm amber tint")
    check(count_where(pill_img("30_storm"), PILL_BAND, is_cmp_same) > 2000,
          "30_storm: same pill wears the neutral tint")
    check(count_where(pill_img("25_stale"), (545, 76, 772, 106), is_cmp_warm_faded) > 2500
          and count_where(pill_img("25_stale"), (545, 76, 772, 106), is_cmp_warm) < 300,
          "25_stale: the compare pill takes the stale fade with the rest of the stage")
    for name in ("26_unavailable", "39_no_yesterday"):
        n_tint = sum(count_where(pill_img(name), PILL_BAND, p)
                     for p in (is_cmp_cold, is_cmp_warm, is_cmp_same, is_cmp_warm_faded))
        check(n_tint < 200,
              f"{name}: no compare pill pixels (no data / no yesterday) [{n_tint}]")

    # Storm header icon (iter-09 fix 2): when the day's story
    # includes storms, the header icon is the icon set's thunder
    # glyph (index 11, the same one the forecast cards use for
    # codes 95-99), never the benign sun-cloud the current condition
    # alone would give. A plain hot day keeps its fair icon.
    check(dumps["30_storm"]["wx_icon"] == "11",
          f"30_storm: header icon is the thunder glyph (wx_icon={dumps['30_storm']['wx_icon']})")
    check(dumps["29_hot"]["wx_icon"] != "11",
          f"29_hot: no storm story, no thunder glyph (wx_icon={dumps['29_hot']['wx_icon']})")

    # Forecast cards carry the full outfit (iter-09 fix 3): each
    # card's tile list (fc*_tiles in the 24_next_days dump) is the
    # same prescription the outfit page computes for that date.
    # Card 1 is the brief Wednesday, the very day 21_evening lays
    # out; cards 2 and 3 land in the light-jacket band and wear one
    # of the dress band's dresses (iter-25: the band rotates by date,
    # so which dress is the date's pick), itemised like 37_dress_day.
    DRESS_TILES = {"Dress|Light jacket|Tights|Dress shoes",
                   "Lavender dress|Light jacket|Tights|Dress shoes",
                   "Mint dress|Light jacket|Tights|Dress shoes",
                   "Mustard dress|Light jacket|Tights|Dress shoes",
                   "Logo dress|Light jacket|Tights|Dress shoes"}
    fc = dump("24_next_days")
    check(fc["fc0_tiles"] == dumps["21_evening"]["tiles"],
          f"24_next_days card 1: full outfit matches the outfit page for that day ({fc['fc0_tiles']})")
    for k in ("fc1_tiles", "fc2_tiles"):
        check(fc[k] in DRESS_TILES,
              f"24_next_days {k}: dress day itemised in full ({fc[k]})")
    for k in ("fc0_tiles", "fc1_tiles", "fc2_tiles"):
        check(len([t for t in fc[k].split("|") if t]) >= 3,
              f"24_next_days {k}: at least a top, a bottom/layer and footwear ({fc[k]})")

    # Forecast list words: every list icon carries its word
    # (fc*_labels in the dump, set from the same tile list), so the
    # card's outfit can be read aloud whole. (Iter-09/10 put the
    # words under a 2x2 icon stack in a short kid vocabulary;
    # iter-11 replaced the stack with the full-width list and the
    # short words with the tiles' own names.) The words are
    # asserted as words here and as pixels (present, one line,
    # inside the card) in the forecast block above.
    # Iter-11: the cards speak the outfit screens' language. The
    # words are the tiles' EXACT names (garment_label verbatim), and
    # for every card the label list equals the tile list string for
    # string: no second, abbreviated vocabulary anywhere.
    check(fc["fc0_labels"] == "Long sleeves|Long pants|Rain jacket|Rain boots",
          f"24_next_days card 1: rain outfit's words are the tiles' own names ({fc['fc0_labels']})")
    for k in ("fc1_labels", "fc2_labels"):
        check(fc[k] in DRESS_TILES,
              f"24_next_days {k}: dress outfit's words are the tiles' own names ({fc[k]})")
    for i in range(3):
        check(fc[f"fc{i}_labels"] == fc[f"fc{i}_tiles"],
              f"24_next_days fc{i}: every forecast word equals the tile name for the same garment (labels={fc[f'fc{i}_labels']}, tiles={fc[f'fc{i}_tiles']})")

    # Hot storm KEEPS the sunglasses chip (iter-11, reverting the
    # iter-10 removal): Pippa wears the heart sunglasses in the
    # picture, so they are prescribed, chipped and named like on any
    # hot day, and they gate all-done with the water bottle. Two
    # panel rounds settled it: an unprescribed garment on the cat
    # breaks the product's copy-the-cat contract (round 9's kids
    # judge dropped the screen to 8 over exactly this; the parent
    # judge's top outfit-screen finding agreed), while the storm
    # framing survives intact, because the safety beat leads the
    # sentence and the sunglasses ride the don't-forget pass like
    # every other day. The plain hot day is unchanged.
    d30 = dumps["30_storm"]
    check(d30["chips"] == "Sunglasses|Water bottle",
          f"30_storm: hot storm keeps both chips, sunglasses restored (chips={d30['chips']})")
    check("sunglasses" in d30["advice"].lower(),
          f"30_storm: the sentence names the chipped sunglasses ({d30['advice']})")
    check("sunglasses" in d30["advice_shown"].lower(),
          f"30_storm: the shown sentence names them too ({d30['advice_shown']})")
    check("thunders" in d30["advice"],
          f"30_storm: the storm safety beat still leads the sentence ({d30['advice']})")
    check(dumps["29_hot"]["chips"] == "Sunglasses|Water bottle",
          f"29_hot: plain hot day keeps both chips (chips={dumps['29_hot']['chips']})")

    # Winter kit afternoon story (iter-10): a kit day with a real
    # warm-up (Halloween: near-freezing departure, much warmer
    # afternoon) gains the coat-off clause and the "Bring" verb,
    # through the same warm-up sentence the other layers use; the
    # kit stays the prescription whole. Days whose own sentence
    # owns the story keep it: the bike-bus Friday, the snow day,
    # the still-cold evening. The dress day (not a kit) is
    # untouched by the kit rule.
    d31 = dumps["31_halloween"]
    check("warm after lunch" in d31["advice"] and "winter coat off later" in d31["advice"],
          f"31_halloween: kit day gains the afternoon clause ({d31['advice']})")
    check(d31["advice"].startswith("Wear long sleeves. Bring your winter coat."),
          f"31_halloween: warm-up verb is Bring for the coat ({d31['advice']})")
    check("warm after lunch" in d31["advice_shown"],
          f"31_halloween: the shown sentence carries the clause too ({d31['advice_shown']})")
    check("warm after lunch" not in dumps["36_bike_bus"]["advice"] and "Bike bus today" in dumps["36_bike_bus"]["advice"],
          f"36_bike_bus: the bike-bus sentence still owns its day ({dumps['36_bike_bus']['advice']})")
    check("warm after lunch" not in dumps["28_snow"]["advice"],
          f"28_snow: the snow sentence still owns its day ({dumps['28_snow']['advice']})")
    check("warm after lunch" not in dumps["35_hanukkah"]["advice"],
          f"35_hanukkah: a kit evening with no warm-up gets no clause ({dumps['35_hanukkah']['advice']})")
    check("warm after lunch" not in dumps["37_dress_day"]["advice"],
          f"37_dress_day: the kit rule does not leak onto dress days ({dumps['37_dress_day']['advice']})")
    check("warm after lunch" in dumps["34_long_text"]["advice"],
          f"34_long_text: the general 15F warm-up clause still fires ({dumps['34_long_text']['advice']})")

    # Unavailable headline (iter-10): when no outfit is offered the
    # headline carries the state, on the Today AND Tomorrow
    # variants; fresh and stale screens keep their headlines.
    check(dumps["26_unavailable"]["title"] == "No weather yet",
          f"26_unavailable: headline is the state (title={dumps['26_unavailable']['title']})")
    check(dumps["26b_unavailable_tomorrow"]["title"] == "No weather yet",
          f"26b_unavailable_tomorrow: headline is the state on Tomorrow too (title={dumps['26b_unavailable_tomorrow']['title']})")
    check(dumps["20_morning"]["title"] == "Let's get dressed!",
          f"20_morning: fresh headline unchanged (title={dumps['20_morning']['title']})")
    check(dumps["25_stale"]["title"] == "What to wear today",
          f"25_stale: stale headline unchanged (title={dumps['25_stale']['title']})")
    check(dumps["21_evening"]["title"] == "Ready for tomorrow?",
          f"21_evening: tomorrow headline unchanged (title={dumps['21_evening']['title']})")

    # Settings info rows (iter-08 fix 4a; rows iter-09 fix 7): five
    # label/value lines, none of them a lone orphan word (the old
    # card wrapped "AM" onto a line by itself), the time whole on
    # the Updated line, and Address and Version on SEPARATE lines
    # (the old card ran them together: "Address: ... Version ...").
    # The speed test is one control now: the dump's speed_label is
    # the pill's own text and carries its state.
    s33 = dump("33_settings")
    info = s33["info_lines"]
    check(len(info) == 5, f"33_settings: info block has 5 rows ({len(info)}: {info})")
    check(all(len(line.split()) > 1 for line in info),
          f"33_settings: no orphan single-word line in the info rows ({info})")
    check(any("Updated" in line and "7:30 AM" in line for line in info),
          f"33_settings: 'Updated 7:30 AM' stays on one line ({info})")
    check(any(line.startswith("Address:") and "Version" not in line for line in info)
          and any(line.startswith("Version:") for line in info),
          f"33_settings: Address and Version on separate rows ({info})")
    check(s33["speed_label"] == "Speed test: off",
          f"33_settings: the speed pill's label is its state (speed_label={s33['speed_label']})")

    # Stale check-off lock (iter-11): the stale screen reports its
    # checks disabled, its badge discs render grey-filled (fresh
    # screens keep the white checkable disc), and driving the real
    # toggle paths at it changes nothing at all: the post-tap frame
    # is the stale frame byte for byte, and no checks register.
    check(dumps["25_stale"]["stale_checks"] == "disabled",
          f"25_stale: checks report disabled under stale (stale_checks={dumps['25_stale']['stale_checks']})")
    check(dumps["20_morning"]["stale_checks"] == "enabled",
          f"20_morning: checks report enabled when fresh (stale_checks={dumps['20_morning']['stale_checks']})")
    check(dumps["25_stale"]["checks"] == "0" and dumps["25c_stale_tapped"]["checks"] == "0",
          f"25c_stale_tapped: taps under stale register no checks (before={dumps['25_stale']['checks']}, after={dumps['25c_stale_tapped']['checks']})")
    check(dumps["25c_stale_tapped"]["advice"] == dumps["25_stale"]["advice"]
          and dumps["25c_stale_tapped"]["tiles"] == dumps["25_stale"]["tiles"],
          "25c_stale_tapped: the dump after tile+chip taps equals the stale dump")
    import hashlib
    h25 = hashlib.sha256(open(os.path.join(OUT, "25_stale.png"), "rb").read()).hexdigest()
    h25c = hashlib.sha256(open(os.path.join(OUT, "25c_stale_tapped.png"), "rb").read()).hexdigest()
    check(h25 == h25c, "25c_stale_tapped: the frame after tile+chip taps is the stale frame unchanged")

    def is_grey_disc(r, g, b):
        return abs(r - g) <= 14 and abs(g - b) <= 14 and 185 <= r <= 240

    img25 = Image.open(os.path.join(OUT, "25_stale.png")).convert("RGB")
    img20 = Image.open(os.path.join(OUT, "20_morning.png")).convert("RGB")
    g_stale = count_where(img25, (514, 204, 530, 220), is_grey_disc)
    g_fresh = count_where(img20, (514, 204, 530, 220), is_grey_disc)
    check(g_stale > 150, f"25_stale: tile badge disc is grey-filled ({g_stale} px in its centre box, need > 150)")
    check(g_fresh < 10, f"20_morning: fresh tile badge disc stays white ({g_fresh} grey px, need < 10)")
    g_chip = count_where(img25, (392, 330, 412, 350), is_grey_disc)
    check(g_chip > 150, f"25_stale: chip badge disc is grey-filled too ({g_chip} px in its centre box, need > 150)")

    # All-done celebrates the same everywhere (iter-11): the evening
    # all-done (23, rain outfit) carries the same treatment as the
    # morning dress one (23b): sparkles up around the cat (74 solid
    # sparkle px per burst position measured; the unchecked
    # counterparts have none) and the heart on her chest (for 23 the
    # extracted heart alone: new heart-pink pixels over the unchecked
    # frame in the chest zone and no new dark paw mass; 23b's own
    # heart frame is asserted by the dress checks above). The
    # all-done line is
    # "Nice job!" with no names, and no dump anywhere carries the
    # children's names.
    check(dumps["23_evening_all_done"]["advice_shown"] == "Nice job!",
          f"23_evening_all_done: all-done line is the plain celebration (advice_shown={dumps['23_evening_all_done']['advice_shown']})")
    name_sources = dict(dumps)
    name_sources["24_next_days"] = fc
    name_sources["33_settings"] = s33
    for stem in ("20_morning", "21_evening", "23_evening_all_done", "24_next_days",
                 "25_stale", "25c_stale_tapped", "29_hot", "30_storm", "33_settings"):
        blob = " ".join(v for v in name_sources[stem].values() if isinstance(v, str))
        blob += " " + " ".join(name_sources[stem].get("info_lines", []))
        check("Cleo" not in blob and "Lila" not in blob,
              f"{stem}: no screen text carries the children's names")

    def is_sparkle(r, g, b):
        return r > 248 and 190 < g < 216 and 30 < b < 85

    sparkle_boxes = [(58, 120, 98, 160), (224, 112, 264, 152), (232, 192, 272, 232)]
    img23 = Image.open(os.path.join(OUT, "23_evening_all_done.png")).convert("RGB")
    img21 = Image.open(os.path.join(OUT, "21_evening.png")).convert("RGB")
    img23b = Image.open(os.path.join(OUT, "23b_morning_all_done_heart.png")).convert("RGB")
    img37 = Image.open(os.path.join(OUT, "37_dress_day.png")).convert("RGB")
    for nm, im, ref in (("23_evening_all_done", img23, img21), ("23b_morning_all_done_heart", img23b, img37)):
        for bi, bx in enumerate(sparkle_boxes):
            n = count_where(im, bx, is_sparkle)
            nr = count_where(ref, bx, is_sparkle)
            check(n > 30, f"{nm}: sparkle {bi + 1} is up at all-done ({n} px, need > 30)")
            check(nr < 3, f"{nm}: its unchecked counterpart has no sparkle {bi + 1} ({nr} px, need < 3)")

    def is_overlay_dark(r, g, b):
        return r < 70 and g < 70 and b < 90

    def is_heart_pink(r, g, b):
        return r >= 120 and r - g >= 30 and r - b >= 10

    new_dark = sum(1 for y in range(198, 260) for x in range(112, 198)
                   if is_overlay_dark(*img23.getpixel((x, y))[:3])
                   and not is_overlay_dark(*img21.getpixel((x, y))[:3]))
    new_heart = sum(1 for y in range(198, 260) for x in range(112, 198)
                    if is_heart_pink(*img23.getpixel((x, y))[:3])
                    and not is_heart_pink(*img21.getpixel((x, y))[:3]))
    # The overlay is the heart ALONE (first hardware flash: when it
    # carried the animation's paws, the layered raised arms read as a
    # second pair of arms over the static sprite's resting ones).
    # So all-done must ADD heart pink to the chest zone and must NOT
    # add a dark paw mass: the paw version of this overlay produced
    # hundreds of new dark pixels here.
    check(new_heart > 80, f"23_evening_all_done: the heart floats on her chest ({new_heart} new heart-pink px vs unchecked, need > 80)")
    check(new_dark < 60, f"23_evening_all_done: no paw mass over her chest ({new_dark} new dark px vs unchecked, need < 60)")

    # Screen edge guard (iter-08 fix 5, kept after the advice glyph
    # it was written for was deleted in iter-09): no outfit screen
    # puts anything in the screen's outermost 4 columns at all.
    for name in ("20_morning", "28_snow", "36_bike_bus", "37_dress_day"):
        img = Image.open(os.path.join(OUT, name + ".png"))
        n = 0
        for box in ((0, 0, 4, 480), (796, 0, 800, 480)):
            n += count_where(img, box,
                             lambda r, g, b: abs(r - 251) + abs(g - 246) + abs(b - 238) > 24)
        check(n == 0, f"{name}: nothing drawn in the outermost 4 screen columns ({n} px, need 0)")

    # ------------------------------------------------------------ My Day
    # (iter-14): the fixture calendar (invented events built from
    # the brief_morning clock, Tue Oct 27 2026) in its three states.
    # Words first, from the dumps: the rows the page actually showed,
    # in order, which also proves cal.c sorted the scrambled file,
    # read the UTC event as 4 PM local, unfolded the folded summary
    # and unescaped the escaped comma.
    md = dump("40_myday")
    check(md["myday_state"] == "events",
          f"40_myday: populated state (myday_state={md['myday_state']})")
    # Iter-35: the flex cards size to content, so the six-event
    # fixture day no longer fits the list beside Pippa: three of
    # today's cards show, the fourth and both of tomorrow's fold
    # into the "+3 more" line (the row strings below still pin
    # every event's text, folded or not).
    check(md["myday_today_n"] == "3" and md["myday_tom_n"] == "0",
          f"40_myday: 3 rows today, tomorrow folded ({md['myday_today_n']}/{md['myday_tom_n']})")
    check(md["myday_more_today"] == "1" and md["myday_more_tom"] == "2",
          f"40_myday: the fold counts 1 today, 2 tomorrow ({md['myday_more_today']}/{md['myday_more_tom']})")
    # Iter-15: the time column carries ranges (pickup time is half
    # the point) and all-day events read ALL DAY (the navy pill).
    check(md["myday_today0"] == "ALL DAY|Picture Day|",
          f"40_myday: today row 1 is the all-day picture day with the ALL DAY pill ({md['myday_today0']})")
    check(md["myday_today1"] == "10:00-10:45 AM|Lila's library story time|",
          f"40_myday: today row 2 is Lila's library story time with its end time ({md['myday_today1']})")
    check(md["myday_today2"] == "3:30-4:30 PM|Cleo's soccer practice|",
          f"40_myday: today row 3 is soccer with its end time ({md['myday_today2']})")
    check(md["myday_today3"] == "4:00-4:45 PM|Video call with Grandma|",
          f"40_myday: today row 4 is the UTC call at 4 PM local with its end time ({md['myday_today3']})")
    check(md["myday_tom0"] == "8:00-9:00 AM|School bake sale|",
          f"40_myday: tomorrow row 1 is the bake sale with its end time ({md['myday_tom0']})")
    check(md["myday_tom1"] == "4:00-4:30 PM|Cleo's piano lesson|",
          f"40_myday: tomorrow row 2 is Cleo's piano with its end time, folded summary unfolded ({md['myday_tom1']})")
    # Iter-15: the freshness line (fixture fetch pinned to 7:02 AM
    # today, so the same-day form). Iter-16: the NEXT tag — at the
    # 7:35 fixture clock the first TIMED event still to come is the
    # 10:00 story time; the all-day row never carries it. Iter-18:
    # the statuses are computed against the clock (DONE/NOW/NEXT);
    # at 7:35 AM nothing is in progress and nothing is over, so
    # story time keeps NEXT and there is no NOW or DONE anywhere.
    # The dumps name the tagged rows' titles.
    check(md["myday_fresh"] == "Updated 7:02 AM",
          f"40_myday: freshness line shows the pinned fetch time ({md['myday_fresh']})")
    check(md["myday_nextup"] == "Lila's library story time",
          f"40_myday: NEXT tag on the story time row ({md['myday_nextup']})")
    check(md["myday_nowup"] == "" and md["myday_done"] == "",
          f"40_myday: no NOW or DONE tags in the morning (nowup={md['myday_nowup']!r}, done={md['myday_done']!r})")
    # Iter-17: Tomorrow's header carries its date (Tue Oct 27's
    # tomorrow is Wed Oct 28), the piano row wears its CLEO name
    # tag, and at 7:35 AM nothing today is over yet, so no row is
    # dimmed. Iter-18: both girls appear across the page, LILA on
    # the story time row, CLEO on the piano row.
    check(md["myday_tom_date"] == "Wed, Oct 28",
          f"40_myday: tomorrow header shows its date ({md['myday_tom_date']})")
    check(md["myday_nametag"] == "CLEO" and md["myday_nametag_row"] == "Cleo's piano lesson",
          f"40_myday: CLEO name tag on the piano row (tag={md['myday_nametag']!r}, row={md['myday_nametag_row']!r})")
    check(md["myday_nametags"] == "LILA:Lila's library story time;CLEO:Cleo's soccer practice;CLEO:Cleo's piano lesson",
          f"40_myday: LILA and CLEO name tags across the page ({md['myday_nametags']!r})")
    check(md["myday_dimmed"] == "",
          f"40_myday: no past rows dimmed in the morning ({md['myday_dimmed']!r})")
    check(md["myday_icons"] == "camera,book,soccer",
          f"40_myday: category icons of the visible rows ({md['myday_icons']})")
    # Iter-35: the half-card form is gone (one card style
    # everywhere); the dump key stays and reads 0.
    check(md["myday_half"] == "0",
          f"40_myday: no half cards in the flex design (myday_half={md['myday_half']})")

    md41 = dump("41_myday_empty")
    check(md41["myday_state"] == "empty" and md41["myday_today_n"] == "0" and md41["myday_tom_n"] == "0",
          f"41_myday_empty: configured, nothing on either day (state={md41['myday_state']})")
    check(md41["myday_fresh"] == "Updated 7:02 AM" and md41["myday_nextup"] == "",
          f"41_myday_empty: freshness line still shown, no NEXT tag (fresh={md41['myday_fresh']}, nextup={md41['myday_nextup']!r})")
    md42 = dump("42_myday_unconfigured")
    check(md42["myday_state"] == "unconfigured",
          f"42_myday_unconfigured: no calendar link state (state={md42['myday_state']})")
    check(md42["myday_fresh"] == "" and md42["myday_nextup"] == "",
          f"42_myday_unconfigured: no freshness line without a link (fresh={md42['myday_fresh']!r})")

    # Iter-17: the three new states/views of the same page. Stale:
    # the populated page at 3:45 PM still showing the 7:02 AM
    # fetch. Never: a link exists but no fetch has ever landed,
    # its own state, which must not claim the days are empty.
    # Late: the same 3:45 PM clock with a fresh 3:02 PM fetch.
    # Iter-18: the stale line steps up and says its age ("Calendar
    # is 8 hours old"); the afternoon statuses are DONE on the
    # finished story time, NOW on soccer (3:30-4:30, in progress
    # at 3:45), NEXT on the Grandma call; the never state has no
    # freshness footer at all, its body copy carries it.
    md43 = dump("43_myday_stale")
    check(md43["myday_state"] == "events" and md43["myday_today_n"] == "2" and md43["myday_tom_n"] == "0",
          f"43_myday_stale: populated data under the stale stamp, the banner eats a row (state={md43['myday_state']}, {md43['myday_today_n']}/{md43['myday_tom_n']})")
    check(md43["myday_more_today"] == "2" and md43["myday_more_tom"] == "2",
          f"43_myday_stale: the fold counts 2 and 2 under the banner ({md43['myday_more_today']}/{md43['myday_more_tom']})")
    check(md43["myday_fresh"] == "Calendar is 8 hours old · from 7:02 AM",
          f"43_myday_stale: freshness line in its stale form, age spelled out ({md43['myday_fresh']})")
    check(md43["myday_nextup"] == "Video call with Grandma" and md43["myday_nowup"] == "Cleo's soccer practice"
          and md43["myday_done"] == "Lila's library story time" and md43["myday_dimmed"] == "Lila's library story time",
          f"43_myday_stale: Grandma next, soccer now, story time done (nextup={md43['myday_nextup']!r}, nowup={md43['myday_nowup']!r}, done={md43['myday_done']!r})")
    check(md43["myday_icons"] == "camera,book" and md43["myday_half"] == "0",
          f"43_myday_stale: the visible rows' icons under the stale stamp (icons={md43['myday_icons']}, half={md43['myday_half']})")
    md44 = dump("44_myday_never")
    check(md44["myday_state"] == "never",
          f"44_myday_never: configured but never loaded state (state={md44['myday_state']})")
    check(md44["myday_fresh"] == "" and md44["myday_today_n"] == "0"
          and md44["myday_tom_n"] == "0" and md44["myday_nextup"] == "",
          f"44_myday_never: no freshness footer, no rows (fresh={md44['myday_fresh']!r})")
    md45 = dump("45_myday_late")
    check(md45["myday_state"] == "events" and md45["myday_fresh"] == "Updated 3:02 PM",
          f"45_myday_late: populated with a fresh afternoon fetch (state={md45['myday_state']}, fresh={md45['myday_fresh']})")
    check(md45["myday_nextup"] == "Video call with Grandma",
          f"45_myday_late: NEXT tag on the Grandma call ({md45['myday_nextup']})")
    check(md45["myday_nowup"] == "Cleo's soccer practice",
          f"45_myday_late: NOW tag on soccer practice, in progress at 3:45 ({md45['myday_nowup']})")
    check(md45["myday_done"] == "Lila's library story time" and md45["myday_dimmed"] == "Lila's library story time",
          f"45_myday_late: story time DONE and dimmed as past (done={md45['myday_done']!r})")
    check(md45["myday_tom1"] == "4:00-4:30 PM|Cleo's piano lesson|",
          f"45_myday_late: tomorrow rows unchanged in the afternoon ({md45['myday_tom1']})")
    check(md45["myday_icons"] == "camera,book,soccer" and md45["myday_half"] == "0",
          f"45_myday_late: the visible rows' icons in the afternoon (icons={md45['myday_icons']}, half={md45['myday_half']})")
    # Iter-29: the fall-back weekend (clock Sat Oct 31 2026, 8:05 AM;
    # tomorrow is Sun Nov 1, when America/New_York repeats 1 AM).
    # The fixture's TZID times resolve through its VTIMEZONE block,
    # so these strings pin the stored instants, not just the text:
    # a fixed -4 reading would put the pancakes at 4:30 AM, a UTC
    # reading at 8:30 AM, and the owl walk on the wrong hour too.
    md46 = dump("46_myday_dst")
    check(md46["myday_state"] == "events" and md46["myday_today_n"] == "2" and md46["myday_tom_n"] == "0",
          f"46_myday_dst: 2 rows today, tomorrow's 3 folded over the fall-back (state={md46['myday_state']}, {md46['myday_today_n']}/{md46['myday_tom_n']})")
    check(md46["myday_more_tom"] == "3" and md46["myday_icons"] == "star,videocall",
          f"46_myday_dst: the fold counts tomorrow's 3 (more={md46['myday_more_tom']}, icons={md46['myday_icons']})")
    check(md46["myday_today0"] == "10:00-11:00 AM|Pumpkin patch visit|",
          f"46_myday_dst: today's first row (today0={md46['myday_today0']!r})")
    check(md46["myday_today1"] == "1:00-1:45 PM|Video call with Grandpa|",
          f"46_myday_dst: the UTC call lands at 1 PM EDT today ({md46['myday_today1']})")
    check(md46["myday_tom0"] == "12:30-1:00 AM|Moonlight owl walk|",
          f"46_myday_dst: 12:30 AM tomorrow is still EDT ({md46['myday_tom0']})")
    check(md46["myday_tom1"] == "1:30-2:30 AM|Fall-back pyjama jam|",
          f"46_myday_dst: the repeated 1:30 resolves to its first (EDT) pass ({md46['myday_tom1']})")
    check(md46["myday_tom2"] == "3:30-4:30 AM|Sunrise pancake breakfast|",
          f"46_myday_dst: 3:30 AM tomorrow is EST ({md46['myday_tom2']})")
    check(md46["myday_tom_date"] == "Sun, Nov 1" and md46["myday_nextup"] == "Pumpkin patch visit"
          and md46["myday_nowup"] == "" and md46["myday_done"] == "",
          f"46_myday_dst: tomorrow is Nov 1, the pumpkin visit is NEXT, nothing NOW or DONE at 8:05 AM (date={md46['myday_tom_date']}, nextup={md46['myday_nextup']!r})")
    check(md46["myday_fresh"] == "Updated 7:02 AM",
          f"46_myday_dst: freshness line shows the pinned fetch time ({md46['myday_fresh']})")

    # Iter-35: the stress and evening fixtures. Stress (9:20 AM):
    # six events today (an all-day, a DONE naming both girls with
    # a long street address in the feed, a NOW, a NEXT with a
    # 38-char title), one tomorrow. Iter-36: the shorter
    # full-width cards fit three, the rest fold.
    md47 = dump("47_myday_stress")
    check(md47["myday_state"] == "events" and md47["myday_today_n"] == "3" and md47["myday_tom_n"] == "0",
          f"47_myday_stress: populated, 3 cards shown of 7 (state={md47['myday_state']}, {md47['myday_today_n']}/{md47['myday_tom_n']})")
    check(md47["myday_more_today"] == "3" and md47["myday_more_tom"] == "1",
          f"47_myday_stress: the fold counts 3 today, 1 tomorrow ({md47['myday_more_today']}/{md47['myday_more_tom']})")
    check(md47["myday_today0"] == "ALL DAY|Autumn classroom photo retake day|",
          f"47_myday_stress: the all-day row ({md47['myday_today0']})")
    check(md47["myday_today1"] == "7:30-8:15 AM|Cleo and Lila dentist checkup and cleaning|",
          f"47_myday_stress: the dentist row (the feed's long address no longer prints on the card, iter-36) ({md47['myday_today1']})")
    check(md47["myday_today3"] == "10:00-11:00 AM|Cleo's reading buddy celebration party|",
          f"47_myday_stress: the 38-char title row ({md47['myday_today3']})")
    check(md47["myday_done"] == "Cleo and Lila dentist checkup and cleaning"
          and md47["myday_nowup"] == "Lila's occupational therapy evaluation"
          and md47["myday_nextup"] == "Cleo's reading buddy celebration party",
          f"47_myday_stress: DONE, NOW and NEXT at 9:20 (done={md47['myday_done']!r}, now={md47['myday_nowup']!r}, next={md47['myday_nextup']!r})")
    check(md47["myday_icons"] == "camera,tooth,star",
          f"47_myday_stress: the visible rows' icons ({md47['myday_icons']})")
    # Evening (6:30 PM): three of today's events are finished and
    # fold into the done line; tonight's call and one of
    # tomorrow's two events show.
    md48 = dump("48_myday_evening")
    check(md48["myday_state"] == "events" and md48["myday_done_line"] == "3 done today",
          f"48_myday_evening: the evening collapse line (done_line={md48['myday_done_line']!r})")
    check(md48["myday_today_n"] == "1" and md48["myday_tom_n"] == "1" and md48["myday_more_tom"] == "1",
          f"48_myday_evening: tonight's call and one tomorrow row show (today={md48['myday_today_n']}, tom={md48['myday_tom_n']}, more_tom={md48['myday_more_tom']})")
    check(md48["myday_today0"] == "7:00-7:45 PM|Video call with Grandma|"
          and md48["myday_nextup"] == "Video call with Grandma",
          f"48_myday_evening: the call is tonight's row and carries NEXT ({md48['myday_today0']})")
    check(md48["myday_done"] == "Cleo's morning swim lesson;Lila's library story time;Cleo's soccer practice",
          f"48_myday_evening: the three folded titles are the finished ones ({md48['myday_done']!r})")
    # Iter-20 dumps: the stale flag the banner and the muting key
    # off, and the outfit the populated page resolved — the
    # message states must resolve the same look (round 16 caught
    # the message states wearing the fall kit on a snow day).
    check(md["myday_stale"] == "0" and md45["myday_stale"] == "0" and md43["myday_stale"] == "1",
          f"myday_stale set only on the stale render (40={md['myday_stale']}, 43={md43['myday_stale']}, 45={md45['myday_stale']})")
    # Iter-30: the look the brief morning resolves to is kit2 on its
    # date (2026-10-27), so the states are pinned to the outfit
    # page's own resolution rather than the literal kit.
    for name, d in (("40_myday", md), ("41_myday_empty", md41), ("42_myday_unconfigured", md42),
                    ("43_myday_stale", md43), ("44_myday_never", md44), ("45_myday_late", md45)):
        check(d["myday_cat_look"] == dumps["20_morning"]["look_name"],
              f"{name}: Pippa wears the day's snow look in every state (myday_cat_look={d['myday_cat_look']!r})")

    # Pixels: the agenda is its own page (not the forecast), its
    # band carries the rows, Pippa stands at the right of the
    # populated page, and both message states show the full sprite.
    check(sha("40_myday.png") != sha("24_next_days.png"),
          "40_myday differs from 24_next_days (the agenda is its own page)")
    img = Image.open(os.path.join(OUT, "40_myday.png"))
    frac = non_bg_fraction(img, (28, 100, 772, 416))
    check(frac > 0.01, f"40_myday: agenda band carries rows ({frac:.1%} non-background, need > 1%)")
    frac = non_bg_fraction(img, (630, 0, 779, 152))
    check(frac > 0.05, f"40_myday: small Pippa in the header at the right (iter-36) ({frac:.1%} non-background, need > 5%)")
    for name in ("41_myday_empty.png", "42_myday_unconfigured.png", "44_myday_never.png"):
        img = Image.open(os.path.join(OUT, name))
        frac = non_bg_fraction(img, (545, 120, 760, 390))
        check(frac > 0.10, f"{name}: full Pippa beside the message, at the right (iter-21) ({frac:.1%} non-background, need > 10%)")

    # Iter-17 pixels, iter-18 zones, iter-19 geometry, iter-20
    # rules, iter-21 (round 17 + owner + concept) in pixels,
    # iter-22 (round 18) pill margins:
    # - the page title's band is clear, and each section header
    #   keeps visible air before its first card (dense-ink gap);
    # - every tag rides its row's DETAIL line and sits fully
    #   inside its card: pill rects are located by their fill
    #   colour and their fill margins are measured from the
    #   pill's own edge to the card's OUTER border (the scan
    #   walks the card fill and stops at the border), at least
    #   4 px below and to the right (fix 1, owner padding);
    # - the half-card tag attaches to its line's text: a small
    #   fixed gap after the place text's end (iter-22, fix 3;
    #   iter-21 anchored its height to the line, fix 6);
    # - NOW is deep green, NEXT pink, the two never confused;
    # - the DONE row's title stays legible while its TIME is
    #   dimmer than the title;
    # - times read a clear step lighter than titles (concept);
    # - plain rows and the all-day band carry no border line at
    #   all (concept): no daypart/peach line colour on their
    #   edges, the white/peach band itself is the separation;
    # - stale keeps its Today header with the warn banner
    #   directly below it, and NOTHING in the stale list reads
    #   live: the NOW/NEXT pills' saturation is a fraction of
    #   fresh, and the highlighted tints and pills sit at the
    #   muted plain cards' saturation level (parity vs the
    #   muted peach card);
    # - fresh "Updated" stands in the tab-bar row at font_tiny
    #   with normal letter spacing: one line of ink, clear of
    #   the last tab pill, with real word gaps inside it;
    # - the message states put Pippa on the RIGHT (the populated
    #   page's side) with the cat-and-words group balanced
    #   between the title band and the tab bar.
    def is_next_pink(r, g, b):
        return r > 200 and 60 < g < 120 and b > 120

    def is_now_green(r, g, b):
        return abs(r - 30) <= 16 and abs(g - 125) <= 18 and abs(b - 70) <= 16

    def is_cleo_fill(r, g, b):
        return abs(r - 216) <= 12 and abs(g - 204) <= 12 and abs(b - 242) <= 12

    def is_lila_fill(r, g, b):
        return abs(r - 181) <= 12 and abs(g - 223) <= 12 and abs(b - 213) <= 12

    def is_lila_pale(r, g, b):
        # The name tag on a past row rides at 50% over the card.
        return abs(r - 213) <= 8 and abs(g - 230) <= 8 and abs(b - 222) <= 8

    def is_done_fill(r, g, b):
        # Iter-35: the DONE chip (COL_MD_DONE_BG) on the one white
        # card style, as the framebuffer renders it.
        return abs(r - 205) <= 10 and abs(g - 202) <= 10 and abs(b - 205) <= 10

    def is_full_ink(r, g, b):
        return r < 110 and g < 110 and b < 130

    def is_warn_tx(r, g, b):
        return 100 < r < 180 and 40 < g < 110 and b < 60

    def is_warn_bg(r, g, b):
        # Iter-23: the My Day banner's own apricot (COL_MD_BANNER_BG,
        # rendered through the RGB565 framebuffer as (255, 242, 222)),
        # not the app warn pair it used through iter-22. The
        # tolerance is tight: the peach card's corner antialiasing
        # blends through this hue on its way to the page cream, and
        # a loose match counts the card's corners as a banner. The
        # banner's own interior is the exact solid colour.
        return abs(r - 255) <= 3 and abs(g - 242) <= 3 and abs(b - 222) <= 3

    def is_saturated(r, g, b):
        return max(r, g, b) - min(r, g, b) > 60

    def is_tab_ink(r, g, b):
        return r < 150 and g < 150 and b < 170

    def is_tile_line(r, g, b):
        return abs(r - 231) <= 12 and abs(g - 217) <= 12 and abs(b - 190) <= 12

    def is_peach_line(r, g, b):
        return abs(r - 240) <= 10 and abs(g - 205) <= 10 and abs(b - 158) <= 10

    def is_white(r, g, b):
        return r >= 253 and g >= 253 and b >= 253

    def ink_rows(img, box, pred):
        px = img.load()
        rows = [y for y in range(box[1], box[3])
                if any(pred(*px[x, y]) for x in range(box[0], box[2]))]
        return rows

    def mean_sat(img, box):
        px = img.load()
        vals = [max(px[x, y]) - min(px[x, y])
                for y in range(box[1], box[3]) for x in range(box[0], box[2])]
        return sum(vals) / len(vals)

    def ink_stats(img, box):
        # (pixels darker than 150, mean luminance of the ink)
        px = img.load()
        tot = 0
        dark = 0
        lsum = 0.0
        for y in range(box[1], box[3]):
            for x in range(box[0], box[2]):
                r, g, b = px[x, y]
                lum = 0.299 * r + 0.587 * g + 0.114 * b
                if lum < 200:
                    tot += 1
                    lsum += lum
                    if lum < 150:
                        dark += 1
        return dark, (lsum / tot if tot else 255.0)

    def pill_rect(img, rgb, tol, box):
        # The bbox of a pill's exact fill colour inside a zone:
        # (left, top, right, bottom, count) or None.
        px = img.load()
        pts = [(x, y) for y in range(box[1], box[3]) for x in range(box[0], box[2])
               if abs(px[x, y][0] - rgb[0]) <= tol and abs(px[x, y][1] - rgb[1]) <= tol
               and abs(px[x, y][2] - rgb[2]) <= tol]
        if not pts:
            return None
        xs = [p[0] for p in pts]
        ys = [p[1] for p in pts]
        return (min(xs), min(ys), max(xs), max(ys), len(pts))

    def dense_bottom(img, box, pred, minpx):
        # The last row of the box carrying at least minpx matches:
        # a header's real ink bottom, ignoring a descender's faint
        # one-or-two-pixel tip.
        px = img.load()
        last = None
        for y in range(box[1], box[3]):
            if sum(1 for x in range(box[0], box[2]) if pred(*px[x, y])) >= minpx:
                last = y
        return last

    img = Image.open(os.path.join(OUT, "40_myday.png"))
    img43 = Image.open(os.path.join(OUT, "43_myday_stale.png"))
    img45 = Image.open(os.path.join(OUT, "45_myday_late.png"))

    # Iter-35: the flex design's pixel checks. The old block
    # measured the absolute-positioned design (pills on detail
    # lines, half cards, tinted card styles); the layout gate in
    # the sim (ui_test_layout_check) now carries the structural
    # proof, so these checks pin the new design's visible facts:
    # one white bordered card style, pills on line 1 only, the
    # fold line inside the list, and the DONE row's legibility.
    img47 = Image.open(os.path.join(OUT, "47_myday_stress.png"))
    img48 = Image.open(os.path.join(OUT, "48_myday_evening.png"))

    # The title's own band: ink bottom vs the Today accent bar's
    # top, plus a hard zero on accent pink inside the band.
    for name, im in (("40_myday", img), ("45_myday_late", img45)):
        rows = ink_rows(im, (20, 0, 200, 48), is_full_ink)
        bar = ink_rows(im, (20, 44, 44, 90),
                       lambda r, g, b: abs(r - 240) <= 25 and abs(g - 70) <= 30 and abs(b - 120) <= 30)
        gap = bar[0] - rows[-1] - 1 if rows and bar else -1
        check(gap >= 4, f"{name}: clear air between the title ink and the Today bar ({gap} px, need >= 4)")
    for name, im in (("40_myday", img), ("43_myday_stale", img43), ("45_myday_late", img45)):
        n = count_where(im, (0, 0, 640, 50),
                        lambda r, g, b: abs(r - 240) <= 25 and abs(g - 70) <= 30 and abs(b - 120) <= 30)
        check(n == 0, f"{name}: no accent bar strikes through the title band ({n} px, need 0)")

    # One card style: white fill inside every visible card, and
    # the border line present along a card's top edge.
    for name, im, zone in (("40_myday", img, (350, 96, 480, 158)),
                           ("40_myday", img, (500, 300, 580, 340)),
                           ("45_myday_late", img45, (350, 96, 480, 158))):
        n = count_where(im, zone, is_white)
        check(n > (zone[2] - zone[0]) * (zone[3] - zone[1]) * 0.9,
              f"{name}: card interior is the one white style ({n} white px in {zone})")
    n = count_where(img, (100, 85, 500, 90), is_tile_line)
    check(n > 100, f"40_myday: the card border line runs along the first card's top ({n} line px, need > 100)")

    # Pills live on line 1 only: NEXT and LILA on the story row's
    # first line, CLEO on the soccer row's first line, and no pill
    # colours anywhere on the title lines under them.
    n = count_where(img, (560, 184, 762, 214), is_next_pink)
    check(n > 400, f"40_myday: NEXT pill on the story row's line 1 ({n} pink px, need > 400)")
    n = count_where(img, (560, 184, 762, 214), is_lila_fill)
    check(n > 400, f"40_myday: LILA tag on the story row's line 1 ({n} fill px, need > 400)")
    n = count_where(img, (106, 215, 762, 246), is_next_pink) + count_where(img, (106, 215, 762, 246), is_lila_fill)
    check(n == 0, f"40_myday: no pills on the story row's title line ({n} px, need 0)")
    n = count_where(img, (560, 273, 762, 303), is_cleo_fill)
    check(n > 400, f"40_myday: CLEO tag on the soccer row's line 1 ({n} fill px, need > 400)")
    n = count_where(img45, (560, 184, 762, 214), is_done_fill)
    check(n > 400, f"45_myday_late: DONE pill on the story row's line 1 ({n} fill px, need > 400)")
    n = count_where(img45, (560, 273, 762, 303), is_now_green)
    check(n > 400, f"45_myday_late: NOW pill in deep green on the soccer row's line 1 ({n} green px, need > 400)")

    # DONE readability: the finished row's title keeps a floor of
    # dark ink, in the lighter DONE colour (a higher mean than
    # the same title in the morning), never faded to the card.
    dark40, mean40 = ink_stats(img, (106, 214, 470, 248))
    dark45, mean45 = ink_stats(img45, (106, 214, 470, 248))
    check(dark45 > 800, f"45_myday_late: DONE title still legible ({dark45} dark px, need > 800)")
    check(mean45 > mean40 + 15,
          f"45_myday_late: DONE title steps down to the lighter ink (mean {mean45:.0f} vs morning {mean40:.0f})")

    # One medallion per visible card, drawn at 48 px.
    for what, zone in (("picture day", (44, 95, 104, 160)),
                       ("story time", (44, 182, 104, 248)),
                       ("soccer", (44, 271, 104, 362))):
        n = count_where(img, zone, is_saturated)
        check(n > 300, f"40_myday: medallion present, {what} ({n} saturated px, need > 300)")
        rows = ink_rows(img, zone, is_saturated)
        check(40 <= len(rows) <= 52, f"40_myday: medallion stands 48 px tall, {what} ({len(rows)} saturated rows; the disc's pale rim rows are not saturated)")

    # The fold line lives inside the list: its ink is present
    # above the list's bottom edge, and the 12 px band between
    # the list and the tab row stays empty.
    frac = non_bg_fraction(img, (28, 352, 240, 384))
    check(frac > 0.005, f"40_myday: the '+3 more' line inks inside the list ({frac:.1%} non-background, need > 0.5%)")
    n = count_where(img, (28, 413, 592, 423), is_full_ink)
    check(n == 0, f"40_myday: nothing inks the band above the tab row ({n} px, need 0)")

    # Stale: the warn banner inks the top of the list, and the
    # cards mute off the one white style.
    n = count_where(img43, (28, 86, 500, 118), is_warn_bg)
    check(n > 800, f"43_myday_stale: the stale banner inks its band ({n} banner px, need > 800)")
    p43 = img43.convert("RGB").getpixel((560, 250))
    p45 = img45.convert("RGB").getpixel((560, 250))
    lum43 = (p43[0] + p43[1] + p43[2]) / 3
    lum45 = (p45[0] + p45[1] + p45[2]) / 3
    check(lum43 < lum45 - 8, f"43_myday_stale: card fill muted vs fresh (lum {lum43:.0f} vs {lum45:.0f})")

    # Stress (iter-36): the dentist title inks its one line, and
    # the band under it stays empty: no second title line and no
    # location line (locations no longer print on cards). The
    # evening collapse line inks above tonight's card.
    n = count_where(img47, (106, 213, 700, 247), is_full_ink)
    check(n > 200, f"47_myday_stress: the dentist title inks its one line ({n} px, need > 200)")
    n = count_where(img47, (106, 246, 758, 260), is_full_ink)
    check(n == 0, f"47_myday_stress: no second line under the dentist title ({n} px, need 0)")
    frac = non_bg_fraction(img48, (28, 116, 300, 150))
    check(frac > 0.01, f"48_myday_evening: the '3 done today' line inks ({frac:.1%} non-background, need > 1%)")

    # Fresh "Updated" rides the tab-bar row at the right, one
    # line of small ink clear of the last tab pill.
    n = count_where(img, (620, 430, 795, 474), is_tab_ink)
    check(n > 150, f"40_myday: freshness ink in the tab-bar row ({n} px, need > 150)")
    for name, im in (("43_myday_stale", img43), ("45_myday_late", img45)):
        n = count_where(im, (630, 44, 772, 84), is_warn_bg)
        check(n == 0, f"{name}: no weather-stale chip, only the calendar can be stale ({n} px, need 0)")
    img44 = Image.open(os.path.join(OUT, "44_myday_never.png"))
    # The zone starts below the message Pippa's boot shadow; a
    # freshness line, if one were drawn, would ink this band.
    frac = non_bg_fraction(img44, (28, 392, 790, 422))
    check(frac < 0.005, f"44_myday_never: footer zone empty, no freshness line ({frac:.2%} non-background, need < 0.5%)")



    # Message states (fix 5): Pippa on the RIGHT like the
    # populated page, words on the left, and the cat's ink
    # balanced between the title band and the tab bar.
    for name in ("41_myday_empty.png", "42_myday_unconfigured.png", "44_myday_never.png"):
        im = Image.open(os.path.join(OUT, name))
        fr = non_bg_fraction(im, (520, 85, 790, 395))
        fl = non_bg_fraction(im, (20, 100, 420, 400))
        check(fr > 0.08 and fl < fr,
              f"{name}: Pippa stands at the right in the message states ({fr:.1%} right vs {fl:.1%} left non-background)")
        rows = ink_rows(im, (520, 54, 795, 424),
                        lambda r, g, b: abs(r - 251) + abs(g - 246) + abs(b - 238) > 40)
        bal = abs((rows[0] - 54) - (423 - rows[-1])) if rows else 99
        check(rows and bal <= 8,
              f"{name}: message group vertically balanced (air above {rows[0] - 54 if rows else -1} px, below {423 - rows[-1] if rows else -1} px)")

    # Pippa one size (iter-24, round 20, fix 5): the same drawn
    # height in the populated and message states, so she never
    # visibly grows or shrinks switching between them. Iter-36
    # supersedes this on the populated page only: she is the
    # small header cat there now, while the message states keep
    # the full-size cat, so the two heights differ on purpose.
    def cat_ink_h(im, box):
        rows = ink_rows(im, box, lambda r, g, b: abs(r - 251) + abs(g - 246) + abs(b - 238) > 40)
        return (rows[-1] - rows[0] + 1) if rows else 0

    hp = cat_ink_h(img, (615, 0, 795, 155))
    img41p = Image.open(os.path.join(OUT, "41_myday_empty.png"))
    hm = cat_ink_h(img41p, (520, 54, 795, 424))
    print(f"  Pippa ink height: populated {hp}, message {hm}")
    check(120 <= hp <= 170 and hm >= 250,
          f"41_myday_empty: populated Pippa is the small header cat, message states keep the full cat (iter-36) (populated {hp} vs message {hm} ink rows)")

    # Iter-25: the outfit rotation, asserted on the dumps. The six
    # dress-band Todays (2026-11-08..13, identical weather) walk the
    # band in candidate order, each Tomorrow shows the NEXT day's
    # pick (the seed is the outfit's date, not the viewing date), and
    # the tiles name the day's dress. The warmcoat morning is the dry
    # warm-coat tier's rotation pick: its own tiles, no chips (the
    # sprite wears no hat or gloves).
    ROT_CANDS = ["dress", "dress_lavender", "dress_mint", "dress_mustard", "uniform_dress"]
    ROT_WORDS = {"dress": "Dress", "dress_lavender": "Lavender dress", "dress_mint": "Mint dress",
                 "dress_mustard": "Mustard dress", "uniform_dress": "Logo dress"}
    for i, name in enumerate(("50_rot_nov08", "51_rot_nov09", "52_rot_nov10",
                              "53_rot_nov11", "54_rot_nov12", "55_rot_nov13")):
        want = ROT_CANDS[i % 5]
        d = dumps[name]
        check(d["look_name"] == want,
              f"{name}: rotation pick for 2026-11-{8 + i:02d} is {want} (look={d['look_name']})")
        check(d["tiles"] == f"{ROT_WORDS[want]}|Light jacket|Tights|Dress shoes",
              f"{name}: tiles name the day's dress ({d['tiles']})")
        t = dumps[name + "b_tomorrow"]
        want_t = ROT_CANDS[(i % 5 + 1) % 5]
        check(t["look_name"] == want_t,
              f"{name}b_tomorrow: tomorrow's pick steps to {want_t} (look={t['look_name']})")
    d = dumps["56_warmcoat"]
    check(d["look_name"] == "warmcoat",
          f"56_warmcoat: dry warm-coat tier rotates to the warmcoat look (look={d['look_name']})")
    check(d["tiles"] == "Long sleeves|Long pants|Warm coat|Warm boots",
          f"56_warmcoat: warmcoat tiles are its own garments ({d['tiles']})")
    check(d["chips"] == "",
          f"56_warmcoat: no chips, the sprite wears no hat or gloves (chips={d['chips']})")
    check("warm coat" in d["advice"],
          f"56_warmcoat: the advice names the warm coat ({d['advice']})")

    # Iter-27: the hot band rotation. Two consecutive hot days
    # (2027-07-13..14, identical weather): the first pick is the
    # standard hot look, the second the Watchung logo hot look, and
    # the first day's Tomorrow already shows the logo pick. The
    # prescription does NOT change on the logo day: the tiles are
    # the hot outfit's own and both chips (sunglasses, water bottle)
    # are prescribed and named, even though the logo sprite wears
    # neither (the owner's explicit relaxation). Only the sprite
    # differs, proven by the stage crops differing.
    d = dumps["57_rot_jul13"]
    check(d["look_name"] == "hot",
          f"57_rot_jul13: rotation pick for 2027-07-13 is the standard hot look (look={d['look_name']})")
    t = dumps["57_rot_jul13b_tomorrow"]
    check(t["look_name"] == "uniform_hot",
          f"57_rot_jul13b_tomorrow: tomorrow's pick steps to the logo hot look (look={t['look_name']})")
    d = dumps["58_rot_jul14"]
    check(d["look_name"] == "uniform_hot",
          f"58_rot_jul14: rotation pick for 2027-07-14 is the logo hot look (look={d['look_name']})")
    for name in ("57_rot_jul13", "57_rot_jul13b_tomorrow", "58_rot_jul14"):
        dd = dumps[name]
        check(dd["tiles"] == "Short sleeves|Shorts|Sneakers",
              f"{name}: hot tiles unchanged by the rotation ({dd['tiles']})")
        check(dd["chips"] == "Sunglasses|Water bottle",
              f"{name}: both chips prescribed, logo day included ({dd['chips']})")
        check("sunglasses" in dd["advice"].lower() and "water bottle" in dd["advice"].lower(),
              f"{name}: the advice names both chips ({dd['advice']})")
    check(stage_sha("57_rot_jul13.png") != stage_sha("58_rot_jul14.png"),
          "stage crops differ: the logo hot sprite is not the standard hot sprite")

    # Iter-30: the warm tier's hat-day rotation. Two consecutive dry
    # warm-coat-tier mornings that prescribe the hat (2026-12-13..14,
    # identical weather): the first pick is the winter kit, the
    # second steps to the next hat-band entry. Iter-35 reordered
    # the band (coats alternate, shirt kinds interleave), so Dec 14
    # now picks the red-coat navy SOLID shirt (band index 1) where
    # iter-33 picked kit2. The prescription does NOT change between
    # the two days: same tiles by name (the coat tile still reads
    # "Winter coat"), same hat and gloves chips; what differs is
    # the sprite on stage and the coat tile's PICTURE (the red coat
    # icon replaces the pink kit coat), proven on the rendered
    # pixels of the coat tile's icon box (third tile of the 2x2
    # grid: tile at (284,216), icon at tile + (10,4), 96 px).
    def tile_icon_sha(name, box):
        img = Image.open(os.path.join(OUT, name)).crop(box)
        return hashlib.sha256(img.tobytes()).hexdigest()

    COAT_ICON_BOX = (294, 220, 390, 316)
    d = dumps["59_kit_dec13"]
    check(d["look_name"] == "snow",
          f"59_kit_dec13: rotation pick for 2026-12-13 is the winter kit (look={d['look_name']})")
    t = dumps["59_kit_dec13b_tomorrow"]
    check(t["look_name"] == "kitr_navy_solid",
          f"59_kit_dec13b_tomorrow: tomorrow's pick steps to the red-coat navy solid (look={t['look_name']})")
    d = dumps["60_kit2_dec14"]
    check(d["look_name"] == "kitr_navy_solid",
          f"60_kit2_dec14: rotation pick for 2026-12-14 is the red-coat navy solid (look={d['look_name']})")
    for name in ("59_kit_dec13", "59_kit_dec13b_tomorrow", "60_kit2_dec14", "61_kit2_pink"):
        dd = dumps[name]
        check(dd["tiles"] == "Long sleeves|Long pants|Winter coat|Warm boots",
              f"{name}: kit tiles unchanged by the rotation ({dd['tiles']})")
        check(dd["chips"] == "Warm hat|Gloves",
              f"{name}: hat and gloves chips prescribed ({dd['chips']})")
        check("winter coat" in dd["advice"],
              f"{name}: the advice names the winter coat ({dd['advice']})")
    check(stage_sha("59_kit_dec13.png") != stage_sha("60_kit2_dec14.png"),
          "stage crops differ: the red-coat solid sprite is not the kit sprite")
    check(tile_icon_sha("59_kit_dec13.png", COAT_ICON_BOX) != tile_icon_sha("60_kit2_dec14.png", COAT_ICON_BOX),
          "coat tile pictures differ: the red-coat day shows the red coat icon, the kit day the pink one")
    d = dumps["61_kit2_pink"]
    check(d["look_name"] == "snow",
          f"61_kit2_pink: the pink colourway renders the kit day (look={d['look_name']})")
    img = Image.open(os.path.join(OUT, "61_kit2_pink.png"))
    n_pink = sum(1 for px in img.crop(STAGE).getdata() if is_pink_fur(*px[:3]))
    check(n_pink > 400,
          f"61_kit2_pink: pink fur on stage ({n_pink} px, need > 400)")
    # The hat-free control: the warmcoat morning's seed would land
    # on a hat-band look if the rotation ignored the hat gate; it
    # must stay the warmcoat (the hat looks wear a hat the day
    # never asked for).
    check(dumps["56_warmcoat"]["look_name"] == "warmcoat",
          "56_warmcoat: hat-free warm-tier day never picks a hat-band look")

    # Iter-33/35: the winter-variant hat days. Iter-35's reorder
    # remapped the December dates (seed = the outfit date mod 16
    # over the new band): Dec 11 -> kitp_navy_stripe (14),
    # Dec 12 -> kitr_teal_solid (15), Dec 15 -> kitp_mustard_stripe
    # (2), Dec 16 -> kitr_dress (3), Dec 21 -> kitp_rust_stripe
    # (8). The shirt variants keep the kit's prescription whole
    # (same tiles by name, same hat and gloves chips); the coat
    # tile's picture follows the coat colour on both sides of the
    # reorder. The dress variant swaps the dress tile in for Long
    # pants and drops the Long sleeves tile: three tiles, the
    # dress named by colour, Warm boots last; its advice names
    # the dress. Dec 16 is the dress day now, and the pink-choice
    # render of Dec 11 must be the SAME black sprite on stage
    # (the batch is black fur only; the pink slot aliases it).
    d = dumps["62_hat_kitp_shirt"]
    check(d["look_name"] == "kitp_mustard_stripe",
          f"62_hat_kitp_shirt: rotation pick for 2026-12-15 is the pink-coat mustard-stripe kit (look={d['look_name']})")
    d = dumps["63_hat_kitr_shirt"]
    check(d["look_name"] == "kitp_rust_stripe",
          f"63_hat_kitr_shirt: rotation pick for 2026-12-21 is the pink-coat rust-stripe kit (look={d['look_name']})")
    d = dumps["64_hat_kitp_dress"]
    check(d["look_name"] == "kitp_navy_stripe",
          f"64_hat_kitp_dress: rotation pick for 2026-12-11 is the pink-coat navy-stripe kit (look={d['look_name']})")
    t = dumps["64b_hat_kitr_dress_tomorrow"]
    check(t["look_name"] == "kitr_teal_solid",
          f"64b_hat_kitr_dress_tomorrow: tomorrow's pick steps to the red-coat teal solid (look={t['look_name']})")
    for name in ("62_hat_kitp_shirt", "63_hat_kitr_shirt", "64_hat_kitp_dress",
                 "64b_hat_kitr_dress_tomorrow", "65_hat_kitp_dress_pink"):
        dd = dumps[name]
        check(dd["tiles"] == "Long sleeves|Long pants|Winter coat|Warm boots",
              f"{name}: shirt variants keep the kit tiles ({dd['tiles']})")
        check(dd["chips"] == "Warm hat|Gloves",
              f"{name}: hat and gloves chips prescribed ({dd['chips']})")
        check("winter coat" in dd["advice"],
              f"{name}: the advice names the winter coat ({dd['advice']})")
    check(tile_icon_sha("62_hat_kitp_shirt.png", COAT_ICON_BOX) == tile_icon_sha("59_kit_dec13.png", COAT_ICON_BOX),
          "coat tile pictures match: the pink-coat shirt day shows the kit's own coat icon")
    check(tile_icon_sha("64b_hat_kitr_dress_tomorrow.png", COAT_ICON_BOX) == tile_icon_sha("60_kit2_dec14.png", COAT_ICON_BOX),
          "coat tile pictures match: two red-coat shirt days share the red coat icon")
    check(stage_sha("62_hat_kitp_shirt.png") != stage_sha("59_kit_dec13.png"),
          "stage crops differ: the mustard-stripe shirt sprite is not the kit sprite")
    check(stage_sha("63_hat_kitr_shirt.png") != stage_sha("60_kit2_dec14.png"),
          "stage crops differ: the rust-stripe shirt sprite is not the red-coat solid sprite")
    check(stage_sha("62_hat_kitp_shirt.png") != stage_sha("63_hat_kitr_shirt.png"),
          "stage crops differ: the mustard-stripe and rust-stripe sprites differ")
    t = dumps["62b_hat_kitp_shirt_tomorrow"]
    check(t["look_name"] == "kitr_dress",
          f"62b_hat_kitp_shirt_tomorrow: tomorrow's pick steps to the mint dress under the red coat (look={t['look_name']})")
    check(t["tiles"] == "Mint dress|Winter coat|Warm boots",
          f"62b_hat_kitp_shirt_tomorrow: dress tiles (dress in for Long pants, no Long sleeves) ({t['tiles']})")
    check(t["chips"] == "Warm hat|Gloves",
          f"62b_hat_kitp_shirt_tomorrow: hat and gloves chips stay ({t['chips']})")
    check(t["advice"].startswith("Wear your dress."),
          f"62b_hat_kitp_shirt_tomorrow: the advice names the dress ({t['advice']})")
    check(stage_sha("62b_hat_kitp_shirt_tomorrow.png") != stage_sha("62_hat_kitp_shirt.png"),
          "stage crops differ: the dress sprite is not a shirt sprite")
    d = dumps["65_hat_kitp_dress_pink"]
    check(d["look_name"] == "kitp_navy_stripe",
          f"65_hat_kitp_dress_pink: the pink choice resolves the same look (look={d['look_name']})")
    check(stage_sha("65_hat_kitp_dress_pink.png") == stage_sha("64_hat_kitp_dress.png"),
          "stage crops identical: the pink slot aliases the black sprite (black fur only)")

    if failures:
        print(f"\n{len(failures)} render check(s) FAILED")
        return 1
    print("\nAll render checks passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
