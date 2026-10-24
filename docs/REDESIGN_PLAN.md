# Weather Besties 2.0: clothing-guide redesign, implementation plan

Handoff spec. Written to be executed by an LLM coding agent (or a person) with no other context.
Repo: `github.com/boriskozak/weather-besties`, branch `main` (last commit `c31dcce`, firmware v1.4.0).
The original product brief (from Boris) is reproduced at the end. This plan implements it.

**Acceptance test:** either child (ages 6 and 8) looks at the screen for five seconds and can say
what clothes to pick, whether they need an extra layer, and which day the outfit is for.

---

## 0. What exists today (inspected, facts)

### Hardware and platform
- Board: Waveshare ESP32-S3-Touch-LCD-4.3. ESP32-S3, 2 cores at 240 MHz, 512 KB internal RAM, 8 MB octal PSRAM, 16 MB flash.
- Panel: 800 x 480 IPS, RGB565 over a 16-bit parallel bus, 16 MHz pixel clock, about 39 fps scan-out. Physical active area 95.5 x 54.4 mm (about 217 ppi).
- Touch: GT911 capacitive, 5 points, I2C. Taps, long press and swipes all work. Swipes are detected by our own code (`on_indev()` in `components/wb_ui/ui.c`: press-to-release, at least 80 px sideways).
- Framework: ESP-IDF v5.5.6 (FreeRTOS), LVGL 9.5.0, cJSON. `components/lv_conf.h` is shared by firmware and the PC renderer. `LV_COLOR_DEPTH 16`, clib malloc.
- Flash layout (`partitions.csv`): nvs 0x9000 (24 KB), phy 0xf000, one factory app slot at 0x10000, 7 MB. App image today: 2.6 MB.
- `CONFIG_SPIRAM_XIP_FROM_PSRAM=y` + `RODATA`: the whole app image (code + images + fonts) is copied into PSRAM at boot. **Every byte of new artwork costs PSRAM as well as flash.** PSRAM budget: 8 MB minus app image minus 768 KB framebuffer minus WiFi/lwIP/heap. Keep the app at 3.5 MB or less.
- Internal RAM: about 78 KB free after a fetch, with the growable LVGL draw buffer (`board_grow_draw_buffer()` in `main/board.c`; starts at 40 lines and grows to 100/80/60).
- Toolchains already set up in the original container (recreate if new): ESP-IDF at `/home/claude/esp` (`source /home/claude/esp/env.sh`), art venv `/home/claude/artenv` (resvg_py, pillow, pypng), `tools/node_modules/.bin/lv_font_conv`, wasi-sdk 33 for the browser sim.

### Code map (what matters for the redesign)
| File | Role today | Redesign action |
|---|---|---|
| `components/wb_ui/ui.c` (1472 lines) | All screens: boot, setup, settings overlay, main screen (today page with big temperature, speech bubble, cat, 3 school-moment cards; next-days page with 3 cards), weather fx (falling rain/leaves/stars), speed test | Rewrite the main screen sections. Keep boot, setup, settings overlay (extend), swipe handler, clock timer |
| `components/wb_ui/advice.c/.h` | Old "what to wear" bands, headline/tip strings, day-vs-day comparisons | Replace with new `outfit.c/.h`. Delete advice.c/.h when nothing uses them |
| `components/wb_ui/cat.c/.h` | Outfit pick for the old head-only cat, Hanukkah dates (Hebcal 2025-2035), weather fx pick | Keep `cat_hanukkah_candles()`. Replace look logic with a small "season" helper (section 5) |
| `components/wb_ui/wx.c/.h` | Open-Meteo parser. Hourly arrays from today 00:00 (96 h): temp, feels, pop, code, is_day. Daily: code, max, min, pop, uv, sunshine. Current: temp, feels, wind, gust, precip | Add hourly gusts (section 6). `wx_kid_moments()` becomes unused |
| `components/wb_ui/wb_request.c/.h` | Builds forecast/geocode URLs; `make test` checks request size against `WB_HTTP_TX_BUF` 2048 | Add `wind_gusts_10m` to hourly |
| `main/net.c` | WiFi, SNTP, fetch every 15 min, retries, calls `ui_show_weather()` and `ui_set_footer("Offline. Last update ...")` | Small change: keep calling `ui_show_weather` on every attempt, pass settings |
| `main/settings.c/.h` | NVS settings | Add `cat` choice |
| `tools/icons.py` | All artwork as SVG (sticker style: navy `#2B3450` outline drawn fat, fill on top) | Add full-body cat + clothing layers + garment icons |
| `tools/make_assets.py` | Renders SVG with resvg, converts to LVGL C arrays (RGB565A8) via `components/lvgl/scripts/LVGLImage.py`, fonts via lv_font_conv, writes `assets/assets.h`, `kitty_sets.c`, `assets_list.cmake` | Add new sets, fonts; drop unused |
| `sim/` | PC renderer (`make shots` -> `sim/out/*.png`), `make test`, browser sim (`make web`) | New scenarios + renders |

### Bugs found while inspecting (must fix in the redesign)
1. **Advice uses the fetch time, not the clock.** `advice_make()` decides today vs tomorrow with `wx->hour` (the API's `current.time` at fetch). When the board is offline (it was, in Boris's video: "Offline. Last update 11:53 AM" at 1:39 PM), the evening switch never happens, and after midnight it shows yesterday's "today". The new engine must use the real local clock (`ui_time_cb`) and index hourly data by **date offset** between the forecast date and the clock date.
2. Stale data is only flagged by a small red footer string. The brief requires honest handling (section 4.6).

