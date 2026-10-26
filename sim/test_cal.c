/*
 * Epoch-level checks for the calendar parser (cal.c), run with:
 * make test   (in sim/)
 *
 * Iter-29: the board showed every My Day time exactly 4 hours late.
 * The feed's instants were fine; the board's C library zone was not
 * (net.c built the POSIX TZ string "WB4", whose two-letter name
 * every libc in play silently rejects, leaving localtime at UTC).
 * These checks pin the parser's side of the contract directly,
 * against hand-computed UTC epochs, in a process zone (UTC0) that
 * cannot leak into the answers:
 *
 *  - a Google-format VTIMEZONE resolves TZID wall times with the
 *    feed's own DST rules, including the Nov 1 2026 fall-back (the
 *    repeated 1:30 is its first, EDT, pass) and the Mar 8 2026
 *    spring-forward (the 2:30 that never happens reads forward);
 *  - "Z" times stay exact;
 *  - floating times and TZIDs without a VTIMEZONE read as local
 *    wall time, and TZID=UTC needs no block;
 *  - the zone string net.c now installs for UTC-4 ("WBT4", three
 *    letters) renders a known instant at 11:15, not 15:15.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cal.h"

static int fails;

static const cal_event_t *by_summary(const char *want)
{
    for (int i = 0; i < cal_count(); i++) {
        const cal_event_t *ev = cal_get(i);
        if (ev && strcmp(ev->summary, want) == 0) {
            return ev;
        }
    }
    return NULL;
}

static void expect_start(const char *summary, long want)
{
    const cal_event_t *ev = by_summary(summary);
    long got = ev ? (long)ev->start : -1;
    printf("%-22s start %ld (want %ld)  %s\n", summary, got, want, got == want ? "ok" : "FAIL");
    if (got != want) {
        fails++;
    }
}

static void expect_end(const char *summary, long want)
{
    const cal_event_t *ev = by_summary(summary);
    long got = ev ? (long)ev->end : -1;
    printf("%-22s end   %ld (want %ld)  %s\n", summary, got, want, got == want ? "ok" : "FAIL");
    if (got != want) {
        fails++;
    }
}

static void parse_or_die(const char *ics)
{
    if (!cal_parse(ics, strlen(ics))) {
        printf("cal_parse refused a fixture\n");
        exit(1);
    }
}

/* Google's export shape for America/New_York. */
static const char VTZ_NY[] =
    "BEGIN:VTIMEZONE\r\nTZID:America/New_York\r\nX-LIC-LOCATION:America/New_York\r\n"
    "BEGIN:DAYLIGHT\r\nTZOFFSETFROM:-0500\r\nTZOFFSETTO:-0400\r\nTZNAME:EDT\r\n"
    "DTSTART:19700308T020000\r\nRRULE:FREQ=YEARLY;BYMONTH=3;BYDAY=2SU\r\nEND:DAYLIGHT\r\n"
    "BEGIN:STANDARD\r\nTZOFFSETFROM:-0400\r\nTZOFFSETTO:-0500\r\nTZNAME:EST\r\n"
    "DTSTART:19701101T020000\r\nRRULE:FREQ=YEARLY;BYMONTH=11;BYDAY=1SU\r\nEND:STANDARD\r\n"
    "END:VTIMEZONE\r\n";

