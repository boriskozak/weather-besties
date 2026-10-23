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
    times, temps, feels, pops, codes, isday = [], [], [], [], [], []
    for i in range(-24, 96):
        di = i // 24 + 1          # index into alld
        h = i % 24
        code, hi, lo = alld[di][0], alld[di][1], alld[di][2]
        t = curve(lo, hi, h)
        times.append(f"{(t0 + dt.timedelta(days=i // 24)).isoformat()}T{h:02d}:00")
        temps.append(round(t, 1))
        feels.append(round(t + feels_delta, 1))
        codes.append(code if i < 0 else codes_fn(i))
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
                   "precipitation_probability": pops, "weather_code": codes, "is_day": isday},
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

for name, data in S.items():
    with open(os.path.join(OUT, name + ".json"), "w") as f:
        json.dump(data, f, separators=(",", ":"))
print("wrote", ", ".join(S))
