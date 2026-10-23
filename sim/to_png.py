#!/usr/bin/env python3
"""Converts raw RGB565 frames from ./render into PNGs."""
import glob
import os
import sys

from PIL import Image

W, H = 800, 480
d = sys.argv[1] if len(sys.argv) > 1 else "out"
for p in sorted(glob.glob(os.path.join(d, "*.rgb565"))):
    raw = open(p, "rb").read()
    im = Image.frombytes("RGB", (W, H), raw, "raw", "BGR;16")
    im.save(p[:-7] + ".png")
    os.remove(p)
    print("png", p[:-7] + ".png")
