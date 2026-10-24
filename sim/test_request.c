/*
 * Checks that every request the board can make fits the HTTP client's buffers.
 * Run with:  make test   (in sim/)
 *
 * v1.1.0 shipped with ESP-IDF's default 512-byte send buffer while the forecast
 * request needed ~560, so the board never sent it. This test uses the same url
 * builder as the firmware (components/wb_ui/wb_request.c) and the same buffer
 * sizes, so a longer query or a smaller buffer fails here instead of on the wall.
 */
#include <stdio.h>
#include <string.h>

#include "wb_request.h"

/* Longest user agent the firmware could send ("WeatherBesties/" WB_VERSION). */
#define UA_WORST "WeatherBesties/99.99.99"
/* Spare room we insist on, so a small query change doesn't land right at the edge. */
#define TX_MARGIN 256

static int fails;

static void check(const char *what, const char *url, int url_len)
{
    int head = wb_request_head_len(url, UA_WORST);
    bool url_ok = url_len < WB_URL_MAX;
    bool head_ok = head >= 0 && head + TX_MARGIN <= WB_HTTP_TX_BUF;
    printf("%-34s url %3d/%d  request %4d/%d (needs %d spare)  %s\n", what, url_len, WB_URL_MAX, head,
           WB_HTTP_TX_BUF, TX_MARGIN, url_ok && head_ok ? "ok" : "FAIL");
    if (!url_ok || !head_ok) {
        fails++;
    }
}

int main(void)
{
    char url[WB_URL_MAX + 512];  /* oversized, so we can see how long a cut-off url would have been */
    int n;

    /* The real one, Upper Montclair. */
    n = wb_forecast_url(url, sizeof(url), 40.8462, -74.2013, false);
    check("forecast, Upper Montclair, F", url, n);
    int montclair = wb_request_head_len(url, "WeatherBesties/1.1.2");

    /* Worst cases: widest coordinates, both unit systems. */
    const double lats[] = {-89.9999, 89.9999};
    const double lons[] = {-179.9999, 179.9999};
    for (int u = 0; u < 2; u++) {
        for (int i = 0; i < 2; i++) {
            for (int j = 0; j < 2; j++) {
                char what[64];
                snprintf(what, sizeof(what), "forecast, %.0f,%.0f, %s", lats[i], lons[j], u ? "C" : "F");
                n = wb_forecast_url(url, sizeof(url), lats[i], lons[j], u);
                check(what, url, n);
            }
        }
    }

    /* Geocoding: a ZIP, a normal town, and the longest setup allows (63 chars)
     * of characters that all need encoding (3 bytes each). */
    n = wb_geocode_url(url, sizeof(url), "07043");
    check("geocode, ZIP", url, n);
    n = wb_geocode_url(url, sizeof(url), "Upper Montclair, NJ");
    check("geocode, town", url, n);
    char worst[64];
    memset(worst, '&', 63);
    worst[63] = 0;
    n = wb_geocode_url(url, sizeof(url), worst);
    check("geocode, 63 chars all encoded", url, n);

    printf("\nESP-IDF's default send buffer is 512. The Montclair forecast needs %d, so it %s.\n", montclair,
           montclair > 512 ? "has to stay raised" : "would fit the default");

    if (fails) {
        printf("\n%d request(s) don't fit. Raise WB_HTTP_TX_BUF / WB_URL_MAX in wb_request.h.\n", fails);
        return 1;
    }
    printf("All requests fit.\n");
    return 0;
}
