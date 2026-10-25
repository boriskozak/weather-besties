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

    load(sc, "brief_evening");
    show();
    save(out, "21_evening");

    ui_test_set_checks(0x3); /* first two garments laid out */
    pump(10);
    save(out, "22_evening_2_checked");

    ui_test_set_checks(0xF); /* everything laid out */
    pump(10);
    save(out, "23_evening_all_done");

    /* Pippa proof still: her all-done heart animation plays on its own
     * in the renderer; pin frame 4 (the eyes-closed heart hold) so the
     * screenshot set shows the gesture deterministically. */
    ui_test_set_pippa_frame(4);
    pump(10);
    save(out, "23b_evening_all_done_heart");
    ui_test_set_pippa_frame(-1);
    ui_test_set_checks(0);

    load(sc, "brief_morning");
    show();
    ui_test_set_page(2);
    pump(10);
    save(out, "24_next_days");
    ui_test_set_page(0);

    load(sc, "stale_2h");
    show();
    save(out, "25_stale");

    load(sc, "too_old");
    show();
    save(out, "26_unavailable");

    load(sc, "rollover");
    show();
    save(out, "27_rollover");

    load(sc, "snow_day");
    show();
    save(out, "28_snow");

    load(sc, "hot_summer");
    show();
    save(out, "29_hot");

    load(sc, "storm_afternoon");
    show();
    save(out, "30_storm");

    load(sc, "halloween_cold");
    show();
    save(out, "31_halloween");

    load(sc, "brief_morning");
    ui_set_cat_choice(1); /* pink cat */
    show();
    save(out, "32_pink_cat");
    ui_set_cat_choice(0);

    ui_open_settings();
    save(out, "33_settings");
    ui_close_settings();

    ui_set_names("Cleo & Lila & Friends");
    load(sc, "long_text");
    show();
    save(out, "34_long_text");
    ui_set_names("Cleo & Lila");

    load(sc, "hanukkah_night3");
    show();
    save(out, "35_hanukkah");

    load(sc, "bike_bus_cold_friday");
    show();
    save(out, "36_bike_bus");

    ui_show_wifi_error("HomeWiFi", NULL);
    save(out, "10_wifi_error");

    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    printf("LVGL heap: %u of %u bytes used (%u%%), biggest free block %u\n", (unsigned)(mon.total_size - mon.free_size),
           (unsigned)mon.total_size, (unsigned)mon.used_pct, (unsigned)mon.free_biggest_size);
    return 0;
}
