#include "advice.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define DEG "\xC2\xB0"

const char *wear_label(wear_t w)
{
    static const char *n[] = {"T-shirt", "Sweater", "Jacket", "Coat", "Hat", "Gloves",
                              "Umbrella", "Boots", "Shades", "Water"};
    return (w >= 0 && w < WEAR_COUNT) ? n[w] : "";
}

typedef struct {
    float max_f;          /* upper bound of the band, feels-like F */
    const char *word;     /* "Chilly" */
    const char *tip;      /* today */
    const char *tip_tmrw; /* the evening before */
    const char *noun;     /* for "umbrella and a ___" */
    wear_t wear[3];
    int nwear;
} band_t;

static const band_t BANDS[] = {
    {15, "Freezing", "Big coat, hat and gloves!", "Lay out coat, hat and gloves.", "big coat",
     {WEAR_COAT, WEAR_BEANIE, WEAR_MITTENS}, 3},
    {28, "Very cold", "Winter coat, hat and gloves.", "Lay out coat, hat and gloves.", "warm coat",
     {WEAR_COAT, WEAR_BEANIE, WEAR_MITTENS}, 3},
    {40, "Cold", "Wear your warm coat.", "Lay out your warm coat.", "warm coat",
     {WEAR_COAT, WEAR_BEANIE}, 2},
    {54, "Chilly", "Bring a jacket!", "Lay out a jacket tonight.", "jacket",
     {WEAR_JACKET}, 1},
    {64, "Cool", "A sweater or hoodie is perfect.", "Lay out a sweater or hoodie.", "sweater",
     {WEAR_SWEATER}, 1},
    {77, "Nice", "T-shirt weather!", "T-shirt weather tomorrow!", NULL,
     {WEAR_TSHIRT}, 1},
    {87, "Warm", "Shorts and a water bottle.", "Shorts and a water bottle!", NULL,
     {WEAR_TSHIRT, WEAR_WATER}, 2},
    {999, "Hot", "Stay cool and drink water!", "Hot! Pack a water bottle.", NULL,
     {WEAR_TSHIRT, WEAR_WATER}, 2},
};

static const band_t *band_for(float f)
{
    for (size_t i = 0; i < sizeof(BANDS) / sizeof(BANDS[0]); i++) {
        if (f <= BANDS[i].max_f) {
            return &BANDS[i];
        }
    }
    return &BANDS[sizeof(BANDS) / sizeof(BANDS[0]) - 1];
}

static int band_index(const band_t *b)
{
    return (int)(b - BANDS);
}

static bool has_wear(const advice_t *a, wear_t w)
{
    for (int i = 0; i < a->nwear; i++) {
        if (a->wear[i] == w) {
            return true;
        }
    }
    return false;
}

static void add_wear(advice_t *a, wear_t w)
{
    if (!has_wear(a, w) && a->nwear < ADVICE_MAX_WEAR) {
        a->wear[a->nwear++] = w;
    }
}

/* Like add_wear, but bumps the last item if the row is full. */
static void must_wear(advice_t *a, wear_t w)
{
    if (has_wear(a, w)) {
        return;
    }
    if (a->nwear < ADVICE_MAX_WEAR) {
        a->wear[a->nwear++] = w;
    } else {
        a->wear[ADVICE_MAX_WEAR - 1] = w;
    }
}

static void append(char *dst, size_t cap, const char *s)
{
    size_t n = strlen(dst);
    if (n + 1 < cap) {
        snprintf(dst + n, cap - n, "%s", s);
    }
}

static void lower_first(char *dst, size_t n, const char *src)
{
    snprintf(dst, n, "%s", src);
    if (dst[0]) {
        dst[0] = (char)tolower((unsigned char)dst[0]);
    }
}

static int iround(float v)
{
    return (int)lroundf(v);
}

/* ------------------------------------------------------------------ day vs day */

