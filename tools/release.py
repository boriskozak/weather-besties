#!/usr/bin/env python3
"""Puts a finished ESP-IDF build into firmware/ and, with --publish, tells boards to update.

Run from the repo root inside the ESP-IDF environment, after `idf.py build`:

    python3 tools/release.py             # copy images for WB_VERSION into firmware/
    python3 tools/release.py --publish   # ...and point firmware/latest.json at them

Boards install the app firmware/latest.json names if it's newer than what they run: when
someone taps Update in Settings, or around 3 AM if "Install updates automatically" is on
in setup (it defaults off here). So commit and push with --publish only for a version
you've decided to ship. The board fetches from this repo:
https://raw.githubusercontent.com/boriskozak/weather-besties/main/firmware/latest.json

firmware/ afterwards:
    weather-besties-vX.Y.Z.bin        full image for a fresh install at 0x0 (erases settings)
    weather-besties-vX.Y.Z-app.bin    the app: what over-the-air updates download
    usb/                              the pieces for a USB install that keeps settings
    latest.json                       what boards update to (only changed by --publish)
Older versioned images are removed, except the one latest.json still points at.
"""
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, "build")
FW = os.path.join(ROOT, "firmware")


def version():
    with open(os.path.join(ROOT, "main", "net.h")) as f:
        m = re.search(r'#define WB_VERSION "([0-9]+\.[0-9]+\.[0-9]+)"', f.read())
    if not m:
        sys.exit("WB_VERSION not found in main/net.h")
    return m.group(1)


def flash_args():
    """[(offset, path)] from build/flash_args, the layout idf.py flashes."""
    out = []
    with open(os.path.join(BUILD, "flash_args")) as f:
        for line in f:
            parts = line.split()
            if len(parts) == 2 and parts[0].startswith("0x"):
                out.append((int(parts[0], 16), os.path.join(BUILD, parts[1])))
    return sorted(out)


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--publish", action="store_true", help="point firmware/latest.json at this version")
    args = ap.parse_args()

    v = version()
    app_src = os.path.join(BUILD, "weather_besties.bin")
    if not os.path.exists(app_src):
        sys.exit("no build/weather_besties.bin; run idf.py build first")
    # The image's own version must match, or boards would reject the update.
    with open(app_src, "rb") as f:
        head = f.read(0x200)
    if v.encode() not in head:
        sys.exit(f"build/weather_besties.bin isn't version {v}; rebuild after changing WB_VERSION")

    os.makedirs(os.path.join(FW, "usb"), exist_ok=True)
    full = f"weather-besties-v{v}.bin"
    app = f"weather-besties-v{v}-app.bin"
    shutil.copyfile(app_src, os.path.join(FW, app))

    layout = flash_args()
    subprocess.run([sys.executable, "-m", "esptool", "--chip", "esp32s3", "merge_bin", "-o",
                    os.path.join(FW, full), "--flash_mode", "dio", "--flash_freq", "80m", "--flash_size", "16MB"]
                   + [x for off, p in layout for x in (hex(off), p)], check=True, stdout=subprocess.DEVNULL)

    usb_names = {"bootloader.bin": "bootloader.bin", "partition-table.bin": "partition-table.bin",
                 "ota_data_initial.bin": "ota_data_initial.bin"}
    lines = []
    for off, p in layout:
        name = os.path.basename(p)
        if name in usb_names:
            shutil.copyfile(p, os.path.join(FW, "usb", usb_names[name]))
            lines.append(f"{off:#07x}  usb/{usb_names[name]}")
        else:
            lines.append(f"{off:#07x}  {app}")
    with open(os.path.join(FW, "usb", "README.txt"), "w") as f:
        f.write("USB install that keeps saved settings: flash these four files at these addresses\n"
                "(the web flasher takes several files; add one row per file):\n\n" + "\n".join(lines) + "\n")

    manifest_path = os.path.join(FW, "latest.json")
    if args.publish:
        manifest = {"version": v, "app": app, "sha256": sha256(os.path.join(FW, app)),
                    "size": os.path.getsize(os.path.join(FW, app))}
        with open(manifest_path, "w") as f:
            json.dump(manifest, f, indent=2)
            f.write("\n")
        print(f"published {v}: boards will update to it once this is pushed to main")

    keep = {full, app}
    if os.path.exists(manifest_path):
        with open(manifest_path) as f:
            keep.add(json.load(f)["app"])
    for name in os.listdir(FW):
        # Only this generation's images are pruned. firmware/ also archives the
        # pre-OTA v1.x images (a different partition layout entirely); keep those.
        m = re.match(r"weather-besties-v(\d+)\.", name)
        if name.endswith(".bin") and name not in keep and m and int(m.group(1)) >= 2:
            os.remove(os.path.join(FW, name))
            print("removed old", name)
    print(f"firmware/{full}, firmware/{app}, firmware/usb/ ready")


if __name__ == "__main__":
    main()
