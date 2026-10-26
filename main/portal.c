/*
 * First-time setup: the board becomes a WiFi hotspot ("WeatherBesties-XXXX"),
 * phones get pushed to a small settings page, and saving it restarts the board.
 */
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "esp_netif.h"
#include "dns_server.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "net.h"

static const char *TAG = "portal";

#define MAX_NETS 20

static wb_settings_t *S;
static char s_ap[32];
static char s_nets[MAX_NETS][33];
static int s_nnets;
static esp_timer_handle_t s_restart;

/* ------------------------------------------------------------------ small string helpers */

typedef struct {
    char *p;
    size_t n, cap;
} sbuf_t;

static void sb_add(sbuf_t *b, const char *fmt, ...)
{
    va_list ap;
    while (1) {
        va_start(ap, fmt);
        int w = vsnprintf(b->p + b->n, b->cap - b->n, fmt, ap);
        va_end(ap);
        if (w < 0) {
            return;
        }
        if (b->n + (size_t)w < b->cap) {
            b->n += w;
            return;
        }
        size_t nc = b->cap * 2 + w;
        char *np = heap_caps_realloc(b->p, nc, MALLOC_CAP_SPIRAM);
        if (!np) {
            return;
        }
        b->p = np;
        b->cap = nc;
    }
}

static void html_esc(const char *in, char *out, size_t cap)
{
    size_t o = 0;
    for (; *in && o + 7 < cap; in++) {
        switch (*in) {
        case '&': o += sprintf(out + o, "&amp;"); break;
        case '<': o += sprintf(out + o, "&lt;"); break;
        case '>': o += sprintf(out + o, "&gt;"); break;
        case '"': o += sprintf(out + o, "&quot;"); break;
        case '\'': o += sprintf(out + o, "&#39;"); break;
        default: out[o++] = *in;
        }
    }
    out[o] = 0;
}

static void url_decode(char *s)
{
    char *o = s;
    for (; *s; s++) {
        if (*s == '+') {
            *o++ = ' ';
        } else if (*s == '%' && isxdigit((unsigned char)s[1]) && isxdigit((unsigned char)s[2])) {
            char h[3] = {s[1], s[2], 0};
            *o++ = (char)strtol(h, NULL, 16);
            s += 2;
        } else {
            *o++ = *s;
        }
    }
    *o = 0;
}

static bool form_get(const char *body, const char *key, char *out, size_t cap)
{
    out[0] = 0;
    if (httpd_query_key_value(body, key, out, cap) != ESP_OK) {
        out[0] = 0;
        return false;
    }
    url_decode(out);
    /* trim */
    size_t n = strlen(out);
    while (n && isspace((unsigned char)out[n - 1])) {
        out[--n] = 0;
    }
    size_t lead = 0;
    while (out[lead] && isspace((unsigned char)out[lead])) {
        lead++;
    }
    if (lead) {
        memmove(out, out + lead, strlen(out + lead) + 1);
    }
    return true;
}

/* ------------------------------------------------------------------ wifi scan */

static void scan(void)
{
    wifi_scan_config_t sc = {.show_hidden = false};
    if (esp_wifi_scan_start(&sc, true) != ESP_OK) {
        return;
    }
    uint16_t n = 40;
    wifi_ap_record_t *rec = calloc(n, sizeof(*rec));
    if (!rec) {
        return;
    }
    esp_wifi_scan_get_ap_records(&n, rec); /* sorted by signal strength */
    s_nnets = 0;
    for (int i = 0; i < n && s_nnets < MAX_NETS; i++) {
        const char *ssid = (const char *)rec[i].ssid;
        if (!ssid[0]) {
            continue;
        }
        bool dup = false;
        for (int j = 0; j < s_nnets; j++) {
            dup |= strcmp(s_nets[j], ssid) == 0;
        }
        if (!dup) {
            snprintf(s_nets[s_nnets++], sizeof(s_nets[0]), "%s", ssid);
        }
    }
    free(rec);
    ESP_LOGI(TAG, "found %d networks", s_nnets);
}

/* ------------------------------------------------------------------ pages */

