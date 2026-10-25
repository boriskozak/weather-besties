/*
 * iCal subset parser for the kids calendar feed. See cal.h.
 *
 * Storage: the live store holds CAL_STORE events, more than the
 * CAL_MAX_EVENTS the API exposes, because a feed is not ordered the
 * way the page needs it. After parsing, events are sorted by start
 * and, when there are more than the cap, the exposed window is the
 * CAL_KEEP_DAYS band ending at the newest event (the mirror feed's
 * live events always sit in that band; ancient history drops out).
 * While filling, a full store evicts its earliest-starting event for
 * a later one, never the other way round.
 */
#include "cal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The two event stores below are ~17 KB each of plain data. In internal
 * DRAM bss they grew the firmware's internal footprint enough to starve
 * esp_wifi_init on boot (ESP_ERR_NO_MEM, then an abort and a reboot
 * loop), so on device they live in PSRAM. The parser and the page only
 * ever touch them through the accessors, never by address arithmetic
 * across the arrays. The desktop renderer has no PSRAM, and plain
 * static storage is exactly equivalent there. */
#ifndef WB_SIM
#include "esp_attr.h"
#define CAL_STORE_ATTR EXT_RAM_BSS_ATTR
#else
#define CAL_STORE_ATTR
#endif

#define CAL_STORE 96
#define CAL_KEEP_DAYS 75

static CAL_STORE_ATTR cal_event_t s_store[CAL_STORE];
static int s_first;   /* index of the first exposed event */
static int s_count;   /* exposed events (<= CAL_MAX_EVENTS) */

int cal_count(void)
{
    return s_count;
}

const cal_event_t *cal_get(int i)
{
    if (i < 0 || i >= s_count) {
        return NULL;
    }
    return &s_store[s_first + i];
}

/* ------------------------------------------------------------------ time */

