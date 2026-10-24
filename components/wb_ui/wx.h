/*
 * Weather data model + Open-Meteo parser. Plain C, no ESP-IDF, so the desktop
 * renderer can run the exact same code against sample responses.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WX_MAX_HOURS 96
#define WX_MAX_DAYS 4
#define WX_MAX_PERIODS 4

/* Same order as icons.WEATHER in tools/icons.py. */
typedef enum {
    WXI_CLEAR_DAY, WXI_CLEAR_NIGHT, WXI_PARTLY_DAY, WXI_PARTLY_NIGHT, WXI_CLOUDY, WXI_FOG,
    WXI_DRIZZLE, WXI_RAIN, WXI_HEAVY_RAIN, WXI_SNOW, WXI_SLEET, WXI_THUNDER, WXI_COUNT
} wx_icon_t;

typedef enum { WXK_CLEAR, WXK_CLOUDS, WXK_FOG, WXK_DRIZZLE, WXK_RAIN, WXK_SNOW, WXK_ICE, WXK_THUNDER } wx_kind_t;

typedef struct {
    char label[20];   /* "Morning", "Tomorrow AM" ... */
    float temp;       /* display units */
    int code;         /* WMO weather code */
    bool is_day;
    int pop;          /* chance of rain/snow, % */
} wx_period_t;

typedef struct {
    int year, month, day, wday;  /* wday: 0 = Sunday */
    int code;
    float tmax, tmin;
    int pop;
    float uv;
    float sun_h;      /* hours of sunshine */
} wx_day_t;

typedef struct {
    bool valid;
    bool fahrenheit;

    /* Local time at the location, as reported by the API ("current.time"). */
    int year, month, day, hour, minute, wday;
    int utc_offset_s;

    /* Right now */
    float temp, feels, wind, gust, precip;
    int code;
    bool is_day;

    /* Hourly, starting today 00:00 local */
    int nhours;
    int now_idx;
    float h_temp[WX_MAX_HOURS];
    float h_feels[WX_MAX_HOURS];
    int8_t h_pop[WX_MAX_HOURS];
    uint8_t h_code[WX_MAX_HOURS];
    bool h_day[WX_MAX_HOURS];

    /* Derived: the next four parts of the day, starting with the current one */
    int nperiods;
    wx_period_t periods[WX_MAX_PERIODS];

    /* Daily, days[0] is today */
    int ndays;
    wx_day_t days[WX_MAX_DAYS];

    /* Yesterday (needs past_days=1 in the request) */
    bool has_yday;
    wx_day_t yday;
} wx_t;

/* The three moments the girls care about: walking to school (8 AM), recess
 * (noon) and going home (3 PM). Weekends and summer use Morning (9 AM),
 * Lunchtime and Afternoon instead. After 3:30 PM it shows tomorrow. */
typedef struct {
    char title[24];   /* "Walk to school" */
    char sub[32];     /* "Morning", "Tomorrow morning", "Saturday" */
    float temp;       /* display units */
    int code;         /* worst weather in the hour either side */
    bool is_day;
    int pop;          /* highest chance of rain/snow in the hour either side, % */
    bool past;        /* already happened today (shown faded) */
} wx_moment_t;

#define WX_MOMENTS 3
/* now_hour/now_min: local clock. Returns how many moments were filled. */
int wx_kid_moments(const wx_t *wx, int now_hour, int now_min, wx_moment_t out[WX_MOMENTS]);

/* Parses an Open-Meteo /v1/forecast response (see net.c for the query) and derives periods. */
bool wx_parse_openmeteo(const char *json, size_t len, bool fahrenheit, wx_t *out);

/* Parses an Open-Meteo geocoding response. Returns false if there were no results. */
bool wx_parse_geocode(const char *json, size_t len, double *lat, double *lon, char *name, size_t name_len);

void wx_derive_periods(wx_t *wx);

wx_icon_t wx_icon_for(int code, bool is_day);
wx_kind_t wx_kind(int code);
int wx_severity(int code);
const char *wx_describe(int code, bool is_day);
const char *wx_wday_name(int wday, bool short_name);
const char *wx_month_name(int month, bool short_name);
int wx_day_of_week(int y, int m, int d);
float wx_to_f(const wx_t *wx, float t);  /* display units -> Fahrenheit */

#ifdef __cplusplus
}
#endif
