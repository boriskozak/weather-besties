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

void cat_season(int year, int month, int day, int hour, cat_season_t *out)
{
    *out = (cat_season_t){.hat = CAT_SEASON_HAT_NONE, .prop = CAT_SEASON_PROP_NONE, .candles = -1};
    out->doy = (int)(days_from_civil(year, month, day) - days_from_civil(year, 1, 1)) + 1;

    int candles = cat_hanukkah_candles(year, month, day, hour);
    bool christmas = month == 12 && day >= 15 && day <= 26;

    if (month == 10) {
        out->hat = CAT_SEASON_HAT_WITCH;
        out->prop = CAT_SEASON_PROP_PUMPKIN;
    }
    if (candles >= 0) {
        out->prop = CAT_SEASON_PROP_MENORAH;   /* the menorah wins the prop slot */
        out->candles = candles;
        if (christmas) {
            out->hat = CAT_SEASON_HAT_SANTA;
        }
    } else if (christmas) {
        out->hat = CAT_SEASON_HAT_SANTA;
        out->prop = CAT_SEASON_PROP_PRESENT;
    }
}
