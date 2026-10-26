/*
 * Desktop renderer: runs the real UI code (ui.c, wx.c, outfit.c, same fonts
 * and images, same lv_conf.h) and saves each screen as a raw RGB565 frame.
 * Usage: ./render <outdir> <scenario dir>
 *
 * A scenario JSON may carry "sim_clock": "2026-10-27T19:30" to set the wall
 * clock independently of the forecast's own time (the plan's stale/rollover
 * cases). Without it the clock is the forecast time + 7 minutes, as before.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cJSON.h"
#include "cal.h"
#include "lvgl.h"
#include "ui.h"
#include "wx.h"

static uint16_t fb[UI_W * UI_H];
static uint32_t ms;
static struct tm now_tm;
static bool now_valid;

static uint32_t tick_cb(void) { return ms; }

static void flush_cb(lv_display_t *d, const lv_area_t *a, uint8_t *px)
{
    (void)a;
    (void)px;
    lv_display_flush_ready(d);
}

static bool time_cb(struct tm *out)
{
    if (!now_valid) {
        return false;
    }
    *out = now_tm;
    return true;
}

static void pump(int frames)
{
    for (int i = 0; i < frames; i++) {
        ms += 40;
        lv_timer_handler();
    }
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(NULL);
}

static void save(const char *dir, const char *name)
{
    pump(30);
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.rgb565", dir, name);
    FILE *f = fopen(path, "wb");
    fwrite(fb, 2, UI_W * UI_H, f);
    fclose(f);
    printf("saved %s\n", path);
}

/* Text dump of the state just saved (ui_test_dump): check_renders.py
 * asserts on the words (advice sentences, tile/chip labels, the stale
 * header, the settings info lines) as well as the pixels. Call right
 * after the save it belongs to, before any state changes. */
static void dump(const char *dir, const char *name)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.txt", dir, name);
    ui_test_dump(path);
}

static char *slurp(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "missing %s\n", path);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    *len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *b = malloc(*len + 1);
    fread(b, 1, *len, f);
    b[*len] = 0;
    fclose(f);
    return b;
}

static wx_t wx;

/* "2026-10-27T19:30" -> tm (wday filled by wx_day_of_week). */
static bool parse_sim_clock(const char *j, struct tm *out)
{
    cJSON *root = cJSON_Parse(j);
    if (!root) {
        return false;
    }
    const cJSON *sc = cJSON_GetObjectItemCaseSensitive(root, "sim_clock");
    bool ok = false;
    if (cJSON_IsString(sc) && sc->valuestring) {
        int y, mo, d, h, mi;
        if (sscanf(sc->valuestring, "%d-%d-%dT%d:%d", &y, &mo, &d, &h, &mi) == 5) {
            memset(out, 0, sizeof(*out));
            out->tm_year = y - 1900;
            out->tm_mon = mo - 1;
            out->tm_mday = d;
            out->tm_hour = h;
            out->tm_min = mi;
            out->tm_wday = wx_day_of_week(y, mo, d);
            ok = true;
        }
    }
    cJSON_Delete(root);
    return ok;
}

static void load(const char *dir, const char *name)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.json", dir, name);
    size_t len;
    char *j = slurp(path, &len);
    if (!wx_parse_openmeteo(j, len, true, &wx)) {
        fprintf(stderr, "parse failed: %s\n", name);
        exit(1);
    }
    struct tm clk;
    if (parse_sim_clock(j, &clk)) {
        now_tm = clk;
    } else {
        memset(&now_tm, 0, sizeof(now_tm));
        now_tm.tm_year = wx.year - 1900;
        now_tm.tm_mon = wx.month - 1;
        now_tm.tm_mday = wx.day;
        now_tm.tm_hour = wx.hour;
        now_tm.tm_min = wx.minute + 7 > 59 ? 59 : wx.minute + 7;
        now_tm.tm_wday = wx.wday;
    }
    now_valid = true;
    free(j);
    printf("%s: clock %04d-%02d-%02d %02d:%02d, %d hours, %d days\n", name, now_tm.tm_year + 1900,
           now_tm.tm_mon + 1, now_tm.tm_mday, now_tm.tm_hour, now_tm.tm_min, wx.nhours, wx.ndays);
}

