#include "cat.h"

#include <stddef.h>

/* First candle (the evening Hanukkah begins), from hebcal.com, checked Oct 2026. */
static const struct {
    short year;
    signed char month, day;
} HANUKKAH[] = {
    {2025, 12, 14}, {2026, 12, 4}, {2027, 12, 24}, {2028, 12, 12}, {2029, 12, 1}, {2030, 12, 20},
    {2031, 12, 9},  {2032, 11, 27}, {2033, 12, 16}, {2034, 12, 6}, {2035, 12, 25},
};

/* Candles go on at sundown; in New Jersey and the Bay Area that's before 5 PM all of Hanukkah. */
#define CANDLE_HOUR 16

/* Days since 1970-01-01 (Howard Hinnant's days_from_civil). */
static long days_from_civil(int y, int m, int d)
{
    y -= m <= 2;
    long era = (y >= 0 ? y : y - 399) / 400;
    long yoe = y - era * 400;
    long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

int cat_hanukkah_candles(int year, int month, int day, int hour)
{
    long today = days_from_civil(year, month, day);
    for (size_t i = 0; i < sizeof(HANUKKAH) / sizeof(HANUKKAH[0]); i++) {
        long first = days_from_civil(HANUKKAH[i].year, HANUKKAH[i].month, HANUKKAH[i].day);
        long nights = today - first + (hour >= CANDLE_HOUR ? 1 : 0);
        if (today >= first && nights <= 8) {
            return (int)nights;
        }
    }
    return -1;
}

static bool wears(const advice_t *a, wear_t w)
{
    for (int i = 0; i < a->nwear; i++) {
        if (a->wear[i] == w) {
            return true;
        }
    }
    return false;
}

cat_fx_t cat_pick_fx(int month, int day, const wx_t *wx)
{
    if (!wx || !wx->valid) {
        return FX_NONE;
    }
    switch (wx_kind(wx->code)) {
    case WXK_SNOW: return FX_SNOW;
    case WXK_ICE: return FX_SLEET;
    case WXK_THUNDER: return FX_STORM;
    case WXK_DRIZZLE: return FX_DRIZZLE;
    case WXK_RAIN: return (wx->code == 65 || wx->code == 82) ? FX_HEAVY_RAIN : FX_RAIN;
    default: break;
    }
    bool clear_sky = wx->code <= 2;   /* clear, mainly clear, partly cloudy */
    if (!wx->is_day && clear_sky) {
        return FX_STARS;
    }
    bool autumn = (month == 9 && day >= 22) || month == 10 || month == 11;
    if (autumn) {
        return FX_LEAVES;
    }
    return FX_NONE;
}

void cat_pick_look(int year, int month, int day, int hour, const wx_t *wx, const advice_t *adv, cat_look_t *out)
{
    *out = (cat_look_t){.why = "weather"};
    out->fx = cat_pick_fx(month, day, wx);

    /* Dressed for the weather, matching the bubble. */
    cat_hat_t w_hat = CAT_HAT_NONE;
    cat_neck_t w_neck = CAT_NECK_NONE;
    cat_face_t w_face = CAT_FACE_NONE;
    if (adv) {
        bool rain = wears(adv, WEAR_UMBRELLA);
        bool cold = wears(adv, WEAR_BEANIE) || wears(adv, WEAR_MITTENS) || wears(adv, WEAR_COAT);
        bool chilly = wears(adv, WEAR_JACKET);
        w_hat = rain ? CAT_HAT_RAIN : cold ? CAT_HAT_BEANIE : CAT_HAT_NONE;
        w_neck = (cold || chilly) ? CAT_NECK_SCARF : CAT_NECK_NONE;
        w_face = (!rain && wears(adv, WEAR_SUNGLASSES)) ? CAT_FACE_SUNGLASSES : CAT_FACE_NONE;
    }
    out->hat = w_hat;
    out->neck = w_neck;
    out->face = w_face;

    int candles = cat_hanukkah_candles(year, month, day, hour);
    bool christmas = month == 12 && day >= 15 && day <= 26;

    if (month == 10) {
        out->hat = CAT_HAT_WITCH;
        out->neck = CAT_NECK_CAPE;
        out->face = CAT_FACE_NONE;
        out->prop = CAT_PROP_PUMPKIN;
        out->why = "Halloween";
    } else if (candles >= 0) {
        out->hat = christmas ? CAT_HAT_SANTA : w_hat;
        out->neck = CAT_NECK_SCARF_BLUE;
        out->prop = CAT_PROP_MENORAH;
        out->candles = candles;
        out->why = christmas ? "Hanukkah and Christmas" : "Hanukkah";
    } else if (christmas) {
        out->hat = CAT_HAT_SANTA;
        out->prop = CAT_PROP_PRESENT;
        out->why = "Christmas";
    }
}
