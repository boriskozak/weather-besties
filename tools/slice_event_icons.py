#!/usr/bin/env python3
"""Slice the My Day event-icon sheet into individual medallion PNGs.

The sheet (design/event-icons-sheet-grok-2026-10-25.jpg, kept outside
the repo) is a 4x4 grid of circular medallions on white, drawn for the
My Day page redesign (iter-19). This tool locates the sixteen circles
by projection (non-white bands on each axis), refines each circle's
centre and radius from the ink through its centre lines, crops it
tightly, and exports it at 96x96 with a circular alpha mask so the
corners are transparent. The crops land in tools/event_icons/<key>.png,
the source of truth that tools/make_assets.py converts into
img_ev_<key> assets (the garment-sheet pattern).

Sheet order, left to right, top to bottom:
camera, book, soccer, videocall, cupcake, piano, school, balloon,
music, palette, swim, snow, gift, star, tooth, paw.

Run: ~/artenv/bin/python tools/slice_event_icons.py [sheet.jpg]
"""
import os
import sys

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
DEFAULT_SHEET = os.path.join(ROOT, "..", "design", "event-icons-sheet-grok-2026-10-25.jpg")
OUT = os.path.join(HERE, "event_icons")
SIZE = 96

KEYS = ["camera", "book", "soccer", "videocall",
        "cupcake", "piano", "school", "balloon",
        "music", "palette", "swim", "snow",
        "gift", "star", "tooth", "paw"]


def nonwhite(px, x, y):
    r, g, b = px[x, y][:3]
    return abs(r - 255) + abs(g - 255) + abs(b - 255) > 60


def bands(sums, thresh):
    out, run = [], None
    for i, n in enumerate(sums):
        if n > thresh and run is None:
            run = i
        if n <= thresh and run is not None:
            out.append((run, i - 1))
            run = None
    if run is not None:
        out.append((run, len(sums) - 1))
    return out


def soften_ring(crop):
    """Recolour a medallion's navy outline ring to a pale tint of
    its own disc (iter-23, round 19: the sheet's dark rings read
    as a sticker-sheet hardness next to the concept's soft,
    near-ringless discs). The ring is found by an angular sweep:
    at every angle the disc's outermost dark run is the ring,
    however a glyph may touch it further in, so the run is capped
    at 5 px and a glyph outline crossing the rim keeps its colour
    below the cap. A fringe pass then blends the ring's
    antialiased inner edge toward the new colour. The disc keeps
    a visible edge against light cards: the tint is the disc
    colour mixed 30% toward white and pulled back 6%, clearly
    darker than the disc, far lighter than the navy."""
    import math
    from collections import Counter

    px = crop.load()
    c = (SIZE - 1) / 2

    def lum(r, g, b):
        return 0.299 * r + 0.587 * g + 0.114 * b

    counts = Counter()
    for y in range(SIZE):
        for x in range(SIZE):
            r, g, b, a = px[x, y]
            if a >= 128 and lum(r, g, b) >= 150 and not (r >= 247 and g >= 247 and b >= 247):
                counts[(r // 16, g // 16, b // 16)] += 1
    bucket = counts.most_common(1)[0][0]
    tot = [0, 0, 0]
    n = 0
    for y in range(SIZE):
        for x in range(SIZE):
            r, g, b, a = px[x, y]
            if a >= 128 and (r // 16, g // 16, b // 16) == bucket:
                tot[0] += r
                tot[1] += g
                tot[2] += b
                n += 1
    tint = [t / n for t in tot]
    ring = tuple(int(round((v + (255 - v) * 0.30) * 0.94)) for v in tint)

    recoloured = set()
    for step in range(720):
        th = step * math.pi / 360
        dx, dy = math.cos(th), math.sin(th)
        run = 0.0
        r = 47.5
        while r >= 38:
            x = int(round(c + dx * r))
            y = int(round(c + dy * r))
            pr, pg, pb, pa = px[x, y]
            if run > 0:
                if lum(pr, pg, pb) < 175 and run <= 5.0:
                    recoloured.add((x, y))
                    run += 0.5
                else:
                    break
            elif pa >= 128 and lum(pr, pg, pb) < 160:
                recoloured.add((x, y))
                run = 0.5
            r -= 0.5
    for x, y in recoloured:
        px[x, y] = ring + (px[x, y][3],)
    for y in range(SIZE):
        for x in range(SIZE):
            r_, g_, b_, a = px[x, y]
            if (x, y) in recoloured or a < 128:
                continue
            if ((x - c) ** 2 + (y - c) ** 2) ** 0.5 < 41.5:
                continue
            if 130 <= lum(r_, g_, b_) < 190:
                px[x, y] = tuple(int(round(v + (t - v) * 0.6)) for v, t in zip((r_, g_, b_), ring)) + (a,)
    return crop


def main():
    sheet = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_SHEET
    im = Image.open(sheet).convert("RGB")
    W, H = im.size
    px = im.load()
    cols = bands([sum(1 for y in range(0, H, 2) if nonwhite(px, x, y)) for x in range(W)], 3)
    rows = bands([sum(1 for x in range(0, W, 2) if nonwhite(px, x, y)) for y in range(H)], 3)
    assert len(cols) == 4 and len(rows) == 4, f"grid not found: {len(cols)} cols, {len(rows)} rows"
    os.makedirs(OUT, exist_ok=True)
    contact = Image.new("RGBA", (4 * SIZE, 4 * SIZE), (251, 246, 238, 255))
    # The projection bands are the circles' bounding boxes: a column
    # (row) through a circle's middle is non-white across the circle's
    # full width, so each band is one circle wide. Centres come from
    # the bands; the diameter is pooled across the grid (the sixteen
    # medallions are congruent, and pooling averages out the JPEG
    # fringe the threshold catches). Walking out from a centre would
    # be fooled by the white inside several icons (book pages, piano
    # keys), so the bands are the measurement, not a first guess.
    diam = sorted([(c[1] - c[0] + 1) for c in cols] + [(r[1] - r[0] + 1) for r in rows])[4]
    for i, key in enumerate(KEYS):
        ccx = (cols[i % 4][0] + cols[i % 4][1]) / 2
        ccy = (rows[i // 4][0] + rows[i // 4][1]) / 2
        d = diam
        side = d + 6                      # 3 px of air around the outline
        box = (round(ccx - side / 2), round(ccy - side / 2),
               round(ccx + side / 2), round(ccy + side / 2))
        crop = im.crop(box).resize((SIZE, SIZE), Image.LANCZOS).convert("RGBA")
        mask = Image.new("L", (SIZE, SIZE), 0)
        r = SIZE / 2 * (d / side) + 1.5   # just outside the navy outline
        ImageDraw.Draw(mask).ellipse((SIZE / 2 - r, SIZE / 2 - r, SIZE / 2 + r, SIZE / 2 + r), fill=255)
        crop.putalpha(mask)
        crop = soften_ring(crop)
        crop.save(os.path.join(OUT, key + ".png"))
        contact.paste(crop, ((i % 4) * SIZE, (i // 4) * SIZE), crop)
        print(f"{key:9s} centre ({ccx:.0f},{ccy:.0f}) diameter {d:.0f}")
    contact.save(os.path.join(OUT, "contact.png"))
    print(f"16 medallions -> {OUT}")


if __name__ == "__main__":
    main()
