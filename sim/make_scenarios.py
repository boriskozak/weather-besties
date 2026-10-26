#!/usr/bin/env python3
"""Writes sample Open-Meteo responses for the renderer and the Windows simulator.

Same shape as the real API with past_days=1 and forecast_days=4: hourly data starts
yesterday at 00:00, daily data starts with yesterday.
"""
import datetime as dt
import json
import math
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "scenarios")
os.makedirs(OUT, exist_ok=True)


def curve(day_min, day_max, h):
    """Temperature through the day: coldest ~6 AM, warmest ~3 PM."""
    if 6 <= h <= 15:
        k = (1 - math.cos(math.pi * (h - 6) / 9)) / 2
    else:
        hh = h if h > 15 else h + 24
        k = (1 + math.cos(math.pi * (hh - 15) / 15)) / 2
    return day_min + (day_max - day_min) * k


def build(today, now_hm, cur, yday, days, codes_fn, pops_fn, sunrise=7, sunset=18, feels_delta=-4):
    """yday/days entries: (weather_code, high, low, rain_chance, uv, sun_hours).
    codes_fn/pops_fn take the hour index from TODAY 00:00 (0..95)."""
    t0 = dt.date.fromisoformat(today)
    alld = [yday] + days
    times, temps, feels, pops, codes, isday, gusts = [], [], [], [], [], [], []
    for i in range(-24, 96):
        di = i // 24 + 1          # index into alld
        h = i % 24
        code, hi, lo = alld[di][0], alld[di][1], alld[di][2]
        t = curve(lo, hi, h)
        times.append(f"{(t0 + dt.timedelta(days=i // 24)).isoformat()}T{h:02d}:00")
        temps.append(round(t, 1))
        feels.append(round(t + feels_delta, 1))
        codes.append(code if i < 0 else codes_fn(i))
        gusts.append(32 if codes[-1] in (95, 96, 97, 99) else 26 if codes[-1] in (65, 82) else
                     18 if codes[-1] in (51, 53, 55, 56, 57, 61, 63, 66, 67, 80, 81) else
                     16 if codes[-1] in (71, 73, 75, 77, 85, 86) else 12)
        pops.append(0 if i < 0 else pops_fn(i))
        isday.append(1 if sunrise <= h < sunset else 0)
    return {
        "latitude": 40.84, "longitude": -74.2, "generationtime_ms": 0.4, "utc_offset_seconds": -14400,
        "timezone": "America/New_York", "timezone_abbreviation": "GMT-4", "elevation": 95.0,
        "current_units": {"time": "iso8601", "interval": "seconds", "temperature_2m": "°F"},
        "current": {
            "time": f"{today}T{now_hm}", "interval": 900,
            "temperature_2m": cur["t"], "apparent_temperature": cur["f"], "is_day": cur["day"],
            "weather_code": cur["code"], "wind_speed_10m": cur.get("wind", 6.0),
            "wind_gusts_10m": cur.get("gust", 12), "precipitation": cur.get("p", 0.0),
        },
        "hourly_units": {"time": "iso8601", "temperature_2m": "°F"},
        "hourly": {"time": times, "temperature_2m": temps, "apparent_temperature": feels,
                   "precipitation_probability": pops, "weather_code": codes, "is_day": isday,
                   "wind_gusts_10m": gusts},
        "daily_units": {"time": "iso8601", "sunshine_duration": "s"},
        "daily": {
            "time": [(t0 + dt.timedelta(days=k - 1)).isoformat() for k in range(len(alld))],
            "weather_code": [d[0] for d in alld],
            "temperature_2m_max": [d[1] for d in alld],
            "temperature_2m_min": [d[2] for d in alld],
            "precipitation_probability_max": [d[3] for d in alld],
            "uv_index_max": [d[4] for d in alld],
            "sunshine_duration": [round(d[5] * 3600) for d in alld],
        },
    }


# A late-October cold front: sunny and 74 on Thursday, 61 and clouding up Friday, rain Saturday.
WED = (1, 70, 50, 0, 4.0, 9.5)
THU = (0, 74, 52, 0, 4.0, 10.0)
FRI = (2, 61, 43, 15, 3.6, 5.5)
SAT = (63, 57, 48, 80, 1.2, 0.5)
SUN = (3, 55, 41, 20, 2.4, 2.0)
MON = (0, 59, 40, 0, 3.1, 9.5)
SAT_RAIN = [3] * 11 + [61, 63, 63, 63, 61, 61] + [3] * 7

