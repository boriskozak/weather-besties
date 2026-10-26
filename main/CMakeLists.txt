idf_component_register(
    SRCS "main.c" "board.c" "net.c" "portal.c" "settings.c" "ota.c"
    INCLUDE_DIRS "."
    REQUIRES wb_ui lvgl esp_lcd esp_lcd_touch esp_lcd_touch_gt911 esp_driver_i2c esp_driver_gpio
             esp_wifi esp_netif esp_event esp_http_client esp_http_server mbedtls nvs_flash
             dns_server esp_timer app_update esp_app_format)
