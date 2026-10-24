#include "wx.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"

/* ------------------------------------------------------------------ codes */

wx_kind_t wx_kind(int code)
{
    switch (code) {
    case 0: case 1: return WXK_CLEAR;
    case 2: case 3: return WXK_CLOUDS;
    case 45: case 48: return WXK_FOG;
    case 51: case 53: case 55: return WXK_DRIZZLE;
    case 56: case 57: case 66: case 67: return WXK_ICE;
    case 61: case 63: case 65: case 80: case 81: case 82: return WXK_RAIN;
    case 71: case 73: case 75: case 77: case 85: case 86: return WXK_SNOW;
    case 95: case 96: case 97: case 99: return WXK_THUNDER;
    default: return code < 45 ? WXK_CLOUDS : WXK_RAIN;
    }
}

int wx_severity(int code)
{
    switch (code) {
    case 0: return 0;
    case 1: return 1;
    case 2: return 2;
    case 3: return 3;
    case 45: case 48: return 4;
    case 51: return 5;
    case 53: return 6;
    case 55: return 7;
    case 61: case 80: return 8;
    case 63: case 81: return 9;
    case 65: case 82: return 10;
    case 71: case 77: case 85: return 11;
    case 73: return 12;
    case 75: case 86: return 13;
    case 56: case 66: return 14;
    case 57: case 67: return 15;
    case 95: return 16;
    case 96: case 97: return 17;
    case 99: return 18;
    default: return 3;
    }
}

wx_icon_t wx_icon_for(int code, bool is_day)
{
    switch (code) {
    case 0: case 1: return is_day ? WXI_CLEAR_DAY : WXI_CLEAR_NIGHT;
    case 2: return is_day ? WXI_PARTLY_DAY : WXI_PARTLY_NIGHT;
    case 3: return WXI_CLOUDY;
    case 45: case 48: return WXI_FOG;
    case 51: case 53: case 55: return WXI_DRIZZLE;
    case 56: case 57: case 66: case 67: return WXI_SLEET;
    case 61: case 63: case 80: case 81: return WXI_RAIN;
    case 65: case 82: return WXI_HEAVY_RAIN;
    case 71: case 73: case 75: case 77: case 85: case 86: return WXI_SNOW;
    case 95: case 96: case 97: case 99: return WXI_THUNDER;
    default: return WXI_CLOUDY;
    }
}

const char *wx_describe(int code, bool is_day)
{
    switch (code) {
    case 0: return is_day ? "Sunny" : "Clear";
    case 1: return is_day ? "Mostly sunny" : "Mostly clear";
    case 2: return "Partly cloudy";
    case 3: return "Cloudy";
    case 45: case 48: return "Foggy";
    case 51: case 53: case 55: return "Drizzly";
    case 56: case 57: return "Freezing drizzle";
    case 61: return "Light rain";
    case 63: return "Rainy";
    case 65: return "Pouring rain";
    case 66: case 67: return "Freezing rain";
    case 71: return "Light snow";
    case 73: case 77: return "Snowy";
    case 75: return "Heavy snow";
    case 80: return "Showers";
    case 81: return "Rain showers";
    case 82: return "Downpours";
    case 85: case 86: return "Snow showers";
    case 95: return "Thunderstorms";
    case 96: case 99: return "Storms with hail";
    case 97: return "Big storms";
    default: return "Cloudy";
    }
}

