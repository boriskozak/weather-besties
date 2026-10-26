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

/* Inverse of days_from_civil (Hinnant's civil_from_days). */
static void ymd_from_days(long z, int *y, int *m, int *d)
{
    z += 719468;
    long era = (z >= 0 ? z : z - 146096) / 146097;
    long doe = z - era * 146097;
    long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    long mp = (5 * doy + 2) / 153;
    *d = (int)(doy - (153 * mp + 2) / 5 + 1);
    *m = (int)(mp + (mp < 10 ? 3 : -9));
    *y = (int)(yoe + era * 400 + (*m <= 2));
}

/* 0 = Sunday .. 6 = Saturday (1970-01-01 was a Thursday). */
static int wday_of(int y, int m, int d)
{
    long w = (days_from_civil(y, m, d) + 4) % 7;
    return (int)(w < 0 ? w + 7 : w);
}

static int mdays_in(int y, int m)
{
    static const int t[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m == 2 && y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) {
        return 29;
    }
    return t[m - 1];
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

/* ------------------------------------------------------------------ zones
 * VTIMEZONE support (iter-29). A TZID date-time used to fall through
 * to mktime, "whatever zone the C library is in", which is only
 * right when the feed's zone and the board's zone happen to match.
 * Now the feed's own VTIMEZONE observances resolve a TZID wall time
 * to the true UTC instant, with the feed's own DST rules, on any
 * libc: Google's export carries STANDARD/DAYLIGHT observances with
 * TZOFFSETFROM/TZOFFSETTO, a DTSTART and a yearly RRULE
 * (BYMONTH/BYDAY), which is everything the conversion needs. A TZID
 * with no matching VTIMEZONE keeps the old local-wall reading. */

#define CAL_MAX_ZONES 4
#define CAL_MAX_OBS 8   /* STANDARD + DAYLIGHT observances per zone */

typedef struct {
    long off_from, off_to;   /* seconds east of UTC */
    int first_year;          /* DTSTART's year: the observance's first */
    int month;               /* onset month: BYMONTH, else DTSTART's */
    int day_code;            /* BYDAY: +n = nth weekday of the month,
                              * -n = nth from the last, 0 = fixed day */
    int wday;                /* 0 = Sunday .. 6 (when day_code != 0) */
    int mday;                /* fixed day of month (day_code == 0) */
    int hh, mi, ss;          /* onset wall time (DTSTART's clock) */
    bool yearly;             /* RRULE FREQ=YEARLY present */
    bool dst;                /* inside BEGIN:DAYLIGHT */
} cal_obs_t;

typedef struct {
    char tzid[48];
    cal_obs_t obs[CAL_MAX_OBS];
    int nobs;
} cal_zone_t;

static CAL_STORE_ATTR cal_zone_t s_zones[CAL_MAX_ZONES];
static int s_nzones;

static const cal_zone_t *find_zone(const char *tzid)
{
    for (int i = 0; i < s_nzones; i++) {
        if (strcmp(s_zones[i].tzid, tzid) == 0) {
            return &s_zones[i];
        }
    }
    return NULL;
}

/* The transition instant of observance o in year y: the rule's wall
 * date at the observance's wall time, read in the offset in effect
 * just before (off_from). Years before the observance's DTSTART do
 * not exist; a non-yearly observance fires in its DTSTART year only. */
static bool obs_onset(const cal_obs_t *o, int y, time_t *out)
{
    if (o->yearly ? y < o->first_year : y != o->first_year) {
        return false;
    }
    int day;
    if (o->day_code > 0) {
        int w1 = wday_of(y, o->month, 1);
        day = 1 + (o->wday - w1 + 7) % 7 + (o->day_code - 1) * 7;
    } else if (o->day_code < 0) {
        int last = mdays_in(y, o->month);
        int wl = wday_of(y, o->month, last);
        day = last - (wl - o->wday + 7) % 7 + (o->day_code + 1) * 7;
    } else {
        day = o->mday;
    }
    *out = epoch_utc(y, o->month, day, o->hh, o->mi, o->ss) - (time_t)o->off_from;
    return true;
}

/* The offset in effect at instant e: the off_to of the most recent
 * onset at or before e, over the observances' rules for the years
 * around e. False when no onset precedes e at all. */
static bool zone_offset_at(const cal_zone_t *z, time_t e, long *off)
{
    long days = e >= 0 ? (long)(e / 86400) : -((long)(-e + 86399) / 86400);
    int y, m, d;
    ymd_from_days(days, &y, &m, &d);
    bool found = false;
    time_t best_onset = 0;
    long best_off = 0;
    for (int yy = y - 1; yy <= y + 1; yy++) {
        for (int i = 0; i < z->nobs; i++) {
            time_t on;
            if (obs_onset(&z->obs[i], yy, &on) && on <= e && (!found || on > best_onset)) {
                found = true;
                best_onset = on;
                best_off = z->obs[i].off_to;
            }
        }
    }
    if (!found) {
        return false;
    }
    *off = best_off;
    return true;
}

/* A wall time in zone z -> the true UTC instant. One candidate per
 * distinct observance offset; the right one is the candidate at
 * which that same offset is actually in effect. Two qualify inside
 * the fall-back overlap (the wall hour happens twice): the earlier
 * instant, the DST occurrence, wins, the way Google renders it. */
static bool zone_epoch(const cal_zone_t *z, int y, int mo, int d, int h, int mi, int s, time_t *out)
{
    time_t w = epoch_utc(y, mo, d, h, mi, s);   /* the wall time, read as if UTC */
    bool have = false;
    time_t best = 0;
    long best_off = 0;
    for (int i = 0; i < z->nobs; i++) {
        long off = z->obs[i].off_to;
        bool dup = false;
        for (int j = 0; j < i; j++) {
            if (z->obs[j].off_to == off) {
                dup = true;
                break;
            }
        }
        if (dup) {
            continue;
        }
        time_t e = w - (time_t)off;
        long at;
        if (zone_offset_at(z, e, &at) && at == off) {
            if (!have || off > best_off) {
                have = true;
                best = e;
                best_off = off;
            }
        }
    }
    if (have) {
        *out = best;
        return true;
    }
    /* Spring-forward gap: the wall time never occurs. Read it in
     * the pre-transition offset, which lands it just past the jump
     * (the 2:30 that shows as 3:30), like other clients do. */
    for (int yy = y - 1; yy <= y + 1; yy++) {
        for (int i = 0; i < z->nobs; i++) {
            time_t on;
            if (!obs_onset(&z->obs[i], yy, &on)) {
                continue;
            }
            time_t wfrom = on + (time_t)z->obs[i].off_from;
            time_t wto = on + (time_t)z->obs[i].off_to;
            if (z->obs[i].off_to > z->obs[i].off_from && w >= wfrom && w < wto) {
                *out = w - (time_t)z->obs[i].off_from;
                return true;
            }
        }
    }
    return false;
}

/* "20261025" (all-day), "20261025T104500" (floating, or TZID when
 * tzid names the zone) or "20261025T144500Z" (UTC). Anything else
 * is not a date we understand. */
static bool parse_dt(const char *v, const char *tzid, bool *all_day, time_t *out)
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
    } else if (tzid && tzid[0]) {
        /* TZID-qualified: resolve through the feed's own VTIMEZONE
         * when it carries one, so the stored instant is exact on
         * any board zone. UTC by another name is just UTC. Without
         * a matching block, keep the local-wall reading. */
        if (strcasecmp(tzid, "UTC") == 0 || strcasecmp(tzid, "Etc/UTC") == 0 ||
            strcasecmp(tzid, "Etc/Universal") == 0 || strcasecmp(tzid, "Universal") == 0 ||
            strcasecmp(tzid, "Zulu") == 0) {
            *out = epoch_utc(y, mo, d, h, mi, s);
            return true;
        }
        const cal_zone_t *z = find_zone(tzid);
        if (!z || !zone_epoch(z, y, mo, d, h, mi, s, out)) {
            *out = epoch_local(y, mo, d, h, mi, s);
        }
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

/* VTIMEZONE under construction (iter-29). */
static bool s_in_tz;
static bool s_in_obs;
static cal_zone_t s_zone;
static cal_obs_t s_obs;

/* "-0500" / "+0530" / "-050000" -> seconds east of UTC. */
static long parse_utcoffset(const char *v)
{
    int sign = 1;
    if (*v == '-') {
        sign = -1;
        v++;
    } else if (*v == '+') {
        v++;
    }
    long s = 0;
    if (isdigit((unsigned char)v[0]) && isdigit((unsigned char)v[1])) {
        s = num2(v) * 3600L;
        v += 2;
    }
    if (isdigit((unsigned char)v[0]) && isdigit((unsigned char)v[1])) {
        s += num2(v) * 60L;
        v += 2;
    }
    if (isdigit((unsigned char)v[0]) && isdigit((unsigned char)v[1])) {
        s += num2(v);
    }
    return sign * s;
}

/* Observance DTSTART: a local wall date-time, no suffix. Only the
 * fields are kept; they seed the observance's rule. */
static bool obs_dtstart(const char *v, cal_obs_t *o)
{
    size_t n = strlen(v);
    if (n < 15 || (v[8] != 'T' && v[8] != 't')) {
        return false;
    }
    for (int i = 0; i < 15; i++) {
        if (i != 8 && !isdigit((unsigned char)v[i])) {
            return false;
        }
    }
    o->first_year = num4(v);
    o->month = num2(v + 4);
    o->mday = num2(v + 6);
    o->hh = num2(v + 9);
    o->mi = num2(v + 11);
    o->ss = num2(v + 13);
    o->day_code = 0;
    return o->month >= 1 && o->month <= 12 && o->mday >= 1 && o->mday <= 31;
}

/* "2SU" / "-1SU" / "1MO" -> nth-weekday code + weekday (0 = Sunday). */
static bool parse_byday(const char *v, cal_obs_t *o)
{
    char *end;
    long code = strtol(v, &end, 10);
    if (end == v || code == 0 || code > 5 || code < -5) {
        return false;
    }
    static const char names[7][3] = {"SU", "MO", "TU", "WE", "TH", "FR", "SA"};
    for (int i = 0; i < 7; i++) {
        if (end[0] == names[i][0] && end[1] == names[i][1]) {
            o->day_code = (int)code;
            o->wday = i;
            return true;
        }
    }
    return false;
}

static void obs_rrule(const char *v, cal_obs_t *o)
{
    /* RRULE parts are ';'-separated key=value pairs. Only the
     * yearly form Google emits is understood; anything else leaves
     * the observance a one-off at its DTSTART. */
    char buf[128];
    snprintf(buf, sizeof(buf), "%s", v);
    for (char *tok = strtok(buf, ";"); tok; tok = strtok(NULL, ";")) {
        char *eq = strchr(tok, '=');
        if (!eq) {
            continue;
        }
        *eq = 0;
        const char *val = eq + 1;
        if (strcmp(tok, "FREQ") == 0) {
            o->yearly = strcmp(val, "YEARLY") == 0;
        } else if (strcmp(tok, "BYMONTH") == 0) {
            int m = atoi(val);
            if (m >= 1 && m <= 12) {
                o->month = m;
            }
        } else if (strcmp(tok, "BYDAY") == 0) {
            parse_byday(val, o);
        }
    }
}

static void obs_line(const char *line)
{
    const char *colon = strchr(line, ':');
    if (!colon) {
        return;
    }
    char name[24];
    size_t hn = 0;
    while (line + hn < colon && line[hn] != ';' && hn < sizeof(name) - 1) {
        name[hn] = line[hn];
        hn++;
    }
    name[hn] = 0;
    const char *value = colon + 1;
    if (strcmp(name, "TZOFFSETFROM") == 0) {
        s_obs.off_from = parse_utcoffset(value);
    } else if (strcmp(name, "TZOFFSETTO") == 0) {
        s_obs.off_to = parse_utcoffset(value);
    } else if (strcmp(name, "DTSTART") == 0) {
        obs_dtstart(value, &s_obs);
    } else if (strcmp(name, "RRULE") == 0) {
        obs_rrule(value, &s_obs);
    }
}

/* The TZID parameter of a DTSTART/DTEND line, if it carries one:
 * the parameter segment between the property name and the ':'.
 * A quoted value loses its quotes. */
static void extract_tzid(const char *line, const char *colon, char *out, size_t cap)
{
    out[0] = 0;
    const char *p = strstr(line, "TZID=");
    if (!p || p >= colon) {
        return;
    }
    p += 5;
    if (*p == '"') {
        p++;
    }
    size_t n = 0;
    while (p + n < colon && p[n] != ';' && p[n] != '"' && n < cap - 1) {
        out[n] = p[n];
        n++;
    }
    out[n] = 0;
}

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
    if (s_in_obs) {
        if (strcmp(line, "END:STANDARD") == 0 || strcmp(line, "END:DAYLIGHT") == 0) {
            s_in_obs = false;
            if (s_zone.nobs < CAL_MAX_OBS) {
                s_zone.obs[s_zone.nobs++] = s_obs;
            }
        } else {
            obs_line(line);
        }
        return;
    }
    if (s_in_tz) {
        if (strcmp(line, "BEGIN:STANDARD") == 0 || strcmp(line, "BEGIN:DAYLIGHT") == 0) {
            s_in_obs = true;
            memset(&s_obs, 0, sizeof(s_obs));
            s_obs.dst = line[6] == 'D';
        } else if (strcmp(line, "END:VTIMEZONE") == 0) {
            s_in_tz = false;
            if (s_zone.tzid[0] && s_zone.nobs > 0 && s_nzones < CAL_MAX_ZONES) {
                s_zones[s_nzones++] = s_zone;
            }
        } else if (strncmp(line, "TZID:", 5) == 0) {
            snprintf(s_zone.tzid, sizeof(s_zone.tzid), "%s", line + 5);
        }
        return;
    }
    if (!s_in_event) {
        if (strcmp(line, "BEGIN:VEVENT") == 0) {
            s_in_event = true;
            memset(&s_cur, 0, sizeof(s_cur));
        } else if (strcmp(line, "BEGIN:VTIMEZONE") == 0) {
            s_in_tz = true;
            memset(&s_zone, 0, sizeof(s_zone));
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
        char tzid[48];
        extract_tzid(line, colon, tzid, sizeof(tzid));
        bool ad;
        time_t t;
        if (parse_dt(value, tzid, &ad, &t)) {
            s_cur.has_start = true;
            s_cur.all_day = ad;
            s_cur.start = t;
        }
    } else if (strcmp(name, "DTEND") == 0) {
        char tzid[48];
        extract_tzid(line, colon, tzid, sizeof(tzid));
        bool ad;
        time_t t;
        if (parse_dt(value, tzid, &ad, &t)) {
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
    s_in_tz = false;
    s_in_obs = false;
    s_nzones = 0;

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
