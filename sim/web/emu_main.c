/*
 * Browser emulator: the board's real screen code (ui.c, advice.c, wx.c, same fonts
 * and artwork, same lv_conf.h) compiled to WebAssembly. JavaScript feeds it the
 * time and finger position and copies its 800 x 480 picture onto a canvas.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "lvgl.h"
#include "scenarios_data.h"
#include "ui.h"
#include "wx.h"

#define EXPORT(name) __attribute__((export_name(#name)))

static uint16_t s_fb[UI_W * UI_H];
static uint32_t s_ms;
static int s_px, s_py;
static bool s_down;
static bool s_dirty;
static wx_t s_wx;
static int s_cur;
static time_t s_base;
static uint32_t s_loaded_ms;
static bool s_bike_bus = true;
static char s_status[48];

static uint32_t tick_cb(void) { return s_ms; }

static void flush_cb(lv_display_t *d, const lv_area_t *a, uint8_t *px)
{
    (void)a;
    (void)px;
    s_dirty = true;
    lv_display_flush_ready(d);
}

static void read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    data->point.x = s_px;
    data->point.y = s_py;
    data->state = s_down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static bool time_cb(struct tm *out)
{
    /* The sample day's clock, ticking forward while you watch. */
    time_t t = s_base + (time_t)((s_ms - s_loaded_ms) / 1000);
    return gmtime_r(&t, out) != NULL;
}

static void show_current(void)
{
    ui_set_bike_bus_day(s_bike_bus ? 5 : -1);
    ui_close_settings();
    ui_show_weather(&s_wx);
    ui_set_footer("Upper Montclair, NJ", s_status, false);
    ui_refresh_clock();
}

EXPORT(emu_load) void emu_load(int i)
{
    if (i < 0 || i >= NUM_SCENARIOS) {
        return;
    }
    s_cur = i;
    const char *j = SCENARIOS[i].json;
    if (!wx_parse_openmeteo(j, strlen(j), true, &s_wx)) {
        ui_show_boot("Couldn't read that sample forecast.");
        return;
    }
    struct tm b;
    memset(&b, 0, sizeof(b));
    b.tm_year = s_wx.year - 1900;
    b.tm_mon = s_wx.month - 1;
    b.tm_mday = s_wx.day;
    b.tm_hour = s_wx.hour;
    b.tm_min = s_wx.minute + 7 > 59 ? 59 : s_wx.minute + 7;
    s_base = timegm(&b);
    s_loaded_ms = s_ms;

    int h12 = s_wx.hour % 12 ? s_wx.hour % 12 : 12;
    snprintf(s_status, sizeof(s_status), "Updated %d:%02d %s", h12, s_wx.minute, s_wx.hour < 12 ? "AM" : "PM");
    ui_info_t info = {.wifi = "HomeWiFi", .place = "Upper Montclair, New Jersey", .ip = "192.168.1.42",
                      .version = "1.1.0"};
    snprintf(info.updated, sizeof(info.updated), "Weather updated %d:%02d %s", h12, s_wx.minute, s_wx.hour < 12 ? "AM" : "PM");
    ui_set_info(&info);
    ui_show_page(0);
    show_current();
}

static void on_action(ui_action_t a, void *user)
{
    (void)user;
    if (a == UI_ACT_SETUP) {
        ui_show_setup("WeatherBesties-3F2A", "192.168.4.1");
    } else {
        emu_load(s_cur);
    }
}

EXPORT(emu_init) void emu_init(void)
{
    lv_init();
    lv_tick_set_cb(tick_cb);
    lv_display_t *d = lv_display_create(UI_W, UI_H);
    lv_display_set_buffers(d, s_fb, NULL, sizeof(s_fb), LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(d, flush_cb);
    lv_indev_t *ind = lv_indev_create();
    lv_indev_set_type(ind, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(ind, read_cb);
    lv_indev_set_long_press_time(ind, 1500);

    ui_init("Cleo & Lila");
    ui_set_time_cb(time_cb);
    ui_set_action_cb(on_action, NULL);
    emu_load(0);
}

EXPORT(emu_fb) uint16_t *emu_fb(void) { return s_fb; }

/* Advances the clock to now_ms, runs the UI; returns 1 when the picture changed. */
EXPORT(emu_tick) int emu_tick(uint32_t now_ms)
{
    s_ms = now_ms;
    lv_timer_handler();
    int d = s_dirty;
    s_dirty = false;
    return d;
}

EXPORT(emu_pointer) void emu_pointer(int x, int y, int down)
{
    s_px = x < 0 ? 0 : x >= UI_W ? UI_W - 1 : x;
    s_py = y < 0 ? 0 : y >= UI_H ? UI_H - 1 : y;
    s_down = down != 0;
}

/* 0 = today, 1 = next days, 2 = settings, 3 = setup, 4 = start-up, 5 = WiFi problem */
EXPORT(emu_screen) void emu_screen(int which)
{
    switch (which) {
    case 0: case 1:
        ui_close_settings();
        show_current();
        ui_show_page(which);
        break;
    case 2:
        show_current();
        ui_open_settings();
        break;
    case 3: ui_close_settings(); ui_show_setup("WeatherBesties-3F2A", "192.168.4.1"); break;
    case 4: ui_close_settings(); ui_show_boot(NULL); break;
    case 5: ui_close_settings(); ui_show_wifi_error("HomeWiFi", NULL); break;
    default: break;
    }
}

EXPORT(emu_set_bike_bus) void emu_set_bike_bus(int on)
{
    s_bike_bus = on != 0;
    show_current();
}

EXPORT(emu_count) int emu_count(void) { return NUM_SCENARIOS; }
EXPORT(emu_name) const char *emu_name(int i) { return (i >= 0 && i < NUM_SCENARIOS) ? SCENARIOS[i].name : ""; }
