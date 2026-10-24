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

/* Garment ids, also the art order in icons.py GARMENTS. Values index tile_kind. */
typedef enum {
    SHORT_SLEEVES, LONG_SLEEVES, SHORTS, LONG_PANTS, HOODIE, LIGHT_JACKET, RAIN_JACKET, WARM_COAT,
    WINTER_COAT, SNEAKERS, RAIN_BOOTS, SNOW_BOOTS, WARM_HAT, GLOVES, UMBRELLA, SUNGLASSES,
    WATER_BOTTLE, SCARF
} garment_t;

/* Extra-item icons for the advice row, in the priority order of plan 4.4
 * (the names match plan 5.5 EXTRA_ICONS). outfit_t.extra_icon is a plain
 * int holding one of these, or EXTRA_ICON_NONE (-1). */
typedef enum {
    EXTRA_ICON_NONE = -1,
    EXTRA_ICON_BOLT = 0,        /* bolt: storm */
    EXTRA_ICON_UMBRELLA,        /* umbrella */
    EXTRA_ICON_HAT_GLOVES,      /* hat_gloves: hat + gloves combined */
    EXTRA_ICON_GLOVES,          /* gloves */
    EXTRA_ICON_SUN_UP,          /* sun_up: warmup */
    EXTRA_ICON_WATER,           /* water: water bottle */
    EXTRA_ICON_SUNGLASSES,      /* sunglasses */
    EXTRA_ICON_WIND               /* wind: windy */
} extra_icon_t;

typedef struct {
    bool ok;                 /* false = don't prescribe (unavailable) */
    int year, month, day, wday;   /* the outfit's date */
    top_t top; bottom_t bottom; layer_t layer; feet_t feet;
    uint32_t extras, change;
    int ntiles; int tile_kind[4];  /* garment ids in display order, see garment_t */
    char advice[160];        /* sentence A + " " + sentence B */
    char advice_short[96];   /* sentence B only (fallback) */
    int extra_icon;          /* extra_icon_t or -1 */
    float t_now, hi, lo; int pop_day; int wx_code; bool is_day;  /* for the weather block */
} outfit_t;

typedef struct { int bike_bus_wday; } outfit_opts_t;

/* now: real local clock. day_offset: 0 = the clock's today, 1 = tomorrow, 2, 3 (forecast page). */
bool outfit_make(const wx_t *wx, const struct tm *now, int day_offset, const outfit_opts_t *o, outfit_t *out);
const char *garment_label(int garment);

#ifdef __cplusplus
}
#endif
