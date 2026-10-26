# Iter-34: PSRAM boot-loop fix (fd2f37c)

## The failure (diagnosed on hardware, Claude's boot log)

b17c1d3 (iter-33) flashed and verified cleanly, booted ota_0 at
0x20000, then looped forever:

```
esp_lcd_new_rgb_panel: alloc frame buffers failed
  -> board_init(349) ESP_ERR_NO_MEM at main.c:75 (ESP_ERROR_CHECK abort)
```

Cause: `CONFIG_SPIRAM_XIP_FROM_PSRAM=y` + `CONFIG_SPIRAM_RODATA=y`
copy the app's `.flash.text` and `.flash.rodata` into the 8 MB PSRAM
(8,388,608 B) at boot. b17c1d3's sections (link map,
`~/wb-build-final/build/weather_besties.map`):

- `.flash.text`   1,134,436 B
- `.flash.rodata` 6,791,324 B (ELF segment figure 0x67a19c incl. header)
- sum 7,925,760 B -> only **462,848 B of PSRAM left**, but the RGB
  panel's framebuffer alone needs 800x480x2 = **768,000 B**.

The winter wardrobe pushed rodata over the cliff; every earlier build
fit under it. app_main's `ESP_ERROR_CHECK(board_init())` turned the
allocation failure into an abort and a tight, silent boot loop: the
board just looked dead (red, then blank).

## Asset audit (b17c1d3, .flash.rodata by object file, from the map)

| group | bytes | objects | notes |
|---|---:|---:|---|
| Pippa sprites + heart frames + head | 4,323,912 | 63 | biggest: img_pippa_rain 99,546; most sprites 74-80 KB |
| Pippa forecast minis | 894,277 | 52 | |
| garment tile icons (96 + 56 px) | 576,204 | | |
| cat2 layers | 272,780 | 32 | still live: ui.c's cat2 stage |
| My Day event medallions | 250,273 | | |
| fonts (7, uncompressed) | 107,706 | | |
| weather icons | 82,454 | | |
| IDF + libraries | 396,554 | | incl. merged string table 118,204 (attributed to esp_app_desc.c.obj), mbedTLS cert bundle 68,988 |
| wb_ui code rodata | 7,947 | | |
| extra icons + misc images | 3,726 | | |

All 257 images are RGB565A8 with LVGL **RLE** compression
(6,554,047 B of image data in total). Findings:

- **No duplicated sprite data**: SHA-256 over all 257 image arrays
  found zero byte-identical pairs. The iter-33 pink-fur slots already
  alias the black sprites instead of copying them.
- **The format was the problem, not duplication.** LVGL's RLE only
  collapses runs of identical pixels; on this textured cartoon art it
  manages barely 30% under raw. LZ4 block compression on the identical
  raw pixel streams measured 3,660,780 B (-44.1%), and LZ4
  high-compression mode (same block format, slower encoder) much less
  again (samples: rain 99,518 -> 42,129; kit2 77,816 -> 31,215;
  base 76,103 -> 25,954).

## The fix (fd2f37c)

1. **Assets RLE -> LZ4 high-compression.** All 257 image `.c` files
   regenerated from their prepared PNGs (`tools/build/png/`, first
   verified to reproduce the b17c1d3 RLE arrays byte-identically, so
   only the compression layer changed; every image descriptor
   cf/w/h/stride asserted unchanged). `LVGLImage.py`'s LZ4 path now
   encodes with `mode="high_compression"` (the on-device decoder
   cannot tell the difference; it is the standard LZ4 block format).
   `tools/make_assets.py` and `tools/make_pippa_assets.py` now emit
   `--compress LZ4`, and `components/lv_conf.h` enables
   `LV_USE_LZ4_INTERNAL` (LV_USE_RLE stays on so any stray RLE asset
   still decodes). Lossless: decoded pixels are bit-identical.
   SPIRAM_RODATA and XIP_FROM_PSRAM are untouched, as are the
   partition table and slot sizes.
2. **board_init no longer dies silently.**
   - `panel_init` (main/board.c): on `esp_lcd_new_rgb_panel` failure
     it logs the failing allocation by name and size ("no room in
     PSRAM for the 800x480 RGB565 framebuffer (768000 bytes)"); on
     `esp_lcd_panel_init` failure it deletes the panel so a retry
     starts clean.
   - `board_init`: panel init is retried 3 times, 500 ms apart, each
     failure logged.
   - `app_main` (main/main.c): `ESP_ERROR_CHECK(board_init())` is
     gone. On failure it logs the error and halts in a slow loop
     (re-log every 10 s) instead of aborting into a reboot loop.
3. **Build-time budget gates** (`tools/check_size_budget.py`, run by
   `tools/release.py` before packaging; also runnable standalone
   against any build dir). Two gates, both read from the build's own
   map + bin:
   - **PSRAM gate:** `.flash.text` + `.flash.rodata` <= 5,000,000 B,
     so >= ~3.39 MB of PSRAM survives the XIP copies for the
     framebuffer, the decode cache and WiFi/TLS.
   - **OTA slot gate:** app binary <= 8,323,072 B (0x7F0000, one slot
     of partitions.csv).
   Proof it bites: run against the b17c1d3 build it FAILS
   (7,925,760 > 5,000,000; "only 462,848 B of PSRAM would remain")
   and exits 1.

## Verification (production build, pristine clone of fd2f37c)

ESP-IDF v5.5.6, build at `~/wb-build-iter34`:

- app binary **4,548,992 B** (was 8,046,624), sha256
  `a214864d7c78c116e2f5291e144b469f896c140fd431caa9b718ef75b7efa90e`
- `.flash.text` 1,135,400 B (+964: LZ4 decoder + retry/halt code)
- `.flash.rodata` **3,292,732 B** (was 6,791,324; -3,498,592)
- text + rodata = **4,428,132 B** <= 5,000,000 budget (571,868 spare)
- PSRAM arithmetic: 8,388,608 - 4,428,132 = **3,960,476 B free** after
  the XIP copies (>= the ~2.5 MB requirement; the 768,000 B
  framebuffer leaves ~3.19 MB for decode cache + WiFi/TLS)
- app 4,548,992 B <= 8,323,072 B slot (3,774,080 B headroom, 45% free)
- `.dram0.bss` 0x3650 = 13,904 B, identical to iter-32/33
- calendar arrays still in PSRAM: cal.c blocks at 0x3c450000
  (0x4500, s_tmp), 0x3c454500 (0x650, s_zones), 0x3c454b50
  (0x4500, s_store) in `.ext_ram.bss`, same layout as before
- sim: `make test` green (request/cal/update) and **813 render
  checks pass** against the unchanged b17c1d3 reference renders:
  every scenario pixel-identical, as required for a lossless change
- budget script on this build: both gates pass, exit 0

## Staged

- `flash-latest/weather-besties-v2.0.0-fd2f37c-app.bin`
- `flash-latest/ota-first-install/` — same four-file set, app
  replaced with the fixed build, README updated (the board that
  boot-looped can be recovered by flashing this set; ota_1 is empty,
  so rollback cannot rescue the b17c1d3 install on its own)

Nothing pushed to GitHub; fd2f37c (code) + this report are local
commits on top of b17c1d3.
