/*
 * Seasonal touches for the cat (plan 5.4). Plain C (no LVGL), so the PC
 * renderer runs the exact same rules. The outfit always wins: the UI only
 * applies a seasonal hat when the outfit recommends no warm hat, and a
 * seasonal prop never covers a recommended garment.
 *
 *   October          witch hat + jack-o'-lantern
 *   Hanukkah         menorah, one more candle each night
 *   Dec 15 - 26      Santa hat + a present (menorah wins the prop slot)
 */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Hanukkah candles for local date/time: -1 when it isn't Hanukkah, otherwise
 * how many are lit (0 on the first day before sundown, 1 that evening, ... 8).
 * Uses Hebcal's dates for 2025-2035; outside those years it returns -1. */
int cat_hanukkah_candles(int year, int month, int day, int hour);

typedef enum { CAT_SEASON_HAT_NONE, CAT_SEASON_HAT_WITCH, CAT_SEASON_HAT_SANTA } cat_season_hat_t;
typedef enum {
    CAT_SEASON_PROP_NONE,
    CAT_SEASON_PROP_PUMPKIN,
    CAT_SEASON_PROP_PRESENT,
    CAT_SEASON_PROP_MENORAH
} cat_season_prop_t;

typedef struct {
    cat_season_hat_t hat;
    cat_season_prop_t prop;
    int candles;   /* menorah candles lit, 0-8, when prop is MENORAH; else -1 */
    int doy;       /* day of year 1-366 (drives the "take turns" cat choice) */
} cat_season_t;

void cat_season(int year, int month, int day, int hour, cat_season_t *out);

#ifdef __cplusplus
}
#endif