S = {}

# Friday morning, bike bus day, a lot colder than yesterday.
S["oct_morning"] = build(
    "2026-10-23", "09:45", {"t": 48.2, "f": 44.6, "day": 1, "code": 2, "gust": 14},
    THU, [FRI, SAT, SUN, MON],
    lambda i: ([1] * 9 + [2] * 6 + [3] * 9)[i] if i < 24 else SAT_RAIN[i - 24] if i < 48 else (3 if i < 72 else 0),
    lambda i: 5 if i < 24 else ((70 + (i % 5) * 4) if 35 <= i <= 40 else 25) if i < 48 else 10,
)

# Thursday after dinner: plan Friday (bike bus, much colder).
S["thu_evening"] = build(
    "2026-10-22", "20:30", {"t": 68.4, "f": 66.0, "day": 0, "code": 0},
    WED, [THU, FRI, SAT, SUN],
    lambda i: 0 if i < 24 else ([1] * 9 + [2] * 6 + [3] * 9)[i - 24] if i < 48 else SAT_RAIN[i - 48] if i < 72 else 3,
    lambda i: 0 if i < 24 else 5 if i < 48 else ((75 if 59 <= i <= 64 else 25) if i < 72 else 20),
)

# Rainy November Friday afternoon (bike bus already happened).
S["rainy_afternoon"] = build(
    "2026-11-06", "14:15", {"t": 52.4, "f": 49.0, "day": 1, "code": 63, "p": 0.08},
    (2, 60, 46, 10, 3.0, 6.0), [(63, 55, 47, 90, 1.0, 0.3), (3, 54, 44, 30, 2.0, 2.5), (0, 58, 42, 0, 2.6, 8.5),
                                (2, 60, 45, 10, 2.8, 6.5)],
    lambda i: (63 if 11 <= i <= 19 else 61 if 20 <= i <= 22 else 3) if i < 24 else (3 if i < 48 else 0),
    lambda i: (85 if 11 <= i <= 22 else 30) if i < 24 else 10,
    sunrise=7, sunset=17, feels_delta=-3,
)

# Snowy January Friday morning.
S["snow_morning"] = build(
    "2027-01-15", "07:30", {"t": 27.1, "f": 18.4, "day": 1, "code": 73, "p": 0.04},
    (3, 38, 29, 20, 1.5, 2.0), [(73, 31, 22, 90, 1.0, 0.0), (3, 29, 18, 20, 1.6, 3.0), (0, 33, 20, 0, 2.0, 8.0),
                                (71, 34, 27, 55, 1.2, 1.0)],
    lambda i: (73 if 5 <= i <= 14 else 71 if 15 <= i <= 17 else 3) if i < 24 else (3 if i < 48 else (0 if i < 72 else 71)),
    lambda i: (88 if 5 <= i <= 17 else 30) if i < 24 else (15 if i < 72 else 55),
    sunrise=7, sunset=17, feels_delta=-8,
)

# Hot July Monday.
S["summer_hot"] = build(
    "2027-07-12", "13:00", {"t": 89.4, "f": 93.0, "day": 1, "code": 0},
    (1, 84, 70, 10, 8.0, 11.0), [(0, 92, 74, 5, 9.1, 13.0), (2, 88, 72, 10, 8.4, 10.0), (95, 85, 70, 65, 6.2, 5.0),
                                 (1, 86, 69, 5, 8.8, 12.0)],
    lambda i: 0 if i < 24 else (2 if i < 48 else (95 if 62 <= i <= 66 else 3) if i < 72 else 1),
    lambda i: 0 if i < 48 else (65 if 60 <= i <= 67 else 10),
    sunrise=6, sunset=20, feels_delta=3,
)

# Third night of Hanukkah 2026 (first candle Dec 4), cold and clear after sunset.
S["hanukkah_evening"] = build(
    "2026-12-06", "17:30", {"t": 33.6, "f": 26.0, "day": 0, "code": 0},
    (0, 41, 29, 0, 1.8, 7.5), [(0, 39, 27, 0, 1.7, 8.0), (2, 42, 30, 10, 1.8, 6.0), (3, 44, 33, 20, 1.5, 3.0),
                               (61, 46, 38, 70, 1.0, 1.0)],
    lambda i: 0 if i < 24 else (2 if i < 48 else (3 if i < 72 else 61)),
    lambda i: 0 if i < 48 else (20 if i < 72 else 70),
    sunrise=7, sunset=16, feels_delta=-7,
)