const char *wx_wday_name(int wday, bool short_name)
{
    static const char *l[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    static const char *s[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    if (wday < 0 || wday > 6) {
        return "";
    }
    return short_name ? s[wday] : l[wday];
}

const char *wx_month_name(int month, bool short_name)
{
    static const char *l[] = {"January", "February", "March", "April", "May", "June", "July",
                              "August", "September", "October", "November", "December"};
    static const char *s[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    if (month < 1 || month > 12) {
        return "";
    }
    return short_name ? s[month - 1] : l[month - 1];
}

int wx_day_of_week(int y, int m, int d)
{
    /* Sakamoto's method, 0 = Sunday */
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) {
        y -= 1;
    }
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

float wx_to_f(const wx_t *wx, float t)
{
    return wx->fahrenheit ? t : t * 9.0f / 5.0f + 32.0f;
}

/* ------------------------------------------------------------------ json helpers */

static double num(const cJSON *obj, const char *key, double def)
{
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsNumber(it) ? it->valuedouble : def;
}

static double arr_num(const cJSON *arr, int i, double def)
{
    const cJSON *it = cJSON_GetArrayItem(arr, i);
    return cJSON_IsNumber(it) ? it->valuedouble : def;
}

static bool parse_time(const char *s, int *y, int *mo, int *d, int *h, int *mi)
{
    int hh = 0, mm = 0;
    if (!s) {
        return false;
    }
    int n = sscanf(s, "%d-%d-%dT%d:%d", y, mo, d, &hh, &mm);
    if (n < 3) {
        return false;
    }
    if (h) {
        *h = hh;
    }
    if (mi) {
        *mi = mm;
    }
    return true;
}

/* ------------------------------------------------------------------ parse */

bool wx_parse_openmeteo(const char *json, size_t len, bool fahrenheit, wx_t *out)
{
    memset(out, 0, sizeof(*out));
    out->fahrenheit = fahrenheit;

    cJSON *root = cJSON_ParseWithLength(json, len);
    if (!root) {
        return false;
    }
    bool ok = false;

    out->utc_offset_s = (int)num(root, "utc_offset_seconds", 0);

    const cJSON *cur = cJSON_GetObjectItemCaseSensitive(root, "current");
    const cJSON *hourly = cJSON_GetObjectItemCaseSensitive(root, "hourly");
    const cJSON *daily = cJSON_GetObjectItemCaseSensitive(root, "daily");
    if (!cJSON_IsObject(cur) || !cJSON_IsObject(hourly) || !cJSON_IsObject(daily)) {
        goto done;
    }

    const cJSON *ct = cJSON_GetObjectItemCaseSensitive(cur, "time");
    if (!cJSON_IsString(ct) ||
        !parse_time(ct->valuestring, &out->year, &out->month, &out->day, &out->hour, &out->minute)) {
        goto done;
    }
    out->wday = wx_day_of_week(out->year, out->month, out->day);
    out->temp = (float)num(cur, "temperature_2m", 0);
    out->feels = (float)num(cur, "apparent_temperature", out->temp);
    out->wind = (float)num(cur, "wind_speed_10m", 0);
    out->gust = (float)num(cur, "wind_gusts_10m", out->wind);
    out->precip = (float)num(cur, "precipitation", 0);
    out->code = (int)num(cur, "weather_code", 3);
    out->is_day = num(cur, "is_day", 1) > 0.5;

    /* hourly: the response may start with yesterday (past_days=1); keep today 00:00 onward */
    const cJSON *ht = cJSON_GetObjectItemCaseSensitive(hourly, "time");
    const cJSON *htemp = cJSON_GetObjectItemCaseSensitive(hourly, "temperature_2m");
    const cJSON *hfeel = cJSON_GetObjectItemCaseSensitive(hourly, "apparent_temperature");
    const cJSON *hpop = cJSON_GetObjectItemCaseSensitive(hourly, "precipitation_probability");
    const cJSON *hcode = cJSON_GetObjectItemCaseSensitive(hourly, "weather_code");
    const cJSON *hday = cJSON_GetObjectItemCaseSensitive(hourly, "is_day");
    const cJSON *hgust = cJSON_GetObjectItemCaseSensitive(hourly, "wind_gusts_10m");
    int total = cJSON_GetArraySize(ht);
    int h0 = 0;
    for (int i = 0; i < total; i++) {
        const cJSON *ts = cJSON_GetArrayItem(ht, i);
        int y, mo, d, h;
        if (cJSON_IsString(ts) && parse_time(ts->valuestring, &y, &mo, &d, &h, NULL) && y == out->year &&
            mo == out->month && d == out->day) {
            h0 = i - h;
            break;
        }
    }
    if (h0 < 0) {
        h0 = 0;
    }
    int n = total - h0;
    if (n > WX_MAX_HOURS) {
        n = WX_MAX_HOURS;
    }
    if (n < 0) {
        n = 0;
    }
    out->nhours = n;
    for (int i = 0; i < n; i++) {
        int j = h0 + i;
        out->h_temp[i] = (float)arr_num(htemp, j, out->temp);
        out->h_feels[i] = (float)arr_num(hfeel, j, out->h_temp[i]);
        out->h_pop[i] = (int8_t)arr_num(hpop, j, 0);
        out->h_code[i] = (uint8_t)arr_num(hcode, j, 3);
        out->h_day[i] = arr_num(hday, j, 1) > 0.5;
        out->h_gust[i] = (float)arr_num(hgust, j, out->gust);
    }
    out->now_idx = out->hour < n ? out->hour : 0;

    /* daily: find today, keep yesterday separately */
    const cJSON *dt = cJSON_GetObjectItemCaseSensitive(daily, "time");
    const cJSON *dcode = cJSON_GetObjectItemCaseSensitive(daily, "weather_code");
    const cJSON *dmax = cJSON_GetObjectItemCaseSensitive(daily, "temperature_2m_max");
    const cJSON *dmin = cJSON_GetObjectItemCaseSensitive(daily, "temperature_2m_min");
    const cJSON *dpop = cJSON_GetObjectItemCaseSensitive(daily, "precipitation_probability_max");
    const cJSON *duv = cJSON_GetObjectItemCaseSensitive(daily, "uv_index_max");
    const cJSON *dsun = cJSON_GetObjectItemCaseSensitive(daily, "sunshine_duration");
    int dtotal = cJSON_GetArraySize(dt);
    int d0 = 0;
    for (int i = 0; i < dtotal; i++) {
        const cJSON *ts = cJSON_GetArrayItem(dt, i);
        int y, mo, d;
        if (cJSON_IsString(ts) && parse_time(ts->valuestring, &y, &mo, &d, NULL, NULL) && y == out->year &&
            mo == out->month && d == out->day) {
            d0 = i;
            break;
        }
    }
    for (int i = d0 - 1; i < dtotal && i - d0 < WX_MAX_DAYS; i++) {
        if (i < 0) {
            continue;
        }
        wx_day_t *dd = i < d0 ? &out->yday : &out->days[i - d0];
        const cJSON *ts = cJSON_GetArrayItem(dt, i);
        if (cJSON_IsString(ts)) {
            parse_time(ts->valuestring, &dd->year, &dd->month, &dd->day, NULL, NULL);
            dd->wday = wx_day_of_week(dd->year, dd->month, dd->day);
        }
        dd->code = (int)arr_num(dcode, i, 3);
        dd->tmax = (float)arr_num(dmax, i, 0);
        dd->tmin = (float)arr_num(dmin, i, 0);
        dd->pop = (int)arr_num(dpop, i, 0);
        dd->uv = (float)arr_num(duv, i, 0);
        dd->sun_h = (float)(arr_num(dsun, i, 0) / 3600.0);
        if (i < d0) {
            out->has_yday = true;
        } else {
            out->ndays = i - d0 + 1;
        }
    }

    ok = out->nhours > 0 && out->ndays > 0;
    if (ok) {
        wx_derive_periods(out);
        out->valid = true;
    }

done:
    cJSON_Delete(root);
    return ok;
}

bool wx_parse_geocode(const char *json, size_t len, double *lat, double *lon, char *name, size_t name_len)
{
    cJSON *root = cJSON_ParseWithLength(json, len);
    if (!root) {
        return false;
    }
    bool ok = false;
    const cJSON *res = cJSON_GetObjectItemCaseSensitive(root, "results");
    const cJSON *first = cJSON_GetArrayItem(res, 0);
    if (cJSON_IsObject(first)) {
        const cJSON *la = cJSON_GetObjectItemCaseSensitive(first, "latitude");
        const cJSON *lo = cJSON_GetObjectItemCaseSensitive(first, "longitude");
        if (cJSON_IsNumber(la) && cJSON_IsNumber(lo)) {
            *lat = la->valuedouble;
            *lon = lo->valuedouble;
            const cJSON *nm = cJSON_GetObjectItemCaseSensitive(first, "name");
            const cJSON *a1 = cJSON_GetObjectItemCaseSensitive(first, "admin1");
            if (name && name_len) {
                if (cJSON_IsString(nm) && cJSON_IsString(a1) && a1->valuestring[0]) {
                    snprintf(name, name_len, "%s, %s", nm->valuestring, a1->valuestring);
                } else if (cJSON_IsString(nm)) {
                    snprintf(name, name_len, "%s", nm->valuestring);
                } else {
                    name[0] = 0;
                }
            }
            ok = true;
        }
    }
    cJSON_Delete(root);
    return ok;
}

/* ------------------------------------------------------------------ periods */

enum { P_MORNING, P_AFTERNOON, P_EVENING, P_NIGHT };

static void period_window(int abs_hour, int *kind, int *start, int *end, int *day)
{
    int d = abs_hour >= 0 ? abs_hour / 24 : -1;
    int hod = abs_hour - d * 24;
    if (hod < 6) {
        *kind = P_NIGHT;
        *day = d - 1;
        *start = (d - 1) * 24 + 21;
        *end = d * 24 + 6;
    } else if (hod < 12) {
        *kind = P_MORNING;
        *day = d;
        *start = d * 24 + 6;
        *end = d * 24 + 12;
    } else if (hod < 17) {
        *kind = P_AFTERNOON;
        *day = d;
        *start = d * 24 + 12;
        *end = d * 24 + 17;
    } else if (hod < 21) {
        *kind = P_EVENING;
        *day = d;
        *start = d * 24 + 17;
        *end = d * 24 + 21;
    } else {
        *kind = P_NIGHT;
        *day = d;
        *start = d * 24 + 21;
        *end = (d + 1) * 24 + 6;
    }
}

static const char *period_label(int kind, int day)
{
    if (day <= 0) {
        static const char *t[] = {"Morning", "Afternoon", "Evening", "Tonight"};
        return t[kind];
    }
    static const char *m[] = {"Tomorrow AM", "Tomorrow PM", "Tmrw evening", "Tmrw night"};
    return m[kind];
}

static int dominant_code(const wx_t *wx, int s, int e)
{
    int counts[100] = {0};
    int wet = 0;
    for (int i = s; i < e; i++) {
        int c = wx->h_code[i];
        if (c < 100) {
            counts[c]++;
        }
        if (wx_kind(c) >= WXK_DRIZZLE) {
            wet++;
        }
    }
    int len = e - s;
    bool use_wet = wet >= 2 || (wet > 0 && (wet * 3 >= len || len <= 2));
    int best = -1, best_n = 0;
    for (int c = 0; c < 100; c++) {
        if (!counts[c]) {
            continue;
        }
        bool is_wet = wx_kind(c) >= WXK_DRIZZLE;
        if (is_wet != use_wet) {
            continue;
        }
        if (counts[c] > best_n || (counts[c] == best_n && wx_severity(c) > wx_severity(best))) {
            best = c;
            best_n = counts[c];
        }
    }
    return best < 0 ? wx->h_code[s] : best;
}

void wx_derive_periods(wx_t *wx)
{
    wx->nperiods = 0;
    int h = wx->now_idx;
    int guard = 0;
    while (wx->nperiods < WX_MAX_PERIODS && h < wx->nhours && guard++ < 12) {
        int kind, ws, we, day;
        period_window(h, &kind, &ws, &we, &day);
        int s = ws > wx->now_idx ? ws : wx->now_idx;
        int e = we < wx->nhours ? we : wx->nhours;
        if (s < 0) {
            s = 0;
        }
        if (e > s) {
            wx_period_t *p = &wx->periods[wx->nperiods++];
            snprintf(p->label, sizeof(p->label), "%s", period_label(kind, day));
            float mn = 1e9f, mx = -1e9f, sum = 0;
            int pop = 0, dayc = 0;
            for (int i = s; i < e; i++) {
                float t = wx->h_temp[i];
                mn = t < mn ? t : mn;
                mx = t > mx ? t : mx;
                sum += t;
                pop = wx->h_pop[i] > pop ? wx->h_pop[i] : pop;
                dayc += wx->h_day[i] ? 1 : 0;
            }
            switch (kind) {
            case P_AFTERNOON: p->temp = mx; break;
            case P_EVENING: p->temp = sum / (e - s); break;
            default: p->temp = mn; break;
            }
            p->pop = pop;
            p->is_day = dayc * 2 >= (e - s);
            p->code = dominant_code(wx, s, e);
        }
        h = we;
    }
}

/* ------------------------------------------------------------------ kid moments */

/* No school: weekends, July-August, and Christmas Eve through New Year's Day.
 * (A guess at the school calendar; other days off still say "Walk to school".) */
static bool school_day(const wx_day_t *d)
{
    if (d->wday == 0 || d->wday == 6 || d->month == 7 || d->month == 8) {
        return false;
    }
    if ((d->month == 12 && d->day >= 24) || (d->month == 1 && d->day == 1)) {
        return false;
    }
    return true;
}

int wx_kid_moments(const wx_t *wx, int now_hour, int now_min, wx_moment_t out[WX_MOMENTS])
{
    if (!wx || !wx->valid || wx->ndays < 1 || wx->nhours < 24) {
        return 0;
    }
    int now = now_hour * 60 + now_min;
    int day = now >= 15 * 60 + 30 ? 1 : 0;
    if (day >= wx->ndays || (day + 1) * 24 > wx->nhours) {
        day = 0;
    }
    const wx_day_t *d = &wx->days[day];
    bool school = school_day(d);

    static const char *school_title[] = {"Walk to school", "Recess", "Going home"};
    static const char *school_sub[] = {"Morning", "Noon", "Afternoon"};
    static const char *free_title[] = {"Morning", "Lunchtime", "Afternoon"};
    const int school_hour[] = {8, 12, 15};
    const int free_hour[] = {9, 12, 15};

    for (int i = 0; i < WX_MOMENTS; i++) {
        wx_moment_t *m = &out[i];
        int h = school ? school_hour[i] : free_hour[i];
        snprintf(m->title, sizeof(m->title), "%s", school ? school_title[i] : free_title[i]);
        if (school) {
            if (day == 1) {
                snprintf(m->sub, sizeof(m->sub), "Tomorrow %s", i == 1 ? "at noon" : i == 0 ? "morning" : "afternoon");
            } else {
                snprintf(m->sub, sizeof(m->sub), "%s", school_sub[i]);
            }
        } else {
            snprintf(m->sub, sizeof(m->sub), "%s", day == 1 ? "Tomorrow" : wx_wday_name(d->wday, false));
        }

        int idx = day * 24 + h;
        m->temp = wx->h_temp[idx];
        m->is_day = wx->h_day[idx];
        m->code = wx->h_code[idx];
        m->pop = 0;
        for (int j = idx - 1; j <= idx + 1; j++) {
            if (j < 0 || j >= wx->nhours) {
                continue;
            }
            if (wx->h_pop[j] > m->pop) {
                m->pop = wx->h_pop[j];
            }
            if (wx_severity(wx->h_code[j]) > wx_severity(m->code)) {
                m->code = wx->h_code[j];
            }
        }
        m->past = day == 0 && now >= h * 60 + 60;
    }
    return WX_MOMENTS;
}
