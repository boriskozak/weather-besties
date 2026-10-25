#!/usr/bin/env python3
"""Slices the iter-25 garment icons out of the new-outfits package sheet.

The sheet (art/pippa-new-outfits/icons/pippa-new-garment-icons.png) is a
4x2 grid on a solid white background, generated with the new Pippa
outfits. It is sliced with exactly the method of
tools/crop_garment_sheet.py: the background is removed by CONNECTIVITY
(a pixel is background only if it is near-white AND connected to the
sheet's border through other near-white pixels), each garment is the
connected opaque component(s) whose centroid falls in its grid cell,
and the crop is the garment's own pixels plus a small pad. Interior
whites (the tops' collars and cuffs) survive, enclosed by the garments'
navy outlines.

Cell mapping, read off the sheet against the package preview
(art/pippa-new-outfits/Pippa-outfit-preview.png) and the package
README's own grid description:

    row 1: warm coat (teal puffer) | warm boots (brown, fur-lined)
           mustard top             | navy star top
    row 2: rust top                | lavender dress
           mint dress              | mustard dress

There is no fall-hoodie icon on the sheet: the fall-hoodie look tiles
with the existing hoodie garment, like its fall twin.

One more icon is extracted here, in the sprite-detail tradition of
crop_garment_sheet.py's iter-08 crops: uniform_dress, the Watchung
logo dress, masked out of its own sprite
(art/pippa-watchung-uniforms/pippa-uniform-dress-black.png). The dress
is royal blue; the jacket worn over it is sky blue (much higher green),
so a colour mask isolates the cloth, a closing pass bridges the pleat
shading lines, and a hole fill keeps the W logo, the white caption
ring and the pink waist bow the cloth encloses.

Output: one transparent PNG per garment in tools/garment_sheet/,
named by the icons.py garment key. tools/make_assets.py picks these
up like every other sheet crop. Rerun only when the art changes.

Run:  python3 tools/slice_new_garment_icons.py   (needs pillow, numpy, scipy)
"""
import os
import sys

import numpy as np
from PIL import Image
from scipy import ndimage

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SHEET = os.path.join(ROOT, "art", "pippa-new-outfits", "icons", "pippa-new-garment-icons.png")
UNIFORM_DRESS = os.path.join(ROOT, "art", "pippa-watchung-uniforms", "pippa-uniform-dress-black.png")
OUT = os.path.join(HERE, "garment_sheet")
PAD = 8
MIN_PART_PX = 200   # smaller opaque specks are sheet noise, not garment parts
WHITE_MIN = 248     # a pixel is "near-white" when its darkest channel exceeds this

# (row, col) -> garment key (tools/icons.py GARMENTS / outfit.h garment_t).
CELLS = {
    (0, 0): "warm_coat_teal",
    (0, 1): "brown_boots",
    (0, 2): "mustard_top",
    (0, 3): "navy_top",
    (1, 0): "rust_top",
    (1, 1): "dress_lavender",
    (1, 2): "dress_mint",
    (1, 3): "dress_mustard",
}
COLS, ROWS = 4, 2