# Snowy Saturday morning a week before Christmas.
S["christmas_snow"] = build(
    "2026-12-19", "09:10", {"t": 29.0, "f": 21.5, "day": 1, "code": 73, "p": 0.03},
    (3, 36, 28, 20, 1.2, 2.0), [(73, 32, 24, 90, 1.0, 0.0), (3, 33, 22, 20, 1.4, 3.0), (0, 35, 21, 0, 1.6, 7.5),
                                (2, 38, 26, 10, 1.6, 6.0)],
    lambda i: (73 if 6 <= i <= 16 else 3) if i < 24 else (3 if i < 48 else 0),
    lambda i: (90 if 6 <= i <= 16 else 30) if i < 24 else 10,
    sunrise=7, sunset=16, feels_delta=-7,
)

# A plain nice day in May: no holiday, nothing special to wear.
S["mild_may"] = build(
    "2027-05-14", "10:20", {"t": 66.2, "f": 66.0, "day": 1, "code": 2},
    (2, 68, 52, 10, 4.5, 8.0), [(2, 71, 54, 10, 5.0, 9.0), (1, 73, 55, 5, 5.2, 10.0), (3, 66, 53, 30, 3.0, 4.0),
                                (2, 70, 54, 10, 4.8, 9.0)],
    lambda i: 2 if i < 48 else 3,
    lambda i: 10,
    sunrise=6, sunset=20, feels_delta=0,
)

# Christmas evening 2027 is also the second night of Hanukkah (first candle Dec 24).
S["chrismukkah"] = build(
    "2027-12-25", "18:05", {"t": 36.0, "f": 30.0, "day": 0, "code": 3},
    (2, 42, 31, 10, 1.4, 4.0), [(3, 40, 30, 10, 1.2, 2.0), (2, 43, 31, 10, 1.4, 5.0), (0, 45, 33, 0, 1.6, 7.5),
                                (3, 41, 34, 30, 1.1, 2.0)],
    lambda i: 3 if i < 48 else 2,
    lambda i: 10,
    sunrise=7, sunset=16, feels_delta=-6,
)

# Stormy July evening: thunder now, clearing overnight.
S["storm_evening"] = build(
    "2027-07-16", "18:20", {"t": 78.4, "f": 81.0, "day": 1, "code": 95, "p": 0.3, "gust": 32},
    (1, 91, 73, 10, 8.0, 11.0), [(95, 86, 70, 80, 6.0, 6.0), (1, 84, 68, 10, 8.2, 12.0), (2, 85, 69, 10, 8.0, 11.0),
                                 (3, 82, 67, 30, 6.0, 7.0)],
    lambda i: (95 if 16 <= i <= 20 else 2) if i < 24 else 1,
    lambda i: (80 if 16 <= i <= 20 else 10) if i < 24 else 10,
    sunrise=6, sunset=20, feels_delta=2,
)

# ---------------------------------------------------------------------------
# 2.0 redesign scenarios (plan 8.1). Each carries a "sim_clock" so the wall
# clock can differ from the forecast's own timestamp (stale / rollover cases).

# The brief's reference week: cold Tuesday morning warming up, rainy Wednesday.
BR_TUE = (1, 64, 32, 0, 4.0, 9.0)
BR_WED = (63, 55, 46, 80, 1.2, 0.5)
BR_THU = (2, 58, 44, 10, 3.0, 6.0)
BR_FRI = (0, 62, 45, 0, 3.5, 9.0)
BR_MON = (2, 60, 44, 10, 3.0, 6.0)


def _brief_codes(i):
    if i < 24:
        return 1
    if i < 48:
        h = i - 24
        return 63 if 10 <= h <= 16 else 2
    return 2 if i < 72 else 0


def _brief_pops(i):
    if i < 24:
        return 0
    if i < 48:
        h = i - 24
        return 80 if 10 <= h <= 16 else 40
    return 10 if i < 72 else 0


