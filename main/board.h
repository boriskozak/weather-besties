/*
 * Waveshare ESP32-S3-Touch-LCD-4.3: 800x480 RGB panel, GT911 touch, CH422G IO expander.
 * Pin map and panel timing come from Waveshare's own demo (09_lvgl_v9_demo).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

esp_err_t board_init(void);          /* I2C, expander, panel, touch, LVGL display + touch input */
void board_start_lvgl_task(void);
bool board_lvgl_lock(int timeout_ms);  /* -1 waits forever */
void board_lvgl_unlock(void);

void board_backlight(bool on);
bool board_backlight_is_on(void);
uint32_t board_ms_since_touch(void);