static const char *CSS =
    ":root{--ink:#1B2542;--pink:#E0457B}*{box-sizing:border-box}"
    "body{margin:0;font-family:-apple-system,system-ui,'Segoe UI',Roboto,sans-serif;"
    "background:linear-gradient(#FFD9EA,#FFF6FA) fixed;color:var(--ink)}"
    "main{max-width:460px;margin:0 auto;padding:22px 18px 40px}"
    "h1{font-size:27px;margin:8px 0 2px}p.sub{margin:0 0 18px;color:#3B4868}"
    ".card{background:#fff;border-radius:20px;padding:18px;margin-bottom:14px;box-shadow:0 6px 20px rgba(176,96,127,.15)}"
    "label{display:block;font-weight:600;margin:0 0 6px}"
    "input,select{width:100%;font-size:17px;padding:12px 14px;border:2px solid #E6E9F2;border-radius:12px;"
    "color:var(--ink);background:#fff}input:focus,select:focus{outline:none;border-color:var(--pink)}"
    ".nets{display:flex;flex-wrap:wrap;gap:8px;margin:10px 0 0}"
    ".nets button{border:0;background:#F1F3F9;color:var(--ink);border-radius:999px;padding:8px 13px;font-size:15px}"
    ".row{display:flex;gap:10px}.row>*{flex:1}.units{display:flex;gap:10px}"
    ".units label{flex:1;font-weight:500;border:2px solid #E6E9F2;border-radius:12px;padding:10px;text-align:center}"
    ".units input,.check input{width:auto;margin:0 8px 0 0;transform:scale(1.3)}"
    ".hint{font-size:14px;color:#5B6683;margin:6px 0 0}.hint a{color:var(--pink)}"
    ".check{display:flex;align-items:center;font-weight:600}"
    "button.save{width:100%;border:0;border-radius:999px;background:var(--pink);color:#fff;font-size:19px;"
    "font-weight:700;padding:16px;margin-top:4px}";

static void hour_options(sbuf_t *b, int sel)
{
    for (int h = 0; h < 24; h++) {
        int h12 = h % 12 ? h % 12 : 12;
        sb_add(b, "<option value=\"%d\"%s>%d %s</option>", h, h == sel ? " selected" : "", h12, h < 12 ? "AM" : "PM");
    }
}