def _brief(now_hm, cur):
    return build("2026-10-27", now_hm, cur, BR_MON, [BR_TUE, BR_WED, BR_THU, BR_FRI],
                 _brief_codes, _brief_pops, sunrise=7, sunset=18, feels_delta=0)


# Tuesday 07:30 forecast, clock 07:35: feels 36F at 8 AM, 63F by 2 PM, dry.
S["brief_morning"] = _brief("07:30", {"t": 33.0, "f": 33.0, "day": 1, "code": 1, "gust": 10})
S["brief_morning"]["sim_clock"] = "2026-10-27T07:35"

# Same data, Tuesday 19:15 forecast, clock 19:30: the brief is tomorrow (rain).
S["brief_evening"] = _brief("19:15", {"t": 58.7, "f": 58.7, "day": 0, "code": 1, "gust": 8})
S["brief_evening"]["sim_clock"] = "2026-10-27T19:30"

# Forecast fetched 11:45, clock 13:39: outfit still shown, stale chip appears.
# Iter-25: this fixture stands on its own Tuesday (2026-11-03, the
# brief week's weather a week later) instead of sharing _brief's
# 2026-10-27. The stale checks exercise the faded scarf chip of the
# fall outfit, and under the outfit rotation 2026-10-27's fall-band
# pick is the Watchung logo fall look, which wears no scarf;
# 2026-11-03's pick is the fall look itself. The day tuples are the
# brief week's own, so the stale expectations (the compare pill's
# "Warmer than yesterday" among them) are unchanged.
S["stale_2h"] = build(
    "2026-11-03", "11:45", {"t": 50.8, "f": 50.8, "day": 1, "code": 1, "gust": 10},
    BR_MON, [BR_TUE, BR_WED, BR_THU, BR_FRI],
    _brief_codes, _brief_pops, sunrise=7, sunset=18, feels_delta=0)
S["stale_2h"]["sim_clock"] = "2026-11-03T13:39"

# Iter-18: the same brief Tuesday, but cold all day (high 40, not
# 64) and fetched at 15:30, for the My Day afternoon renders. The
# afternoon departure still lands in the warm-coat band, so Pippa
# wears the morning's snow look, and the weather is fresh at the
# 3:45 PM clock: the afternoon renders isolate the calendar's own
# states (stale stamp, DONE/NOW/NEXT) with no weather-stale chip
# competing for the same corner of the page.
# Iter-25: the high is 37, not 40. At 40 the 3:45 PM departure was
# hat-free, and under the outfit rotation that tier's pick for
# 2026-10-27 is the warmcoat variant; at 37 the departure needs the
# hat, the winter kit takes the day whole (as the fixture intends),
# and the rotation stays out of it.
BR_TUE_COLD = (1, 37, 27, 0, 4.0, 9.0)
S["myday_afternoon"] = build(
    "2026-10-27", "15:30", {"t": 36.0, "f": 36.0, "day": 1, "code": 1, "gust": 10},
    BR_MON, [BR_TUE_COLD, BR_WED, BR_THU, BR_FRI],
    _brief_codes, _brief_pops, sunrise=7, sunset=18, feels_delta=0)
S["myday_afternoon"]["sim_clock"] = "2026-10-27T15:45"

# Forecast is Monday 18:00, clock Tuesday 07:30: too old, unavailable screen.
S["too_old"] = build(
    "2026-10-26", "18:00", {"t": 58.5, "f": 58.5, "day": 0, "code": 2, "gust": 9},
    (3, 57, 45, 20, 2.6, 3.0), [BR_MON, BR_TUE, BR_WED, BR_THU],
    _brief_codes, _brief_pops, sunrise=7, sunset=18, feels_delta=0)
S["too_old"]["sim_clock"] = "2026-10-27T07:30"

# Forecast fetched Tuesday 23:50, clock Wednesday 00:10: fresh, brief is Wednesday.
S["rollover"] = _brief("23:50", {"t": 46.3, "f": 46.3, "day": 0, "code": 1, "gust": 7})
S["rollover"]["sim_clock"] = "2026-10-28T00:10"