/* Days since 1970-01-01 (Howard Hinnant's days_from_civil; same as ui.c). */
static long days_from_civil(int y, int m, int d)
{
    y -= m <= 2;
    long era = (y >= 0 ? y : y - 399) / 400;
    long yoe = y - era * 400;
    long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

static time_t epoch_utc(int y, int mo, int d, int h, int mi, int s)
{
    return (time_t)(days_from_civil(y, mo, d) * 86400L + h * 3600 + mi * 60 + s);
}

static time_t epoch_local(int y, int mo, int d, int h, int mi, int s)
{
    struct tm t;
    memset(&t, 0, sizeof(t));
    t.tm_year = y - 1900;
    t.tm_mon = mo - 1;
    t.tm_mday = d;
    t.tm_hour = h;
    t.tm_min = mi;
    t.tm_sec = s;
    t.tm_isdst = -1;
    return mktime(&t);
}

static int num2(const char *p)
{
    return (p[0] - '0') * 10 + (p[1] - '0');
}

static int num4(const char *p)
{
    return (p[0] - '0') * 1000 + (p[1] - '0') * 100 + (p[2] - '0') * 10 + (p[3] - '0');
}

/* "20261025" (all-day), "20261025T104500" (local/TZID) or
 * "20261025T144500Z" (UTC). Anything else is not a date we understand. */
static bool parse_dt(const char *v, bool *all_day, time_t *out)
{
    size_t n = strlen(v);
    if (n < 8) {
        return false;
    }
    for (int i = 0; i < 8; i++) {
        if (!isdigit((unsigned char)v[i])) {
            return false;
        }
    }
    int y = num4(v), mo = num2(v + 4), d = num2(v + 6);
    if (mo < 1 || mo > 12 || d < 1 || d > 31) {
        return false;
    }
    if (n == 8) {
        *all_day = true;
        *out = epoch_local(y, mo, d, 0, 0, 0);
        return true;
    }
    if ((v[8] != 'T' && v[8] != 't') || n < 15) {
        return false;
    }
    for (int i = 9; i < 15; i++) {
        if (!isdigit((unsigned char)v[i])) {
            return false;
        }
    }
    int h = num2(v + 9), mi = num2(v + 11), s = num2(v + 13);
    if (h > 23 || mi > 59 || s > 60) {
        return false;
    }
    *all_day = false;
    if (v[n - 1] == 'Z' || v[n - 1] == 'z') {
        *out = epoch_utc(y, mo, d, h, mi, s);
    } else {
        *out = epoch_local(y, mo, d, h, mi, s);
    }
    return true;
}

/* ------------------------------------------------------------------ text */

/* ICS text unescaping: \, \; and \\ name themselves, \n is a line
 * break a one-line label cannot show, so it becomes a space. */
static void unescape_copy(char *dst, size_t cap, const char *src)
{
    size_t o = 0;
    for (size_t i = 0; src[i] && o + 1 < cap; i++) {
        char c = src[i];
        if (c == '\\' && src[i + 1]) {
            char e = src[++i];
            c = (e == 'n' || e == 'N') ? ' ' : e;
        }
        dst[o++] = c;
    }
    dst[o] = 0;
}

/* ------------------------------------------------------------------ parse */

typedef struct {
    bool has_start, has_end, cancelled, all_day;
    time_t start, end;
    char summary[96];
    char location[64];
} cur_t;

static CAL_STORE_ATTR cal_event_t s_tmp[CAL_STORE];
static int s_tmp_n;
static bool s_in_event;
static cur_t s_cur;

static void store_event(const cur_t *c)
{
    cal_event_t ev;
    ev.start = c->start;
    ev.end = c->has_end ? c->end : (c->all_day ? c->start + 86400 : c->start);
    ev.all_day = c->all_day;
    snprintf(ev.summary, sizeof(ev.summary), "%s", c->summary);
    snprintf(ev.location, sizeof(ev.location), "%s", c->location);
    if (s_tmp_n < CAL_STORE) {
        s_tmp[s_tmp_n++] = ev;
        return;
    }
    /* Full: keep the latest-starting CAL_STORE events. */
    int lo = 0;
    for (int i = 1; i < s_tmp_n; i++) {
        if (s_tmp[i].start < s_tmp[lo].start) {
            lo = i;
        }
    }
    if (ev.start > s_tmp[lo].start) {
        s_tmp[lo] = ev;
    }
}

static void process_line(const char *line)
{
    if (!s_in_event) {
        if (strcmp(line, "BEGIN:VEVENT") == 0) {
            s_in_event = true;
            memset(&s_cur, 0, sizeof(s_cur));
        }
        return;
    }
    if (strcmp(line, "END:VEVENT") == 0) {
        s_in_event = false;
        if (s_cur.has_start && !s_cur.cancelled) {
            store_event(&s_cur);
        }
        return;
    }
    const char *colon = strchr(line, ':');
    if (!colon) {
        return;
    }
    /* Property name is the head up to ';' (parameters) or ':'. */
    char name[24];
    size_t hn = 0;
    while (line + hn < colon && line[hn] != ';' && hn < sizeof(name) - 1) {
        name[hn] = line[hn];
        hn++;
    }
    name[hn] = 0;
    const char *value = colon + 1;
    if (strcmp(name, "DTSTART") == 0) {
        bool ad;
        time_t t;
        if (parse_dt(value, &ad, &t)) {
            s_cur.has_start = true;
            s_cur.all_day = ad;
            s_cur.start = t;
        }
    } else if (strcmp(name, "DTEND") == 0) {
        bool ad;
        time_t t;
        if (parse_dt(value, &ad, &t)) {
            (void)ad;
            s_cur.has_end = true;
            s_cur.end = t;
        }
    } else if (strcmp(name, "SUMMARY") == 0) {
        unescape_copy(s_cur.summary, sizeof(s_cur.summary), value);
    } else if (strcmp(name, "LOCATION") == 0) {
        unescape_copy(s_cur.location, sizeof(s_cur.location), value);
    } else if (strcmp(name, "STATUS") == 0) {
        s_cur.cancelled = strcasecmp(value, "CANCELLED") == 0;
    }
    /* RRULE and everything else: ignored by design (see cal.h). */
}

static int cmp_start(const void *a, const void *b)
{
    time_t x = ((const cal_event_t *)a)->start, y = ((const cal_event_t *)b)->start;
    return (x > y) - (x < y);
}

bool cal_parse(const char *ics, size_t len)
{
    if (!ics || len == 0) {
        return false;
    }
    if (!strstr(ics, "BEGIN:VEVENT") && !strstr(ics, "BEGIN:VCALENDAR")) {
        return false;   /* not a calendar: keep whatever we already have */
    }
    s_tmp_n = 0;
    s_in_event = false;

    /* Walk raw lines, unfolding continuations (a line starting with
     * a space or tab belongs to the previous line, minus that char). */
    char line[1024];
    size_t llen = 0;
    bool have = false;
    size_t i = 0;
    while (i < len) {
        size_t j = i;
        while (j < len && ics[j] != '\n') {
            j++;
        }
        size_t raw = j - i;
        while (raw && ics[i + raw - 1] == '\r') {
            raw--;
        }
        if (raw && (ics[i] == ' ' || ics[i] == '\t') && have) {
            size_t add = raw - 1;
            if (llen + add >= sizeof(line)) {
                add = sizeof(line) - 1 - llen;
            }
            memcpy(line + llen, ics + i + 1, add);
            llen += add;
            line[llen] = 0;
        } else {
            if (have) {
                process_line(line);
            }
            size_t cp = raw < sizeof(line) - 1 ? raw : sizeof(line) - 1;
            memcpy(line, ics + i, cp);
            line[cp] = 0;
            llen = cp;
            have = true;
        }
        i = j + 1;
    }
    if (have) {
        process_line(line);
    }

    qsort(s_tmp, s_tmp_n, sizeof(s_tmp[0]), cmp_start);
    int first = 0;
    if (s_tmp_n > CAL_MAX_EVENTS) {
        time_t newest = s_tmp[s_tmp_n - 1].start;
        time_t threshold = newest - (time_t)CAL_KEEP_DAYS * 86400;
        while (first < s_tmp_n && s_tmp[first].start < threshold) {
            first++;
        }
    }
    /* The whole sorted store moves over; cal_get exposes only the
     * window from s_first, capped at CAL_MAX_EVENTS (the soonest in
     * the band win, the far future is what the page never shows). */
    memmove(s_store, s_tmp, sizeof(s_store[0]) * s_tmp_n);
    s_first = first;
    s_count = s_tmp_n - first;
    if (s_count > CAL_MAX_EVENTS) {
        s_count = CAL_MAX_EVENTS;
    }
    return true;
}