static esp_err_t page_root(httpd_req_t *req)
{
    char q[32];
    if (httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK && strstr(q, "rescan")) {
        scan();
    }
    sbuf_t b = {.cap = 12 * 1024};
    b.p = heap_caps_malloc(b.cap, MALLOC_CAP_SPIRAM);
    if (!b.p) {
        return httpd_resp_send_500(req);
    }
    char e1[200], e2[200], e3[120], e4[1700];
    sb_add(&b, "<!doctype html><html><head><meta charset=\"utf-8\">"
               "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
               "<title>Weather Besties setup</title><style>%s</style></head><body><main>"
               "<h1>Weather Besties</h1><p class=\"sub\">Tell the cats where they live.</p>"
               "<form method=\"post\" action=\"/save\">",
           CSS);

    html_esc(S->ssid, e1, sizeof(e1));
    sb_add(&b, "<div class=\"card\"><label for=\"ssid\">Home WiFi</label>"
               "<input id=\"ssid\" name=\"ssid\" value=\"%s\" autocomplete=\"off\" autocapitalize=\"none\" required>"
               "<div class=\"nets\">",
           e1);
    for (int i = 0; i < s_nnets; i++) {
        html_esc(s_nets[i], e1, sizeof(e1));
        sb_add(&b, "<button data-s=\"%s\">%s</button>", e1, e1);
    }
    sb_add(&b, "</div><p class=\"hint\">Tap your network, or <a href=\"/?rescan=1\">scan again</a>.</p>"
               "<label for=\"pass\" style=\"margin-top:14px\">WiFi password</label>"
               "<input id=\"pass\" name=\"pass\" type=\"password\" placeholder=\"%s\">"
               "<label class=\"check\" style=\"margin-top:10px;font-weight:500\"><input type=\"checkbox\" "
               "onclick=\"pass.type=this.checked?'text':'password'\">Show password</label></div>",
           S->pass[0] ? "Leave blank to keep the saved one" : "");

    html_esc(S->place_query, e2, sizeof(e2));
    html_esc(S->names, e3, sizeof(e3));
    html_esc(S->cal_url, e4, sizeof(e4));
    sb_add(&b, "<div class=\"card\"><label for=\"place\">Town or ZIP code</label>"
               "<input id=\"place\" name=\"place\" value=\"%s\" required>"
               "<p class=\"hint\">Like \"Montclair, NJ\" or \"07043\".</p>"
               "<label for=\"names\" style=\"margin-top:14px\">Names on the screen</label>"
               "<input id=\"names\" name=\"names\" value=\"%s\" maxlength=\"40\">"
               "<label for=\"cal\" style=\"margin-top:14px\">Kids calendar link (iCal)</label>"
               "<input id=\"cal\" name=\"cal\" value=\"%s\" autocomplete=\"off\" autocapitalize=\"none\">"
               "<p class=\"hint\">Optional. Paste the secret iCal link of the kids calendar and the "
               "My Day page will show what the girls have today and tomorrow. Leave blank for none.</p></div>",
           e2, e3, e4);

    sb_add(&b, "<div class=\"card\"><label>Temperature</label><div class=\"units\">"
               "<label><input type=\"radio\" name=\"units\" value=\"f\"%s>&deg;F</label>"
               "<label><input type=\"radio\" name=\"units\" value=\"c\"%s>&deg;C</label></div>"
               "<label class=\"check\" style=\"margin-top:16px\"><input type=\"checkbox\" name=\"noff\" value=\"1\"%s>"
               "Turn the screen off at night</label><div class=\"row\" style=\"margin-top:8px\"><select name=\"from\">",
           S->celsius ? "" : " checked", S->celsius ? " checked" : "", S->night_off ? " checked" : "");
    hour_options(&b, S->off_from);
    sb_add(&b, "</select><select name=\"to\">");
    hour_options(&b, S->off_to);
    sb_add(&b, "</select></div><p class=\"hint\">Off at, back on at. A tap wakes it for a minute.</p>");
    sb_add(&b, "<label class=\"check\" style=\"margin-top:16px\"><input type=\"checkbox\" name=\"autoup\" value=\"1\"%s>"
               "Install updates automatically</label><p class=\"hint\">Checks around 3 AM. Settings on the screen "
               "has an Update button too. Off unless you turn it on.</p>",
           S->auto_update ? " checked" : "");
    sb_add(&b, "<label for=\"bbus\" style=\"margin-top:16px\">Bike bus day</label><select id=\"bbus\" name=\"bbus\">");
    static const char *days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "No bike bus"};
    for (int d = 0; d < 8; d++) {
        sb_add(&b, "<option value=\"%d\"%s>%s</option>", d, d == S->bike_bus ? " selected" : "", days[d]);
    }
    sb_add(&b, "</select><p class=\"hint\">Cold mornings that day get a gloves reminder.</p></div>"
               "<button class=\"save\" type=\"submit\">Save</button></form></main>"
               "<script>document.querySelectorAll('.nets button').forEach(function(b){b.onclick=function(e){"
               "e.preventDefault();document.getElementById('ssid').value=b.dataset.s;"
               "document.getElementById('pass').focus()}})</script></body></html>");

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t r = httpd_resp_send(req, b.p, b.n);
    free(b.p);
    return r;
}

static void restart_cb(void *arg)
{
    (void)arg;
    esp_restart();
}

