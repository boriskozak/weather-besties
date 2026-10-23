#include "net.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "esp_crt_bundle.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "wx.h"

static const char *TAG = "net";

#define EV_CONNECTED BIT0
#define EV_REFRESH BIT1
#define EV_PORTAL BIT2

#define REFRESH_MS (15 * 60 * 1000)
#define RETRY_FAST_MS (60 * 1000)
#define RETRY_SLOW_MS (5 * 60 * 1000)
#define CONNECT_WAIT_MS (30 * 1000)

static EventGroupHandle_t s_ev;
static wb_settings_t *S;
static esp_timer_handle_t s_reconnect;
static volatile bool s_portal;
static char s_ip[20];
static volatile int s_utc_offset;
static volatile bool s_have_offset;
static wx_t s_wx;

/* ------------------------------------------------------------------ time */

bool net_local_time(struct tm *out)
{
    time_t now = time(NULL);
    if (now < 1700000000 || !s_have_offset) {
        return false;
    }
    now += s_utc_offset;
    gmtime_r(&now, out);
    return true;
}

static void fmt_clock(char *buf, size_t n, int hour, int min)
{
    int h12 = hour % 12 ? hour % 12 : 12;
    snprintf(buf, n, "%d:%02d %s", h12, min, hour < 12 ? "AM" : "PM");
}

/* ------------------------------------------------------------------ ui glue */

void net_ui_action(ui_action_t a, void *user)
{
    (void)user;
    if (a == UI_ACT_SETUP) {
        xEventGroupSetBits(s_ev, EV_PORTAL);
    } else if (a == UI_ACT_REFRESH) {
        xEventGroupSetBits(s_ev, EV_REFRESH);
    }
}

#define UI_LOCKED(code)              \
    do {                             \
        if (board_lvgl_lock(-1)) {   \
            code;                    \
            board_lvgl_unlock();     \
        }                            \
    } while (0)

/* ------------------------------------------------------------------ wifi */

static void reconnect_cb(void *arg)
{
    (void)arg;
    if (!s_portal) {
        esp_wifi_connect();
    }
}

static void on_wifi(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        if (!s_portal) {
            esp_wifi_connect();
        }
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_ev, EV_CONNECTED);
        s_ip[0] = 0;
        if (!s_portal) {
            wifi_event_sta_disconnected_t *d = data;
            ESP_LOGW(TAG, "wifi disconnected (reason %d), retrying in 5 s", d ? d->reason : -1);
            esp_timer_stop(s_reconnect);
            esp_timer_start_once(s_reconnect, 5 * 1000 * 1000);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = data;
        snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&e->ip_info.ip));
        ESP_LOGI(TAG, "got ip %s", s_ip);
        xEventGroupSetBits(s_ev, EV_CONNECTED);
    }
}

static void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi, NULL));
    const esp_timer_create_args_t t = {.callback = reconnect_cb, .name = "reconnect"};
    ESP_ERROR_CHECK(esp_timer_create(&t, &s_reconnect));
}

static void wifi_start_sta(void)
{
    wifi_config_t wc = {0};
    memcpy(wc.sta.ssid, S->ssid, strnlen(S->ssid, sizeof(wc.sta.ssid)));
    memcpy(wc.sta.password, S->pass, strnlen(S->pass, sizeof(wc.sta.password)));
    wc.sta.threshold.authmode = S->pass[0] ? WIFI_AUTH_WEP : WIFI_AUTH_OPEN;
    wc.sta.pmf_cfg.capable = true;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
}

/* ------------------------------------------------------------------ http */

