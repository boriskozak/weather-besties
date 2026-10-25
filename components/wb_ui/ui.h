/*
 * Weather Besties screens. Pure LVGL: the firmware and the desktop renderer
 * both call these. Call everything from the LVGL thread (or with the LVGL
 * lock held).
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#include "wx.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UI_W 800
#define UI_H 480

typedef enum {
    UI_ACT_SETUP,     /* user asked to change WiFi / town */
    UI_ACT_REFRESH,   /* user asked for fresh weather now */
} ui_action_t;

typedef void (*ui_action_cb_t)(ui_action_t act, void *user);

/* Fills local time. Return false while the clock isn't set yet. */
typedef bool (*ui_time_cb_t)(struct tm *local_out);

typedef struct {
    char wifi[40];
    char place[64];
    char ip[20];
    char updated[48];
    char version[24];
} ui_info_t;

/* Speed test (tap "Updated ..." on the screen): called with old_mode when the mode
 * changes and once a second while it's on; fill info with a short readout. */
typedef void (*ui_speed_cb_t)(bool old_mode, char *info, size_t n);
void ui_set_speed_cb(ui_speed_cb_t cb);

void ui_init(const char *names);
void ui_set_names(const char *names);
void ui_set_bike_bus_day(int wday);  /* 0 = Sunday ... 6 = Saturday, -1 = none. Default Friday. */
void ui_set_action_cb(ui_action_cb_t cb, void *user);
void ui_set_time_cb(ui_time_cb_t cb);

void ui_show_boot(const char *msg);
void ui_show_setup(const char *ap_name, const char *url);
void ui_show_connecting(const char *ssid);
void ui_show_wifi_error(const char *ssid, const char *detail);

void ui_show_weather(const wx_t *wx);
void ui_set_footer(const char *place, const char *status, bool warn);  /* place: kept for callers, shown in Settings */
void ui_set_info(const ui_info_t *info);
void ui_open_settings(void);
void ui_close_settings(void);
void ui_show_page(int page);   /* 0 = today, 1 = tomorrow, 2 = next days */
void ui_refresh_clock(void);

/* Cat choice from Settings: 0 = black cat, 1 = pink cat, 2 = take turns. */
typedef void (*ui_cat_cb_t)(int choice);
void ui_set_cat_choice(int choice);
void ui_set_cat_cb(ui_cat_cb_t cb);

#ifdef WB_SIM
/* Desktop-renderer test hooks. */
void ui_test_set_checks(uint8_t mask);
void ui_test_set_page(int page);
void ui_test_set_pippa_frame(int frame);   /* pin Pippa's heart animation to a frame, -1 releases */
#endif

#ifdef __cplusplus
}
#endif