int main(void)
{
    /* A process zone that cannot leak into any answer below. */
    setenv("TZ", "UTC0", 1);
    tzset();

    char ics[8192];
    int n = 0;
    n += snprintf(ics + n, sizeof(ics) - n, "BEGIN:VCALENDAR\r\nVERSION:2.0\r\n%s", VTZ_NY);
    /* File order scrambled on purpose: cal.c sorts by start. */
    n += snprintf(ics + n, sizeof(ics) - n,
                  "BEGIN:VEVENT\r\nUID:t1\r\nDTSTART;TZID=America/New_York:20261101T033000\r\n"
                  "DTEND;TZID=America/New_York:20261101T043000\r\nSUMMARY:After fallback\r\nEND:VEVENT\r\n");
    n += snprintf(ics + n, sizeof(ics) - n,
                  "BEGIN:VEVENT\r\nUID:t2\r\nDTSTART;TZID=America/New_York:20261027T153000\r\n"
                  "DTEND;TZID=America/New_York:20261027T163000\r\nSUMMARY:EDT afternoon\r\nEND:VEVENT\r\n");
    n += snprintf(ics + n, sizeof(ics) - n,
                  "BEGIN:VEVENT\r\nUID:t3\r\nDTSTART;TZID=America/New_York:20261101T013000\r\n"
                  "DTEND;TZID=America/New_York:20261101T023000\r\nSUMMARY:Repeated hour\r\nEND:VEVENT\r\n");
    n += snprintf(ics + n, sizeof(ics) - n,
                  "BEGIN:VEVENT\r\nUID:t4\r\nDTSTART:20261101T140000Z\r\nDTEND:20261101T144500Z\r\n"
                  "SUMMARY:Zulu call\r\nEND:VEVENT\r\n");
    n += snprintf(ics + n, sizeof(ics) - n,
                  "BEGIN:VEVENT\r\nUID:t5\r\nDTSTART;TZID=America/New_York:20261101T003000\r\n"
                  "DTEND;TZID=America/New_York:20261101T010000\r\nSUMMARY:After midnight EDT\r\nEND:VEVENT\r\n");
    n += snprintf(ics + n, sizeof(ics) - n,
                  "BEGIN:VEVENT\r\nUID:t6\r\nDTSTART;TZID=America/New_York:20260308T013000\r\n"
                  "DTEND;TZID=America/New_York:20260308T023000\r\nSUMMARY:Spring early\r\nEND:VEVENT\r\n");
    n += snprintf(ics + n, sizeof(ics) - n,
                  "BEGIN:VEVENT\r\nUID:t7\r\nDTSTART;TZID=America/New_York:20260308T023000\r\n"
                  "DTEND;TZID=America/New_York:20260308T033000\r\nSUMMARY:Spring gap\r\nEND:VEVENT\r\n");
    n += snprintf(ics + n, sizeof(ics) - n,
                  "BEGIN:VEVENT\r\nUID:t8\r\nDTSTART;TZID=America/New_York:20260308T033000\r\n"
                  "DTEND;TZID=America/New_York:20260308T043000\r\nSUMMARY:Spring after\r\nEND:VEVENT\r\n");
    n += snprintf(ics + n, sizeof(ics) - n, "END:VCALENDAR\r\n");
    parse_or_die(ics);

    /* Tue Oct 27 2026, 3:30 PM EDT = 19:30 UTC. */
    expect_start("EDT afternoon", 1793129400L);
    expect_end("EDT afternoon", 1793133000L);
    /* Sun Nov 1 2026: 12:30 AM is still EDT (04:30 UTC); the 1:30
     * hour happens twice and resolves to the first pass (05:30
     * UTC), its 2:30 end is unambiguous EST (07:30 UTC); 3:30 AM
     * is EST (08:30 UTC). A fixed -4 reading fails the last two. */
    expect_start("After midnight EDT", 1793507400L);
    expect_start("Repeated hour", 1793511000L);
    expect_end("Repeated hour", 1793518200L);
    expect_start("After fallback", 1793521800L);
    /* The "Z" form never moves: 14:00 UTC = 9:00 AM EST that day. */
    expect_start("Zulu call", 1793541600L);
    /* Sun Mar 8 2026 spring-forward: 1:30 AM EST = 06:30 UTC; the
     * 2:30 that never happens reads in the old offset and lands at
     * 07:30 UTC (shown as 3:30 EDT); 3:30 AM EDT = 07:30 UTC. */
    expect_start("Spring early", 1772951400L);
    expect_start("Spring gap", 1772955000L);
    expect_start("Spring after", 1772955000L);

    /* No VTIMEZONE in this feed: TZID falls back to local wall
     * (UTC0 here, so the fields read as UTC), floating likewise,
     * and TZID=UTC is exact without any block. */
    parse_or_die("BEGIN:VCALENDAR\r\nVERSION:2.0\r\n"
                 "BEGIN:VEVENT\r\nUID:f1\r\nDTSTART;TZID=America/New_York:20261027T153000\r\n"
                 "SUMMARY:TZID no block\r\nEND:VEVENT\r\n"
                 "BEGIN:VEVENT\r\nUID:f2\r\nDTSTART:20261027T153000\r\n"
                 "SUMMARY:Floating\r\nEND:VEVENT\r\n"
                 "BEGIN:VEVENT\r\nUID:f3\r\nDTSTART;TZID=UTC:20261027T100000\r\n"
                 "SUMMARY:TZID UTC\r\nEND:VEVENT\r\n"
                 "END:VCALENDAR\r\n");
    expect_start("TZID no block", 1793115000L);
    expect_start("Floating", 1793115000L);
    expect_start("TZID UTC", 1793095200L);

    /* The render side of the field bug: the instant the board got
     * wrong (Sun Oct 25 2026, 15:15 UTC = 11:15 AM EDT, the Camp
     * Fair's start) under the zone string net.c installs now. */
    setenv("TZ", "WBT4", 1);
    tzset();
    time_t fair = 1792941300L;
    struct tm lt;
    localtime_r(&fair, &lt);
    int ok = lt.tm_hour == 11 && lt.tm_min == 15;
    printf("%-22s WBT4 renders %02d:%02d (want 11:15)  %s\n", "Board zone string", lt.tm_hour, lt.tm_min,
           ok ? "ok" : "FAIL");
    if (!ok) {
        fails++;
    }

    /* And the floating semantic under a real zone agrees with the
     * VTIMEZONE answer for the same wall time. */
    setenv("TZ", "America/New_York", 1);
    tzset();
    struct tm wall = {0};
    wall.tm_year = 2026 - 1900;
    wall.tm_mon = 10 - 1;
    wall.tm_mday = 27;
    wall.tm_hour = 15;
    wall.tm_min = 30;
    wall.tm_isdst = -1;
    long mk = (long)mktime(&wall);
    printf("%-22s mktime %ld (want %ld)  %s\n", "NY wall parity", mk, 1793129400L,
           mk == 1793129400L ? "ok" : "FAIL");
    if (mk != 1793129400L) {
        fails++;
    }

    if (fails) {
        printf("%d cal checks FAILED\n", fails);
        return 1;
    }
    printf("All cal checks passed.\n");
    return 0;
}