# Snow day (the snow_morning data with an explicit clock).
S["snow_day"] = build(
    "2027-01-15", "07:30", {"t": 27.1, "f": 18.4, "day": 1, "code": 73, "p": 0.04},
    (3, 38, 29, 20, 1.5, 2.0), [(73, 31, 22, 90, 1.0, 0.0), (3, 29, 18, 20, 1.6, 3.0), (0, 33, 20, 0, 2.0, 8.0),
                                (71, 34, 27, 55, 1.2, 1.0)],
    lambda i: (73 if 5 <= i <= 14 else 71 if 15 <= i <= 17 else 3) if i < 24 else (3 if i < 48 else (0 if i < 72 else 71)),
    lambda i: (88 if 5 <= i <= 17 else 30) if i < 24 else (15 if i < 72 else 55),
    sunrise=7, sunset=17, feels_delta=-8)
S["snow_day"]["sim_clock"] = "2027-01-15T07:35"

# Hot summer day: shorts, water bottle, sunglasses. Iter-27: the date
# moved 2027-07-12 -> 2027-07-13 because the hot band rotates now (see
# rot_hot_* below): 2027-07-13's rotation pick is the standard hot
# look, so this fixture (the plain hot day's reference scenario, heart
# sunglasses and all) still shows the reference outfit. The yesterday
# tuple is unchanged, so the compare pill still reads the +14 tier.
S["hot_summer"] = build(
    "2027-07-13", "13:00", {"t": 89.4, "f": 93.0, "day": 1, "code": 0},
    (1, 78, 70, 10, 8.0, 11.0), [(0, 92, 74, 5, 9.1, 13.0), (2, 88, 72, 10, 8.4, 10.0), (95, 85, 70, 65, 6.2, 5.0),
                                 (1, 86, 69, 5, 8.8, 12.0)],
    lambda i: 0 if i < 24 else (2 if i < 48 else (95 if 62 <= i <= 66 else 3) if i < 72 else 1),
    lambda i: 0 if i < 48 else (65 if 60 <= i <= 67 else 10),
    sunrise=6, sunset=20, feels_delta=3)
S["hot_summer"]["sim_clock"] = "2027-07-13T13:05"

# Storms roll in mid-afternoon. Iter-27: the date moved 2027-07-14 ->
# 2027-07-15 for the same reason as hot_summer: the hot band rotates,
# and 2027-07-15's pick is the standard hot look, so the hot-storm
# reference (heart sunglasses prescribed and worn) is unchanged.
S["storm_afternoon"] = build(
    "2027-07-15", "13:00", {"t": 88.0, "f": 92.0, "day": 1, "code": 2},
    (1, 88, 72, 10, 8.0, 11.0), [(2, 90, 74, 20, 8.5, 10.0), (1, 87, 71, 10, 8.4, 11.0), (3, 84, 70, 30, 6.0, 7.0),
                                 (0, 88, 72, 5, 8.8, 12.0)],
    lambda i: (95 if 15 <= i <= 18 else 2) if i < 24 else 1,
    lambda i: (80 if 15 <= i <= 18 else 10) if i < 24 else 10,
    sunrise=6, sunset=20, feels_delta=2)
S["storm_afternoon"]["sim_clock"] = "2027-07-15T13:05"

# Cold Friday bike bus: small warm-up swing so the bike-bus sentence wins.
S["bike_bus_cold_friday"] = build(
    "2026-10-23", "07:30", {"t": 44.0, "f": 40.0, "day": 1, "code": 1, "gust": 12},
    THU, [(2, 52, 43, 15, 3.6, 5.5), SAT, SUN, MON],
    lambda i: ([1] * 9 + [2] * 6 + [3] * 9)[i] if i < 24 else SAT_RAIN[i - 24] if i < 48 else (3 if i < 72 else 0),
    lambda i: 5 if i < 24 else ((70 + (i % 5) * 4) if 35 <= i <= 40 else 25) if i < 48 else 10)
S["bike_bus_cold_friday"]["sim_clock"] = "2026-10-23T07:35"

# Halloween morning, properly cold: the prescribed warm hat beats the witch hat.
S["halloween_cold"] = build(
    "2026-10-31", "08:00", {"t": 40.0, "f": 34.0, "day": 1, "code": 2},
    (2, 55, 42, 10, 3.0, 6.0), [(2, 48, 36, 10, 2.5, 5.0), (1, 52, 38, 5, 3.0, 7.0), (3, 50, 40, 20, 2.4, 3.0),
                                (0, 55, 41, 0, 3.0, 8.0)],
    lambda i: 2 if i < 48 else 3,
    lambda i: 10,
    sunrise=7, sunset=18, feels_delta=-6)