bool net_http_get(const char *url, char **body, size_t *len)
{
    *body = NULL;
    *len = 0;
    esp_http_client_config_t cfg = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 15000,
        .buffer_size = 2048,
        .user_agent = "WeatherBesties/" WB_VERSION,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) {
        return false;
    }
    bool ok = false;
    size_t cap = 16 * 1024;
    char *buf = heap_caps_malloc(cap, MALLOC_CAP_SPIRAM);
    if (!buf || esp_http_client_open(c, 0) != ESP_OK) {
        ESP_LOGW(TAG, "http open failed: %s", url);
        goto out;
    }
    esp_http_client_fetch_headers(c);
    int status = esp_http_client_get_status_code(c);
    size_t n = 0;
    while (1) {
        if (cap - n < 1024) {
            if (cap >= 256 * 1024) {
                break;
            }
            char *nb = heap_caps_realloc(buf, cap * 2, MALLOC_CAP_SPIRAM);
            if (!nb) {
                break;
            }
            buf = nb;
            cap *= 2;
        }
        int r = esp_http_client_read(c, buf + n, cap - n - 1);
        if (r <= 0) {
            break;
        }
        n += r;
    }
    buf[n] = 0;
    if (status == 200 && n > 0) {
        ok = true;
        *body = buf;
        *len = n;
        buf = NULL;
    } else {
        ESP_LOGW(TAG, "http status %d, %u bytes", status, (unsigned)n);
    }
out:
    free(buf);
    esp_http_client_close(c);
    esp_http_client_cleanup(c);
    return ok;
}

static void url_encode(const char *in, char *out, size_t cap)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t o = 0;
    for (; *in && o + 4 < cap; in++) {
        unsigned char ch = (unsigned char)*in;
        if (isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            out[o++] = ch;
        } else {
            out[o++] = '%';
            out[o++] = hex[ch >> 4];
            out[o++] = hex[ch & 15];
        }
    }
    out[o] = 0;
}

/* ------------------------------------------------------------------ geocode + weather */

static bool geocode(void)
{
    char q[200], url[320];
    url_encode(S->place_query, q, sizeof(q));
    bool zip = strlen(S->place_query) == 5;
    for (int i = 0; zip && i < 5; i++) {
        zip = isdigit((unsigned char)S->place_query[i]);
    }
    snprintf(url, sizeof(url),
             "https://geocoding-api.open-meteo.com/v1/search?name=%s&count=1&language=en&format=json%s", q,
             zip ? "&countryCode=US" : "");
    char *body;
    size_t len;
    if (!net_http_get(url, &body, &len)) {
        return false;
    }
    double lat, lon;
    char name[64];
    bool ok = wx_parse_geocode(body, len, &lat, &lon, name, sizeof(name));
    free(body);
    if (ok) {
        S->lat = lat;
        S->lon = lon;
        S->has_coords = true;
        snprintf(S->place_name, sizeof(S->place_name), "%s", name);
        settings_save(S);
        ESP_LOGI(TAG, "'%s' -> %s (%.4f, %.4f)", S->place_query, name, lat, lon);
    } else {
        ESP_LOGW(TAG, "no match for '%s'", S->place_query);
    }
    return ok;
}

static bool fetch_weather(void)
{
    char url[700];
    snprintf(url, sizeof(url),
             "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
             "&current=temperature_2m,apparent_temperature,is_day,weather_code,wind_speed_10m,wind_gusts_10m,precipitation"
             "&hourly=temperature_2m,apparent_temperature,precipitation_probability,weather_code,is_day"
             "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max,uv_index_max,"
             "sunshine_duration"
             "&temperature_unit=%s&wind_speed_unit=%s&precipitation_unit=%s&timezone=auto&forecast_days=4&past_days=1",
             S->lat, S->lon, S->celsius ? "celsius" : "fahrenheit", S->celsius ? "kmh" : "mph",
             S->celsius ? "mm" : "inch");
    char *body;
    size_t len;
    if (!net_http_get(url, &body, &len)) {
        return false;
    }
    static wx_t fresh;
    bool ok = wx_parse_openmeteo(body, len, !S->celsius, &fresh);
    free(body);
    if (!ok) {
        ESP_LOGW(TAG, "could not parse forecast");
        return false;
    }
    s_wx = fresh;
    s_utc_offset = s_wx.utc_offset_s;
    s_have_offset = true;
    ESP_LOGI(TAG, "now %.1f (feels %.1f) code %d, %d periods", s_wx.temp, s_wx.feels, s_wx.code, s_wx.nperiods);
    return true;
}