bool advice_compare(const wx_t *wx, const wx_day_t *prev, const wx_day_t *cur, const char *ref, char *buf,
                    size_t len, float *dtemp_f)
{
    buf[0] = 0;
    if (dtemp_f) {
        *dtemp_f = 0;
    }
    if (!prev || !cur) {
        return false;
    }
    float d = (cur->tmax - prev->tmax) * (wx->fahrenheit ? 1.0f : 1.8f);
    float ds = cur->sun_h - prev->sun_h;
    if (dtemp_f) {
        *dtemp_f = d;
    }
    const char *t = NULL;
    if (d >= BIG_CHANGE_F) {
        t = "a lot warmer";
    } else if (d >= 4) {
        t = "warmer";
    } else if (d <= -BIG_CHANGE_F) {
        t = "a lot colder";
    } else if (d <= -4) {
        t = "colder";
    }
    const char *k = NULL;
    if (ds >= 3.0f) {
        k = "sunnier";
    } else if (ds <= -3.0f) {
        k = "cloudier";
    }
    if (t && k) {
        snprintf(buf, len, "%s and %s than %s", t, k, ref);
    } else if (t) {
        snprintf(buf, len, "%s than %s", t, ref);
    } else if (k) {
        snprintf(buf, len, "%s than %s", k, ref);
    } else {
        snprintf(buf, len, "about the same as %s", ref);
    }
    buf[0] = (char)toupper((unsigned char)buf[0]);
    return true;
}

void advice_day_trend(const wx_t *wx, int i, char *buf, size_t len)
{
    buf[0] = 0;
    if (i < 1 || i >= wx->ndays) {
        return;
    }
    const wx_day_t *prev = &wx->days[i - 1];
    const wx_day_t *cur = &wx->days[i];
    const char *ref = i == 1 ? "today" : wx_wday_name(prev->wday, true);
    int d = iround(cur->tmax - prev->tmax);
    float ds = cur->sun_h - prev->sun_h;
    if (d >= 3 || d <= -3) {
        snprintf(buf, len, "%d" DEG " %s than %s", d < 0 ? -d : d, d < 0 ? "colder" : "warmer", ref);
    } else if (ds >= 3.0f) {
        snprintf(buf, len, "Sunnier than %s", ref);
    } else if (ds <= -3.0f) {
        snprintf(buf, len, "Cloudier than %s", ref);
    } else {
        snprintf(buf, len, "About like %s", ref);
    }
}

/* ------------------------------------------------------------------ main advice */

