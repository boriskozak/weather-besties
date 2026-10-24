/*
 * Outfit engine for the 2.0 clothing-guide redesign (plan section 4).
 * Plain C, no LVGL. All temperature thresholds are feels-like in degrees F
 * (converted with wx_to_f); gusts are compared in mph.
 */
#include "outfit.h"

#include <stdio.h>
#include <string.h>

/* Days since 1970-01-01 (Howard Hinnant's days_from_civil). A static copy
 * already exists in cat.c; it is static there, so this file keeps its own. */
static long days_from_civil(int y, int m, int d)
{
    y -= m <= 2;
    long era = (y >= 0 ? y : y - 399) / 400;
    long yoe = y - era * 400;
    long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

/* Inverse of days_from_civil (Hinnant's civil_from_days). */
static void civil_from_days(long z, int *y, int *m, int *d)
{
    z += 719468;
    long era = (z >= 0 ? z : z - 146096) / 146097;
    long doe = z - era * 146097;
    long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long yy = yoe + era * 400;
    long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    long mp = (5 * doy + 2) / 153;
    *d = (int)(doy - (153 * mp + 2) / 5 + 1);
    *m = (int)(mp + (mp < 10 ? 3 : -9));
    *y = (int)(yy + (*m <= 2));
}

/* Display wind units -> mph (the request ties the wind unit to the
 * temperature unit: mph when Fahrenheit, km/h when Celsius). */
static float gust_to_mph(const wx_t *wx, float v)
{
    return wx->fahrenheit ? v : v / 1.609344f;
}

const char *garment_label(int garment)
{
    static const char *n[] = {"Short sleeves", "Long sleeves", "Shorts",     "Long pants",   "Hoodie",      "Light jacket",
                              "Rain jacket",   "Warm coat",     "Winter coat", "Sneakers",     "Rain boots",  "Snow boots",
                              "Warm hat",      "Gloves",        "Umbrella",    "Sunglasses",   "Water bottle", "Scarf"};
    return (garment >= 0 && garment < (int)(sizeof(n) / sizeof(n[0]))) ? n[garment] : "";
}

static const char *top_phrase(top_t t)
{
    return t == TOP_SHORT_SLEEVES ? "short sleeves" : "long sleeves";
}

static const char *bottom_phrase(bottom_t b)
{
    return b == BOTTOM_SHORTS ? "shorts" : "long pants";
}

/* "a hoodie" / "your rain jacket" ... for sentence A. */
static const char *layer_phrase(layer_t l)
{
    switch (l) {
    case LAYER_HOODIE: return "a hoodie";
    case LAYER_LIGHT_JACKET: return "a light jacket";
    case LAYER_RAIN_JACKET: return "your rain jacket";
    case LAYER_WARM_COAT: return "your warm coat";
    case LAYER_WINTER_COAT: return "your winter coat";
    default: return "";
    }
}

/* Bare noun ("light jacket") for the WARMUP / WINDY sentences. */
static const char *layer_noun(layer_t l)
{
    switch (l) {
    case LAYER_HOODIE: return "hoodie";
    case LAYER_LIGHT_JACKET: return "light jacket";
    case LAYER_RAIN_JACKET: return "rain jacket";
    case LAYER_WARM_COAT: return "warm coat";
    case LAYER_WINTER_COAT: return "winter coat";
    default: return "";
    }
}

bool outfit_make(const wx_t *wx, const struct tm *now, int day_offset, const outfit_opts_t *o, outfit_t *out)
{
    if (!out) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->extra_icon = EXTRA_ICON_NONE;
    if (!wx || !now || !wx->valid || day_offset < 0) {
        return false;
    }
    const bool tomorrow = day_offset >= 1;   /* tomorrow wording (plan 4.5) */

    /* ---- 4.2 which hours ---- */
    long fc_day = days_from_civil(wx->year, wx->month, wx->day);
    long now_day = days_from_civil(now->tm_year + 1900, now->tm_mon + 1, now->tm_mday);
    long base_days = (now_day - fc_day) + day_offset;
    long base = base_days * 24;
    civil_from_days(fc_day + base_days, &out->year, &out->month, &out->day);
    out->wday = wx_day_of_week(out->year, out->month, out->day);

    int win_start, win_end;   /* hours of the target day, end exclusive: 7..17 means 7,8,...,17 */
    if (day_offset == 0) {
        if (now->tm_hour >= 17) {
            win_start = now->tm_hour;
            win_end = 22;   /* now..21 */
        } else {
            win_start = now->tm_hour > 7 ? now->tm_hour : 7;
            win_end = 18;
        }
    } else {
        win_start = 7;
        win_end = 18;
    }
    int dep_hour = day_offset == 0 ? win_start : 8;   /* departure = the window start, today */
    if (base < 0 || base + win_end > wx->nhours) {
        return false;   /* out->ok stays false */
    }
    int base_i = (int)base;

    /* ---- window aggregates (plan 4.3 inputs) ---- */
    float f_dep = wx_to_f(wx, wx->h_feels[base_i + dep_hour]);
    float f_max = -1e9f, f_min = 1e9f, f_pm_min = 1e9f;
    bool has_pm = false;
    int rain_h = 0, first_rain = -1, snow_h = 0, pop_max = 0;
    bool ice = false, thunder = false, heavy = false;
    float gust_max = 0;
    for (int i = base_i + win_start; i < base_i + win_end; i++) {
        int hod = i - base_i;
        float f = wx_to_f(wx, wx->h_feels[i]);
        if (f > f_max) f_max = f;
        if (f < f_min) f_min = f;
        if (hod >= 14) {
            has_pm = true;
            if (f < f_pm_min) f_pm_min = f;
        }
        int pop = wx->h_pop[i];
        if (pop > pop_max) pop_max = pop;
        int code = wx->h_code[i];
        wx_kind_t kind = wx_kind(code);
        if (pop >= 40 && (kind == WXK_RAIN || kind == WXK_DRIZZLE || kind == WXK_THUNDER)) {
            rain_h++;
            if (first_rain < 0) first_rain = hod;
        }
        if (pop >= 40 && kind == WXK_SNOW) snow_h++;
        if (pop >= 30 && kind == WXK_ICE) ice = true;
        if (kind == WXK_THUNDER) thunder = true;
        if (code == 65 || code == 82) heavy = true;
        float g = gust_to_mph(wx, wx->h_gust[i]);
        if (g > gust_max) gust_max = g;
    }
    (void)f_min;
    if (gust_max <= 0 && day_offset == 0) {
        gust_max = gust_to_mph(wx, wx->gust);   /* gusts missing: fall back to the current gust */
    }
    bool has_snow = snow_h > 0;
    bool snow_or_ice = has_snow || ice;
    bool is_rain = rain_h >= 2 || (rain_h >= 1 && pop_max >= 60);

    int day_idx = (int)base_days;
    float uv = (day_idx >= 0 && day_idx < wx->ndays) ? wx->days[day_idx].uv : 0;

    bool bike_bus_day = o && o->bike_bus_wday >= 0 && out->wday == o->bike_bus_wday;
    bool bike_bus_cold = bike_bus_day && f_dep <= BIKE_BUS_COLD_F;

    /* ---- 4.3 rules ---- */
    out->top = (f_max >= 72 && f_dep >= 58) ? TOP_SHORT_SLEEVES : TOP_LONG_SLEEVES;

    out->bottom = ((f_dep >= 66 && f_max >= 74) || f_max >= 82) ? BOTTOM_SHORTS : BOTTOM_LONG_PANTS;
    if (snow_or_ice) out->bottom = BOTTOM_LONG_PANTS;

    if (f_dep <= 32) {
        out->layer = LAYER_WINTER_COAT;
    } else if (f_dep <= 45) {
        out->layer = LAYER_WARM_COAT;
    } else if (f_dep <= 57) {
        out->layer = LAYER_LIGHT_JACKET;
    } else if (f_dep <= 64) {
        out->layer = LAYER_HOODIE;
    } else {
        out->layer = LAYER_NONE;
    }
    if (is_rain && (out->layer == LAYER_NONE || out->layer == LAYER_HOODIE || out->layer == LAYER_LIGHT_JACKET)) {
        out->layer = LAYER_RAIN_JACKET;   /* cold rain keeps the coat, and adds an umbrella below */
    }
    if (has_snow && out->layer != LAYER_WARM_COAT && out->layer != LAYER_WINTER_COAT) {
        out->layer = LAYER_WARM_COAT;   /* snow: at least a warm coat */
    }

    if (snow_or_ice) {
        out->feet = FEET_SNOW_BOOTS;
    } else if (rain_h >= 3 || heavy) {
        out->feet = FEET_RAIN_BOOTS;
    } else {
        out->feet = FEET_SNEAKERS;
    }

    uint32_t extras = 0;
    if (f_dep <= 38 || has_snow) extras |= EX_HAT;
    if (f_dep <= 35 || has_snow || ice || bike_bus_cold) extras |= EX_GLOVES;
    if (is_rain && !thunder && (out->layer == LAYER_WARM_COAT || out->layer == LAYER_WINTER_COAT || rain_h >= 3)) {
        extras |= EX_UMBRELLA;   /* never on a thunder day: the advice says stay inside */
    }
    bool dry = rain_h == 0 && snow_h == 0 && !ice && !thunder;
    if (uv >= 7 && dry && f_max >= 65) extras |= EX_SUNGLASSES;
    if (f_max >= 85) extras |= EX_WATER;
    if (f_dep <= 32) extras |= EX_SCARF;
    out->extras = extras;

    uint32_t change = 0;
    if (f_max - f_dep >= 15 && out->layer != LAYER_NONE) change |= CH_WARMUP;
    if (has_pm && f_dep - f_pm_min >= 12) change |= CH_COOLDOWN;
    if (is_rain && first_rain > dep_hour) change |= CH_RAIN_LATER;   /* any start after departure; the old +3 left 1-3 h gaps with no sentence at all */
    if (is_rain && first_rain >= 0 && first_rain <= dep_hour) change |= CH_RAIN_ALL_DAY;
    if (has_snow) change |= CH_SNOW;
    if (thunder) change |= CH_STORM;
    if (gust_max >= 25) change |= CH_WINDY;
    if (f_max >= 85) change |= CH_HOT;
    if (bike_bus_cold) change |= CH_BIKE_BUS;
    out->change = change;

    /* ---- tiles: top, bottom, layer if any, feet (plan 3.1 / 4.1) ---- */
    int n = 0;
    out->tile_kind[n++] = out->top == TOP_SHORT_SLEEVES ? SHORT_SLEEVES : LONG_SLEEVES;
    out->tile_kind[n++] = out->bottom == BOTTOM_SHORTS ? SHORTS : LONG_PANTS;
    if (out->layer != LAYER_NONE) {
        static const int layer_garment[] = {0, HOODIE, LIGHT_JACKET, RAIN_JACKET, WARM_COAT, WINTER_COAT};
        out->tile_kind[n++] = layer_garment[out->layer];
    }
    out->tile_kind[n++] = out->feet == FEET_SNEAKERS ? SNEAKERS : out->feet == FEET_RAIN_BOOTS ? RAIN_BOOTS : SNOW_BOOTS;
    out->ntiles = n;

    /* ---- 4.4 extra icon, by priority ---- */
    if (change & CH_STORM) {
        out->extra_icon = EXTRA_ICON_BOLT;
    } else if (extras & EX_UMBRELLA) {
        out->extra_icon = EXTRA_ICON_UMBRELLA;
    } else if (extras & EX_GLOVES) {
        out->extra_icon = (extras & EX_HAT) ? EXTRA_ICON_HAT_GLOVES : EXTRA_ICON_GLOVES;
    } else if (change & CH_WARMUP) {
        out->extra_icon = EXTRA_ICON_SUN_UP;
    } else if (extras & EX_WATER) {
        out->extra_icon = EXTRA_ICON_WATER;
    } else if (extras & EX_SUNGLASSES) {
        out->extra_icon = EXTRA_ICON_SUNGLASSES;
    } else if (change & CH_WINDY) {
        out->extra_icon = EXTRA_ICON_WIND;
    } else {
        out->extra_icon = EXTRA_ICON_NONE;
    }

    /* ---- 4.5 advice sentences (no numbers) ---- */
    char a[64], b[80];   /* longest A is 43 chars, longest B is 70, both fit advice_short */
    b[0] = 0;
    if (out->layer == LAYER_NONE) {
        snprintf(a, sizeof(a), "Wear %s and %s.", top_phrase(out->top), bottom_phrase(out->bottom));
    } else {
        snprintf(a, sizeof(a), "Wear %s. %s %s.", top_phrase(out->top), (change & CH_WARMUP) ? "Bring" : "Wear",
                 layer_phrase(out->layer));
    }
    if (change & CH_STORM) {
        snprintf(b, sizeof(b), "Storms later. Stay inside if it thunders.");
    } else if (change & CH_SNOW) {
        snprintf(b, sizeof(b), "Snow day! Hat, gloves and boots.");
    } else if (change & CH_RAIN_ALL_DAY) {
        snprintf(b, sizeof(b), "%s", out->feet == FEET_SNEAKERS ? "Rainy all day. Bring your umbrella."
                                                                : "Rainy all day. Rain boots on!");
    } else if (change & CH_RAIN_LATER) {
        if (first_rain >= 12) {
            snprintf(b, sizeof(b), "Rain after lunch. Keep your rain jacket handy.");
        } else {
            snprintf(b, sizeof(b), "Rain later %s. Keep your rain jacket handy.", tomorrow ? "in the morning" : "this morning");
        }
    } else if (change & CH_WARMUP) {
        snprintf(b, sizeof(b), "%s %s, warm after lunch. Take your %s off later.", f_dep <= 45 ? "Cold" : "Cool",
                 tomorrow ? "in the morning" : "this morning", layer_noun(out->layer));
    } else if (change & CH_COOLDOWN) {
        snprintf(b, sizeof(b), "Warm now, chilly later. Bring a layer.");
    } else if (change & CH_BIKE_BUS) {
        snprintf(b, sizeof(b), "%s", tomorrow ? "Bike bus tomorrow! Lay out your gloves." : "Bike bus today! Wear your gloves.");
    } else if (change & CH_WINDY) {
        if (out->layer != LAYER_NONE) {
            snprintf(b, sizeof(b), "Windy! Zip up your %s.", layer_noun(out->layer));
        } else {
            snprintf(b, sizeof(b), "%s", tomorrow ? "Windy tomorrow!" : "Windy today!");
        }
    } else if (change & CH_HOT) {
        snprintf(b, sizeof(b), "%s", tomorrow ? "Hot tomorrow. Drink lots of water." : "Hot today. Drink lots of water.");
    }
    if (b[0]) {
        snprintf(out->advice, sizeof(out->advice), "%s %s", a, b);
        snprintf(out->advice_short, sizeof(out->advice_short), "%s", b);
    } else {
        snprintf(out->advice, sizeof(out->advice), "%s", a);
        snprintf(out->advice_short, sizeof(out->advice_short), "%s", a);
    }

    /* ---- weather-block fields, from the target day ---- */
    out->t_now = wx->temp;
    if (day_idx >= 0 && day_idx < wx->ndays) {
        const wx_day_t *d = &wx->days[day_idx];
        out->hi = d->tmax;
        out->lo = d->tmin;
        out->pop_day = d->pop;
        out->wx_code = day_idx == 0 ? wx->code : d->code;
        out->is_day = day_idx == 0 ? wx->is_day : true;
    } else {
        float mx = -1e9f, mn = 1e9f;
        for (int i = base_i + win_start; i < base_i + win_end; i++) {
            if (wx->h_temp[i] > mx) mx = wx->h_temp[i];
            if (wx->h_temp[i] < mn) mn = wx->h_temp[i];
        }
        out->hi = mx;
        out->lo = mn;
        out->pop_day = pop_max;
        out->wx_code = wx->h_code[base_i + dep_hour];
        out->is_day = wx->h_day[base_i + dep_hour];
    }

    out->ok = true;
    return true;
}