---

## 1. Design principles (from the brief, made concrete)

- The **dressed cat and the garment list** are the heart of the screen. They take the most space and contrast.
- Reading order: **outfit (cat + garments) -> which day -> extra item/layer and advice -> supporting weather**.
- Calm, premium, joyful. Solid backgrounds, no gradients, no constant animation, no heavy cards, no tiny captions.
- One type family: **Fredoka** (already in `tools/fonts/`). Clear hierarchy, minimum 22 px for anything a child must read.
- Direct, short, encouraging language. No numbers in advice sentences. No comparison math.
- Useful with zero interaction. Touch adds check-offs, a manual day switch and the forecast page.

### Remove from the main screen
Big 140 px temperature, speech bubble, the three school-moment cards, greeting line, page dots, falling weather fx (rain/leaves/stars/lightning timer `fx_*`: "constant animation" is explicitly out), the cape costume, "warmer than yesterday" comparisons. Delete the code and assets (`font_temp_xl` alone is 224 KB of source), don't just hide them.

Note for Boris: the weather animations he asked for in v1.4 are removed by this brief ("avoid constant animation"). Keep the commit history so they can come back.

---

## 2. Visual system

### Colors (hex)
| Token | Value | Use |
|---|---|---|
| `BG_MORNING` | `#FBF6EE` | warm ivory, Today page |
| `BG_EVENING` | `#F2EFF8` | very pale dusk lavender, Tomorrow page |
| `BG_FORECAST` | `#F4F8FC` | very pale sky, Next days page |
| `INK` | `#1E2340` | all primary text (contrast > 12:1 on the backgrounds) |
| `INK_2` | `#545A72` | secondary text (contrast about 6.5:1) |
| `TILE` | `#FFFFFF` | garment tiles, forecast columns |
| `TILE_LINE_M` / `TILE_LINE_E` | `#ECE4D5` / `#E2DCEF` | 2 px tile border, morning / evening |
| `CHIP_M_BG` / `CHIP_M_TX` | `#FFE6A6` / `#5A3B00` | "Today" day chip |
| `CHIP_E_BG` / `CHIP_E_TX` | `#E2DCF7` / `#3B3486` | "Tomorrow" day chip |
| `OK` | `#2FA866` | check marks; checked tile bg `#E8F6EE`, border `#9ED8B6` |
| `WARN_BG` / `WARN_TX` | `#FFEBC7` / `#8A4B00` | stale-data chip |
| `ACCENT` | `#E0457B` | sparingly: hearts, all-done line |
| Outline navy (art) | `#2B3450` | every illustration outline |

### Fonts (generate with lv_font_conv, 4 bpp, range `TEXT` + the FontAwesome symbols already listed in make_assets.py)
| LVGL name | Face | Size | Use |
|---|---|---|---|
| `font_h1` | Fredoka SemiBold | 40 | page titles (replaces `font_temp_l`, same settings) |
| `font_label` | Fredoka SemiBold | 30 | garment labels, weather "43° now", forecast day names and highs |
| `font_body` | Fredoka Medium | 26 | advice text, tabs, forecast lows (exists) |
| `font_chip` | Fredoka SemiBold | 24 | day chip, tab labels |
| `font_small` | Fredoka Medium | 22 | secondary weather line, stale chip, settings (exists) |
| `font_tiny` | 18 | Settings fine print only (exists) |
Delete `font_temp_xl` and `font_title` if unused after the rewrite (`font_title` 32 is used by boot/setup/settings; keep it if so).

### Grid
800 x 480, side margins 28 px, gutter 12 px. All coordinates below are screen pixels.

---

## 3. Screens and exact layout

Three pages, switched with a bottom tab bar (and swipes: left = next page, right = previous):
**Today** (index 0), **Tomorrow** (1), **Next days** (2).

### 3.1 Today / Tomorrow page (same layout, different data and tint)

```
y 18  ┌ Let's get dressed!                         [wx icon] 43° now      ┐
y 70  │ (Today · Saturday)  <- day chip                       High 64° · Low 41°
y 104 │ ┌──────────┐  ┌─────────────────┐ ┌─────────────────┐
      │ │          │  │[96] Long        │ │[96] Long        │   tiles row 1 (y 108..224)
      │ │  DRESSED │  │     sleeves  (o)│ │     pants    (o)│
      │ │   CAT    │  └─────────────────┘ └─────────────────┘
      │ │ 260x250  │  ┌─────────────────┐ ┌─────────────────┐
      │ │          │  │[96] Light       │ │[96] Sneakers (o)│   tiles row 2 (y 236..352)
      │ └──────────┘  │     jacket   (o)│ │                 │
      │               └─────────────────┘ └─────────────────┘
y 362 │ [48 icon] Wear long sleeves. Bring a light jacket. Cold this
      │           morning, warm after lunch. Take your jacket off later.
y 420 │ (Today) ( Tomorrow ) ( Next days )            [Weather from 11:53 AM]
y 472 └───────────────────────────────────────────────────────────────────┘
```

**Background:** solid `BG_MORNING` on Today, `BG_EVENING` on Tomorrow.