void advice_make(const wx_t *wx, const advice_opts_t *opts, advice_t *a)
{
    memset(a, 0, sizeof(*a));
    if (!wx->valid || wx->nhours == 0) {
        snprintf(a->headline, sizeof(a->headline), "Hi there!");
        snprintf(a->tip, sizeof(a->tip), "Checking the sky...");
        return;
    }

    const int n = wx->nhours;
    const int day = wx->hour >= 19 ? 1 : 0;
    a->tomorrow = day == 1;

    /* The hours we care about: from now (or 6 AM) until 8 PM of that day. */
    int ws = day * 24 + 6;
    if (day == 0 && wx->now_idx > ws) {
        ws = wx->now_idx;
    }
    int we = day * 24 + 20;
    if (we > n) {
        we = n;
    }
    if (ws >= we) {
        ws = wx->now_idx;
        we = ws + 6 < n ? ws + 6 : n;
    }

    /* The "leaving the house" moment: now, or 8 AM if it's early or tomorrow. */
    int si;
    float t_start, feels_start;
    if (day == 1 || wx->hour < 6) {
        si = day * 24 + 8;
        if (si >= n) {
            si = n - 1;
        }
        t_start = wx->h_temp[si];
        feels_start = wx->h_feels[si];
    } else {
        si = wx->now_idx;
        t_start = wx->temp;
        feels_start = wx->feels;
    }

    float fmax = -1e9f;
    int pop_max = 0, rain_hours = 0, snow_hours = 0;
    bool thunder = false, ice = false, heavy = false;
    for (int i = ws; i < we; i++) {
        float f = wx_to_f(wx, wx->h_feels[i]);
        fmax = f > fmax ? f : fmax;
        int pop = wx->h_pop[i];
        pop_max = pop > pop_max ? pop : pop_max;
        int c = wx->h_code[i];
        wx_kind_t k = wx_kind(c);
        if (pop >= 35) {
            if (k == WXK_THUNDER) {
                thunder = true;
                rain_hours++;
            } else if (k == WXK_ICE) {
                ice = true;
            } else if (k == WXK_SNOW) {
                snow_hours++;
            } else if (k == WXK_RAIN || k == WXK_DRIZZLE) {
                rain_hours++;
                if (c == 65 || c == 82) {
                    heavy = true;
                }
            }
        }
    }

    wx_kind_t now_kind = wx_kind(wx->code);
    bool raining_now = day == 0 && (now_kind == WXK_RAIN || now_kind == WXK_DRIZZLE || now_kind == WXK_THUNDER);
    bool snowing_now = day == 0 && now_kind == WXK_SNOW;
    if (raining_now && (wx->code == 65 || wx->code == 82)) {
        heavy = true;
    }

    const float fstart = wx_to_f(wx, feels_start);
    const band_t *b = band_for(fstart);
    const int bi = band_index(b);
    const int T = iround(t_start);

    const char *when;
    int hod = si % 24;
    if (day == 1) {
        when = "tomorrow";
    } else if (hod < 11) {
        when = "morning";
    } else if (hod < 16) {
        when = "afternoon";
    } else {
        when = "evening";
    }

    char word_lc[24];
    lower_first(word_lc, sizeof(word_lc), b->word);

    /* ---- absolute: temperature headline + clothes for the temperature ---- */
    if (day == 0) {
        snprintf(a->headline, sizeof(a->headline), "%s %d" DEG " %s", b->word, T, when);
    } else {
        snprintf(a->headline, sizeof(a->headline), "Tomorrow: %s, %d" DEG, word_lc, T);
    }
    snprintf(a->tip, sizeof(a->tip), "%s", day ? b->tip_tmrw : b->tip);
    for (int i = 0; i < b->nwear; i++) {
        add_wear(a, b->wear[i]);
    }

    if (thunder) {
        if (day == 0) {
            snprintf(a->headline, sizeof(a->headline), "Stormy %s, %d" DEG, when, T);
        } else {
            snprintf(a->headline, sizeof(a->headline), "Tomorrow: storms, %d" DEG, T);
        }
        snprintf(a->tip, sizeof(a->tip), "Stay inside when it thunders!");
        a->nwear = 0;
        add_wear(a, WEAR_UMBRELLA);
        add_wear(a, b->wear[0]);
    } else if (ice) {
        if (day == 0) {
            snprintf(a->headline, sizeof(a->headline), "Icy %s, %d" DEG, when, T);
        } else {
            snprintf(a->headline, sizeof(a->headline), "Tomorrow: icy, %d" DEG, T);
        }
        snprintf(a->tip, sizeof(a->tip), "Slippery! Boots on, walk slowly.");
        a->nwear = 0;
        add_wear(a, WEAR_BOOTS);
        add_wear(a, WEAR_COAT);
        add_wear(a, WEAR_MITTENS);
    } else if (snow_hours > 0 || snowing_now) {
        if (day == 0) {
            snprintf(a->headline, sizeof(a->headline), "Snowy %s, %d" DEG, when, T);
        } else {
            snprintf(a->headline, sizeof(a->headline), "Tomorrow: snow, %d" DEG, T);
        }
        snprintf(a->tip, sizeof(a->tip), "Snow! %s and boots.", bi <= 2 ? "Coat" : "Jacket");
        a->nwear = 0;
        add_wear(a, bi <= 2 ? WEAR_COAT : WEAR_JACKET);
        add_wear(a, WEAR_BOOTS);
        add_wear(a, WEAR_MITTENS);
    } else if (raining_now || rain_hours >= 2 || (rain_hours >= 1 && pop_max >= 60)) {
        if (day == 0) {
            snprintf(a->headline, sizeof(a->headline), "Rainy %s, %d" DEG, when, T);
        } else {
            snprintf(a->headline, sizeof(a->headline), "Tomorrow: rainy, %d" DEG, T);
        }
        if (b->noun) {
            snprintf(a->tip, sizeof(a->tip), "Grab your umbrella and a %s!", b->noun);
        } else {
            snprintf(a->tip, sizeof(a->tip), "Grab your umbrella!");
        }
        a->nwear = 0;
        add_wear(a, WEAR_UMBRELLA);
        add_wear(a, b->wear[0]);
        if (heavy) {
            add_wear(a, WEAR_BOOTS);
        }
    } else if (pop_max >= 35 && rain_hours >= 1) {
        char base[64];
        snprintf(base, sizeof(base), "%s", day ? b->tip_tmrw : b->tip);
        snprintf(a->tip, sizeof(a->tip), "%s Umbrella just in case.", base);
        a->nwear = 0;
        add_wear(a, b->wear[0]);
        add_wear(a, WEAR_UMBRELLA);
    } else {
        /* Dry. Big warm-up during the day means layers. */
        if (bi <= 4 && fmax - fstart >= 18.0f) {
            snprintf(a->tip, sizeof(a->tip), "%s", day ? "Cold early, warm later. Layers!"
                                                       : "Cold now, warm later. Wear layers!");
            a->nwear = 0;
            add_wear(a, bi <= 2 ? WEAR_COAT : WEAR_JACKET);
            add_wear(a, WEAR_TSHIRT);
        }
        /* Strong sun */
        float uv = wx->ndays > day ? wx->days[day].uv : 0;
        if (uv >= 6.0f && bi >= 5) {
            add_wear(a, WEAR_SUNGLASSES);
            if (bi == 5) {
                snprintf(a->tip, sizeof(a->tip), "%s", day ? "T-shirt and sunglasses tomorrow!"
                                                           : "T-shirt weather! Sunglasses too.");
            }
        }
        /* Wind */
        float gust_mph = wx->fahrenheit ? wx->gust : wx->gust / 1.609f;
        if (day == 0 && gust_mph >= 30.0f && strlen(a->tip) < 34) {
            append(a->tip, sizeof(a->tip), " Super windy too!");
        }
    }

    /* ---- relative: compared with the day before ---- */
    float dtemp = 0;
    bool have_cmp = false;
    if (day == 0 && wx->has_yday && wx->ndays > 0) {
        have_cmp = advice_compare(wx, &wx->yday, &wx->days[0], "yesterday", a->compare, sizeof(a->compare), &dtemp);
    } else if (day == 1 && wx->ndays > 1) {
        have_cmp = advice_compare(wx, &wx->days[0], &wx->days[1], "today", a->compare, sizeof(a->compare), &dtemp);
    }
    bool big_drop = have_cmp && dtemp <= -BIG_CHANGE_F;

    /* ---- bike bus (a morning ride, so only until 10 AM, or the evening before) ---- */
    int target_wday = wx->ndays > day ? wx->days[day].wday : (wx->wday + day) % 7;
    bool ride_ahead = day == 1 || wx->hour < 10;
    a->bike_bus = opts && opts->bike_bus_wday >= 0 && target_wday == opts->bike_bus_wday && ride_ahead;
    int ride = day * 24 + 8;
    bool ride_cold = a->bike_bus && ride < n && wx_to_f(wx, wx->h_feels[ride]) <= BIKE_BUS_COLD_F;

    if (ride_cold) {
        must_wear(a, WEAR_MITTENS);
        if (strstr(a->tip, "gloves")) {
            append(a->tip, sizeof(a->tip), day ? " Bike bus tomorrow!" : " Bike bus day!");
        } else {
            append(a->tip, sizeof(a->tip), day ? " Gloves too, for bike bus!" : " Remember your gloves for bike bus!");
        }
    } else if (big_drop) {
        must_wear(a, WEAR_MITTENS);
        if (!strstr(a->tip, "gloves")) {
            append(a->tip, sizeof(a->tip), " Wear gloves!");
        }
    }
}

