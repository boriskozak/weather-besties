/*
 * Weather Besties: a weather station for Cleo and Lila.
 * Board: Waveshare ESP32-S3-Touch-LCD-4.3. Weather: Open-Meteo (no API key).
 */
#include <stdio.h>

#include "board.h"
#include "esp_log.h"
#include "lvgl.h"
#include "net.h"
#include "nvs_flash.h"
#include "ota.h"
#include "settings.h"
#include "ui.h"

static const char *TAG = "main";
static wb_settings_t S;

/* Optional "screen off at night": checked once a second in the LVGL thread. */
static void power_timer(lv_timer_t *t)
{
    (void)t;
    if (!S.night_off) {
        if (!board_backlight_is_on()) {
            board_backlight(true);
        }
        return;
    }
    struct tm now;
    if (!net_local_time(&now)) {
        return;
    }
    int h = now.tm_hour;
    bool night = S.off_from < S.off_to ? (h >= S.off_from && h < S.off_to) : (h >= S.off_from || h < S.off_to);
    if (S.off_from == S.off_to) {
        night = false;
    }
    if (night && board_ms_since_touch() > 60 * 1000) {
        if (board_backlight_is_on()) {
            board_backlight(false);
        }
    } else if (!night && !board_backlight_is_on()) {
        board_backlight(true);
    }
}

/* Cat choice from Settings: remember it and show it right away. */
static void cat_cb(int choice)
{
    S.cat = (uint8_t)choice;
    settings_save(&S);
    board_lvgl_lock(-1);
    ui_set_cat_choice(choice);
    board_lvgl_unlock();
}

/* Speed test readout and old/new switch (the UI calls this; see ui_set_speed_cb). */
static void speed_cb(bool old_mode, char *info, size_t n)
{
    board_speed_old_mode(old_mode);
    snprintf(info, n, "%d lines, %u KB free", board_draw_lines(), (unsigned)(board_free_internal() / 1024));
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    settings_load(&S);
    ota_boot_check();   /* if this version is new and unproven, start its trial clock */

    ESP_ERROR_CHECK(board_init());

    board_lvgl_lock(-1);
    ui_init(S.names);
    ui_set_cat_cb(cat_cb);
    ui_set_cat_choice(S.cat);
    ui_set_bike_bus_day(S.bike_bus <= 6 ? S.bike_bus : -1);
    ui_set_action_cb(net_ui_action, NULL);
    ui_set_time_cb(net_local_time);
    ui_set_speed_cb(speed_cb);
    ui_set_calendar_configured(S.cal_url[0] != 0);
    lv_timer_create(power_timer, 1000, NULL);
    lv_refr_now(NULL);
    board_lvgl_unlock();

    board_backlight(true);
    board_start_lvgl_task();

    ESP_LOGI(TAG, "Weather Besties %s starting", WB_VERSION);
    net_start(&S);
}
