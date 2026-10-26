#include "ota.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mbedtls/sha256.h"
#include "net.h"
#include "ui.h"
#include "update.h"
#include "wb_request.h"   /* WB_URL_MAX (net.h does not pull it in here) */

static const char *TAG = "ota";

static esp_timer_handle_t s_trial;
static bool s_on_trial;

/* ------------------------------------------------------------------ trial run */

static void trial_expired(void *arg)
{
    (void)arg;
    ESP_LOGE(TAG, "new version never got the weather in %d minutes; restarting into the previous one",
             OTA_TRIAL_MIN);
    esp_restart();   /* still "pending verify", so the bootloader rolls back */
}

void ota_boot_check(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t st;
    ESP_LOGI(TAG, "running %s from %s", esp_app_get_description()->version, running->label);
    if (esp_ota_get_state_partition(running, &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY) {
        s_on_trial = true;
        const esp_timer_create_args_t t = {.callback = trial_expired, .name = "ota_trial"};
        esp_timer_create(&t, &s_trial);
        esp_timer_start_once(s_trial, (uint64_t)OTA_TRIAL_MIN * 60 * 1000 * 1000);
        ESP_LOGW(TAG, "new version on trial: needs a weather download within %d minutes", OTA_TRIAL_MIN);
    }
}

void ota_mark_good(void)
{
    if (!s_on_trial) {
        return;
    }
    s_on_trial = false;
    esp_timer_stop(s_trial);
    esp_ota_mark_app_valid_cancel_rollback();
    ESP_LOGI(TAG, "new version works; keeping it");
}

/* ------------------------------------------------------------------ screen */

static void say(const char *msg)
{
    if (board_lvgl_lock(-1)) {
        ui_show_boot(msg);
        board_lvgl_unlock();
    }
}

static void say_briefly(const char *msg)
{
    say(msg);
    vTaskDelay(pdMS_TO_TICKS(3000));   /* the weather screen comes back on the next refresh */
}

/* ------------------------------------------------------------------ download */

typedef struct {
    esp_ota_handle_t h;
    mbedtls_sha256_context sha;
    size_t got, size;
    int shown_pct;
    bool write_failed;
} dl_t;

static bool on_chunk(const char *data, size_t len, void *ctx)
{
    dl_t *d = ctx;
    if (d->got + len > d->size) {
        return false;   /* bigger than the manifest said */
    }
    if (esp_ota_write(d->h, data, len) != ESP_OK) {
        d->write_failed = true;
        return false;
    }
    mbedtls_sha256_update(&d->sha, (const unsigned char *)data, len);
    d->got += len;
    int pct = (int)(d->got * 100 / d->size);
    if (pct / 10 != d->shown_pct / 10) {
        d->shown_pct = pct;
        char m[96];
        snprintf(m, sizeof(m), "Learning new tricks... %d%%\nPlease don't unplug me!", pct);
        say(m);
    }
    return true;
}

void ota_run(bool manual)
{
    char *body;
    size_t len;
    wb_manifest_t m;
    if (manual) {
        say("Looking for new tricks...");
    }
    bool have = net_http_get(WB_UPDATE_MANIFEST, &body, &len);
    if (have) {
        have = update_parse_manifest(body, len, &m);
        free(body);
    }
    if (!have) {
        ESP_LOGW(TAG, "couldn't read %s", WB_UPDATE_MANIFEST);
        if (manual) {
            say_briefly("Couldn't check for updates.\nTrying again tonight.");
        }
        return;
    }
    if (update_version_cmp(m.version, WB_VERSION) <= 0) {
        ESP_LOGI(TAG, "up to date (%s; latest is %s)", WB_VERSION, m.version);
        if (manual) {
            char msg[64];
            snprintf(msg, sizeof(msg), "Up to date! (version %s)", WB_VERSION);
            say_briefly(msg);
        }
        return;
    }

    const esp_partition_t *part = esp_ota_get_next_update_partition(NULL);
    if (!part || m.size > part->size) {
        ESP_LOGE(TAG, "no room for %s (%u bytes)", m.app, (unsigned)m.size);
        return;
    }
    ESP_LOGI(TAG, "updating %s -> %s into %s", WB_VERSION, m.version, part->label);
    say("Learning new tricks... 0%\nPlease don't unplug me!");

    static dl_t d;
    memset(&d, 0, sizeof(d));
    d.size = m.size;
    d.shown_pct = 0;
    esp_err_t err = esp_ota_begin(part, OTA_WITH_SEQUENTIAL_WRITES, &d.h);
    if (err != ESP_OK) {
        /* e.g. ESP_ERR_OTA_ROLLBACK_INVALID_STATE: this version is still on trial */
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        say_briefly("Can't update right now.\nTrying again tonight.");
        return;
    }
    mbedtls_sha256_init(&d.sha);
    mbedtls_sha256_starts(&d.sha, 0);

    char url[WB_URL_MAX];
    snprintf(url, sizeof(url), "%s%s", WB_UPDATE_BASE, m.app);
    int status;
    size_t total;
    bool ok = net_http_stream(url, on_chunk, &d, &status, &total) && d.got == m.size;
    uint8_t sha[32];
    mbedtls_sha256_finish(&d.sha, sha);
    mbedtls_sha256_free(&d.sha);
    if (ok && memcmp(sha, m.sha256, 32) != 0) {
        ESP_LOGE(TAG, "SHA-256 doesn't match latest.json");
        ok = false;
    }
    if (!ok) {
        ESP_LOGE(TAG, "download failed (HTTP %d, %u of %u bytes%s)", status, (unsigned)d.got, (unsigned)m.size,
                 d.write_failed ? ", flash write error" : "");
        esp_ota_abort(d.h);
        say_briefly("Update didn't finish.\nTrying again tonight.");
        return;
    }
    /* esp_ota_end checks the image itself (header, segments, its own checksum). */
    esp_app_desc_t desc;
    if (esp_ota_end(d.h) != ESP_OK || esp_ota_get_partition_description(part, &desc) != ESP_OK ||
        strcmp(desc.version, m.version) != 0) {
        ESP_LOGE(TAG, "downloaded image isn't a valid %s", m.version);
        say_briefly("Update didn't finish.\nTrying again tonight.");
        return;
    }
    if (esp_ota_set_boot_partition(part) != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed");
        return;
    }
    ESP_LOGI(TAG, "installed %s; restarting", m.version);
    say("New tricks learned! Restarting...");
    vTaskDelay(pdMS_TO_TICKS(1500));
    esp_restart();
}
