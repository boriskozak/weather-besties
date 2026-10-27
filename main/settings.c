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
    /* No preset town: a fresh board has no weather until the owner
     * enters a town in setup. A board that was set up keeps the town
     * it saved in NVS. */
    s->place_query[0] = 0;
    s->place_name[0] = 0;
    s->lat = 0.0;
    s->lon = 0.0;
    s->has_coords = false;
    snprintf(s->names, sizeof(s->names), "Cleo & Lila");
    snprintf(s->kid1, sizeof(s->kid1), "Cleo");
    snprintf(s->kid2, sizeof(s->kid2), "Lila");
    s->celsius = false;
    s->night_off = false;
    s->auto_update = false;   /* iter-32: off unless a grown-up turns it on in setup */
    /* Base value for an upgrade: a board set up before this field
     * existed keeps the uniform looks. settings_load sets it false
     * when the board was never set up. */
    s->uniform = true;
    s->off_from = 21;
    s->off_to = 6;
    s->bike_bus = 7; /* no bike bus until the owner picks a day */
    s->cat = 0;      /* black cat */
}

/* True when the key holds a value in this namespace. nvs_get_str
 * with a NULL buffer only reports the stored length, which is all
 * the upgrade check in settings_load needs. */
static bool nvs_key_present(nvs_handle_t h, const char *key)
{
    size_t len = 0;
    return nvs_get_str(h, key, NULL, &len) == ESP_OK && len > 0;
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

/* Copy the span [begin, end) into dst, trimming spaces at both ends. */
static void trim_span(char *dst, size_t cap, const char *begin, const char *end)
{
    while (begin < end && *begin == ' ') {
        begin++;
    }
    while (end > begin && end[-1] == ' ') {
        end--;
    }
    size_t len = (size_t)(end - begin);
    if (len >= cap) {
        len = cap - 1;
    }
    memcpy(dst, begin, len);
    dst[len] = 0;
}

/* Iter-38 migration: boards set up before the per-kid fields existed
 * have only the legacy greeting string in NVS ("Cleo & Lila"). Seed
 * whichever kid field is still empty from that string, split on '&'
 * or ',', so the My Day name tags keep working after the upgrade
 * with zero re-setup. A field neither source can fill falls back to
 * the shipped defaults. */
static void seed_kids_from_names(wb_settings_t *s)
{
    if (!s->kid1[0] || !s->kid2[0]) {
        const char *nm = s->names;
        const char *sep = nm;
        while (*sep && *sep != '&' && *sep != ',') {
            sep++;
        }
        char first[24], second[24];
        trim_span(first, sizeof(first), nm, sep);
        trim_span(second, sizeof(second), *sep ? sep + 1 : sep, nm + strlen(nm));
        if (!s->kid1[0] && first[0]) {
            snprintf(s->kid1, sizeof(s->kid1), "%s", first);
        }
        if (!s->kid2[0] && second[0]) {
            snprintf(s->kid2, sizeof(s->kid2), "%s", second);
        }
    }
    if (!s->kid1[0]) {
        snprintf(s->kid1, sizeof(s->kid1), "Cleo");
    }
    if (!s->kid2[0]) {
        snprintf(s->kid2, sizeof(s->kid2), "Lila");
    }
}

void settings_load(wb_settings_t *s)
{
    settings_defaults(s);
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) {
        /* Nothing was ever stored: a fresh board. Uniform outfits
         * default off there; the owner opts in during setup. */
        s->uniform = false;
        ESP_LOGI(TAG, "no saved settings, using defaults");
        return;
    }
    /* Uniform default: on for an upgrade (the board was set up
     * before the "unif" key existed, so its behavior must not
     * change), off when the namespace holds nothing from a previous
     * setup. Any previously known key marks an upgrade. A stored
     * "unif" value, loaded below, wins either way. */
    if (!nvs_key_present(h, "ssid") && !nvs_key_present(h, "pq") &&
        !nvs_key_present(h, "names") && !nvs_key_present(h, "cal")) {
        s->uniform = false;
    }
    get_str(h, "ssid", s->ssid, sizeof(s->ssid));
    get_str(h, "pass", s->pass, sizeof(s->pass));
    get_str(h, "pq", s->place_query, sizeof(s->place_query));
    get_str(h, "pn", s->place_name, sizeof(s->place_name));
    get_str(h, "names", s->names, sizeof(s->names));
    /* The defaults filled these already; clear them so an absent NVS
     * key is visible and the legacy-names migration below can run. */
    s->kid1[0] = 0;
    s->kid2[0] = 0;
    get_str(h, "kid1", s->kid1, sizeof(s->kid1));
    get_str(h, "kid2", s->kid2, sizeof(s->kid2));
    seed_kids_from_names(s);
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
    b = s->auto_update;
    get_u8(h, "autoup", &b);
    s->auto_update = b;
    b = s->uniform;
    get_u8(h, "unif", &b);
    s->uniform = b;
    get_u8(h, "nfrom", &s->off_from);
    get_u8(h, "nto", &s->off_to);
    get_u8(h, "bbus", &s->bike_bus);
    get_u8(h, "cat", &s->cat);
    get_str(h, "cal", s->cal_url, sizeof(s->cal_url));
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
    nvs_set_str(h, "kid1", s->kid1);
    nvs_set_str(h, "kid2", s->kid2);
    nvs_set_blob(h, "lat", &s->lat, sizeof(s->lat));
    nvs_set_blob(h, "lon", &s->lon, sizeof(s->lon));
    nvs_set_u8(h, "coords", s->has_coords);
    nvs_set_u8(h, "cel", s->celsius);
    nvs_set_u8(h, "noff", s->night_off);
    nvs_set_u8(h, "autoup", s->auto_update);
    nvs_set_u8(h, "unif", s->uniform);
    nvs_set_u8(h, "nfrom", s->off_from);
    nvs_set_u8(h, "nto", s->off_to);
    nvs_set_u8(h, "bbus", s->bike_bus);
    nvs_set_u8(h, "cat", s->cat);
    nvs_set_str(h, "cal", s->cal_url);
    nvs_commit(h);
    nvs_close(h);
}
