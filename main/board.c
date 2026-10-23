#include "board.h"

#include <string.h>

#include "driver/gpio.h"
#include "esp_check.h"
#include "driver/i2c_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lvgl.h"

static const char *TAG = "board";

/* ---- pins (Waveshare wiki + demo) ---- */
#define PIN_I2C_SDA 8
#define PIN_I2C_SCL 9
#define PIN_TOUCH_INT 4

#define LCD_W 800
#define LCD_H 480
#define LCD_PCLK_HZ (16 * 1000 * 1000)
#define LCD_BOUNCE_LINES 10

static const int RGB_DATA[16] = {
    14, 38, 18, 17, 10,       /* B3..B7 */
    39, 0, 45, 48, 47, 21,    /* G2..G7 */
    1, 2, 42, 41, 40,         /* R3..R7 */
};
#define PIN_HSYNC 46
#define PIN_VSYNC 3
#define PIN_DE 5
#define PIN_PCLK 7

/* ---- CH422G: writing 1 byte to "address" 0x24 sets mode, to 0x38 sets IO0..IO7 ---- */
#define CH422G_MODE 0x24
#define CH422G_OUT 0x38
#define EXIO_TP_RST (1 << 1)
#define EXIO_BACKLIGHT (1 << 2)
#define EXIO_LCD_RST (1 << 3)
#define EXIO_SD_CS (1 << 4)
#define EXIO_USB_SEL (1 << 5) /* low = USB, high = CAN */

#define LVGL_BUF_LINES 20

static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_exp_mode, s_exp_out;
static uint8_t s_exio = EXIO_LCD_RST | EXIO_SD_CS;
static esp_lcd_panel_handle_t s_panel;
static esp_lcd_touch_handle_t s_touch;
static SemaphoreHandle_t s_lvgl_mux;
static volatile int64_t s_last_touch_us;
static volatile bool s_swallow;

/* ------------------------------------------------------------------ expander */

static esp_err_t exio_write(void)
{
    return i2c_master_transmit(s_exp_out, &s_exio, 1, 100);
}

static void exio_set(uint8_t bits, bool on)
{
    if (on) {
        s_exio |= bits;
    } else {
        s_exio &= ~bits;
    }
    esp_err_t err = exio_write();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "expander write failed: %s", esp_err_to_name(err));
    }
}

void board_backlight(bool on)
{
    exio_set(EXIO_BACKLIGHT, on);
}

bool board_backlight_is_on(void)
{
    return (s_exio & EXIO_BACKLIGHT) != 0;
}

uint32_t board_ms_since_touch(void)
{
    return (uint32_t)((esp_timer_get_time() - s_last_touch_us) / 1000);
}

static esp_err_t i2c_and_expander_init(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &s_bus), TAG, "i2c bus");

    i2c_device_config_t dev = {.dev_addr_length = I2C_ADDR_BIT_LEN_7, .scl_speed_hz = 400000};
    dev.device_address = CH422G_MODE;
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(s_bus, &dev, &s_exp_mode), TAG, "ch422g mode");
    dev.device_address = CH422G_OUT;
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(s_bus, &dev, &s_exp_out), TAG, "ch422g out");

    uint8_t mode = 0x01; /* IO0..IO7 as push-pull outputs */
    ESP_RETURN_ON_ERROR(i2c_master_transmit(s_exp_mode, &mode, 1, 100), TAG, "ch422g mode write");

    /* Backlight stays off until the first frame is drawn. Touch held in reset. */
    s_exio = EXIO_LCD_RST | EXIO_SD_CS;
    return exio_write();
}

/* ------------------------------------------------------------------ panel */

static esp_err_t panel_init(void)
{
    esp_lcd_rgb_panel_config_t cfg = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings = {
            .pclk_hz = LCD_PCLK_HZ,
            .h_res = LCD_W,
            .v_res = LCD_H,
            .hsync_pulse_width = 4,
            .hsync_back_porch = 8,
            .hsync_front_porch = 8,
            .vsync_pulse_width = 4,
            .vsync_back_porch = 8,
            .vsync_front_porch = 8,
            .flags.pclk_active_neg = 1,
        },
        .data_width = 16,
        .bits_per_pixel = 16,
        .num_fbs = 1,
        .bounce_buffer_size_px = LCD_W * LCD_BOUNCE_LINES,
        .dma_burst_size = 64,
        .hsync_gpio_num = PIN_HSYNC,
        .vsync_gpio_num = PIN_VSYNC,
        .de_gpio_num = PIN_DE,
        .pclk_gpio_num = PIN_PCLK,
        .disp_gpio_num = -1,
        .flags.fb_in_psram = 1,
    };
    for (int i = 0; i < 16; i++) {
        cfg.data_gpio_nums[i] = RGB_DATA[i];
    }
    ESP_RETURN_ON_ERROR(esp_lcd_new_rgb_panel(&cfg, &s_panel), TAG, "rgb panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "panel init");
    return ESP_OK;
}

/* ------------------------------------------------------------------ touch */

