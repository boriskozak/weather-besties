/*
 * The yellow-eyed cat on the today screen: what it's wearing.
 * Plain C (no LVGL), so the PC renderer runs the exact same rules.
 *
 *   October          witch hat, purple cape with an orange bow, jack-o'-lantern
 *   Hanukkah         blue-and-white scarf and a menorah, one more candle each night
 *   Dec 15 - 26      Santa hat and a present (with the menorah too if they overlap)
 *   Snowing now      snowflakes fall across the page, whatever the cat is wearing
 *   Otherwise        dressed like the speech bubble says: rain hat with the umbrella,
 *                    beanie and scarf when it's cold, scarf when it's chilly,
 *                    heart sunglasses on strong-sun days
 */
#pragma once

#include <stdbool.h>

#include "advice.h"
#include "wx.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { CAT_HAT_NONE, CAT_HAT_WITCH, CAT_HAT_SANTA, CAT_HAT_BEANIE, CAT_HAT_RAIN } cat_hat_t;
typedef enum { CAT_NECK_NONE, CAT_NECK_CAPE, CAT_NECK_SCARF, CAT_NECK_SCARF_BLUE } cat_neck_t;
typedef enum { CAT_FACE_NONE, CAT_FACE_SUNGLASSES } cat_face_t;
typedef enum { CAT_PROP_NONE, CAT_PROP_PUMPKIN, CAT_PROP_PRESENT, CAT_PROP_MENORAH } cat_prop_t;

/* What falls (or twinkles) behind the today page, from the weather right now:
 *   rain, drizzle, sleet, snow, thunderstorm (rain plus lightning flickers);
 *   otherwise autumn leaves on fall days (Sep 22 - Nov 30) and twinkling stars on
 *   clear nights. */
typedef enum { FX_NONE, FX_RAIN, FX_HEAVY_RAIN, FX_DRIZZLE, FX_SLEET, FX_SNOW, FX_STORM, FX_LEAVES, FX_STARS } cat_fx_t;

typedef struct {
    cat_hat_t hat;
    cat_neck_t neck;
    cat_face_t face;
    cat_prop_t prop;
    int candles;       /* menorah: candles lit besides the shamash, 0-8 */
    cat_fx_t fx;       /* weather effect behind the page */
    const char *why;   /* "Halloween", "Hanukkah", ... for logs and the renderer */
} cat_look_t;

/* Hanukkah candles for local date/time: -1 when it isn't Hanukkah, otherwise
 * how many are lit (0 on the first day before sundown, 1 that evening, ... 8).
 * Uses Hebcal's dates for 2025-2035; outside those years it returns -1. */
int cat_hanukkah_candles(int year, int month, int day, int hour);

/* The weather effect for right now (month/day: local date). */
cat_fx_t cat_pick_fx(int month, int day, const wx_t *wx);

/* Picks the outfit. adv is the speech bubble's advice for the same moment. */
void cat_pick_look(int year, int month, int day, int hour, const wx_t *wx, const advice_t *adv, cat_look_t *out);

#ifdef __cplusplus
}
#endif