S["halloween_cold"]["sim_clock"] = "2026-10-31T08:05"

# Third night of Hanukkah (first candle Dec 4, 2026), with an explicit clock.
S["hanukkah_night3"] = build(
    "2026-12-06", "17:30", {"t": 33.6, "f": 26.0, "day": 0, "code": 0},
    (0, 41, 29, 0, 1.8, 7.5), [(0, 39, 27, 0, 1.7, 8.0), (2, 42, 30, 10, 1.8, 6.0), (3, 44, 33, 20, 1.5, 3.0),
                               (61, 46, 38, 70, 1.0, 1.0)],
    lambda i: 0 if i < 24 else (2 if i < 48 else (3 if i < 72 else 61)),
    lambda i: 0 if i < 48 else (20 if i < 72 else 70),
    sunrise=7, sunset=16, feels_delta=-7)
S["hanukkah_night3"]["sim_clock"] = "2026-12-06T17:37"

# A mild morning in the dress band (iter-06; retuned iter-09):
# departure feels-like ~48F, dry, warming to 60F. That is the
# light-jacket tier, the only band the dress look may be chosen in
# (iter-09 bound: the dress sprite's jacket IS a light jacket; the old
# 40F-low version of this fixture was a warm-coat departure, which is
# a winter-kit morning now, covered by brief_morning at 33F). Light
# jacket, no hat or gloves (both thresholds are colder), so the outfit
# is the dress and its jacket, nothing else. This is the dress outfit's
# reference scenario (and the heart animation's, since the frames
# exist for the dress sprite only). Iter-25: the date moved from
# 2026-11-04 to 2026-11-08 because the dress band rotates now (see
# rot_dress_* below): 2026-11-08 is a date whose rotation pick is the
# original dress, so this fixture, and the heart proof built on it,
# still show the reference outfit.
S["dress_morning"] = build(
    "2026-11-08", "08:00", {"t": 50.0, "f": 48.0, "day": 1, "code": 1, "gust": 9},
    (2, 62, 48, 10, 3.0, 6.0), [(1, 62, 48, 0, 3.0, 7.0), (2, 56, 41, 10, 2.8, 6.0), (3, 54, 42, 20, 2.4, 4.0),
                                (0, 57, 40, 0, 3.2, 8.0)],
    lambda i: 1 if i < 48 else 2,
    lambda i: 0 if i < 48 else 10,
    sunrise=7, sunset=18, feels_delta=-2)
S["dress_morning"]["sim_clock"] = "2026-11-08T08:05"

# A warm late-spring day in the pants band (iter-06): departure
# feels-like 67F (no layer), high 73F (short sleeves, but long pants:
# the shorts rule needs a 74F high with a 66F departure). Tee, jeans,
# sneakers, no extras: the pants outfit's reference scenario.
S["warm_day"] = build(
    "2027-06-09", "11:00", {"t": 71.0, "f": 71.0, "day": 1, "code": 2},
    (2, 70, 56, 5, 4.8, 9.0), [(2, 73, 58, 5, 5.0, 9.0), (1, 75, 59, 5, 5.4, 10.0), (3, 72, 58, 20, 3.4, 5.0),
                               (2, 74, 57, 10, 5.0, 9.0)],
    lambda i: 2 if i < 48 else 3,
    lambda i: 5 if i < 48 else 10,
    sunrise=6, sunset=20, feels_delta=0)
S["warm_day"]["sim_clock"] = "2027-06-09T11:05"

# Longest advice combo: big warm-up swing (sentence A + the long sentence B).
# Iter-30: the date moved 2026-11-10 -> 2026-11-09 because the warm
# tier's hat days rotate kit/kit2 now (see rot_kit_* below): 2026-11-09's
# seed picks the original kit, so this reference scenario (the kit's
# longest advice) still shows the reference outfit.
# Iter-33: the hat band is 16 looks now, so the date moved again,
# 2026-11-09 -> 2026-11-11: Nov 9's seed picks kitp_dress in the new
# band, Nov 11's picks the original kit (seed mod 16 = 0).
S["long_text"] = build(
    "2026-11-11", "07:30", {"t": 38.0, "f": 35.0, "day": 1, "code": 1},
    (2, 55, 40, 10, 3.0, 6.0), [(1, 66, 36, 0, 3.5, 8.0), (2, 60, 42, 10, 3.0, 6.0), (3, 58, 44, 20, 2.6, 4.0),
                                (0, 63, 45, 0, 3.2, 8.0)],
    lambda i: 1 if i < 24 else 2,
    lambda i: 0 if i < 48 else 10,
    sunrise=7, sunset=17, feels_delta=-3)
