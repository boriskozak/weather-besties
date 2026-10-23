# Weather Besties firmware

Weather station for Cleo and Lila on the Waveshare ESP32-S3-Touch-LCD-4.3 (800 x 480 touch).
Shows the weather now, a "what to wear" bubble that also compares with yesterday ("A lot colder and cloudier than yesterday"), the rest of the day in four chunks, and the next three days (swipe left).

![Main screen](docs/today.png)

Weather comes from Open-Meteo: free for non-commercial use, no API key, up to 10,000 calls a day. The board asks every 15 minutes (96 calls a day).

## Flash it

You only need `firmware/weather-besties-v1.1.0.bin`. It's a single image that goes at address `0x0`.

**Chrome or Edge, no installs:** open https://espressif.github.io/esptool-js/, plug the board into the computer with the USB-C port labeled **UART**, click Connect, pick the port, set Flash Address to `0x0`, choose the .bin, click Program. Takes about a minute.

**Command line:**

```
pip install esptool
python -m esptool --chip esp32s3 write_flash 0x0 weather-besties-v1.1.0.bin
```

**If it won't connect:** hold **BOOT**, tap **RESET**, let go of BOOT, then try again. On a Mac or PC that doesn't see a serial port on the UART connector, try the other USB-C port (native USB).

## First start

1. The screen shows a QR code and a hotspot name like `WeatherBesties-3F2A`.
2. Scan the QR with a phone (or join that WiFi by hand). A setup page pops up. If it doesn't, open `http://192.168.4.1`.
3. Pick the home WiFi, type the password, town or ZIP, names for the greeting, F or C, bike bus day, and optional "screen off at night". Save.
4. The board restarts, joins WiFi, and shows the weather.

Defaults if you skip things: Upper Montclair, NJ, "Cleo & Lila", Fahrenheit, bike bus on Fridays, screen always on.

## Using it

- Swipe left for the next three days, right to come back.
- Hold your finger on the greeting, the clock, or any empty spot for 1.5 seconds to open Settings (change WiFi or town, refresh now).
- After 7 PM the bubble switches to tomorrow, so outfits can be picked the night before.
- Every day is compared with the day before: warmer or colder (by the daily high), sunnier or cloudier (by hours of sun). A drop of 12°F or more says "a lot colder" and adds gloves.
- Bike bus day (Friday unless you change it in setup): if the 8 AM "feels like" is 50°F or below, the bubble reminds everyone to wear gloves for bike bus. The evening before, it says to lay them out.
- With "screen off at night" on, the screen sleeps during those hours. A tap wakes it for a minute (that first tap doesn't press anything).

## Build from source

Needs ESP-IDF v5.5 (built and tested with v5.5.6).

```
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

Everything else is already in `components/`: LVGL 9.5.0, Espressif's GT911 touch driver, and the captive-portal DNS server from the ESP-IDF examples.

## Layout

| Path | What |
|---|---|
| `main/board.c` | Screen, touch, IO expander (backlight, touch reset), LVGL glue. Pins and timing from Waveshare's own demo. |
| `main/net.c` | WiFi, clock, Open-Meteo calls, refresh loop. |
| `main/portal.c` | Setup hotspot and phone form. |
| `main/settings.c` | Saved settings. |
| `components/wb_ui/ui.c` | All screens. |
| `components/wb_ui/advice.c` | What-to-wear rules (temperature bands, rain, snow, storms, sun). |
| `components/wb_ui/wx.c` | Forecast parsing, parts of the day. |
| `components/lv_conf.h` | LVGL settings, shared with the PC renderer. |
| `tools/icons.py` | The artwork (weather faces, clothes, cats), drawn as SVG. |
| `tools/make_assets.py` | Turns artwork and the Fredoka font into C files. |
| `sim/` | Renders every screen on a PC with the exact same UI code: `make shots`, results in `sim/out/`. |
| `firmware/` | Ready-to-flash image (bootloader, partition table and app merged, address `0x0`). |
| `docs/` | Screenshots rendered from the UI code. |
| `sim/web/` | Browser emulator: the same UI code compiled to WebAssembly (`make web`, needs wasi-sdk). `emu.js` drives it; `weather-besties.asm.js` is a plain JavaScript copy for browsers that block WebAssembly. |

## Tweaking

- Clothing rules: the `BANDS` table at the top of `components/wb_ui/advice.c` (temperatures are "feels like" in F).
- Bike bus cutoff and "a lot colder" threshold: `BIKE_BUS_COLD_F` and `BIG_CHANGE_F` in `components/wb_ui/advice.h`.
- Colors: the theme table at the top of `components/wb_ui/ui.c`.
- Artwork: edit `tools/icons.py`, then `python3 tools/make_assets.py` (needs `pip install resvg-py pillow pypng` and `npm install lv_font_conv` inside `tools/`).

## Not yet checked on the real board

The code compiles cleanly and every screen was rendered from the real UI code, but it hasn't run on the hardware yet. Things to watch on first boot:

- Touch lands in the wrong place: flip `swap_xy` / `mirror_x` / `mirror_y` in `touch_init()` in `main/board.c`.
- Picture shifts sideways or flickers while WiFi is busy: lower `LCD_PCLK_HZ` in `main/board.c` from 16 MHz to 14 MHz.
- Screen stays dark but the log says "display ready": the backlight goes through the IO expander, check the log for "expander write failed".

Serial logs at 115200 baud on the UART port (`idf.py monitor`) say what's happening.

Font: Fredoka (SIL Open Font License, see `tools/fonts/OFL.txt`). Weather data: Open-Meteo.com (CC BY 4.0).
