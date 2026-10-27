# Weather Besties

A weather station for kids. It answers one question: what do I wear today?

Pippa the cat wears the outfit the screen recommends, so kids can lay out their clothes the night before, then check in the morning whether the plan still works. Built for Cleo and Lila.

![Morning screen: Pippa in her dress outfit, garment tiles, and the extra layer called out in words](docs/screens/morning.png)

## The idea

This is not a general weather dashboard. It answers one kid-sized question, in words and pictures a six-year-old can read on her own:

**What do I wear?**

- **The night before:** in the evening the screen switches to tomorrow, so the outfit gets laid out before bed.
- **In the morning:** one glance says whether it will be cold, warm, wet, or snowy, and whether yesterday's plan still works.

The design rule behind every screen: a kid should be able to look for **five seconds** and say what clothes to pick, whether she needs an extra layer, and which day the outfit is for. If a screen fails that, the screen is wrong.

## What it shows

- **Garment tiles:** big picture tiles for exactly what to wear (long sleeves, long pants, winter coat, snow boots...), picked by the outfit engine in `components/wb_ui/outfit.c` from the feels-like temperature over the school day (7 AM to 5 PM), plus rain, snow, storms, wind, and sun. Tap a tile as each item gets laid out and it turns green with a check.
- **The extra layer, in words:** when the day needs a coat on top, that tile is outlined and labeled "Extra layer". No guessing from icons.
- **Extras are check-offs too:** a warm hat, gloves, scarf, umbrella, sunglasses, or water bottle shows up as a small chip under the tiles, and it has to be checked off as well. The all-done celebration only fires when *everything* is laid out.
- **Pippa wears the prescription:** the same outfit the tiles recommend, head to toe, so the picture and the list always agree. Check everything off and she does a heart pose.
- **One plain sentence** under the tiles explains the day: "Cold this morning, warm after lunch. Take your warm coat off later."
- **Top right:** the weather in words a kid can use: "33° now", "High 64° · Low 32°". Tomorrow adds what matters: "Low 46° · Rain 80%".
- **Tabs at the bottom:** Today, Tomorrow, My Day, and Next days (three plain forecast cards, each with the one key garment for that day). Swipe works too. After 3 idle minutes the screen falls back to the page for the time of day.
- **My Day:** the kids' calendar events for today and tomorrow, read from the calendar link entered in setup. The list scrolls. An event with a kid's name in its title gets a name tag in that kid's color (first kid lavender, second kid teal). The names come from setup.
- **Stale data is marked:** if the weather is over 90 minutes old, a chip says when it is from ("Weather from 11:45 AM"). Over 12 hours (or no data at all), Pippa shows up undressed and the screen says "I can't check the weather right now." instead of prescribing from stale numbers.
- **Little seasonal touches:** a jack-o'-lantern sits beside her in October, a menorah gains a lit candle each night of Hanukkah, and a present shows up at Christmas. A real prescription always wins: a cold Halloween still gets the winter coat.
- **Bike bus day** (picked in setup, off by default): if the 8 AM feels-like is 50°F or below, the sentence reminds everyone to lay out gloves. The evening before, it says to get them ready.
- **Cat choice:** Settings (long press anywhere empty) has a cat picker: black Pippa, pink Pippa, or take turns, swapping daily. Same cat on the boot and setup screens.
- An optional "screen off at night" mode. A tap wakes the screen for a minute, and that first tap doesn't press anything.