**Title** at (28, 18), `font_h1`, `INK`, single line (`LV_LABEL_LONG_MODE_DOTS`, width 470):
- Today page, before 12:00: "Let's get dressed!"; 12:00 and later: "What to wear today".
- Tomorrow page: "Ready for tomorrow?"
- All garments checked: Today "All dressed. Have a great day!"; Tomorrow "All ready for tomorrow!" (shrink to `font_label` if it doesn't fit 470 px).

**Day chip** at (28, 70): pill (radius circle), height 36, horizontal padding 16, `font_chip`.
Text "Today · Saturday" (`CHIP_M_*`) or "Tomorrow · Sunday" (`CHIP_E_*`). This is the "which day" answer, so it must never be hidden.

**Weather block**, right-aligned to x 772, top y 18:
- Weather icon 72 px (existing `img_wx_*_s`), left of two text lines.
- Line 1 `font_label` `INK`: Today: "43° now" (current temp, rounded); Tomorrow: "High 55°".
- Line 2 `font_small` `INK_2`: Today: "High 64° · Low 41°"; Tomorrow: "Low 44° · Rain 80%" (rain part only when daily pop >= 30).
- Always label highs and lows with words.

**Cat stage** at (24, 100), 260 x 252, feet baseline at y 352. Section 5 has the art. Tapping the cat makes it hop (existing hop anim, -1/8 of height, 170 ms up, 230 ms down) and blink.

**Garment tiles**, 3 or 4, area x 284..772, y 108..352:
- 4 tiles: 2 x 2, each 238 x 116, columns at x 284 and 534, rows at y 108 and 236.
- 3 tiles: row 1 two tiles as above, row 2 one tile 488 x 116 at (284, 236).
- Tile: radius 22, bg `TILE`, 2 px border `TILE_LINE_M/E`, no shadow.
- Icon 96 x 96 at tile (10, 10). Label at tile x 116, width (tile w - 116 - 50), `font_label`, `INK`, wraps to 2 lines max, vertically centered.
- Check circle 36 px at tile top-right (inset 12): unchecked = 3 px ring `#CFC7B8`, empty; checked = filled `OK`, white `LV_SYMBOL_OK` (in `font_chip`), tile bg `#E8F6EE`, border `#9ED8B6`.
- Whole tile is the touch target (tap toggles). Feedback immediate: check circle scales 0.6 -> 1.0 in 150 ms (`lv_anim`, transform scale).
- Order: top, bottom, layer (if any), feet.

**Advice row** y 362..414: icon 48 px at (28, 364) for the most important extra item (priority list in 4.4), text at x 90, width 682, `font_body` `INK`, 2 lines max. If it needs more than 2 lines, drop sentence A (4.5) and keep B. If there's no extra icon, the text starts at x 28.

**Bottom bar** y 420..472:
- Three tab pills from x 28, height 50, gap 10, `font_chip`, padding 22: "Today", "Tomorrow", "Next days". Active: bg `INK`, text white. Inactive: transparent, 2 px border `#D9D1C2`, text `INK`. Touch target at least 50 px tall (add `lv_obj_set_ext_click_area` 6 px).
- Right side (right-aligned at 772): stale chip only when needed (4.6): pill, `WARN_BG`, `font_small` `WARN_TX`, height 40.
- Fresh data shows **no** status text. "Updated 7:45 AM" moves into Settings.

### 3.2 Unavailable state (no data, or too old, or the day isn't covered)
- Tiles and advice row hidden. Cat shows the "puzzled" overlay (5.3).
- At x 284, y 150: "I can't check the weather right now." `font_label` `INK`, width 488, wrap. Below at y +80: "Look outside, or ask a grown-up." `font_body` `INK_2`.
- Weather block shows the icon hidden and "--".
- Stale chip: "Offline since Fri 6:10 PM" (or "No weather yet" if there was never data).
- Day chip and title stay (title Today "Let's get dressed!", Tomorrow "Ready for tomorrow?").

### 3.3 All-done state
- When every visible tile is checked: title changes (3.1), advice row text becomes "Nice job, Cleo & Lila!" (`ACCENT`, names from settings; fall back to "Nice job!" if too long for one line), advice icon becomes a heart (48 px).
- Cat reaction once: hop, happy-eyes overlay for 1.5 s, three small sparkles (16 px, existing `fx_star` art) that pop around the head and fade out over 600 ms. Then stillness. No looping.
- Unchecking any tile returns to the normal state.
- Checks are stored in RAM per **target date** (y/m/d of the outfit's day). They reset when that date changes (day rollover or mode switch to another date). No NVS.

### 3.4 Next days page
- Background `BG_FORECAST`. Title "Next few days" at (28, 18) `font_h1`.
- Three columns for forecast days 1..3 relative to the clock's date (Tomorrow, then weekday names), x = 28 + i * 252, width 236, y 92..404, tile style as 3.1 (no check circles).
- Inside each column, centered: day name `font_label` ("Tomorrow", "Monday"); date `font_small` `INK_2` ("Oct 26"); weather icon 110 px (`img_wx_*_m`); "High 61°" `font_label` `INK`; "Low 43°" `font_body` `INK_2`; if pop >= 30: drop icon + "Rain 80%" `font_body`; bottom: the day's **key garment** (layer if any, else top) as a 56 px garment icon + label `font_small` ("Light jacket"), computed by the same outfit engine for that day.
- No comparisons, no "warmer than", no repeated labels.
- Bottom tab bar identical to 3.1, "Next days" active.

### 3.5 Mode logic
- Default page by local clock: 00:00-16:59 Today, 17:00-23:59 Tomorrow.
- Tapping a tab or swiping selects a page manually. After **180 s with no touch**, it returns to the default page.
- Re-evaluate once a minute in the existing `clock_timer`: compute a key `(default page, target date, stale bucket, hour)`. Re-render only when the key changes or new weather arrives.
- If the clock isn't set yet (`ui_time_cb` returns false), use the forecast's own time as "now" and show no stale chip.

### 3.6 Settings overlay (long press anywhere, as today)
- Keep WiFi/town/refresh/close. Add a "Cat" row: three pills "Black cat", "Pink cat", "Take turns". Take turns = black on even day-of-year, pink on odd.
- Show "Weather updated 7:45 AM" here.
- Move the speed test trigger here (pill "Speed test") since the footer label goes away. Keep `ui_set_speed_cb` behavior.
- New API: `void ui_set_cat_choice(int choice); typedef void (*ui_cat_cb_t)(int choice); void ui_set_cat_cb(ui_cat_cb_t cb);` Firmware saves to NVS key `cat` (u8: 0 black, 1 pink, 2 take turns) in `main/settings.c` and calls `ui_set_cat_choice()` at boot.

---

## 4. Outfit engine: new `components/wb_ui/outfit.c/.h` (plain C, no LVGL)

### 4.1 Types
```c
typedef enum { TOP_SHORT_SLEEVES, TOP_LONG_SLEEVES } top_t;
typedef enum { BOTTOM_SHORTS, BOTTOM_LONG_PANTS } bottom_t;
typedef enum { LAYER_NONE, LAYER_HOODIE, LAYER_LIGHT_JACKET, LAYER_RAIN_JACKET, LAYER_WARM_COAT, LAYER_WINTER_COAT } layer_t;
typedef enum { FEET_SNEAKERS, FEET_RAIN_BOOTS, FEET_SNOW_BOOTS } feet_t;
enum { EX_HAT = 1, EX_GLOVES = 2, EX_UMBRELLA = 4, EX_SUNGLASSES = 8, EX_WATER = 16, EX_SCARF = 32 };
enum { CH_WARMUP = 1, CH_COOLDOWN = 2, CH_RAIN_LATER = 4, CH_RAIN_ALL_DAY = 8, CH_SNOW = 16, CH_STORM = 32, CH_WINDY = 64, CH_HOT = 128, CH_BIKE_BUS = 256 };

typedef struct {
    bool ok;                 /* false = don't prescribe (unavailable) */
    int year, month, day, wday;   /* the outfit's date */
    top_t top; bottom_t bottom; layer_t layer; feet_t feet;
    uint32_t extras, change;
    int ntiles; int tile_kind[4];  /* garment ids in display order, see garment_t */
    char advice[160];        /* sentence A + " " + sentence B */
    char advice_short[96];   /* sentence B only (fallback) */
    int extra_icon;          /* extra_icon_t or -1 */
    float t_now, hi, lo; int pop_day; int wx_code; bool is_day;  /* for the weather block */
} outfit_t;

typedef struct { int bike_bus_wday; } outfit_opts_t;

/* now: real local clock. day_offset: 0 = the clock's today, 1 = tomorrow, 2, 3 (forecast page). */
bool outfit_make(const wx_t *wx, const struct tm *now, int day_offset, const outfit_opts_t *o, outfit_t *out);
const char *garment_label(int garment);
```
`garment_t` enum (also the art order in `icons.py` `GARMENTS`): SHORT_SLEEVES, LONG_SLEEVES, SHORTS, LONG_PANTS, HOODIE, LIGHT_JACKET, RAIN_JACKET, WARM_COAT, WINTER_COAT, SNEAKERS, RAIN_BOOTS, SNOW_BOOTS, WARM_HAT, GLOVES, UMBRELLA, SUNGLASSES, WATER_BOTTLE, SCARF.
Labels: "Short sleeves", "Long sleeves", "Shorts", "Long pants", "Hoodie", "Light jacket", "Rain jacket", "Warm coat", "Winter coat", "Sneakers", "Rain boots", "Snow boots", "Warm hat", "Gloves", "Umbrella", "Sunglasses", "Water bottle", "Scarf".

### 4.2 Which hours
- `base` = (days between forecast date `wx->year/month/day` and `now` date) + `day_offset`, times 24. Use a days-from-civil helper (one exists in cat.c).
- Departure hour = 8. Window = hours 7..17 of that day.
- For the clock's today (`day_offset == 0`): window start = max(now hour, 7); if now hour >= 17, window = now..21. "Departure" = the window start.
- Need hourly data for the window: if `base + window_end > wx->nhours` or `base < 0` -> `ok = false`.
- All thresholds below are **feels-like in °F** (convert with `wx_to_f`). Gusts in mph.

### 4.3 Rules (starting values; tune with scenarios)
Inputs over the window: `f_dep` (feels at departure), `f_max`, `f_min`, `f_pm_min` (min feels after 14:00), `rain_h` = hours with pop >= 40 and kind rain/drizzle/thunder, `first_rain` hour, `snow_h` (pop >= 40, kind snow), `ice` (kind ice, pop >= 30), `thunder`, `heavy` (code 65/82), `pop_max`, `gust_max` (from `h_gust`; if missing and day_offset 0, current gust), `uv` = daily uv.
- **Top:** short sleeves if `f_max >= 72 && f_dep >= 58`, else long sleeves.
- **Bottom:** shorts if `(f_dep >= 66 && f_max >= 74) || f_max >= 82`, else long pants. Snow or ice: always long pants.
- **Layer by `f_dep`:** <= 32 winter coat; <= 45 warm coat; <= 57 light jacket; <= 64 hoodie; else none.
  Rain (`rain_h >= 2`, or `rain_h >= 1 && pop_max >= 60`) and layer is none/hoodie/light jacket -> rain jacket. Cold rain keeps the coat and adds umbrella. Snow: at least warm coat.
- **Feet:** snow or ice -> snow boots; `rain_h >= 3` or heavy -> rain boots; else sneakers.
- **Extras:** hat if `f_dep <= 38` or snow; gloves if `f_dep <= 35`, snow, ice, or bike-bus cold (bike bus weekday, departure feels <= 50, `BIKE_BUS_COLD_F` stays 50); umbrella if rain and (layer is a coat or `rain_h >= 3`); sunglasses if `uv >= 7`, dry, `f_max >= 65`; water bottle if `f_max >= 85`; scarf if `f_dep <= 32`.
- **Change flags:** WARMUP if `f_max - f_dep >= 15` and layer != none; COOLDOWN if `f_dep - f_pm_min >= 12`; RAIN_LATER if rain and `first_rain >= dep + 3`; RAIN_ALL_DAY if rain from departure; SNOW; STORM if thunder; WINDY if `gust_max >= 25`; HOT if `f_max >= 85`; BIKE_BUS as above.

### 4.4 Extra icon (48 px) priority
STORM (bolt) > UMBRELLA > GLOVES (if hat too, show the hat+gloves combined icon) > WARMUP (sun-up icon) > WATER > SUNGLASSES > WINDY > none.

### 4.5 Advice sentences (no numbers)
- **A (what):** layer none: "Wear {short sleeves|long sleeves} and {shorts|long pants}." Otherwise: "Wear {top}. {Bring|Wear} {a light jacket|a hoodie|your rain jacket|your warm coat|your winter coat}." Use "Bring" when WARMUP.
- **B (why / consequence)**, first match:
  - STORM: "Storms later. Stay inside if it thunders."
  - SNOW: "Snow day! Hat, gloves and boots."
  - RAIN_ALL_DAY: "Rainy all day. Rain boots on!" (or "Rainy all day. Bring your umbrella." with sneakers)
  - RAIN_LATER: "Rain after lunch. Keep your rain jacket handy." (pick "after lunch" if first_rain >= 12, else "later this morning")
  - WARMUP: "{Cold|Cool} this morning, warm after lunch. Take your {layer noun} off later." (Cold if f_dep <= 45)
  - COOLDOWN: "Warm now, chilly later. Bring a layer."
  - BIKE_BUS: "Bike bus today! Wear your gloves." (Tomorrow page: "Bike bus tomorrow! Lay out your gloves.")
  - WINDY: "Windy! Zip up your {layer noun}." (no layer: "Windy today!")
  - HOT: "Hot today. Drink lots of water."
  - else: empty.
- Tomorrow page wording: replace "this morning" with "in the morning", "today" with "tomorrow".
- `advice` = A + " " + B. `advice_short` = B (or A if B empty). UI picks per 3.1.

### 4.6 Freshness rules (in the UI, using `wx` time vs clock)
- `age` = clock now minus the forecast's `current.time` (wx year..minute, local).
- `age < 90 min`: fresh, no chip.
- `90 min <= age < 12 h`: show outfit, stale chip "Weather from 11:53 AM" (add the weekday if the date differs: "Weather from Fri 6:10 PM").
- `age >= 12 h`, or `!wx->valid`, or `outfit_make()` returns false: unavailable state (3.2), chip "Offline since Fri 6:10 PM".
- net.c's footer string is no longer shown on the main screen.

---

## 5. Artwork (tools/icons.py), new full-body cat

### 5.1 Style
Same sticker style as the existing art: navy outline `#2B3450` drawn fat underneath, flat fill on top, a single soft highlight or shade per shape at most, round joins. Outline width 2.6 units at stage scale (1 unit = 1 px). Every garment on the cat and its tile icon share colors and details, so the tile visibly is "what the cat is wearing".

### 5.2 Stage and proportions (`CAT2_W, CAT2_H = 260, 252`; cat centered at x 150 so props fit at x 0..70)
- Head: ellipse center (150, 74), rx 60, ry 50; ears triangles to (106, 20) and (194, 20); inner ears pink `#FF9EC4`. Eyes ellipses at (128, 76) and (172, 76), rx 9, ry 11 (black cat: `#FFD23F`; pink cat: `#7FD7FF`), navy pupils, white glint. Pink nose, small "w" mouth, cheeks blush, whiskers white (black cat) / navy (pink cat).
- Torso: rounded rect 80 x 78 at (110, 122).
- Arms: capsules from shoulders (116, 134) and (184, 134) to paws at (100, 190) and (200, 190), radius 13; paws circles r 14.
- Legs: capsules (134, 196)->(134, 236) and (166, 196)->(166, 236), radius 14; feet ellipses at y 240, rx 18, ry 10.
- Tail: thick curve from (190, 205) out to (238, 150), radius 9, behind the body.
- Base colors: black cat `#2E2E3A`, pink cat `#FFB3CF` (matches the 3D-printed case's two cats).
- Overlays: `blink` (closed eyes), `happy` (^ ^ eyes + open smile), `puzzled` (one raised eyebrow + a small "?" thought bubble at top-right of the head, x 205..250, y 0..50).

### 5.3 Layers (each a function returning an SVG on the stage; make_assets crops and records offsets)
Stacking order in LVGL (bottom to top): tail+body base, bottoms, feet, top, layer (outer), scarf, gloves, held item, face (sunglasses), hat, expression overlay. Hoods are drawn as a collar ring around the neck, never behind the head.
| Layer id | Look |
|---|---|
| `top_short` | t-shirt `#FF8FB1` with white heart on chest, sleeves to mid upper arm |
| `top_long` | long-sleeve shirt, stripes `#7FD1AE` / `#FFFFFF`, sleeves to the paws |
| `bottom_shorts` | denim `#4A7BD9` shorts to mid thigh |
| `bottom_pants` | denim `#4A7BD9` long pants to the ankles, cuff line |
| `feet_sneakers` | white sneakers, pink `#FF5FA2` side stripe, navy sole |
| `feet_rain` | yellow `#FFD23F` rain boots to mid shin |
| `feet_snow` | brown `#8B5E3C` boots with cream `#F4F1EA` fur cuff |
| `layer_hoodie` | lavender `#9C8CFF` hoodie over the top, pocket, collar-ring hood |
| `layer_light` | orange `#FFB454` zip jacket, open collar, two pockets |
| `layer_rain` | yellow `#FFD23F` raincoat with toggles, collar-ring hood |
| `layer_warm` | blue `#6F8CFF` puffer, 3 quilt lines |
| `layer_winter` | deep blue `#3D7FE0` long puffer, cream fur collar |
| `hat_warm` | pink `#FF7EB3` beanie with white pompom (reuse current beanie style) |
| `hat_witch`, `hat_santa` | seasonal (5.4) |
| `gloves` | lavender mittens replacing paws |
| `held_umbrella` | open pink `#FF5C8A` umbrella held in the cat's left paw (viewer's right), canopy above and right of the head; must not cover eyes or tiles |
| `held_water` | water bottle in the right paw (viewer's left) |
| `face_sun` | heart sunglasses `#FF5FA2` (existing style) |
| `scarf` | teal `#2EC4B6` scarf with white stripes |
| `prop_pumpkin`, `prop_menorah` (+9 flame positions), `prop_present` | seasonal props at stage x 0..70, y 180..252 |

### 5.4 Seasons (keep the old rules, but the outfit always wins)
October: witch hat **only if no warm hat is recommended**, pumpkin prop. Hanukkah (`cat_hanukkah_candles()`): menorah with that night's candles (existing right-to-left order). Dec 15-26: Santa hat only if no warm hat recommended, present prop. Drop the cape. Never cover a recommended garment.

### 5.5 Garment tile icons (96 px) and extra icons (48 px)
- `GARMENTS` dict in the `garment_t` order: flat-lay versions of the exact garments above (same colors/details), filling the 100-unit icon box like the current clothing icons. Also render each at 56 px for the forecast page.
- `EXTRA_ICONS`: bolt, umbrella, hat+gloves (combined), gloves, sun-up (small sun with an up arrow), water bottle, sunglasses, wind (three curved lines), heart.

### 5.6 make_assets.py changes
- Render cat layers on the 260 x 252 stage at 1x, crop with `render_cropped()`, convert RGB565A8. Generate `assets/cat2_sets.c` with `const wb_cat2_t wb_cat2_black, wb_cat2_pink` where the shared clothing layers are one copy (only `base`, `blink`, `happy`, `puzzled` differ per color). Emit an enum header `cat2_layers.h` (`WB_C2_TOP_SHORT ...`) so C indexes layers by id.
- Garments `img_g_<name>_96` and `_56`; extras `img_x_<name>`.
- Fonts per section 2. Remove `font_temp_xl`, the old `img_kitty_*` set, `img_wear_*`, `img_fx_*` (if fx code is deleted), `img_drop` if unused. Keep `img_cat_black/pink` (boot/setup screens) and weather icons.
- Budget: report total asset bytes. Target new art at 1.5 MB or less. If over, try LVGL's RLE compression (`LV_USE_RLE`, LVGLImage `--compress RLE`) for the cat layers, since they have large flat areas.

---

## 6. Data changes
- `wb_forecast_url()`: append `wind_gusts_10m` to `&hourly=` (adds 16 chars; `make test` must still pass).
- `wx_t`: add `float h_gust[WX_MAX_HOURS];` (in display wind units: mph or km/h), parse in `wx_parse_openmeteo` like the others (default = current gust).
- `sim/make_scenarios.py`: emit hourly `wind_gusts_10m`.
- Check `net_http_get` max body size handles the about 1 KB larger response.

---

## 7. UI implementation steps (ui.c)
1. Add `outfit.c` to `components/wb_ui/CMakeLists.txt` and the sim Makefiles (`sim/Makefile` for `render`, and the web target).
2. In ui.c delete: theme table/`theme_for`, `fx_*` and `build_fx`, bubble code, moment cards, `build_today`, `build_days`, `apply_theme`, `style_card`, `card_fill`, `sky_at`, `update_dots`, dots, greeting/time labels, old kitty code (`kitty_*`, `build_kitty`, `kitty_show`).
3. Add state: `static int s_page, s_default_page; static uint32_t s_last_touch; static uint8_t s_checks; static int s_check_y, s_check_m, s_check_d; static int s_cat_choice; static wx_t s_wx_copy` (store a copy, or a pointer if net.c's `s_wx` lifetime is guaranteed).
4. Build functions: `build_page_outfit()` (header, chip, weather block, cat stage with all layer image objects pre-created and hidden, 4 tiles, unavailable labels, advice row), `build_page_forecast()`, `build_tabs()` (on `lv_layer_top()` or `scr_main`, shared by pages), `build_stale_chip()`.
5. `render()` computes now, page, freshness, calls `outfit_make()` for day_offset 0 or 1 (and 1..3 for the forecast page), then sets texts, tile icons/labels/positions (3 vs 4 layout), check visuals, cat layers (`cat2_apply(const outfit_t*)`: show/hide/set src+pos per layer, base by cat choice), season props, stale chip, background color.
6. Events: tile `LV_EVENT_CLICKED` toggles bit, updates visual, checks all-done -> `celebrate()`. Tabs set `s_page`, `s_last_touch`. Global indev event updates `s_last_touch` on every press. Swipe handler now moves `s_page` +-1 (clamped 0..2).
7. `clock_timer` (1 s): if minute changed -> recompute the key (3.5); revert manual page after 180 s idle; re-render on change. Day rollover resets checks via the target-date compare.
8. `ui_show_weather(wx)`: copy, re-render. `ui_set_footer()`: keep the signature, store text for Settings only.
9. Settings: add cat pills + speed test pill + "Weather updated ..." line.
10. Keep `ui.h` API stable for net.c; add the new cat setters.

Firmware side: `main/settings.c` (NVS `cat`), `main/net.c`/`main.c` wire `ui_set_cat_cb` -> save, and `ui_set_cat_choice()` at boot. Bump `WB_VERSION` to "2.0.0".

---

## 8. Mockups first, then verification

### 8.1 Scenarios to add in `sim/make_scenarios.py`
| Name | Data | Clock |
|---|---|---|
| `brief_morning` | Tue 2026-10-27. Feels 36 °F at 8 AM rising to 63 °F at 2 PM (temps about 40 -> 66), dry (codes 1/2), uv 4. Wed: rain code 63, pop 80% 10:00-17:00, high 55, low 46, gusts 20 mph | 07:35 Tue (forecast time 07:30) |
| `brief_evening` | same data | 19:30 Tue (forecast time 19:15) |
| `stale_2h` | same data | clock 13:39, forecast time 11:45 |
| `too_old` | forecast time Mon 18:00 | clock Tue 07:30 (13.5 h) |
| `rollover` | forecast time 23:50 | clock 00:10 next day |
| `snow_day`, `hot_summer`, `storm_afternoon`, `bike_bus_cold_friday`, `halloween_cold` (beanie must win over the witch hat), `hanukkah_night3` | | |
| `long_text` | names "Cleo & Lila & Friends", the longest advice combination (WARMUP + rain + windy) | |
`sim_main.c` needs a way to set the clock independently of the forecast time (today it uses forecast time + 7 min). Add an optional `"sim_clock": "2026-10-27T19:30"` field in the scenario JSON, read by the renderer only. Under `#ifdef WB_SIM` add `ui_test_set_checks(uint8_t mask)` and `ui_test_set_page(int)` hooks.

### 8.2 Renders (`make shots`)
`20_morning`, `21_evening`, `22_evening_2_checked`, `23_evening_all_done`, `24_next_days`, `25_stale`, `26_unavailable`, `27_rollover`, `28_snow`, `29_hot`, `30_storm`, `31_halloween`, `32_pink_cat`, `33_settings`, `34_long_text`. **Send 20 and 21 to Boris as the mockups before polishing the rest.**

### 8.3 Checks
- Five-second test on 20/21/28/29: can you read outfit, extra layer, day.
- Distance check: downscale renders to 33% and 25% (`PIL`, LANCZOS) to approximate 1.5-2 m; tile labels, day chip and title must stay readable.
- Long text: no clipping, max 2 lines in labels and advice; title shrinks.
- Missing data: `nhours` short, `ndays` 1, pop all zero, no gust array.
- Offline: stale chip at 90 min, unavailable at 12 h; outfit still correct after midnight with stale data (date offset).
- Day rollover: checks reset; default page flips at 17:00 and 00:00.
- Touch: tile toggle, tabs, swipe, 180 s auto-return, long press opens Settings, cat tap hops.
- Performance: no timers running when idle except clock (1 s) and blink (50 ms; consider 100 ms). Full-screen redraw only on page change.
- `make test` (request size) passes. `idf.py build` passes; `idf.py size`: app at 3.5 MB or less.
- On the board: flash the **update image at 0x10000** (keeps WiFi and town): esptool-js at https://espressif.github.io/esptool-js/, UART port, 921600 baud, address 0x10000, file `build/weather_besties.bin`, Program, then **Disconnect** and press **RESET**. Serial log: internal RAM free after a fetch should stay at 64 KB or more.
- Update README (features, screenshots `docs/today.png` etc. from the new renders), `firmware/` images (full image at 0x0 via `esptool merge_bin`, update at 0x10000), commit, push to `main`.

---

## 9. Separate pending items
1. **Demo GIF for the README** (paused): from Boris's 10.6 s, 480x360 phone video of the real board. `ffmpeg -i in.mov -an -vf "fps=12,scale=480:-2:flags=lanczos,split[a][b];[a]palettegen=max_colors=192:stats_mode=diff[p];[b][p]paletteuse=dither=sierra2_4a:diff_mode=rectangle" docs/demo.gif`, keep it under about 5 MB, add near the top of README under a heading like "Running on the real board". The video must be re-attached (it isn't in the repo).
2. **Board was offline for 2 h** in that video ("Offline. Last update 11:53 AM" at 1:39 PM). Earlier the cause was WiFi beacon timeouts while powered from a computer USB port. Check the serial log; if it's RF, a wall charger or moving the board closer to the router helped before. The redesign's stale handling makes this visible but doesn't fix the cause.

---

## 10. Original brief (verbatim, from Boris)

Redesign this as a world-class, beautifully crafted clothing guide for my 6- and 8-year-old children, Cleo and Lila, who love cats. It runs on an embedded display using an RTOS and C. Work within the existing hardware and rendering framework.

The core purpose is to help them **get dressed in the morning and lay out tomorrow's clothes in the evening.** Weather supports that decision. Aim for Apple-level care in composition, typography, illustration, spacing, interaction, and polish, adapted for children.

**Critique of the current design.** The current app is charming, but its hierarchy serves a weather dashboard: the temperature dominates, the clothing icon is tiny inside a largely empty speech bubble, and the cat is squeezed between other elements. Pale cards and small secondary text weaken readability. The forecast page contains too many competing details, including repetitive labels and temperature comparisons with changing baselines. Please rethink the composition rather than merely adjusting those elements.

**Product direction.**
1. Make a dressed cat the centerpiece. Create a coherent, expressive cat character wearing the recommended clothing. Beside it, display three or four large garment illustrations with short, clear labels, for example "Long sleeves," "Long pants," "Light jacket," and "Sneakers." The cat creates emotional appeal; the garment list makes the instructions unmistakable. Keep the outfit recommendations visually dominant.
2. Create two purposeful modes. Morning: "Let's get dressed!" Show what to wear today, accounting for the morning departure and the conditions later in the day. Evening: "Ready for tomorrow?" Show tomorrow's outfit so they can lay it out before bed. Clearly identify the day the outfit is for. Switch the default mode based on local time, while allowing manual switching if the hardware supports it. Tomorrow's recommendations must use tomorrow's forecast.
3. Give direct, useful advice. Prefer "Wear long sleeves. Bring a light jacket" to "Chilly afternoon." When conditions change, explain the practical consequence briefly: "Cool in the morning, warm after lunch. Take your jacket off later." Account for relevant temperature changes, rain, and wind using available data. Avoid overwhelming them with numbers or explanations.
4. Make cats integral to the experience. Use consistent proportions, outlines, shading, and expressions across all artwork. Clothing should look intentionally fitted to the character. Seasonal details are welcome, but must not obscure the recommended outfit. If feasible, let each child choose a cat without adding clutter to the main screen.

**Visual direction.** Create a calm, joyful, premium aesthetic: warm ivory or very pale sky backgrounds, deep ink-colored text, and a restrained accent palette; one highly legible, friendly type family with a clear hierarchy; generous spacing, deliberate alignment, consistent margins, and minimal unnecessary containers; beautiful, cohesive illustrations that remain recognizable at the panel's actual size; strong contrast for all meaningful text and symbols; short, encouraging language appropriate for both ages, without feeling babyish. Avoid a generic dashboard, excessive rounded cards, mismatched clip art, decorative gradients, tiny captions, or constant animation. Every element should earn its space. The reading order should be: **Recommended outfit -> when it's for -> important extra item or layer -> supporting weather.** Keep detailed forecasts on a secondary screen. Use clearly labeled highs and lows if shown. Remove low-value repetition and comparison math. Navigation should be understandable without relying solely on tiny dots.

**Interaction and reliability.** If touch is available, consider letting the children check off garments as they lay them out. Finish with a subtle cat reaction and "All ready for tomorrow." Make touch targets generous and feedback immediate. The screen must also be useful without interaction. Handle stale forecasts honestly. Keep cached information usable, but visibly distinguish it from current data. When data is too old or unavailable, do not confidently prescribe an outfit. Keep status messaging legible and within safe screen margins.

**Implementation and review.** First inspect the existing code, display resolution, graphics framework, memory budget, assets, and input capabilities. Do not assume browser features or propose replacing the platform unnecessarily. Favor excellent static composition and lightweight, purposeful animation. Create polished morning and evening mockups at the actual display aspect ratio before implementing the redesign. Use realistic example conditions to demonstrate a cold morning with a warmer afternoon and a rainy tomorrow. Treat those as examples, not live weather. Then implement and verify on the target display, including long text, missing data, offline operation, and day rollover. Check readability from the distance at which the children will actually use it.

The acceptance test is simple: **Can either child look at the screen for five seconds and tell me what clothes to pick, whether they need an extra layer, and which day the outfit is for?** Preserve the warmth and personality of the original, but make the cat and clothing guidance the unmistakable heart of the product.
