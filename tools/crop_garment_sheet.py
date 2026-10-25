#!/usr/bin/env python3
"""Crops the 20 garment icons out of the wardrobe package's icon sheet.

The sheet (icons/garment-sheet-5x4.png in the wardrobe expansion package)
is a 5x4 grid on a solid white background, and several garments contain
white themselves (sneaker bodies, shirt stripes, snow-boot fur). So the
background is removed by CONNECTIVITY, never by color alone: a pixel is
background only if it is near-white AND connected to the sheet's border
through other near-white pixels (flood fill from the edges). Interior
whites are enclosed by the garments' navy outlines and survive.

Each garment is then isolated as the connected opaque component(s)
whose centroid falls in its grid cell. That keeps genuine multi-part
icons whole (two mittens, two boots, two sneakers) while excluding the
sliver of a neighbouring garment that crosses the nominal cell line:
the neighbour is one component whose centroid sits in its own cell.

Output: one transparent PNG per garment in tools/garment_sheet/, named
by the icons.py garment key, cropped to the garment's own pixels plus a
small pad. tools/make_assets.py picks these up and produces the usual
img_g_<key>_96 / _56 LVGL images from them. Rerun this script only when
the sheet itself changes.

Run:  python3 tools/crop_garment_sheet.py   (needs pillow, numpy, scipy)
"""
import json
import os
import sys

import numpy as np
from PIL import Image
from scipy import ndimage

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
PKG = os.environ.get(
    "WB_WARDROBE_DIR",
    os.path.join(os.path.dirname(ROOT), "pippa-complete", "Pippa", "wardrobe-expansion"))
OUT = os.path.join(HERE, "garment_sheet")
PAD = 8
MIN_PART_PX = 200   # smaller opaque specks are sheet noise, not garment parts

# Sheet name (icon-order.json) -> garment key (tools/icons.py GARMENTS).
# "dress" and "star-badge" have no garment_t entry: the dress is Pippa's
# own outfit, never a tile, and the star badge was retired in favour of
# the words "Extra layer" (panel round 3 fix 1). Both are still cropped
# and reported, just not written as garment icons.
KEYMAP = {
    "long-sleeve-shirt": "long_sleeves",
    "short-sleeve-shirt": "short_sleeves",
    "pants": "long_pants",
    "shorts": "shorts",
    "dress": None,
    "hoodie": "hoodie",
    "light-jacket": "light_jacket",
    "raincoat": "rain_jacket",
    "warm-coat": "warm_coat",
    "winter-coat": "winter_coat",
    "sneakers": "sneakers",
    "rain-boots": "rain_boots",
    "snow-boots": "snow_boots",
    "beanie": "warm_hat",
    "mittens": "gloves",
    "scarf": "scarf",
    "umbrella": "umbrella",
    "sunglasses": "sunglasses",
    "water-bottle": "water_bottle",
    "star-badge": None,
}
WHITE_MIN = 248   # a pixel is "near-white" when its darkest channel exceeds this


def main():
    with open(os.path.join(PKG, "icons", "icon-order.json")) as f:
        order = json.load(f)
    cols, rows, names = order["columns"], order["rows"], order["order"]
    im = Image.open(os.path.join(PKG, "icons", "garment-sheet-5x4.png")).convert("RGB")
    a = np.asarray(im).astype(int)
    h, w = a.shape[:2]

    near_white = a.min(axis=2) > WHITE_MIN
    lab, _ = ndimage.label(near_white)   # 4-connectivity: diagonal leaks don't count
    border = set(np.unique(np.concatenate([lab[0, :], lab[-1, :], lab[:, 0], lab[:, -1]])))
    border.discard(0)
    background = np.isin(lab, list(border))
    opaque = ~background

    parts, nparts = ndimage.label(opaque, structure=np.ones((3, 3)))
    sizes = ndimage.sum_labels(opaque, parts, range(1, nparts + 1))
    cents = ndimage.center_of_mass(opaque, parts, range(1, nparts + 1))
    cw, ch = w / cols, h / rows
    cell_parts = {}
    for pid in range(1, nparts + 1):
        if sizes[pid - 1] < MIN_PART_PX:
            continue
        cy, cx = cents[pid - 1]
        cell = (min(int(cy // ch), rows - 1), min(int(cx // cw), cols - 1))
        cell_parts.setdefault(cell, []).append(pid)

    os.makedirs(OUT, exist_ok=True)
    for i, name in enumerate(names):
        r, c = divmod(i, cols)
        pids = cell_parts.get((r, c))
        if not pids:
            raise SystemExit(f"no garment component found for sheet icon {name}")
        mine = np.isin(parts, pids)
        ys, xs = np.nonzero(mine)
        bb = (max(xs.min() - PAD, 0), max(ys.min() - PAD, 0),
              min(xs.max() + 1 + PAD, w), min(ys.max() + 1 + PAD, h))
        rgba = np.dstack([a.astype(np.uint8),
                          np.where(mine, 255, 0).astype(np.uint8)])
        crop = Image.fromarray(rgba, "RGBA").crop(bb)
        key = KEYMAP[name]
        print(f"{name:18s} -> {key or '(unused)':14s} crop {crop.width}x{crop.height} "
              f"at ({bb[0]}, {bb[1]}), {len(pids)} part(s), {int(mine.sum())} opaque px")
        if key:
            crop.save(os.path.join(OUT, key + ".png"))
    print(f"garment icons -> {OUT}")


if __name__ == "__main__":
    sys.exit(main())