static void show(void)
{
    ui_show_weather(&wx);
    ui_refresh_clock();
    pump(10);
}

/* ------------------------------------------------------------------ My Day fixtures (iter-14)
 * Invented events only (this repo is public), built relative to the
 * sim clock currently loaded, so the Today/Tomorrow sections land on
 * the rendered dates. The renderer process runs in the scenario's
 * zone, like the board, so the page's localtime agrees with the
 * instants cal.c resolves (TZID ones through the VTIMEZONE block
 * below, since iter-29). File order is scrambled on purpose: cal.c sorts. One summary
 * is line-folded and one location carries an escaped comma, so the
 * renders also prove unfolding and unescaping. */
static void ymd(char *buf, const struct tm *t)
{
    sprintf(buf, "%04d%02d%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
}

/* The VTIMEZONE block of a Google Calendar export for
 * America/New_York, verbatim in shape (iter-29): since this block
 * rides along, the fixtures' TZID date-times resolve through the
 * feed's own observances in cal.c, not through the renderer
 * process's zone, and the asserted row strings below are exact
 * local times, the fall-back render (46) included. */
static const char VTZ_NY[] =
    "BEGIN:VTIMEZONE\r\nTZID:America/New_York\r\nX-LIC-LOCATION:America/New_York\r\n"
    "BEGIN:DAYLIGHT\r\nTZOFFSETFROM:-0500\r\nTZOFFSETTO:-0400\r\nTZNAME:EDT\r\n"
    "DTSTART:19700308T020000\r\nRRULE:FREQ=YEARLY;BYMONTH=3;BYDAY=2SU\r\nEND:DAYLIGHT\r\n"
    "BEGIN:STANDARD\r\nTZOFFSETFROM:-0400\r\nTZOFFSETTO:-0500\r\nTZNAME:EST\r\n"
    "DTSTART:19701101T020000\r\nRRULE:FREQ=YEARLY;BYMONTH=11;BYDAY=1SU\r\nEND:STANDARD\r\n"
    "END:VTIMEZONE\r\n";

static void fixture_cal(bool with_events)
{
    struct tm today = now_tm, tom = now_tm, later = now_tm;
    tom.tm_mday += 1;
    tom.tm_isdst = -1;
    mktime(&tom);
    later.tm_mday += 10;
    later.tm_isdst = -1;
    mktime(&later);
    char d0[9], d1[9], d10[9];
    ymd(d0, &today);
    ymd(d1, &tom);
    ymd(d10, &later);
    char ics[4096];
    int n = 0;
    n += snprintf(ics + n, sizeof(ics) - n,
                  "BEGIN:VCALENDAR\r\nPRODID:-//WeatherBesties Sim//EN\r\nVERSION:2.0\r\n");
    n += snprintf(ics + n, sizeof(ics) - n, "%s", VTZ_NY);
    if (with_events) {
        n += snprintf(ics + n, sizeof(ics) - n,
                      "BEGIN:VEVENT\r\nUID:sim-1\r\nDTSTART;TZID=America/New_York:%sT153000\r\n"
                      "DTEND;TZID=America/New_York:%sT163000\r\nSUMMARY:Cleo's soccer practice\r\n"
                      "LOCATION:Roosevelt Field\r\nEND:VEVENT\r\n",
                      d0, d0);
        n += snprintf(ics + n, sizeof(ics) - n,
                      "BEGIN:VEVENT\r\nUID:sim-2\r\nDTSTART;VALUE=DATE:%s\r\nDTEND;VALUE=DATE:%s\r\n"
                      "SUMMARY:Picture Day\r\nEND:VEVENT\r\n",
                      d0, d1);
        n += snprintf(ics + n, sizeof(ics) - n,
                      "BEGIN:VEVENT\r\nUID:sim-3\r\nDTSTART:%sT200000Z\r\nDTEND:%sT204500Z\r\n"
                      "SUMMARY:Video call with Grandma\r\nEND:VEVENT\r\n",
                      d0, d0);
        n += snprintf(ics + n, sizeof(ics) - n,
                      "BEGIN:VEVENT\r\nUID:sim-4\r\nDTSTART;TZID=America/New_York:%sT100000\r\n"
                      "DTEND;TZID=America/New_York:%sT104500\r\nSUMMARY:Lila's library story time\r\nEND:VEVENT\r\n",
                      d0, d0);
        n += snprintf(ics + n, sizeof(ics) - n,
                      "BEGIN:VEVENT\r\nUID:sim-5\r\nDTSTART;TZID=America/New_York:%sT160000\r\n"
                      "DTEND;TZID=America/New_York:%sT163000\r\nSUMMARY:Cleo's piano les\r\n son\r\n"
                      "LOCATION:Music school\r\nEND:VEVENT\r\n",
                      d1, d1);
        n += snprintf(ics + n, sizeof(ics) - n,
                      "BEGIN:VEVENT\r\nUID:sim-6\r\nDTSTART;TZID=America/New_York:%sT080000\r\n"
                      "DTEND;TZID=America/New_York:%sT090000\r\nSUMMARY:School bake sale\r\n"
                      "LOCATION:Watchung School gym\r\nEND:VEVENT\r\n",
                      d1, d1);
    } else {
        /* A real event, just not today or tomorrow: the empty state
         * must come from the dates, not from an empty feed. */
        n += snprintf(ics + n, sizeof(ics) - n,
                      "BEGIN:VEVENT\r\nUID:sim-9\r\nDTSTART;TZID=America/New_York:%sT090000\r\n"
                      "DTEND;TZID=America/New_York:%sT100000\r\nSUMMARY:School play rehearsal\r\nEND:VEVENT\r\n",
                      d10, d10);
    }
    n += snprintf(ics + n, sizeof(ics) - n, "END:VCALENDAR\r\n");
    if (!cal_parse(ics, (size_t)n)) {
        fprintf(stderr, "fixture calendar did not parse\n");
        exit(1);
    }
}

/* Iter-29: the fall-back weekend, built against the loaded clock
 * (halloween_cold: Sat Oct 31 2026), so tomorrow is Sun Nov 1,
 * when America/New_York repeats the 1 AM hour. The asserted rows
 * pin the resolved instants end to end: 12:30 AM is still EDT,
 * 1:30 AM is the FIRST 1:30 (EDT), 3:30 AM is EST, and today's
 * "Z" call lands at 1 PM EDT. Invented events, like fixture_cal. */
static void fixture_cal_dst(void)
{
    struct tm today = now_tm, tom = now_tm;
    tom.tm_mday += 1;
    tom.tm_isdst = -1;
    mktime(&tom);
    char d0[9], d1[9];
    ymd(d0, &today);
    ymd(d1, &tom);
    char ics[4096];
    int n = 0;
    n += snprintf(ics + n, sizeof(ics) - n,
                  "BEGIN:VCALENDAR\r\nPRODID:-//WeatherBesties Sim//EN\r\nVERSION:2.0\r\n");
    n += snprintf(ics + n, sizeof(ics) - n, "%s", VTZ_NY);
    n += snprintf(ics + n, sizeof(ics) - n,
                  "BEGIN:VEVENT\r\nUID:sim-d1\r\nDTSTART;TZID=America/New_York:%sT100000\r\n"
                  "DTEND;TZID=America/New_York:%sT110000\r\nSUMMARY:Pumpkin patch visit\r\n"
                  "LOCATION:Chestnut Hill Farm\r\nEND:VEVENT\r\n",
                  d0, d0);
    n += snprintf(ics + n, sizeof(ics) - n,
                  "BEGIN:VEVENT\r\nUID:sim-d2\r\nDTSTART;TZID=America/New_York:%sT003000\r\n"
                  "DTEND;TZID=America/New_York:%sT010000\r\nSUMMARY:Moonlight owl walk\r\nEND:VEVENT\r\n",
                  d1, d1);
    n += snprintf(ics + n, sizeof(ics) - n,
                  "BEGIN:VEVENT\r\nUID:sim-d3\r\nDTSTART;TZID=America/New_York:%sT013000\r\n"
                  "DTEND;TZID=America/New_York:%sT023000\r\nSUMMARY:Fall-back pyjama jam\r\nEND:VEVENT\r\n",
                  d1, d1);
    n += snprintf(ics + n, sizeof(ics) - n,
                  "BEGIN:VEVENT\r\nUID:sim-d4\r\nDTSTART;TZID=America/New_York:%sT033000\r\n"
                  "DTEND;TZID=America/New_York:%sT043000\r\nSUMMARY:Sunrise pancake breakfast\r\nEND:VEVENT\r\n",
                  d1, d1);
    n += snprintf(ics + n, sizeof(ics) - n,
                  "BEGIN:VEVENT\r\nUID:sim-d5\r\nDTSTART:%sT170000Z\r\nDTEND:%sT174500Z\r\n"
                  "SUMMARY:Video call with Grandpa\r\nEND:VEVENT\r\n",
                  d0, d0);
    n += snprintf(ics + n, sizeof(ics) - n, "END:VCALENDAR\r\n");
    if (!cal_parse(ics, (size_t)n)) {
        fprintf(stderr, "DST fixture calendar did not parse\n");
        exit(1);
    }
}

/* Records the fixture fetch at a fixed time on the sim clock, so
 * the My Day freshness line renders deterministically whatever
 * the scenario clock says (iter-15). The clock is restored before
 * anything renders for keeps. */
static void fixture_fetched_at(int hour, int min)
{
    struct tm save = now_tm;
    now_tm.tm_hour = hour;
    now_tm.tm_min = min;
    ui_show_calendar();
    now_tm = save;
}

/* The usual pin: a 7:02 AM fetch ("Updated 7:02 AM"). */
static void fixture_fetched(void)
{
    fixture_fetched_at(7, 2);
}

int main(int argc, char **argv)
{
    const char *out = argc > 1 ? argv[1] : "out";
    const char *sc = argc > 2 ? argv[2] : "scenarios";

    lv_init();
    lv_tick_set_cb(tick_cb);
    lv_display_t *d = lv_display_create(UI_W, UI_H);
    lv_display_set_buffers(d, fb, NULL, sizeof(fb), LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(d, flush_cb);

    ui_init("Cleo & Lila");
    ui_set_time_cb(time_cb);
    ui_info_t info = {.wifi = "HomeWiFi", .place = "Upper Montclair, New Jersey", .ip = "192.168.1.42",
                      .updated = "Weather updated 7:30 AM", .version = "2.0.0"};
    ui_set_info(&info);

    ui_show_boot(NULL);
    save(out, "01_boot");

    ui_show_setup("WeatherBesties-3F2A", "192.168.4.1");
    save(out, "02_setup");

    /* The brief's two mockups: cold morning warming up; rainy tomorrow laid out
     * the evening before. */
    load(sc, "brief_morning");
    show();
    save(out, "20_morning");
    dump(out, "20_morning");

    load(sc, "brief_evening");
    show();
    save(out, "21_evening");
    dump(out, "21_evening");

    ui_test_set_checks(0x3); /* first two garments laid out (2 of 5: the
                              * umbrella extra is a checkable chip too) */
    pump(10);
    save(out, "22_evening_2_checked");
    dump(out, "22_evening_2_checked");   /* iter-10: the progress pill's screen */

    ui_test_set_checks(0x1F); /* everything laid out: 4 tiles + the
                               * umbrella chip (bit 4). All-done must not
                               * fire on the tiles alone any more. The
                               * rain sprite stays static, but iter-11
                               * overlays the heart gesture on her chest
                               * (the animation itself exists only for
                               * the black dress sprite and is never
                               * stretched over another outfit). */
    pump(10);
    save(out, "23_evening_all_done");
    dump(out, "23_evening_all_done");
    ui_test_set_checks(0);

    /* Pippa proof still: her all-done heart animation plays on its own
     * in the renderer, on the black dress sprite only. The dress
     * scenario's outfit is the dress (4 tiles since iter-08 itemised
     * the tights and dress shoes, no chips since iter-06 canonicalised
     * the prescription to what the sprite wears); lay everything out
     * and pin frame 4 (the eyes-closed heart hold) so the screenshot
     * set shows the gesture deterministically. (The brief morning used
     * to double as this proof; its outfit changed with the panel
     * rounds, most recently to a dress day in iter-07, and the heart
     * frames were never any other sprite's.) */
    load(sc, "dress_morning");
    show();
    ui_test_set_checks(0x3F);
    pump(10);
    ui_test_set_pippa_frame(4);
    pump(10);
    save(out, "23b_morning_all_done_heart");
    ui_test_set_pippa_frame(-1);
    ui_test_set_checks(0);

    load(sc, "brief_morning");
    show();
    ui_test_set_page(3);   /* iter-14: forecast is the fourth page now */
    pump(10);
    save(out, "24_next_days");
    dump(out, "24_next_days");   /* iter-09: the fc*_tiles lines assert the cards' full outfits */
    ui_test_set_page(0);

    load(sc, "stale_2h");
    show();
    save(out, "25_stale");
    dump(out, "25_stale");

    /* Iter-11: a stale prescription is not checkable. Drive the real
     * toggle paths exactly as taps would (a tile and the scarf chip);
     * both must be swallowed by the stale lock, so the screen after
     * them is the stale screen unchanged. */
    ui_test_tap_tile(0);
    ui_test_tap_chip(0);
    pump(10);
    save(out, "25c_stale_tapped");
    dump(out, "25c_stale_tapped");

    /* iter-10: the same stale data on the forecast page: the status
     * pill's header-band placement there, and the forecast ground
     * following the outfit pages' tint rule. */
    ui_test_set_page(3);
    pump(10);
    save(out, "25b_stale_next_days");
    dump(out, "25b_stale_next_days");
    ui_test_set_page(0);

    load(sc, "too_old");
    show();
    save(out, "26_unavailable");
    dump(out, "26_unavailable");

    /* iter-10: the unavailable state's Tomorrow variant: the
     * headline must carry the state on this page too. */
    ui_test_set_page(1);
    pump(10);
    save(out, "26b_unavailable_tomorrow");
    dump(out, "26b_unavailable_tomorrow");
    ui_test_set_page(0);

    load(sc, "rollover");
    show();
    save(out, "27_rollover");
    dump(out, "27_rollover");

    load(sc, "snow_day");
    show();
    save(out, "28_snow");
    dump(out, "28_snow");

    load(sc, "hot_summer");
    show();
    save(out, "29_hot");
    dump(out, "29_hot");

    load(sc, "storm_afternoon");
    show();
    save(out, "30_storm");
    dump(out, "30_storm");

    load(sc, "halloween_cold");
    show();
    save(out, "31_halloween");
    dump(out, "31_halloween");

    load(sc, "brief_morning");
    ui_set_cat_choice(1); /* pink cat */
    show();
    save(out, "32_pink_cat");
    dump(out, "32_pink_cat");
    ui_set_cat_choice(0);

    ui_open_settings();
    save(out, "33_settings");
    dump(out, "33_settings");
    ui_close_settings();

    ui_set_names("Cleo & Lila & Friends");
    load(sc, "long_text");
    show();
    save(out, "34_long_text");
    dump(out, "34_long_text");
    ui_set_names("Cleo & Lila");

    load(sc, "hanukkah_night3");
    show();
    save(out, "35_hanukkah");
    dump(out, "35_hanukkah");

    load(sc, "bike_bus_cold_friday");
    show();
    save(out, "36_bike_bus");
    dump(out, "36_bike_bus");

    /* iter-06 outfit references: the dress day and the pants day,
     * unchecked. Round 4's fidelity failures lived on exactly these
     * two outfits (and the winter kit on the cold days above). */
    load(sc, "dress_morning");
    show();
    save(out, "37_dress_day");
    dump(out, "37_dress_day");

    load(sc, "warm_day");
    show();
    save(out, "38_warm_day");
    dump(out, "38_warm_day");

    /* Iter-12: a payload with no yesterday in it (first boot, partial
     * fetch). The outfit still prescribes, but the compare pill must
     * stay hidden: there is no honest comparison to show. */
    load(sc, "mild_no_yday");
    show();
    save(out, "39_no_yesterday");
    dump(out, "39_no_yesterday");

    /* Iter-25: the outfit rotation. Six consecutive dress-band days
     * (scenarios rot_dress_0..5, 2026-11-08..13) whose Today looks
     * walk the dress band in candidate order (dress, lavender, mint,
     * mustard, the Watchung logo dress, dress again), plus each day's
     * Tomorrow page, which must show the NEXT day's pick: the
     * rotation is seeded by the outfit's own date, not the viewing
     * date. The cat is pinned to the black colourway so the sequence
     * reads as outfit changes, not colourway changes. Last, the dry
     * warm-coat tier's warmcoat variant. */
    ui_set_cat_choice(0);
    {
        static const char *rot_names[] = {"50_rot_nov08", "51_rot_nov09", "52_rot_nov10",
                                          "53_rot_nov11", "54_rot_nov12", "55_rot_nov13"};
        char scen[32], tname[40];
        for (int i = 0; i < 6; i++) {
            snprintf(scen, sizeof(scen), "rot_dress_%d", i);
            load(sc, scen);
            show();
            save(out, rot_names[i]);
            dump(out, rot_names[i]);
            ui_test_set_page(1);
            pump(10);
            snprintf(tname, sizeof(tname), "%.24sb_tomorrow", rot_names[i]);
            save(out, tname);
            dump(out, tname);
            ui_test_set_page(0);
        }
    }
    load(sc, "warmcoat_morning");
    show();
    save(out, "56_warmcoat");
    dump(out, "56_warmcoat");

    /* Iter-27: the hot band rotation. Two consecutive hot days
     * (scenarios rot_hot_0..1, 2027-07-13..14): the first day's pick
     * is the standard hot look, the next day's is the Watchung logo
     * hot look, and the first day's Tomorrow page must already show
     * the logo pick (the seed is the outfit's own date). The logo
     * day's tiles and chips must prescribe exactly what the standard
     * day prescribes, sunglasses and water bottle included; only the
     * sprite differs. The cat stays pinned to black, as above. */
    load(sc, "rot_hot_0");
    show();
    save(out, "57_rot_jul13");
    dump(out, "57_rot_jul13");
    ui_test_set_page(1);
    pump(10);
    save(out, "57_rot_jul13b_tomorrow");
    dump(out, "57_rot_jul13b_tomorrow");
    ui_test_set_page(0);
    load(sc, "rot_hot_1");
    show();
    save(out, "58_rot_jul14");
    dump(out, "58_rot_jul14");

    /* Iter-30: the warm tier's hat-day rotation. Two consecutive dry
     * warm-coat-tier mornings that prescribe the hat (scenarios
     * rot_kit_0..1, 2026-12-15..16): the first day's pick is the
     * winter kit, the next day's is the red winter kit (kit2), and
     * the first day's Tomorrow page must already show kit2 (the seed
     * is the outfit's own date). Both days prescribe identically
     * (same tiles by name, hat and gloves chips); only the sprite
     * and the coat tile's picture differ. A third render shows the
     * kit2 day in the pink colourway, then the choice is restored to
     * the black pin the blocks above run under. */
    load(sc, "rot_kit_0");
    show();
    save(out, "59_kit_dec15");
    dump(out, "59_kit_dec15");
    ui_test_set_page(1);
    pump(10);
    save(out, "59_kit_dec15b_tomorrow");
    dump(out, "59_kit_dec15b_tomorrow");
    ui_test_set_page(0);
    load(sc, "rot_kit_1");
    show();
    save(out, "60_kit2_dec16");
    dump(out, "60_kit2_dec16");
    ui_set_cat_choice(1);
    show();
    save(out, "61_kit2_pink");
    dump(out, "61_kit2_pink");
    ui_set_cat_choice(0);

    /* Iter-14: the My Day page, third screen. The populated state
     * (fixture events on both days), the configured-but-clear state,
     * and the no-calendar-link state, all on the brief_morning clock
     * (Tue Oct 27 2026). Iter-17 adds three more: the link set but
     * nothing ever loaded, a stale feed, and a late-afternoon view
     * (past events dimmed, the next one tagged). */
    setenv("TZ", "America/New_York", 1);
    tzset();
    load(sc, "brief_morning");
    show();
    ui_set_calendar_configured(true);
    ui_test_set_page(2);
    /* The never-loaded state (iter-17) must be captured before any
     * fetch is recorded in this process: once ui_show_calendar has
     * run, the page can never go back to never-fetched. The feed
     * store is still empty here, so there is no data either. */
    pump(10);
    save(out, "44_myday_never");
    dump(out, "44_myday_never");

    fixture_cal(true);
    fixture_fetched();
    pump(10);
    save(out, "40_myday");
    dump(out, "40_myday");

    fixture_cal(false);
    fixture_fetched();
    pump(10);
    save(out, "41_myday_empty");
    dump(out, "41_myday_empty");

    ui_set_calendar_configured(false);
    pump(10);
    save(out, "42_myday_unconfigured");
    dump(out, "42_myday_unconfigured");

    /* Stale (iter-17; iter-18 fixture hygiene): the same populated
     * page at 3:45 PM with the last good fetch still the 7:02 AM
     * one, over six hours old, so the freshness line takes its
     * warn form. Iter-18 loads the afternoon weather first (the
     * same Tuesday, cold all day, fetched at 3:30 PM): Pippa wears
     * the morning's snow look and the weather is fresh, so no
     * weather-stale chip competes with the calendar's own stale
     * stamp. Renders 40, 43 and 45 are the same fictional day;
     * only the clock and the fetch ages differ. */
    load(sc, "myday_afternoon");
    show();
    ui_set_calendar_configured(true);
    fixture_cal(true);
    fixture_fetched();
    now_tm.tm_hour = 15;
    now_tm.tm_min = 45;
    ui_refresh_clock();
    pump(10);
    save(out, "43_myday_stale");
    dump(out, "43_myday_stale");

    /* Late (iter-17; iter-18): the same 3:45 PM clock with a fresh
     * 3:02 PM fetch, isolating the afternoon statuses: story time
     * is over (DONE, dimmed), soccer (3:30-4:30) is in progress
     * (NOW), and the Grandma call is still to come (NEXT). The
     * refresh matters: the fetch helper renders with the clock
     * parked at 3:02, when soccer is still NEXT, so the page must
     * render again at the real 3:45 before it is captured. */
    fixture_fetched_at(15, 2);
    ui_refresh_clock();
    pump(10);
    save(out, "45_myday_late");
    dump(out, "45_myday_late");

    /* Iter-29: the fall-back weekend on the same page. The
     * Halloween scenario's clock is Sat Oct 31 2026, 8:05 AM, so
     * the DST fixture's tomorrow is the Nov 1 fall-back itself;
     * the fetch pins at 7:02 AM like the renders above. */
    load(sc, "halloween_cold");
    show();
    fixture_cal_dst();
    fixture_fetched();
    pump(10);
    save(out, "46_myday_dst");
    dump(out, "46_myday_dst");
    ui_test_set_page(0);

    ui_show_wifi_error("HomeWiFi", NULL);
    save(out, "10_wifi_error");

    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    printf("LVGL heap: %u of %u bytes used (%u%%), biggest free block %u\n", (unsigned)(mon.total_size - mon.free_size),
           (unsigned)mon.total_size, (unsigned)mon.used_pct, (unsigned)mon.free_biggest_size);
    return 0;
}