static esp_err_t touch_init(void)
{
    /* GT911 picks I2C address 0x5D when INT is low while it leaves reset. */
    gpio_config_t io = {.pin_bit_mask = 1ULL << PIN_TOUCH_INT, .mode = GPIO_MODE_OUTPUT};
    gpio_config(&io);
    gpio_set_level(PIN_TOUCH_INT, 0);
    exio_set(EXIO_TP_RST, false);
    vTaskDelay(pdMS_TO_TICKS(100));
    exio_set(EXIO_TP_RST, true);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_direction(PIN_TOUCH_INT, GPIO_MODE_INPUT);
    vTaskDelay(pdMS_TO_TICKS(100));

    esp_lcd_panel_io_handle_t io_h = NULL;
    esp_lcd_panel_io_i2c_config_t io_cfg = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    io_cfg.scl_speed_hz = 400000;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(s_bus, &io_cfg, &io_h), TAG, "touch io");

    esp_lcd_touch_config_t tp = {
        .x_max = LCD_W,
        .y_max = LCD_H,
        .rst_gpio_num = -1,
        .int_gpio_num = -1,
    };
    return esp_lcd_touch_new_i2c_gt911(io_h, &tp, &s_touch);
}

/* ------------------------------------------------------------------ lvgl glue */

static uint32_t tick_cb(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void flush_cb(lv_display_t *d, const lv_area_t *a, uint8_t *px)
{
    esp_lcd_panel_draw_bitmap(s_panel, a->x1, a->y1, a->x2 + 1, a->y2 + 1, px);
    lv_display_flush_ready(d);
}

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    data->state = LV_INDEV_STATE_RELEASED;
    if (!s_touch) {
        return;
    }
    esp_lcd_touch_point_data_t pt[1];
    uint8_t cnt = 0;
    if (esp_lcd_touch_read_data(s_touch) != ESP_OK ||
        esp_lcd_touch_get_data(s_touch, pt, &cnt, 1) != ESP_OK || cnt == 0) {
        s_swallow = false;
        return;
    }
    s_last_touch_us = esp_timer_get_time();
    if (!board_backlight_is_on()) {
        /* First tap only wakes the screen. */
        board_backlight(true);
        s_swallow = true;
    }
    if (s_swallow) {
        return;
    }
    data->point.x = pt[0].x;
    data->point.y = pt[0].y;
    data->state = LV_INDEV_STATE_PRESSED;
}

static void *alloc_draw_buf(size_t sz)
{
    void *p = heap_caps_aligned_alloc(64, sz, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!p) {
        ESP_LOGW(TAG, "draw buffer in PSRAM (internal RAM is tight)");
        p = heap_caps_aligned_alloc(64, sz, MALLOC_CAP_SPIRAM);
    }
    return p;
}

esp_err_t board_init(void)
{
    s_lvgl_mux = xSemaphoreCreateRecursiveMutex();
    s_last_touch_us = esp_timer_get_time();

    ESP_RETURN_ON_ERROR(i2c_and_expander_init(), TAG, "expander");
    ESP_RETURN_ON_ERROR(panel_init(), TAG, "panel");
    esp_err_t terr = touch_init();
    if (terr != ESP_OK) {
        ESP_LOGW(TAG, "no touch controller (%s), carrying on without touch", esp_err_to_name(terr));
        s_touch = NULL;
    }

    lv_init();
    lv_tick_set_cb(tick_cb);
    lv_display_t *disp = lv_display_create(LCD_W, LCD_H);
    size_t sz = LCD_W * LVGL_BUF_LINES * sizeof(uint16_t);
    void *b1 = alloc_draw_buf(sz);
    void *b2 = alloc_draw_buf(sz);
    if (!b1 || !b2) {
        return ESP_ERR_NO_MEM;
    }
    lv_display_set_buffers(disp, b1, b2, sz, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, flush_cb);

    lv_indev_t *ind = lv_indev_create();
    lv_indev_set_type(ind, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(ind, touch_read_cb);
    lv_indev_set_long_press_time(ind, 1500);

    ESP_LOGI(TAG, "display ready, touch %s", s_touch ? "ok" : "missing");
    return ESP_OK;
}

bool board_lvgl_lock(int timeout_ms)
{
    TickType_t t = timeout_ms < 0 ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(s_lvgl_mux, t) == pdTRUE;
}

void board_lvgl_unlock(void)
{
    xSemaphoreGiveRecursive(s_lvgl_mux);
}

static void lvgl_task(void *arg)
{
    (void)arg;
    while (1) {
        uint32_t wait = 10;
        if (board_lvgl_lock(-1)) {
            wait = lv_timer_handler();
            board_lvgl_unlock();
        }
        if (wait < 5) {
            wait = 5;
        } else if (wait > 30) {
            wait = 30;
        }
        vTaskDelay(pdMS_TO_TICKS(wait));
    }
}

void board_start_lvgl_task(void)
{
    xTaskCreatePinnedToCore(lvgl_task, "lvgl", 8192, NULL, 4, NULL, 1);
}