static esp_err_t page_save(httpd_req_t *req)
{
    int total = req->content_len;
    if (total <= 0 || total > 4096) {   /* the calendar link alone can run long */
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad form");
    }
    char *body = calloc(1, total + 1);
    if (!body) {
        return httpd_resp_send_500(req);
    }
    int got = 0;
    while (got < total) {
        int r = httpd_req_recv(req, body + got, total - got);
        if (r <= 0) {
            free(body);
            return ESP_FAIL;
        }
        got += r;
    }

    wb_settings_t n = *S;
    char v[130];
    form_get(body, "ssid", n.ssid, sizeof(n.ssid));
    if (form_get(body, "pass", v, sizeof(v)) && v[0]) {
        snprintf(n.pass, sizeof(n.pass), "%s", v);
    } else if (strcmp(n.ssid, S->ssid) != 0) {
        n.pass[0] = 0; /* new network with no password = open network */
    }
    if (form_get(body, "place", v, sizeof(v)) && v[0] && strcmp(v, S->place_query) != 0) {
        snprintf(n.place_query, sizeof(n.place_query), "%s", v);
        n.has_coords = false; /* look it up after restart */
        snprintf(n.place_name, sizeof(n.place_name), "%s", v);
    }
    if (form_get(body, "names", v, sizeof(v))) {
        snprintf(n.names, sizeof(n.names), "%s", v);
    }
    char cv[300];
    if (form_get(body, "cal", cv, sizeof(cv))) {
        snprintf(n.cal_url, sizeof(n.cal_url), "%s", cv);   /* blank clears it */
    }
    n.celsius = form_get(body, "units", v, sizeof(v)) && v[0] == 'c';
    n.night_off = form_get(body, "noff", v, sizeof(v)) && v[0] == '1';
    n.auto_update = form_get(body, "autoup", v, sizeof(v)) && v[0] == '1';
    if (form_get(body, "from", v, sizeof(v))) {
        n.off_from = (uint8_t)(atoi(v) % 24);
    }
    if (form_get(body, "to", v, sizeof(v))) {
        n.off_to = (uint8_t)(atoi(v) % 24);
    }
    if (form_get(body, "bbus", v, sizeof(v)) && v[0]) {
        int d = atoi(v);
        n.bike_bus = (uint8_t)(d >= 0 && d <= 7 ? d : 7);
    }
    free(body);

    if (!n.ssid[0]) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Please pick a WiFi network");
    }
    *S = n;
    settings_save(S);
    ESP_LOGI(TAG, "saved: wifi='%s' place='%s'", S->ssid, S->place_query);

    sbuf_t b = {.cap = 4096};
    b.p = heap_caps_malloc(b.cap, MALLOC_CAP_SPIRAM);
    char e1[200];
    html_esc(S->ssid, e1, sizeof(e1));
    sb_add(&b, "<!doctype html><html><head><meta charset=\"utf-8\">"
               "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>Saved</title>"
               "<style>%s</style></head><body><main><h1>Saved!</h1><div class=\"card\">"
               "<p>The cats are restarting and will join <b>%s</b>.</p>"
               "<p>You can switch your phone back to your home WiFi now. "
               "If the screen says it can't connect, tap <b>Set up WiFi</b> and try the password again.</p>"
               "</div></main></body></html>",
           CSS, e1);
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, b.p, b.n);
    free(b.p);

    if (board_lvgl_lock(-1)) {
        ui_show_boot("Saved! Restarting...");
        board_lvgl_unlock();
    }
    esp_timer_start_once(s_restart, 1500 * 1000);
    return ESP_OK;
}

static esp_err_t not_found(httpd_req_t *req, httpd_err_code_t err)
{
    (void)err;
    /* Every unknown URL (including phones' captive-portal checks) goes to the form. */
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_send(req, "Redirecting to setup", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* ------------------------------------------------------------------ run */

void portal_run(wb_settings_t *s)
{
    S = s;
    const esp_timer_create_args_t t = {.callback = restart_cb, .name = "restart"};
    esp_timer_create(&t, &s_restart);

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(s_ap, sizeof(s_ap), "WeatherBesties-%02X%02X", mac[4], mac[5]);

    if (board_lvgl_lock(-1)) {
        ui_show_boot("Getting setup ready...");
        board_lvgl_unlock();
    }

    esp_wifi_stop();
    wifi_config_t ap = {0};
    snprintf((char *)ap.ap.ssid, sizeof(ap.ap.ssid), "%s", s_ap);
    ap.ap.ssid_len = strlen(s_ap);
    ap.ap.channel = 6;
    ap.ap.max_connection = 4;
    ap.ap.authmode = WIFI_AUTH_OPEN;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_set_ps(WIFI_PS_NONE);
    scan();

    /* Tell phones (DHCP option 114) where the setup page is. */
    esp_netif_t *apif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (apif) {
        static char uri[] = "http://192.168.4.1";
        esp_netif_dhcps_stop(apif);
        esp_netif_dhcps_option(apif, ESP_NETIF_OP_SET, ESP_NETIF_CAPTIVEPORTAL_URI, uri, strlen(uri));
        esp_netif_dhcps_start(apif);
    }

    httpd_handle_t srv = NULL;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size = 8192;
    cfg.max_open_sockets = 7;
    cfg.lru_purge_enable = true;
    ESP_ERROR_CHECK(httpd_start(&srv, &cfg));
    httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = page_root};
    httpd_uri_t save = {.uri = "/save", .method = HTTP_POST, .handler = page_save};
    httpd_register_uri_handler(srv, &root);
    httpd_register_uri_handler(srv, &save);
    httpd_register_err_handler(srv, HTTPD_404_NOT_FOUND, not_found);

    dns_server_config_t dns = DNS_SERVER_CONFIG_SINGLE("*", "WIFI_AP_DEF");
    start_dns_server(&dns);

    ESP_LOGI(TAG, "setup hotspot '%s' is up at http://192.168.4.1", s_ap);
    if (board_lvgl_lock(-1)) {
        ui_show_setup(s_ap, "192.168.4.1");
        board_lvgl_unlock();
    }
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