def slice_sheet():
    im = Image.open(SHEET).convert("RGB")
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
    cw, ch = w / COLS, h / ROWS
    cell_parts = {}
    for pid in range(1, nparts + 1):
        if sizes[pid - 1] < MIN_PART_PX:
            continue
        cy, cx = cents[pid - 1]
        cell = (min(int(cy // ch), ROWS - 1), min(int(cx // cw), COLS - 1))
        cell_parts.setdefault(cell, []).append(pid)

    os.makedirs(OUT, exist_ok=True)
    for (r, c), key in sorted(CELLS.items()):
        pids = cell_parts.get((r, c))
        if not pids:
            raise SystemExit(f"no garment component found for cell {(r, c)} ({key})")
        mine = np.isin(parts, pids)
        ys, xs = np.nonzero(mine)
        bb = (max(xs.min() - PAD, 0), max(ys.min() - PAD, 0),
              min(xs.max() + 1 + PAD, w), min(ys.max() + 1 + PAD, h))
        rgba = np.dstack([a.astype(np.uint8),
                          np.where(mine, 255, 0).astype(np.uint8)])
        crop = Image.fromarray(rgba, "RGBA").crop(bb)
        crop.save(os.path.join(OUT, key + ".png"))
        print(f"cell {(r, c)} -> {key:16s} crop {crop.width}x{crop.height} "
              f"at ({bb[0]}, {bb[1]}), {len(pids)} part(s), {int(mine.sum())} opaque px")


def slice_uniform_dress():
    im = Image.open(UNIFORM_DRESS).convert("RGBA")
    a = np.asarray(im).astype(int)
    r, g, b, al = a[:, :, 0], a[:, :, 1], a[:, :, 2], a[:, :, 3]
    # The dress cloth, sampled off the sprite: royal blue (0, 40-68,
    # 141-199) -- red channel ~0, blue strong and well above green.
    # The jacket worn over it is sky blue (65, 197, 255): its green is
    # nearly its blue, so the b-g gap excludes it.
    cloth = (al > 200) & (r < 40) & (b > 100) & (b - g > 60)
    # The pleat shading lines break the cloth into strips, and the
    # waist bow splits bodice from skirt; close the small gaps and
    # keep every real cloth component (the dress colour appears
    # nowhere else on the sprite), then fill the holes: that keeps
    # the W logo, its white caption ring and the pink waist bow the
    # cloth encloses.
    closed = ndimage.binary_closing(cloth, structure=np.ones((3, 3)), iterations=3)
    parts, nparts = ndimage.label(closed, structure=np.ones((3, 3)))
    if nparts == 0:
        raise SystemExit("uniform dress mask found no dress cloth")
    sizes = ndimage.sum_labels(closed, parts, range(1, nparts + 1))
    keep = [pid + 1 for pid in range(nparts) if sizes[pid] >= MIN_PART_PX]
    mine = np.isin(parts, keep)
    mine = ndimage.binary_fill_holes(mine)
    # The jacket's royal-blue cuffs touch the skirt's top corners in
    # the flattened raster; they pass the cloth test but are not the
    # dress. Their bridge to the skirt is a few pixels wide: erode,
    # keep the dress's own component, grow back inside the fill.
    eroded = ndimage.binary_erosion(mine, structure=np.ones((3, 3)), iterations=1)
    parts, nparts = ndimage.label(eroded, structure=np.ones((3, 3)))
    sizes = ndimage.sum_labels(eroded, parts, range(1, nparts + 1))
    core = parts == (int(np.argmax(sizes)) + 1)
    regrown = ndimage.binary_dilation(core, structure=np.ones((3, 3)), iterations=1)
    mine = mine & ndimage.binary_dilation(regrown, structure=np.ones((3, 3)), iterations=2)
    mine = ndimage.binary_fill_holes(mine)
    # The dress's own navy outline sits just outside the cloth: grow
    # the fill 2px and keep opaque pixels, minus the sky-blue jacket
    # where the two garments touch at the bodice sides.
    jacket = (al > 200) & (g > 150) & (b > 200)
    grown = ndimage.binary_dilation(mine, structure=np.ones((3, 3)), iterations=2)
    mine = mine | (grown & (al > 200) & ~jacket)
    ys, xs = np.nonzero(mine)
    bb = (max(xs.min() - PAD, 0), max(ys.min() - PAD, 0),
          min(xs.max() + 1 + PAD, im.width), min(ys.max() + 1 + PAD, im.height))
    rgba = np.dstack([a[:, :, :3].astype(np.uint8),
                      np.where(mine, 255, 0).astype(np.uint8)])
    crop = Image.fromarray(rgba, "RGBA").crop(bb)
    crop.save(os.path.join(OUT, "uniform_dress.png"))
    print(f"sprite dress mask -> uniform_dress  crop {crop.width}x{crop.height} "
          f"at ({bb[0]}, {bb[1]}), {int(mine.sum())} opaque px")


def main():
    slice_sheet()
    slice_uniform_dress()
    print(f"new garment icons -> {OUT}")


if __name__ == "__main__":
    sys.exit(main())
