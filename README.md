# Weather Besties

A kitchen weather station for Cleo and Lila, built on the Waveshare ESP32-S3-Touch-LCD-4.3 (800 x 480 touch screen). It answers the one question that matters before school: **what do we wear today?**

![Today screen](docs/today.png)

## What it shows

- The weather right now, in big friendly type.
- A "what to wear" bubble with clothing picks (jacket, beanie, mittens, umbrella, boots...) based on feels-like temperature, rain, snow, storms, and sun.
- Every day is compared with yesterday: warmer or colder (by the daily high), sunnier or cloudier (by hours of sun). A drop of 12°F or more says "a lot colder" and adds gloves.
- The rest of the day in four chunks (morning, midday, afternoon, evening), then the next three days on a second page (swipe left).
- After 7 PM the bubble switches to tomorrow, so outfits can be picked the night before.
- **Bike bus day** (Friday by default): if the 8 AM feels-like is 50°F or below, the bubble reminds everyone to wear gloves for bike bus. The evening before, it says to lay them out.
- An optional "screen off at night" mode. A tap wakes the screen for a minute.

![Evening screen](docs/evening.png)
![Next days](docs/next-days.png)

Weather data comes from [Open-Meteo](https://open-meteo.com): free for non-commercial use, no API key, up to 10,000 calls a day. The board refreshes every 15 minutes (96 calls a day).

## Hardware

- Waveshare ESP32-S3-Touch-LCD-4.3 (ESP32-S3-WROOM-1-N16R8: 16 MB flash, 8 MB octal PSRAM)
- 800 x 480 RGB panel with GT911 capacitive touch
- Any USB-C 5V supply. The board has two USB-C ports: **UART** (serial, used for flashing) and **USB** (native USB)

## Flash it

You only need `firmware/weather-besties-v1.1.0.bin`. It is a single merged image (bootloader, partition table, and app) that goes at address `0x0`.

**Chrome or Edge, no installs:** open the [esptool-js web flasher](https://espressif.github.io/esptool-js/), plug the board into the **UART** USB-C port, click Connect, pick the port, set Flash Address to `0x0`, choose the .bin, click Program. Takes about a minute.

**Command line:**

```
pip install esptool
python -m esptool --chip esp32s3 write_flash 0x0 weather-besties-v1.1.0.bin
```

**If it won't connect:** hold **BOOT**, tap **RESET**, let go of BOOT, then try again. If your computer doesn't see a serial port on the UART connector, try the other USB-C port (native USB).

## First start

1. The screen shows a QR code and a hotspot name like `WeatherBesties-3F2A`.
2. Scan the QR with a phone (or join that WiFi network by hand). A setup page pops up. If it doesn't, open `http://192.168.4.1`.
3. Pick the home WiFi, type the password, then set: town or ZIP, names for the greeting, °F or °C, bike bus day, and optional night hours. Save.
4. The board restarts, joins WiFi, and shows the weather.

![Setup screen](docs/setup.png)

Defaults if you skip things: Upper Montclair, NJ, "Cleo & Lila", Fahrenheit, bike bus on Fridays, screen always on.

## Using it

- Swipe left for the next three days, right to come back.
- Long-press the greeting, the clock, or any empty spot for 1.5 seconds to open Settings (change WiFi or town, refresh now).
- Settings are stored on the board and survive reboots and reflashing the app.

## Build from source

Needs ESP-IDF v5.5 (built and tested with v5.5.6).

```
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

Everything else is already in `components/`: LVGL 9.5.0, Espressif's GT911 touch driver, and the captive-portal DNS server from the ESP-IDF examples. Serial logs run at 115200 baud on the UART port.

## Try it without the board

The UI, weather parsing, and advice logic are plain C with no ESP-IDF dependencies, so the exact same code renders on a PC:

```
cd sim
make shots     # renders every screen to sim/out/*.png
make web       # builds the browser emulator in sim/web/ (needs wasi-sdk)
```

Sample forecasts for testing live in `sim/scenarios/` (snow morning, rainy afternoon, summer heat, and more).

## Layout

| Path | What |
|---|---|
| `main/main.c` | Boot flow, night-off power timer |
| `main/board.c` | Screen, touch, IO expander (backlight, touch reset), LVGL glue. Pins and timing from Waveshare's own demo |
| `main/net.c` | WiFi, clock (SNTP), Open-Meteo calls, refresh loop |
| `main/portal.c` | Setup hotspot, captive portal, phone form |
| `main/settings.c` | Saved settings (NVS) |
| `components/wb_ui/ui.c` | All screens |
| `components/wb_ui/advice.c` | What-to-wear rules (temperature bands, rain, snow, storms, sun) |
| `components/wb_ui/wx.c` | Forecast parsing, parts of the day |
| `components/lv_conf.h` | LVGL settings, shared with the PC renderer |
| `tools/icons.py` | The artwork (weather faces, clothes, cats), drawn as SVG |
| `tools/make_assets.py` | Turns artwork and the Fredoka font into C files |
| `sim/` | PC renderer and browser emulator (same UI code) |
| `firmware/` | Ready-to-flash image (address `0x0`) |
| `docs/` | Screenshots rendered from the UI code |

## Tweaking

- Clothing rules: the `BANDS` table at the top of `components/wb_ui/advice.c` (temperatures are feels-like in °F).
- Bike bus cutoff and "a lot colder" threshold: `BIKE_BUS_COLD_F` and `BIG_CHANGE_F` in `components/wb_ui/advice.h`.
- Colors: the theme table at the top of `components/wb_ui/ui.c`.
- Artwork: edit `tools/icons.py`, then run `python3 tools/make_assets.py` (needs `pip install resvg-py pillow pypng` and `npm install lv_font_conv` inside `tools/`).

## Not yet checked on the real board

The code compiles cleanly and every screen was rendered from the real UI code, but it hasn't run on the hardware yet. Things to watch on first boot:

- Touch lands in the wrong place: flip `swap_xy` / `mirror_x` / `mirror_y` in `touch_init()` in `main/board.c`.
- Picture shifts sideways or flickers while WiFi is busy: lower `LCD_PCLK_HZ` in `main/board.c` from 16 MHz to 14 MHz.
- Screen stays dark but the log says "display ready": the backlight goes through the IO expander, check the log for "expander write failed".

## Credits

Font: Fredoka (SIL Open Font License, see `tools/fonts/OFL.txt`). Weather data: Open-Meteo.com (CC BY 4.0).
