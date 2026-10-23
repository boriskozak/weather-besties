/*
 * Desktop renderer: runs the real UI code (ui.c, wx.c, advice.c, same fonts and
 * images, same lv_conf.h) and saves each screen as a raw RGB565 frame.
 * Usage: ./render <outdir> <scenario dir>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
    free(j);
    memset(&now_tm, 0, sizeof(now_tm));
    now_tm.tm_year = wx.year - 1900;
    now_tm.tm_mon = wx.month - 1;
    now_tm.tm_mday = wx.day;
    now_tm.tm_hour = wx.hour;
    now_tm.tm_min = wx.minute + 7 > 59 ? 59 : wx.minute + 7;
    now_tm.tm_wday = wx.wday;
    now_valid = true;
    printf("%s: %d periods:", name, wx.nperiods);
    for (int i = 0; i < wx.nperiods; i++) {
        printf(" [%s %.0f code%d pop%d]", wx.periods[i].label, wx.periods[i].temp, wx.periods[i].code,
               wx.periods[i].pop);
    }
    printf("\n");
}

static void show(const char *place, const char *status)
{
    ui_show_page(0);
    ui_show_weather(&wx);
    ui_set_footer(place, status, false);
    ui_refresh_clock();
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

    ui_show_boot(NULL);
    save(out, "01_boot");

    ui_show_setup("WeatherBesties-3F2A", "192.168.4.1");
    save(out, "02_setup");

    load(sc, "oct_morning");
    show("Upper Montclair, NJ", "Updated 9:45 AM");
    save(out, "03_today_oct_morning");

    ui_show_page(1);
    save(out, "04_next_days");
    ui_show_page(0);

    load(sc, "rainy_afternoon");
    show("Upper Montclair, NJ", "Updated 2:15 PM");
    save(out, "05_rainy_afternoon");

    load(sc, "snow_morning");
    show("Upper Montclair, NJ", "Updated 7:30 AM");
    save(out, "06_snow_morning");

    load(sc, "summer_hot");
    show("Upper Montclair, NJ", "Updated 1:00 PM");
    save(out, "07_summer_hot");

    load(sc, "thu_evening");
    show("Upper Montclair, NJ", "Updated 8:30 PM");
    save(out, "08_evening_plans_tomorrow");

    load(sc, "oct_morning");
    show("Upper Montclair, NJ", "Updated 9:45 AM");
    ui_info_t info = {.wifi = "HomeWiFi", .place = "Upper Montclair, New Jersey", .ip = "192.168.1.42",
                      .updated = "Weather updated 9:45 AM", .version = "1.0.0"};
    ui_set_info(&info);
    ui_open_settings();
    save(out, "09_settings");
    ui_close_settings();

    ui_show_wifi_error("HomeWiFi", NULL);
    save(out, "10_wifi_error");
    return 0;
}