Weather data comes from [Open-Meteo](https://open-meteo.com): free for non-commercial use, no API key. The board refreshes every 15 minutes.

## Screenshots

**Evening: tomorrow's rain outfit, ready to lay out**

![Evening screen: Pippa in her rain outfit with umbrella, rain jacket marked as the extra layer](docs/screens/evening.png)

**All checked off: the heart celebration**

![All-done screen: every tile and chip checked, Pippa holding a heart](docs/screens/all-done.png)

**Snow day: winter coat, snow boots, hat, gloves, and scarf**

![Snow screen: Pippa in her snow outfit with beanie and mittens](docs/screens/snow.png)

**Hot day: shorts, sunglasses, and a water bottle**

![Hot screen: Pippa in shorts and heart sunglasses, holding a water bottle](docs/screens/hot.png)

**Pink Pippa (Settings cat picker)**

![Pink cat screen: pink Pippa in the same dress outfit](docs/screens/pink.png)

**My Day: today's and tomorrow's calendar events, with name tags**

![My Day screen: calendar events for today and tomorrow with per-kid name tags](docs/screens/myday.png)

**Next days: one key garment per day**

![Next days screen: three forecast cards with rain jacket and light jackets](docs/screens/next-days.png)

## Pippa's wardrobe

Six outfits, two colourways. The outfit engine prescribes; Pippa models it. From left: dress, pants, fall (hoodie and scarf), rain, hot, and snow.

![Pippa's wardrobe, black: dress, pants, fall, rain, hot, snow](docs/screens/wardrobe-black.png)

![Pippa's wardrobe, pink: dress, pants, fall, rain, hot, snow](docs/screens/wardrobe-pink.png)

## Status

The current version is **2.0.4**. It runs on the real board. The prebuilt images in `firmware/` are this version: `weather-besties-v2.0.4.bin` (full image) and `weather-besties-v2.0.4-app.bin` (app update). The screens on this page are rendered by the PC simulator, which runs the same screen code the board runs.

## Build one

### Buy

- Waveshare ESP32-S3-Touch-LCD-4.3B (ESP32-S3-WROOM-1-N16R8: 16 MB flash, 8 MB octal PSRAM), 800 x 480 RGB display with GT911 capacitive touch.
- [Buy the board on Amazon](https://a.co/d/0a7hHUrl). The bare board is around $37. The version with a case costs more; a 3D printed case works, see [Case](#case).
- Any USB-C 5V supply. The board has two USB-C ports: **UART** (serial, used for flashing) and **USB** (native USB).

### Flash

Flashing uses the [esptool-js web flasher](https://espressif.github.io/esptool-js/) in Chrome or Edge. No installs.

1. Get the firmware file, from `firmware/` in this repo or from the GitHub release page:
   - Blank board, or start over: `weather-besties-v2.0.4.bin` (the full image).
   - Update a board that already runs Weather Besties: `weather-besties-v2.0.4-app.bin`.
2. Plug the board into the **UART** USB-C port.
3. In the flasher, set the baud rate to 460800, click Connect, and pick the serial port.
4. Set the flash address: `0x0` for the full image, `0x20000` for the app update.
5. Choose the file and click Program.
6. Click **Disconnect** afterwards (the flasher can hold the board in reset, which leaves a dark screen), then press **RESET** on the board.

The full image erases saved settings (WiFi, town, names, calendar link), so setup runs again. The app update keeps them.

**If it won't connect:** hold **BOOT**, tap **RESET**, let go of BOOT, then try again. If no new port shows up at all, the cable may be charge-only.

**Command line**, with the same files and addresses:

```
pip install esptool
python -m esptool --chip esp32s3 --port PORT --baud 460800 write_flash 0x20000 weather-besties-v2.0.4-app.bin
```

### Setup

1. On first start the screen shows a QR code and a hotspot name like `WeatherBesties-3F2A`.
2. Scan the QR with a phone, or join that WiFi network by hand. The setup page opens. If it does not, open `http://192.168.4.1`.
3. Fill in the form, top to bottom:
   - **Home WiFi** and password.
   - **Town or ZIP code.** The board looks it up and stores the coordinates.
   - **First kid's name** and **Second kid's name.** Used in the greeting, and for the name tags on the My Day page. One kid: fill in the first only.
   - **Kids calendar link (optional).** In Google Calendar: open Settings, pick the calendar, and copy the "Secret address in iCal format". Paste that link. A shared family calendar works; it does not need to be a separate kids calendar. The board reads it and shows today and tomorrow on the My Day page. Treat the link like a password: anyone with it can read the calendar.
   - **Temperature:** °F or °C.
   - **Screen off at night**, and the off and on hours.
   - **Install updates automatically** (off by default, see Updates).
   - **School uniform outfits.** On: the outfit rotation includes the school logo looks. Off: it never picks them. Turn it off if your school has no uniform.
   - **Bike bus day**, or "No bike bus".
4. Save. The board restarts, joins the home WiFi, downloads the weather, and shows the Today page.

![Setup screen](docs/screens/setup.png)

A board with nothing saved has no town, so it has no weather until setup runs. The kids' names default to Cleo and Lila. The cat color is picked on the board, in Settings.

## Using it

- Tap the garment tiles and extra chips to check things off as they get laid out. The check marks belong to the outfit's day: a new day starts unchecked.
- Use the Today / Tomorrow / My Day / Next days tabs, or swipe left and right. The page flips the moment the finger lifts (no dragging: moving the whole screen under a finger is the slowest thing this chip draws). A swipe is 80 px or more sideways from where the finger went down (`on_indev()` in `ui.c`); a finger that moved that far doesn't count as a tap or a long press.
- Settings are stored on the board and survive reboots and app updates.

## Settings

Hold a finger on any empty spot for 1200 ms to open Settings. It has:

- **Cat:** black cat, pink cat, or take turns.
- **Update:** checks for a newer version, and downloads and installs it (see Updates).
- **Change WiFi or town:** restarts the board into the setup form, with the saved values filled in.
- **Refresh:** downloads the weather again now.
- **Speed test:** see below.
- **Close.**

With "screen off at night" on, the screen sleeps during those hours. A tap wakes it for a minute. Weather keeps updating in the background, so the morning screen is fresh.

## Updates

Boards check this repo for new versions: the manifest is `firmware/latest.json` in this repo. The Update button in Settings runs the check; if a newer version exists, the board downloads and installs it, and shows the progress and the result on the screen. After an install the board restarts on the new version. If the new version cannot download weather within 10 minutes, the board rolls back to the previous version on its own.

Automatic install is off by default. Turn on "Install updates automatically" in setup and the board installs new versions on its own, around 3 AM.

If you fork this repo, point your boards at your fork: change `WB_UPDATE_BASE` in `components/wb_ui/update.h` to your repo's firmware folder, and publish builds with `tools/release.py`.

## Case

- **The case:** [cad/](cad/) is a 3D-printable enclosure with two sculpted cats peeking over the screen, made for a Bambu P1S. Python source, print-ready STLs, a STEP file and renders. Measure the screen module's thickness before printing (see the case README).
- **How it works inside:** [docs/INTERNALS.md](docs/INTERNALS.md) draws out the board, the software layers, the setup hotspot and the flash layout. Written for v1.0, so a few numbers (image size, drawing buffer) have changed since.

## Simulator

The UI, weather parsing, and outfit logic are plain C with no ESP-IDF dependencies, so the exact same code runs on a PC and in a browser.

**Browser simulator:** `make web` in `sim/` writes `sim/web/site/Weather-Besties-Simulator.html`. Double-click it; it works offline. It is the board's real screen code compiled to WebAssembly. The mouse is the finger: drag sideways to swipe, hold any empty spot for 1200 ms for Settings. **Live weather** fetches today's forecast for Upper Montclair with the same request the board sends; the other buttons are sample days, including the holidays.

**Screenshots of every screen** (Linux, or WSL on Windows):

```
sudo apt install build-essential python3-pil
cd sim
make shots      # PNGs in sim/out/, sample forecasts generated into sim/scenarios/
```

**Rebuilding the simulator** needs [wasi-sdk](https://github.com/WebAssembly/wasi-sdk) (set `WASI_SDK` in `sim/Makefile` if it isn't at the default path), then `make shots && make web`. The plain-JavaScript fallback (`weather-besties.asm.js`, for browsers that block WebAssembly) is made with binaryen's `wasm-opt` + `wasm2js` and `web/make_fallback.py`; see the comments there. The speed meter in the simulator measures your computer, not the board.

## Speed test

Open Settings (long press) and tap "Speed test". A meter appears bottom left with frames per second and CPU, and the readout in Settings shows how many screen lines are drawn per pass and how much internal RAM is free. Tap again for the old v1.3.0 settings (small drawing area, blurred shadows) to compare, and a third time to turn it off.

The drawing area starts at 40 lines (the same 64 KB of internal RAM v1.3.0 used). After the first successful weather download it grows to 100, 80 or 60 lines, whichever leaves at least 64 KB of internal RAM free (`board_grow_draw_buffer()` in `main/board.c`). If a download fails while free internal RAM is under 24 KB, it shrinks back. The serial log prints the free RAM after every refresh.

## Build from source

Needs ESP-IDF v5.5 (built and tested with v5.5.6).

```
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

The firmware uses two app partitions, `ota_0` at `0x20000` and `ota_1` at `0x810000`, each `0x7F0000` bytes (see `partitions.csv`). The app image is about 4.5 MB, most of it artwork. `tools/check_size_budget.py` fails a release build if the app outgrows its partition or the read-only data passes its budget.

Before shipping a change to the weather requests, run `make test` in `sim/`. It builds every request the board can make with the firmware's own code (`components/wb_ui/wb_request.c`) and checks it fits the HTTP client's send buffer with room to spare. That is the bug that broke v1.1.0: ESP-IDF's default send buffer is 512 bytes and the forecast request needs about 560.

Everything else is already in `components/`: LVGL 9.5.0, Espressif's GT911 touch driver, and the captive-portal DNS server from the ESP-IDF examples. Serial logs run at 115200 baud on the UART port.

## Layout

| Path | What |
|---|---|
| `main/main.c` | Boot flow, night-off power timer, speed test hook |
| `main/board.c` | Screen, touch, IO expander (backlight, touch reset), LVGL glue, the growable drawing buffer. Pins and timing from Waveshare's own demo |
| `main/net.c` | WiFi, clock (SNTP), Open-Meteo calls, refresh and retry loop |
| `main/portal.c` | Setup hotspot, captive portal, phone form |
| `main/settings.c` | Saved settings (NVS) |
| `components/wb_ui/ui.c` | All screens: outfit pages, My Day, forecast page, settings, swipes |
| `components/wb_ui/outfit.c` | The outfit engine: what-to-wear rules over the school-day window (feels-like bands, rain, snow, storms, wind, sun) and the advice sentences |
| `components/wb_ui/cal.c` | The kids calendar: iCal parsing and the timezone rules that put events at the right local time |
| `components/wb_ui/cat.c` | The seasonal props that sit beside Pippa (pumpkin, menorah, present) |
| `components/wb_ui/wx.c` | Forecast parsing (hourly wind gusts feed the outfit engine's windy rule) |
| `components/wb_ui/wb_request.c` | Builds the Open-Meteo request addresses; buffer sizes in `wb_request.h` |
| `components/lv_conf.h` | LVGL settings, shared with the PC renderer |
| `tools/make_pippa_assets.py` | Turns the Pippa sprites (six outfits x two colourways, the base, the heart animation) into LVGL C arrays |
| `tools/crop_garment_sheet.py` | Crops the garment icons out of the icon sheet in `tools/garment_sheet/` |
| `tools/icons.py` | The remaining artwork (weather faces and small icons), drawn as SVG |
| `tools/make_assets.py` | Turns artwork and the Fredoka font into C files |
| `sim/` | PC renderer (`make shots`), request-size test (`make test`), browser simulator in `sim/web/` (`make web`) |
| `firmware/` | Ready-to-flash images of the current version: full image (flash at `0x0`), app update (flash at `0x20000`), `firmware/latest.json` (the update manifest boards check), and `firmware/usb/` (the four separate files with their addresses, for a settings-keeping USB install) |
| `cad/` | The 3D-printed case: CadQuery housing, sculpted cats, STLs, STEP, renders |
| `docs/` | Screenshots of the current design in `docs/screens/`, and the internals write-up |

## Tweaking

- Clothing rules: `components/wb_ui/outfit.c` (thresholds are feels-like in °F; the school-day window and departure hour are at the top of `outfit_make()`).
- Where things sit on the outfit pages: `build_outfit_page()` and `render()` in `components/wb_ui/ui.c`. Pippa's stage is 260x252; her sprites are full-body pictures swapped whole, never layered.
- Bike bus cutoff: `BIKE_BUS_COLD_F` in `components/wb_ui/outfit.h`.
- Colors: the palette at the top of `components/wb_ui/ui.c`.
- Artwork: Pippa sprites are finished pictures, converted by `tools/make_pippa_assets.py`. The SVG artwork (weather faces, icons) lives in `tools/icons.py`; run `python3 tools/make_assets.py` after changing it (needs `pip install resvg-py pillow pypng` and `npm install lv_font_conv` inside `tools/`).

## On the real board

The v1 firmware ran on the Waveshare board from v1.1.1 on. Things learned the hard way, still true for 2.x:

- The forecast request is longer than ESP-IDF's default 512-byte send buffer (fixed in v1.1.1, guarded by `make test`).
- On a weak WiFi signal, power saving made the forecast download time out. v1.1.2 turned power saving off (the board is wall powered), allows 30 seconds, and retries after 10 seconds.
- Plugged into a computer's USB port, the WiFi signal can drop out ("can't reach weather service"). A wall charger fixed it.
- The `0x0` full image overwrites saved settings; use the app update file at `0x20000` to keep them.

If the screen ever shifts sideways or flickers while WiFi is busy, lower `LCD_PCLK_HZ` in `main/board.c` from 16 MHz to 14 MHz.

## Notes

- Pippa is this project's cat. A different pet means drawing a new sprite set at the same sizes and regenerating it with `tools/make_pippa_assets.py`; the sizes are documented in that script. This is an advanced change.
- The Hanukkah candle dates are a built-in table in `components/wb_ui/cat.c`. It lists the first candle for 2025 to 2035. Outside those years the menorah does not appear.
- The outfit rules assume a US school year. Weekends, July and August, and Christmas Eve through New Year's Day count as no-school days, and no-school days use different kid-moment hours (see `school_day()` in `components/wb_ui/wx.c`). Other days off are not in the table.
- Weather data is Open-Meteo, free for non-commercial use, no API key.
- This board has no speaker and no speaker connector. The firmware makes no sounds.

## Credits

Font: Fredoka (SIL Open Font License, see `tools/fonts/OFL.txt`). Weather data: Open-Meteo.com (CC BY 4.0).
