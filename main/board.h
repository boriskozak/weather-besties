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

/* Drawing area (how many screen lines LVGL draws per pass). It starts at 40
 * lines, the same memory v1.3.0 used. board_grow_draw_buffer() makes it bigger,
 * but only if enough internal RAM stays free for WiFi and the secure weather
 * download; call it after a weather fetch has worked. Both take the LVGL lock. */
void board_grow_draw_buffer(void);
void board_shrink_draw_buffer(void);   /* back to the start size when memory is tight */
int board_draw_lines(void);
size_t board_free_internal(void);      /* bytes of internal RAM free right now */
/* Speed test: true = draw like v1.3.0 (20 lines a pass) to compare. */
void board_speed_old_mode(bool old);

void board_backlight(bool on);
bool board_backlight_is_on(void);
uint32_t board_ms_since_touch(void);
