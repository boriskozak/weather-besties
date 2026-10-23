/* "What should I wear?" logic. Plain C, shared with the desktop renderer. */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "wx.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Same order as icons.CLOTHES in tools/icons.py. */
typedef enum {
    WEAR_TSHIRT, WEAR_SWEATER, WEAR_JACKET, WEAR_COAT, WEAR_BEANIE, WEAR_MITTENS,
    WEAR_UMBRELLA, WEAR_BOOTS, WEAR_SUNGLASSES, WEAR_WATER, WEAR_COUNT
} wear_t;

#define ADVICE_MAX_WEAR 3

/* Bike bus is a morning ride; gloves get a reminder when it's cold. */
#define BIKE_BUS_COLD_F 50.0f
/* A drop this big in the daily high counts as "a lot colder" (and means gloves). */
#define BIG_CHANGE_F 12.0f

typedef struct {
    int bike_bus_wday;   /* 0 = Sunday ... 6 = Saturday, -1 = no bike bus */
} advice_opts_t;

typedef struct {
    char headline[64];   /* "Chilly 48° morning" (absolute) */
    char compare[80];    /* "A lot colder and cloudier than yesterday" (relative, may be empty) */
    char tip[128];       /* "Bring a jacket! Remember your gloves for bike bus!" */
    int nwear;
    wear_t wear[ADVICE_MAX_WEAR];
    bool tomorrow;       /* advice is about tomorrow (shown in the evening) */
    bool bike_bus;       /* the advice day is a bike bus day */
} advice_t;

void advice_make(const wx_t *wx, const advice_opts_t *opts, advice_t *out);

/* Compares two days (daily high and hours of sun). ref is "yesterday", "today", "Sat"...
 * Returns false when there's nothing to compare against. */
bool advice_compare(const wx_t *wx, const wx_day_t *prev, const wx_day_t *cur, const char *ref, char *buf,
                    size_t len, float *dtemp_f);

/* One short line for a day in the forecast page, e.g. "Bring an umbrella". */
void advice_day_line(const wx_t *wx, int day_idx, char *buf, size_t len);

/* Compact change from the day before, e.g. "6° warmer than Sat". */
void advice_day_trend(const wx_t *wx, int day_idx, char *buf, size_t len);

const char *wear_label(wear_t w);

#ifdef __cplusplus
}
#endif