S["long_text"]["sim_clock"] = "2026-11-11T07:35"

# Yesterday's high in hot_summer is 78 (above) on purpose: today is
# 92, a +14 jump, so the set covers the "A lot warmer than yesterday"
# tier of the iter-12 compare pill. Yesterday feeds nothing else: the
# parser drops pre-today hours and the outfit never reads yday, so no
# prescription changes.

# Iter-12 honest absence: a payload with NO yesterday in it (a first
# boot, or a fetch that carried forecast days only). The compare pill
# must hide rather than guess, so every daily array drops its
# yesterday entry and every hourly array its yesterday hours.
_no_yday = json.loads(json.dumps(S["mild_may"]))
for _key in _no_yday["daily"]:
    _no_yday["daily"][_key] = _no_yday["daily"][_key][1:]
for _key in _no_yday["hourly"]:
    _no_yday["hourly"][_key] = _no_yday["hourly"][_key][24:]
S["mild_no_yday"] = _no_yday


# Iter-25: the outfit rotation proof. Six consecutive days in the
# dress band whose rotation picks walk the dress band in candidate
# order: 2026-11-08..13 have days_from_civil mod 5 = 0,1,2,3,4,0, so
# the looks are the dress, the lavender dress, the mint dress, the
# mustard dress, the Watchung logo dress, then the dress again.
# Every day of each payload wears the dress_morning weather (high
# 62, low 48, dry), so each day's Tomorrow page resolves in the same
# band and must show the NEXT day's pick: the rotation is seeded by
# the outfit's own date, not the viewing date. sim_main.c renders
# each day's Today and Tomorrow.
def _rot_dress(day):
    return build(
        day, "08:00", {"t": 50.0, "f": 48.0, "day": 1, "code": 1, "gust": 9},
        (2, 62, 48, 10, 3.0, 6.0), [(1, 62, 48, 0, 3.0, 7.0), (2, 62, 48, 10, 2.8, 6.0), (1, 62, 48, 20, 2.4, 4.0),
                                    (0, 62, 48, 0, 3.2, 8.0)],
        lambda i: 1 if i < 48 else 2,
        lambda i: 0 if i < 48 else 10,
        sunrise=7, sunset=18, feels_delta=-2)


for _i, _day in enumerate(("2026-11-08", "2026-11-09", "2026-11-10",
                           "2026-11-11", "2026-11-12", "2026-11-13")):
    S[f"rot_dress_{_i}"] = _rot_dress(_day)
    S[f"rot_dress_{_i}"]["sim_clock"] = f"{_day}T08:05"

# Iter-27: the hot band rotation proof. Two consecutive hot days:
# 2027-07-13 has days_from_civil mod 2 = 0, so its pick is the
# standard hot look; 2027-07-14 is mod 2 = 1, the Watchung logo hot
# look. Every day of each payload wears the hot_summer weather (high
# 92, dry), so each day's Tomorrow page resolves in the same band and
# must show the NEXT day's pick. sim_main.c renders each day's Today
# and the first day's Tomorrow. The logo day prescribes exactly what
# the standard day prescribes (sunglasses and water bottle chips
# included); only the sprite differs.
def _rot_hot(day):
    return build(
        day, "13:00", {"t": 89.4, "f": 93.0, "day": 1, "code": 0},
        (1, 78, 70, 10, 8.0, 11.0), [(0, 92, 74, 5, 9.1, 13.0), (2, 88, 72, 10, 8.4, 10.0), (95, 85, 70, 65, 6.2, 5.0),
                                     (1, 86, 69, 5, 8.8, 12.0)],
        lambda i: 0 if i < 24 else (2 if i < 48 else (95 if 62 <= i <= 66 else 3) if i < 72 else 1),
        lambda i: 0 if i < 48 else (65 if 60 <= i <= 67 else 10),
        sunrise=6, sunset=20, feels_delta=3)