static void show_weather(bool fresh_ok, const char *last_update)
{
    ui_info_t info = {0};
    snprintf(info.wifi, sizeof(info.wifi), "%s", S->ssid);
    snprintf(info.place, sizeof(info.place), "%s", S->place_name);
    snprintf(info.ip, sizeof(info.ip), "%s", s_ip);
    snprintf(info.version, sizeof(info.version), "%s", WB_VERSION);
    if (last_update[0]) {
        snprintf(info.updated, sizeof(info.updated), "Weather updated %s", last_update);
    }
    char status[64];
    if (fresh_ok) {
        snprintf(status, sizeof(status), "Updated %s", last_update);
    } else {
        snprintf(status, sizeof(status), "Offline. Last update %s", last_update);
    }
    UI_LOCKED({
        ui_show_weather(&s_wx);
        ui_set_footer(S->place_name, status, !fresh_ok);
        ui_set_info(&info);
    });
}

/* ------------------------------------------------------------------ main loop */

static void go_portal(void)
{
    s_portal = true;
    esp_timer_stop(s_reconnect);
    portal_run(S); /* does not return */
}

static void net_task(void *arg)
{
    (void)arg;
    wifi_init();

    if (!S->ssid[0]) {
        go_portal();
    }

    UI_LOCKED(ui_show_connecting(S->ssid));
    wifi_start_sta();

    /* Wait for the first connection, showing an error (with a setup button) if it takes too long. */
    bool shown_error = false;
    while (1) {
        EventBits_t b = xEventGroupWaitBits(s_ev, EV_CONNECTED | EV_PORTAL, pdFALSE, pdFALSE,
                                            pdMS_TO_TICKS(CONNECT_WAIT_MS));
        if (b & EV_PORTAL) {
            go_portal();
        }
        if (b & EV_CONNECTED) {
            break;
        }
        if (!shown_error) {
            UI_LOCKED(ui_show_wifi_error(S->ssid, NULL));
            shown_error = true;
        }
    }

    esp_sntp_config_t sntp = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_netif_sntp_init(&sntp);

    if (!S->has_coords) {
        UI_LOCKED(ui_show_boot("Finding your town..."));
        while (!geocode()) {
            UI_LOCKED(ui_show_wifi_error(S->ssid, "Couldn't find your town. Check the spelling in setup."));
            EventBits_t b = xEventGroupWaitBits(s_ev, EV_PORTAL | EV_REFRESH, pdTRUE, pdFALSE,
                                                pdMS_TO_TICKS(RETRY_FAST_MS));
            if (b & EV_PORTAL) {
                go_portal();
            }
        }
    }

    UI_LOCKED(ui_show_boot("Asking the sky..."));
    bool have = false;
    int fails = 0;
    char last[16] = "";
    while (1) {
        /* Don't hammer the API while WiFi is down; the driver keeps reconnecting on its own. */
        xEventGroupWaitBits(s_ev, EV_CONNECTED, pdFALSE, pdFALSE, pdMS_TO_TICKS(CONNECT_WAIT_MS));
        bool ok = (xEventGroupGetBits(s_ev) & EV_CONNECTED) && fetch_weather();
        if (ok) {
            fails = 0;
            have = true;
            struct tm t;
            if (net_local_time(&t)) {
                fmt_clock(last, sizeof(last), t.tm_hour, t.tm_min);
            } else {
                fmt_clock(last, sizeof(last), s_wx.hour, s_wx.minute);
            }
            show_weather(true, last);
        } else {
            fails++;
            if (have) {
                show_weather(false, last);
            } else if (!(xEventGroupGetBits(s_ev) & EV_CONNECTED)) {
                UI_LOCKED(ui_show_wifi_error(S->ssid, NULL));
            } else {
                UI_LOCKED(ui_show_boot("Can't reach the weather service. Trying again soon."));
            }
        }
        uint32_t wait = ok ? REFRESH_MS : (fails < 3 ? RETRY_FAST_MS : RETRY_SLOW_MS);
        EventBits_t b = xEventGroupWaitBits(s_ev, EV_REFRESH | EV_PORTAL, pdTRUE, pdFALSE, pdMS_TO_TICKS(wait));
        if (b & EV_PORTAL) {
            go_portal();
        }
    }
}

void net_start(wb_settings_t *s)
{
    S = s;
    s_ev = xEventGroupCreate();
    xTaskCreatePinnedToCore(net_task, "net", 12288, NULL, 5, NULL, 0);
}
