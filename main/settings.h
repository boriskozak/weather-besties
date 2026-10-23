#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    char ssid[33];
    char pass[65];
    char place_query[64];  /* what was typed: "Upper Montclair, NJ" or "07043" */
    char place_name[64];   /* what the geocoder found */
    double lat, lon;
    bool has_coords;
    char names[48];        /* shown in the greeting */
    bool celsius;
    bool night_off;        /* screen off at night (tap to wake) */
    uint8_t off_from, off_to;  /* hours, local time */
    uint8_t bike_bus;          /* 0 = Sunday ... 6 = Saturday, 7 = no bike bus */
} wb_settings_t;

void settings_defaults(wb_settings_t *s);
void settings_load(wb_settings_t *s);
void settings_save(const wb_settings_t *s);
