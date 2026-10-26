#!/usr/bin/env python3
"""Size budget gates for a finished ESP-IDF build (iter-34).

Two independent cliffs, both checked against the build's own artifacts:

1. PSRAM budget: CONFIG_SPIRAM_XIP_FROM_PSRAM + CONFIG_SPIRAM_RODATA copy
   the app's .flash.text and .flash.rodata into the 8 MB PSRAM at boot.
   What is left afterwards must still hold the RGB panel's framebuffer
   (800x480x2 = 768,000 B), the decoded-image cache and the WiFi/TLS
   working set. Gate: text + rodata <= 5,000,000 B, which leaves
   8,388,608 - 5,000,000 = 3,388,608 B (~3.4 MB) of PSRAM free.
   (b17c1d3 shipped at 7,926,016 B and boot-looped on exactly this:
   esp_lcd_new_rgb_panel could not allocate the framebuffer.)
2. OTA slot budget: the app binary must fit one OTA slot of the
   partition table, 0x7F0000 = 8,323,072 B, or boards cannot install it.

Usage:
    python3 tools/check_size_budget.py [--build DIR] [--map FILE] [--bin FILE]

Defaults: --build is the repo's build/ directory; the map and app bin
are read from it. Exit status is 0 only if both gates pass.
tools/release.py runs this before packaging a release.
"""
import argparse
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

PSRAM_TOTAL = 8 * 1024 * 1024          # 8,388,608 B on the board
TEXT_RODATA_BUDGET = 5_000_000         # gate 1: leaves ~3.4 MB of PSRAM
FRAMEBUFFER = 800 * 480 * 2            # 768,000 B, for the report
OTA_SLOT_SIZE = 0x7F0000               # 8,323,072 B, partitions.csv


def section_sizes(map_path):
    """{.flash.text: bytes, .flash.rodata: bytes} from a GNU ld map."""
    sizes = {}
    with open(map_path, errors="ignore") as f:
        for line in f:
            m = re.match(r"\.(flash\.text|flash\.rodata)\s+0x[0-9a-f]+\s+0x([0-9a-f]+)\s*$",
                         line.rstrip())
            if m:
                sizes["." + m.group(1)] = int(m.group(2), 16)
    return sizes


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--build", default=os.path.join(ROOT, "build"),
                    help="ESP-IDF build directory (default: repo build/)")
    ap.add_argument("--map", default=None, help="linker map (default: <build>/weather_besties.map)")
    ap.add_argument("--bin", default=None, help="app bin (default: <build>/weather_besties.bin)")
    args = ap.parse_args()

    map_path = args.map or os.path.join(args.build, "weather_besties.map")
    bin_path = args.bin or os.path.join(args.build, "weather_besties.bin")
    for p in (map_path, bin_path):
        if not os.path.exists(p):
            sys.exit(f"missing {p}; run idf.py build first")

    sizes = section_sizes(map_path)
    if ".flash.text" not in sizes or ".flash.rodata" not in sizes:
        sys.exit(f"could not find .flash.text/.flash.rodata sizes in {map_path}")
    text, rodata = sizes[".flash.text"], sizes[".flash.rodata"]
    xip = text + rodata
    psram_free = PSRAM_TOTAL - xip
    app_size = os.path.getsize(bin_path)

    print(f".flash.text    {text:>10,} B")
    print(f".flash.rodata  {rodata:>10,} B")
    print(f"text + rodata  {xip:>10,} B  (budget {TEXT_RODATA_BUDGET:,}; "
          f"PSRAM left for framebuffer {FRAMEBUFFER:,}, decode cache and WiFi/TLS: "
          f"{psram_free:,} B)")
    print(f"app binary     {app_size:>10,} B  (OTA slot {OTA_SLOT_SIZE:,})")

    ok = True
    if xip > TEXT_RODATA_BUDGET:
        print(f"FAIL: text + rodata {xip:,} B exceeds the {TEXT_RODATA_BUDGET:,} B PSRAM budget; "
              f"only {psram_free:,} B of PSRAM would remain for the framebuffer and runtime. "
              "Shrink the image assets (see tools/make_assets.py) before shipping this build.")
        ok = False
    if app_size > OTA_SLOT_SIZE:
        print(f"FAIL: app binary {app_size:,} B exceeds the OTA slot size {OTA_SLOT_SIZE:,} B (0x7F0000); "
              "boards could not install it.")
        ok = False
    print("size budget: " + ("OK" if ok else "FAILED"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
