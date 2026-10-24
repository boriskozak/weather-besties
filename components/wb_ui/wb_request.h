/*
 * Builds the Open-Meteo request addresses and works out how big the request
 * will be on the wire. Plain C, so sim/test_request.c checks the exact same
 * code the board runs.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Buffers handed to esp_http_client (main/net.c). The send buffer has to hold
 * the request line plus every header in one go; ESP-IDF's default is 512, and
 * the forecast request alone is ~490 bytes, which is what broke v1.1.0. */
#define WB_HTTP_TX_BUF 2048
#define WB_HTTP_RX_BUF 2048

/* Size of the url buffers in net.c. */
#define WB_URL_MAX 700

#define WB_FORECAST_HOST "api.open-meteo.com"
#define WB_GEOCODE_HOST "geocoding-api.open-meteo.com"

/* Both return the full length the url needs (like snprintf). A result >= n
 * means it was cut off. */
int wb_forecast_url(char *buf, size_t n, double lat, double lon, bool celsius);
int wb_geocode_url(char *buf, size_t n, const char *place_query);

/* Percent-encodes everything except A-Z a-z 0-9 - _ . ~ */
void wb_url_encode(const char *in, char *out, size_t cap);

/* Bytes esp_http_client (ESP-IDF v5.5) needs in its send buffer for a GET of
 * this url: "GET <path?query> HTTP/1.1\r\n", then "User-Agent" and "Host"
 * headers, the blank line, and the room its header writer keeps spare. Counts
 * the Host header with a ":65535" port on it to stay on the safe side.
 * Returns -1 for a url it can't parse. */
int wb_request_head_len(const char *url, const char *user_agent);

#ifdef __cplusplus
}
#endif
