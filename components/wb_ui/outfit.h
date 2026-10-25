/*
 * Outfit engine for the 2.0 clothing-guide redesign (plan section 4).
 * Plain C, no LVGL, so the desktop renderer runs the exact same rules.
 * Types below are copied verbatim from REDESIGN_PLAN.md section 4.1.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "wx.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BIKE_BUS_COLD_F
#define BIKE_BUS_COLD_F 50.0f
#endif

typedef enum { TOP_SHORT_SLEEVES, TOP_LONG_SLEEVES } top_t;
typedef enum { BOTTOM_SHORTS, BOTTOM_LONG_PANTS } bottom_t;
typedef enum { LAYER_NONE, LAYER_HOODIE, LAYER_LIGHT_JACKET, LAYER_RAIN_JACKET, LAYER_WARM_COAT, LAYER_WINTER_COAT } layer_t;
typedef enum { FEET_SNEAKERS, FEET_RAIN_BOOTS, FEET_SNOW_BOOTS } feet_t;
enum { EX_HAT = 1, EX_GLOVES = 2, EX_UMBRELLA = 4, EX_SUNGLASSES = 8, EX_WATER = 16, EX_SCARF = 32 };
enum { CH_WARMUP = 1, CH_COOLDOWN = 2, CH_RAIN_LATER = 4, CH_RAIN_ALL_DAY = 8, CH_SNOW = 16, CH_STORM = 32, CH_WINDY = 64, CH_HOT = 128, CH_BIKE_BUS = 256 };

/* Garment ids, also the art order in icons.py GARMENTS. Values index tile_kind.
 * WARM_BOOTS (iter-07) is appended at the end so the existing ids keep their
 * values: it is the winter-kit boots tile on a day with no snow or ice in
 * the forecast, labelled "Warm boots" (the icon is the same boot art; the
 * word "Snow" is reserved for days that actually have snow).
 * TIGHTS and DRESS_SHOES (iter-08) are appended the same way: the dress
 * look's remaining garments, itemised as lay-out tiles since panel
 * round 6. Their icons are crops of the dress sprite itself. */
typedef enum {
    SHORT_SLEEVES, LONG_SLEEVES, SHORTS, LONG_PANTS, HOODIE, LIGHT_JACKET, RAIN_JACKET, WARM_COAT,
    WINTER_COAT, SNEAKERS, RAIN_BOOTS, SNOW_BOOTS, WARM_HAT, GLOVES, UMBRELLA, SUNGLASSES,
    WATER_BOTTLE, SCARF, DRESS, WARM_BOOTS, TIGHTS, DRESS_SHOES
} garment_t;

/* The resolved "look": which flattened Pippa sprite family shows this
 * outfit, and (iter-06, panel round 4) the fidelity anchor. outfit_make
 * first computes the weather prescription, then picks the look whose
 * sprite can actually wear it, then rewrites the prescription into the
 * canonical outfit that sprite wears. Invariant: every garment visible
 * on the sprite is a tile or a checkable chip, and every tile and chip
 * is visible on the sprite. The UI picks the sprite from this field
 * alone, so tiles, chips, advice and cat can never disagree. */
typedef enum {
    LOOK_DRESS = 0,   /* dress + tights + Mary Janes + her blue LIGHT jacket:
                       * the sprite's jacket is short, light and open, so the
                       * tile says "Light jacket" and the look is only chosen
                       * when a light jacket is the truthful departure layer
                       * (iter-09; see the bound in outfit.c) */
    LOOK_PANTS,       /* tee + jeans + sneakers + her open blue jacket,
                       * prescribed as the light-jacket tile (iter-07) */
    LOOK_SNOW,        /* puffer, beanie, mittens, snow boots, long everything */
    LOOK_RAIN,        /* rain jacket, long pants, rain boots, umbrella in hand */
    LOOK_HOT,         /* tee, shorts, sneakers, heart sunglasses, water bottle */
    LOOK_FALL         /* hoodie, long pants, sneakers, light scarf */
} look_t;

typedef struct {
    bool ok;                 /* false = don't prescribe (unavailable) */
    int year, month, day, wday;   /* the outfit's date */
    top_t top; bottom_t bottom; layer_t layer; feet_t feet;
    int look;                /* look_t: the sprite family that wears this outfit */
    uint32_t extras, change;
    int ntiles; int tile_kind[4];  /* garment ids in display order, see garment_t */
    char advice[192];        /* A + B + the chip-naming clause (iter-08) */
    char advice_short[128];  /* sentence B only (fallback), same clause */
    float t_now, hi, lo; int pop_day; int wx_code; bool is_day;  /* for the weather block */
} outfit_t;

typedef struct { int bike_bus_wday; } outfit_opts_t;

/* now: real local clock. day_offset: 0 = the clock's today, 1 = tomorrow, 2, 3 (forecast page). */
bool outfit_make(const wx_t *wx, const struct tm *now, int day_offset, const outfit_opts_t *o, outfit_t *out);
const char *garment_label(int garment);

#ifdef __cplusplus
}
#endif