void advice_day_line(const wx_t *wx, int i, char *buf, size_t len)
{
    if (i < 0 || i >= wx->ndays) {
        buf[0] = 0;
        return;
    }
    const wx_day_t *d = &wx->days[i];
    wx_kind_t k = wx_kind(d->code);
    float hi = wx_to_f(wx, d->tmax);
    const char *s;
    if (k == WXK_THUNDER && d->pop >= 40) {
        s = "Storms, stay cozy";
    } else if (k == WXK_ICE && d->pop >= 30) {
        s = "Icy, wear boots";
    } else if (k == WXK_SNOW && d->pop >= 40) {
        s = "Snow! Bundle up";
    } else if (k >= WXK_DRIZZLE && d->pop >= 60) {
        s = "Bring an umbrella";
    } else if (d->pop >= 35 && k >= WXK_DRIZZLE) {
        s = "Maybe some rain";
    } else if (hi <= 32) {
        s = "Super cold, bundle up";
    } else if (hi <= 45) {
        s = "Coat weather";
    } else if (hi <= 55) {
        s = "Jacket weather";
    } else if (hi <= 64) {
        s = "Sweater weather";
    } else if (hi <= 79) {
        s = "Nice out!";
    } else if (hi <= 89) {
        s = "Warm, drink water";
    } else {
        s = "Hot! Stay cool";
    }
    snprintf(buf, len, "%s", s);
}
