/*
 * Kids-calendar data model + iCal (RFC 5545) subset parser. Plain C,
 * no LVGL, so the desktop renderer runs the exact same code.
 *
 * The feed is a dedicated kids calendar (the "Kozak Kids" mirror), so
 * every event in it is a kid event: the device does NO filtering. The
 * parser understands the event forms Google Calendar's secret iCal
 * feed emits: TZID and UTC and floating date-times, all-day dates,
 * line unfolding, and text unescaping. Recurrence is ignored on
 * purpose: the mirror feed carries single instances only, so an
 * RRULE master's DTSTART occurrence is all we ever take.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CAL_MAX_EVENTS 48

typedef struct {
    time_t start, end;
    bool all_day;
    char summary[96];
    char location[64];
} cal_event_t;

/* Parses a whole .ics body. Returns false (keeping any previously
 * parsed data) when the text does not look like a calendar at all.
 * Times convert through the C library local timezone: TZID values
 * other than UTC are read as local wall time, which is close enough
 * for this feed (the mirror calendar lives in the board's own zone).
 * Events come back sorted by start. Call from the LVGL thread (or
 * with its lock held), like the rest of the UI state. */
bool cal_parse(const char *ics, size_t len);

int cal_count(void);
const cal_event_t *cal_get(int i);   /* NULL when i is out of range */

#ifdef __cplusplus
}
#endif