for _i, _day in enumerate(("2027-07-13", "2027-07-14")):
    S[f"rot_hot_{_i}"] = _rot_hot(_day)
    S[f"rot_hot_{_i}"]["sim_clock"] = f"{_day}T13:05"

# Iter-25: a dry warm-coat-tier morning (departure feels-like ~44F:
# warm coat tier, but above the hat and gloves thresholds) whose
# rotation pick is the warmcoat variant. The day before (an even
# seed) would pick the winter kit in the same tier.
S["warmcoat_morning"] = build(
    "2026-11-16", "08:00", {"t": 45.0, "f": 44.0, "day": 1, "code": 1, "gust": 9},
    (2, 55, 41, 10, 3.0, 6.0), [(1, 56, 42, 0, 3.0, 7.0), (2, 54, 40, 10, 2.8, 6.0), (3, 52, 39, 20, 2.4, 4.0),
                                (0, 55, 40, 0, 3.2, 8.0)],
    lambda i: 1 if i < 48 else 2,
    lambda i: 0 if i < 48 else 10,
    sunrise=7, sunset=17, feels_delta=0)
S["warmcoat_morning"]["sim_clock"] = "2026-11-16T08:05"

# Iter-30: the warm tier's hat-day rotation proof. Two consecutive
# dry mornings in the warm-coat tier cold enough to prescribe the
# hat (departure feels-like ~34F: warm coat tier, hat and gloves
# thresholds both crossed, no snow or ice). Until now these days
# collapsed to the winter kit alone, because the warmcoat wears no
# hat; the red winter kit (kit2) wears hat and gloves like the kit,
# so the days alternate kit / kit2 by the outfit-date seed:
# 2026-12-15 has days_from_civil mod 2 = 0 (the kit), 2026-12-16 is
# mod 2 = 1 (kit2). Every day of each payload wears the same weather,
# so each day's Tomorrow page resolves in the same band and must show
# the NEXT day's pick, and the forecast cards carry the day's mini.
# Iter-33: the hat band is 16 looks (seed mod 16), so the pair moved
# to the dates that still pick kit then kit2: 2026-12-13 (mod 16 = 0,
# the kit) and 2026-12-14 (mod 16 = 1, kit2).
def _rot_kit(day):
    return build(
        day, "08:00", {"t": 38.0, "f": 36.0, "day": 1, "code": 1, "gust": 9},
        (2, 50, 38, 10, 2.6, 5.0), [(1, 47, 34, 0, 2.5, 6.0), (2, 46, 33, 10, 2.4, 5.0),
                                    (1, 48, 35, 0, 2.6, 6.5), (3, 45, 34, 10, 2.2, 4.0)],
        lambda i: 1 if i < 48 else 2,
        lambda i: 0 if i < 48 else 10,
        sunrise=7, sunset=17, feels_delta=-1)


for _i, _day in enumerate(("2026-12-13", "2026-12-14")):
    S[f"rot_kit_{_i}"] = _rot_kit(_day)
    S[f"rot_kit_{_i}"]["sim_clock"] = f"{_day}T08:05"

# Iter-33: the winter-variant hat days, same weather as rot_kit_* on
# the dates the 16-look band picks them: 2026-12-15 (mod 16 = 2,
# kitp_navy_stripe: a pink-coat shirt variant; its Tomorrow, Dec 16,
# steps to kitp_rust_stripe), 2026-12-21 (mod 16 = 8,
# kitr_navy_stripe: a red-coat shirt variant) and 2026-12-11
# (mod 16 = 14, kitp_dress: the lavender dress under the pink coat;
# its Tomorrow, Dec 12, is mod 16 = 15, kitr_dress: the mint dress
# under the red coat).
for _i, _day in enumerate(("2026-12-15", "2026-12-21", "2026-12-11")):
    S[f"rot_hat_{_i}"] = _rot_kit(_day)
    S[f"rot_hat_{_i}"]["sim_clock"] = f"{_day}T08:05"

for name, data in S.items():
    with open(os.path.join(OUT, name + ".json"), "w") as f:
        json.dump(data, f, separators=(",", ":"))
print("wrote", ", ".join(S))
