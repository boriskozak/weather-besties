#include "wb_request.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

int wb_forecast_url(char *buf, size_t n, double lat, double lon, bool celsius)
{
    return snprintf(buf, n,
                    "https://" WB_FORECAST_HOST "/v1/forecast?latitude=%.4f&longitude=%.4f"
                    "&current=temperature_2m,apparent_temperature,is_day,weather_code,wind_speed_10m,wind_gusts_10m,"
                    "precipitation"
                    "&hourly=temperature_2m,apparent_temperature,precipitation_probability,weather_code,is_day"
                    "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max,"
                    "uv_index_max,sunshine_duration"
                    "&temperature_unit=%s&wind_speed_unit=%s&precipitation_unit=%s&timezone=auto&forecast_days=4"
                    "&past_days=1",
                    lat, lon, celsius ? "celsius" : "fahrenheit", celsius ? "kmh" : "mph", celsius ? "mm" : "inch");
}

void wb_url_encode(const char *in, char *out, size_t cap)
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

int wb_geocode_url(char *buf, size_t n, const char *place_query)
{
    char q[200];
    wb_url_encode(place_query, q, sizeof(q));
    bool zip = strlen(place_query) == 5;
    for (int i = 0; zip && i < 5; i++) {
        zip = isdigit((unsigned char)place_query[i]);
    }
    return snprintf(buf, n, "https://" WB_GEOCODE_HOST "/v1/search?name=%s&count=1&language=en&format=json%s", q,
                    zip ? "&countryCode=US" : "");
}

int wb_request_head_len(const char *url, const char *user_agent)
{
    const char *p = strstr(url, "://");
    if (!p) {
        return -1;
    }
    p += 3;
    const char *path = strchr(p, '/');
    if (!path) {
        return -1;
    }
    int host_len = (int)(path - p);
    int path_len = (int)strlen(path);

    int first_line = (int)strlen("GET ") + path_len + (int)strlen(" HTTP/1.1\r\n");
    int headers = 0;
    headers += (int)strlen("User-Agent") + (int)strlen(user_agent) + 4;  /* ": " + "\r\n" */
    headers += (int)strlen("Host") + host_len + (int)strlen(":65535") + 4;
    /* http_header_generate_string() wants size + 1 <= space - 2: the blank
     * line plus a terminating NUL. */
    return first_line + headers + 3;
}
