#include "settings.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "settings";
#define NS "wb"

void settings_defaults(wb_settings_t *s)
{
    memset(s, 0, sizeof(*s));
    snprintf(s->place_query, sizeof(s->place_query), "Upper Montclair, NJ");
    snprintf(s->place_name, sizeof(s->place_name), "Upper Montclair, New Jersey");
    s->lat = 40.8462;
    s->lon = -74.2013;
    s->has_coords = true;
    snprintf(s->names, sizeof(s->names), "Cleo & Lila");
    s->celsius = false;
    s->night_off = false;
    s->off_from = 21;
    s->off_to = 6;
    s->bike_bus = 5; /* Friday */
    s->cat = 0;      /* black cat */
}

static void get_str(nvs_handle_t h, const char *key, char *dst, size_t cap)
{
    size_t len = cap;
    if (nvs_get_str(h, key, dst, &len) != ESP_OK) {
        /* keep default */
    }
}

static void get_u8(nvs_handle_t h, const char *key, uint8_t *dst)
{
    uint8_t v;
    if (nvs_get_u8(h, key, &v) == ESP_OK) {
        *dst = v;
    }
}

static void get_double(nvs_handle_t h, const char *key, double *dst)
{
    double v;
    size_t len = sizeof(v);
    if (nvs_get_blob(h, key, &v, &len) == ESP_OK && len == sizeof(v)) {
        *dst = v;
    }
}

void settings_load(wb_settings_t *s)
{
    settings_defaults(s);
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) {
        ESP_LOGI(TAG, "no saved settings, using defaults");
        return;
    }
    get_str(h, "ssid", s->ssid, sizeof(s->ssid));
    get_str(h, "pass", s->pass, sizeof(s->pass));
    get_str(h, "pq", s->place_query, sizeof(s->place_query));
    get_str(h, "pn", s->place_name, sizeof(s->place_name));
    get_str(h, "names", s->names, sizeof(s->names));
    get_double(h, "lat", &s->lat);
    get_double(h, "lon", &s->lon);
    uint8_t b = s->has_coords;
    get_u8(h, "coords", &b);
    s->has_coords = b;
    b = s->celsius;
    get_u8(h, "cel", &b);
    s->celsius = b;
    b = s->night_off;
    get_u8(h, "noff", &b);
    s->night_off = b;
    get_u8(h, "nfrom", &s->off_from);
    get_u8(h, "nto", &s->off_to);
    get_u8(h, "bbus", &s->bike_bus);
    get_u8(h, "cat", &s->cat);
    nvs_close(h);
    ESP_LOGI(TAG, "loaded: wifi='%s' place='%s' (%.4f, %.4f)", s->ssid, s->place_name, s->lat, s->lon);
}

void settings_save(const wb_settings_t *s)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed");
        return;
    }
    nvs_set_str(h, "ssid", s->ssid);
    nvs_set_str(h, "pass", s->pass);
    nvs_set_str(h, "pq", s->place_query);
    nvs_set_str(h, "pn", s->place_name);
    nvs_set_str(h, "names", s->names);
    nvs_set_blob(h, "lat", &s->lat, sizeof(s->lat));
    nvs_set_blob(h, "lon", &s->lon, sizeof(s->lon));
    nvs_set_u8(h, "coords", s->has_coords);
    nvs_set_u8(h, "cel", s->celsius);
    nvs_set_u8(h, "noff", s->night_off);
    nvs_set_u8(h, "nfrom", s->off_from);
    nvs_set_u8(h, "nto", s->off_to);
    nvs_set_u8(h, "bbus", s->bike_bus);
    nvs_set_u8(h, "cat", s->cat);
    nvs_commit(h);
    nvs_close(h);
}
