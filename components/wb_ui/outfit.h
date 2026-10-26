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
typedef enum { FEET_SNEAKERS, FEET_RAIN_BOOTS, FEET_SNOW_BOOTS, FEET_WARM_BOOTS } feet_t;
enum { EX_HAT = 1, EX_GLOVES = 2, EX_UMBRELLA = 4, EX_SUNGLASSES = 8, EX_WATER = 16, EX_SCARF = 32 };
enum { CH_WARMUP = 1, CH_COOLDOWN = 2, CH_RAIN_LATER = 4, CH_RAIN_ALL_DAY = 8, CH_SNOW = 16, CH_STORM = 32, CH_WINDY = 64, CH_HOT = 128, CH_BIKE_BUS = 256 };

/* Garment ids, also the art order in icons.py GARMENTS. Values index tile_kind.
 * WARM_BOOTS (iter-07) is appended at the end so the existing ids keep their
 * values: it is the winter-kit boots tile on a day with no snow or ice in
 * the forecast, labelled "Warm boots" (the icon is the same boot art; the
 * word "Snow" is reserved for days that actually have snow).
 * TIGHTS and DRESS_SHOES (iter-08) are appended the same way: the dress
 * look's remaining garments, itemised as lay-out tiles since panel
 * round 6. Their icons are crops of the dress sprite itself.
 * The iter-25 rotation garments are appended the same way again: the
 * garments only the rotation looks wear. WARM_COAT_TEAL and BROWN_BOOTS
 * exist because the warmcoat sprite wears a teal puffer and brown
 * fur-lined boots, not the kit's blue coat and snow boots, and the
 * tiles must picture what the sprite wears.
 * WINTER_COAT_RED (iter-30) is appended the same way again: the red
 * winter kit's coat. Its label stays "Winter coat": the tile names
 * the garment, the picture is the day's look's own coat. */
typedef enum {
    SHORT_SLEEVES, LONG_SLEEVES, SHORTS, LONG_PANTS, HOODIE, LIGHT_JACKET, RAIN_JACKET, WARM_COAT,
    WINTER_COAT, SNEAKERS, RAIN_BOOTS, SNOW_BOOTS, WARM_HAT, GLOVES, UMBRELLA, SUNGLASSES,
    WATER_BOTTLE, SCARF, DRESS, WARM_BOOTS, TIGHTS, DRESS_SHOES,
    MUSTARD_TOP, NAVY_TOP, RUST_TOP, DRESS_LAVENDER, DRESS_MINT, DRESS_MUSTARD,
    WARM_COAT_TEAL, BROWN_BOOTS, UNIFORM_DRESS, WINTER_COAT_RED
} garment_t;

/* The resolved "look": which flattened Pippa sprite family shows this
 * outfit, and (iter-06, panel round 4) the fidelity anchor. outfit_make
 * first computes the weather prescription, then picks the look whose
 * sprite can actually wear it, then rewrites the prescription into the
 * canonical outfit that sprite wears. Invariant: every garment visible
 * on the sprite is a tile or a checkable chip, and every tile and chip
 * is visible on the sprite. The UI picks the sprite from this field
 * alone, so tiles, chips, advice and cat can never disagree. One
 * owner-approved exception (iter-27): the Watchung logo hot sprite
 * wears neither the sunglasses nor the water bottle, but both stay
 * prescribed on a logo hot day; only the sprite differs. */
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
    LOOK_FALL,        /* hoodie, long pants, sneakers, light scarf */
    /* Iter-25 rotation looks, appended so the original six keep their
     * values. Each is a variant INSIDE one of the original weather
     * bands, picked by the date-seeded rotation in outfit.c; every one
     * canonicalises to the same prescription as its band's original
     * look (see the switch in outfit.c), so the weather truth never
     * changes, only which truthful sprite wears it. */
    LOOK_WARMCOAT,        /* warm-coat tier, dry and hat-free: teal puffer,
                           * jeans, brown fur-lined boots, no hat or gloves */
    LOOK_TOP_MUSTARD,     /* pants band with a long-sleeve engine top:
                           * mustard top under the open blue jacket */
    LOOK_TOP_NAVY,        /* same band: navy star top */
    LOOK_TOP_RUST,        /* same band: rust top */
    LOOK_DRESS_LAVENDER,  /* dress band: lavender dress, same jacket,
                           * tights and Mary Janes as LOOK_DRESS */
    LOOK_DRESS_MINT,      /* dress band: mint dress */
    LOOK_DRESS_MUSTARD,   /* dress band: mustard dress */
    LOOK_FALL_HOODIE,     /* hoodie band: lavender hooded jacket, pink
                           * heart tee and the white scarf */
    LOOK_UNIFORM_DRESS,   /* dress band: the Watchung logo dress */
    LOOK_UNIFORM_PANTS,   /* pants band: the Watchung logo tee + jeans */
    LOOK_UNIFORM_HOT,     /* hot band: the Watchung logo hot look
                           * (iter-27). It rotates in by date like the
                           * other logo looks; its sprite wears neither
                           * the sunglasses nor the water bottle, and
                           * the owner has explicitly accepted that for
                           * this look: both stay prescribed as chips
                           * (see outfit.c's iter-27 note) */
    LOOK_UNIFORM_FALL,    /* hoodie band: logo tee + lavender hooded
                           * jacket, no scarf */
    LOOK_KIT2,            /* iter-30: the red winter kit (red quilted
                           * coat, hat, scarf, mittens). A rotation
                           * variant of LOOK_SNOW inside the dry
                           * warm-coat tier on days that prescribe a
                           * hat, where the warmcoat may not go (it
                           * wears no hat): kit2 wears hat and gloves
                           * exactly like the kit, so it canonicalises
                           * to the kit's outfit whole; only the coat
                           * tile's picture changes (WINTER_COAT_RED) */
    /* Iter-33: the winter-variant batch (art/pippa-winter-variants),
     * appended so every existing look keeps its value. Fourteen more
     * rotation variants of LOOK_SNOW inside the same dry warm-coat
     * tier hat band, which is now all 16 truthful hat looks picked by
     * the outfit-date seed: the twelve shirt variants wear a named
     * long-sleeve shirt under the pink (kitp) or red (kitr) winter
     * coat and canonicalise to the kit's outfit whole, with the coat
     * tile following the coat colour exactly like kit2's; the two
     * dress variants wear a dress under the open coat (lavender
     * under pink, mint under red) with warm leggings instead of long
     * pants, so their tiles swap the dress in for Long pants and
     * drop the Long sleeves tile (the dress is the torso garment;
     * the coat's arms cover the arms). Every one wears hat, scarf,
     * mittens and winter boots, so hat and gloves chips stay. The
     * batch is wired black fur only: the pink enum slots alias the
     * black sprites (see tools/make_pippa_assets.py). */
    LOOK_KITP_NAVY_STRIPE,
    LOOK_KITP_RUST_STRIPE,
    LOOK_KITP_MUSTARD_STRIPE,
    LOOK_KITP_NAVY_SOLID,
    LOOK_KITP_MUSTARD_SOLID,
    LOOK_KITP_TEAL_SOLID,
    LOOK_KITR_NAVY_STRIPE,
    LOOK_KITR_RUST_STRIPE,
    LOOK_KITR_MUSTARD_STRIPE,
    LOOK_KITR_NAVY_SOLID,
    LOOK_KITR_MUSTARD_SOLID,
    LOOK_KITR_TEAL_SOLID,
    LOOK_KITP_DRESS,      /* lavender dress under the open pink coat */
    LOOK_KITR_DRESS       /* mint dress under the open red coat */
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
