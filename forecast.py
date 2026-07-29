#!/usr/bin/env python3
"""
Pronóstico meteorológico para ciudades de Italia v2.0
Basado en: datos históricos observados + forecast Open-Meteo + análisis estadístico
Uso: python forecast.py [--ciudad CIUDAD] [--dias N] [--extendido] [--today]
"""

import argparse
import json
import urllib.request
import urllib.parse
import datetime
import sys
from collections import Counter

# Todas las capitales de región de Italia + coordenadas + normales climáticas julio (1991-2020)
CIUDADES = {
    "AOSTA":     {"lat": 45.74, "lon": 7.32,  "alt": 583, "region": "Valle d'Aosta",        "t_mean": 21.0, "precip": 45},
    "AQUILA":    {"lat": 42.35, "lon": 13.40, "alt": 714, "region": "Abruzzo",              "t_mean": 21.5, "precip": 35},
    "BARI":      {"lat": 41.12, "lon": 16.87, "alt": 5,   "region": "Puglia",               "t_mean": 26.5, "precip": 20},
    "BERGAMO":   {"lat": 45.70, "lon": 9.67,  "alt": 249, "region": "Lombardia",            "t_mean": 23.5, "precip": 75},
    "BOLOGNA":   {"lat": 44.49, "lon": 11.34, "alt": 54,  "region": "Emilia-Romagna",       "t_mean": 25.0, "precip": 40},
    "CAGLIARI":  {"lat": 39.22, "lon": 9.12,  "alt": 4,   "region": "Sardegna",             "t_mean": 26.5, "precip": 3},
    "CAMPOBASSO":{"lat": 41.56, "lon": 14.66, "alt": 701, "region": "Molise",               "t_mean": 22.5, "precip": 30},
    "CATANZARO": {"lat": 38.91, "lon": 16.60, "alt": 342, "region": "Calabria",             "t_mean": 25.0, "precip": 10},
    "FIRENZE":   {"lat": 43.77, "lon": 11.26, "alt": 50,  "region": "Toscana",              "t_mean": 25.0, "precip": 40},
    "GENOVA":    {"lat": 44.41, "lon": 8.93,  "alt": 19,  "region": "Liguria",              "t_mean": 24.5, "precip": 30},
    "MILANO":    {"lat": 45.46, "lon": 9.19,  "alt": 122, "region": "Lombardia",            "t_mean": 24.0, "precip": 65},
    "NAPOLI":    {"lat": 40.85, "lon": 14.27, "alt": 17,  "region": "Campania",             "t_mean": 26.0, "precip": 25},
    "PALERMO":   {"lat": 38.12, "lon": 13.36, "alt": 14,  "region": "Sicilia",              "t_mean": 27.0, "precip": 5},
    "PERUGIA":   {"lat": 43.11, "lon": 12.39, "alt": 493, "region": "Umbria",               "t_mean": 24.0, "precip": 35},
    "POTENZA":   {"lat": 40.64, "lon": 15.80, "alt": 819, "region": "Basilicata",           "t_mean": 22.0, "precip": 25},
    "ROMA":      {"lat": 41.90, "lon": 12.50, "alt": 21,  "region": "Lazio",                "t_mean": 25.5, "precip": 20},
    "TORINO":    {"lat": 45.07, "lon": 7.67,  "alt": 239, "region": "Piemonte",             "t_mean": 23.2, "precip": 56},
    "TRENTO":    {"lat": 46.07, "lon": 11.12, "alt": 190, "region": "Trentino-Alto Adige",  "t_mean": 22.5, "precip": 70},
    "TRIESTE":   {"lat": 45.65, "lon": 13.77, "alt": 2,   "region": "Friuli-Venezia Giulia","t_mean": 24.5, "precip": 65},
    "VENEZIA":   {"lat": 45.44, "lon": 12.32, "alt": 1,   "region": "Veneto",               "t_mean": 24.5, "precip": 50},
}
CIUDAD_DEFAULT = "TORINO"

NOMBRES_VISIBLES = {
    "AQUILA": "L'Aquila",
}


def fetch_openmeteo(ciudad, days_forecast, days_history=7):
    """Fetch data from Open-Meteo API (free, no key required)."""
    base = "https://api.open-meteo.com/v1/forecast"

    today = datetime.date.today()
    past_start = today - datetime.timedelta(days=days_history)
    future_end = today + datetime.timedelta(days=days_forecast - 1)

    params = {
        "latitude": ciudad["lat"],
        "longitude": ciudad["lon"],
        "timezone": "Europe/Rome",
        "start_date": past_start.isoformat(),
        "end_date": future_end.isoformat(),
        "daily": "temperature_2m_max,temperature_2m_min,precipitation_sum,precipitation_probability_max",
        "hourly": "temperature_2m,relative_humidity_2m,precipitation_probability,precipitation,weather_code,surface_pressure",
        "models": "best_match",
    }

    url = base + "?" + urllib.parse.urlencode(params, doseq=True)
    try:
        with urllib.request.urlopen(url, timeout=30) as resp:
            return json.loads(resp.read().decode())
    except Exception as e:
        print(f"  ERROR fetching data: {e}", file=sys.stderr)
        return None


def weather_code_to_condition(code):
    """Map WMO weather codes to human-readable conditions."""
    if code is None or code == -1:
        return "Despejado"
    if code == 0:
        return "Despejado"
    if code == 1:
        return "Mayormente despejado"
    if code == 2:
        return "Parcialmente nublado"
    if code == 3:
        return "Nublado"
    if 4 <= code <= 19:
        return "Niebla"
    if 20 <= code <= 29:
        return "Lluvia ligera"
    if 30 <= code <= 39:
        return "Tormenta"
    if 40 <= code <= 49:
        return "Niebla densa"
    if 50 <= code <= 59:
        return "Lluvia ligera"
    if 60 <= code <= 69:
        return "Lluvia"
    if 70 <= code <= 79:
        return "Nieve"
    if 80 <= code <= 84:
        return "Lluvia"
    if 85 <= code <= 86:
        return "Nieve"
    if 90 <= code <= 99:
        return "Tormenta"
    return "Otros"


def condition_icon(cond):
    """Return a compact icon for terminal display."""
    icons = {
        "Despejado": "☀",
        "Mayormente despejado": "🌤",
        "Parcialmente nublado": "⛅",
        "Nublado": "☁",
        "Niebla": "🌫",
        "Lluvia ligera": "🌦",
        "Lluvia": "🌧",
        "Tormenta": "⛈",
        "Nieve": "❄",
    }
    for k, v in icons.items():
        if k in cond:
            return v
    return "❓"


def ciudad_display_name(key):
    return NOMBRES_VISIBLES.get(key, key.capitalize())


def list_ciudades():
    """Print all available cities in a formatted table."""
    print()
    print(f"  Ciudades disponibles ({len(CIUDADES)}):")
    print(f"  {'Región':<24} {'Ciudad':<14} {'Lat':>8} {'Lon':>8} {'Alt':>5} {'T media':>8} {'Precip':>7}")
    print(f"  {'-'*24} {'-'*14} {'-'*8} {'-'*8} {'-'*5} {'-'*8} {'-'*7}")
    for key in sorted(CIUDADES.keys()):
        c = CIUDADES[key]
        name = ciudad_display_name(key)
        lat_s = f"{c['lat']:.2f}°{'N' if c['lat']>=0 else 'S'}"
        lon_s = f"{c['lon']:.2f}°{'E' if c['lon']>=0 else 'O'}"
        print(f"  {c['region']:<24} {name:<14} {lat_s:>8} {lon_s:>8} {c['alt']:>4}m {c['t_mean']:>5.1f}°C  {c['precip']:>3}mm")
    print()
    print(f"  Usar: --ciudad CIUDAD (ej: --ciudad ROMA, --ciudad FIRENZE)")
    print()


def compute_mosquito_risk(forecast_dates, daily_stats, hist):
    """Compute daily mosquito proliferation risk (0-10) from weather data."""
    risks = []
    for fd, ds in daily_stats:
        t_mean = ds["t_mean"]
        h_mean = (ds["h_min"] + ds["h_max"]) / 2
        precip = fd["precip_sum"]

        if t_mean < 15:
            t_score = 0.0
        elif t_mean < 25:
            t_score = (t_mean - 15) / 10
        elif t_mean <= 30:
            t_score = 1.0
        elif t_mean < 38:
            t_score = 1 - (t_mean - 30) / 8
        else:
            t_score = 0.0

        h_score = min(h_mean / 80, 1.0)
        p_score = min(precip / 5, 1.0) if precip > 0 else 0.0

        risk = (t_score * 0.45 + h_score * 0.35 + p_score * 0.20) * 10
        risks.append(round(min(risk, 10), 1))

    avg_risk = sum(risks) / len(risks) if risks else 0

    if len(risks) >= 4:
        half = len(risks) // 2
        first = sum(risks[:half]) / half
        second = sum(risks[half:]) / half
        diff = second - first
        if diff > 1.0:
            trend = "aumentando"
        elif diff < -1.0:
            trend = "disminuyendo"
        else:
            trend = "estable"
    else:
        trend = "estable"

    dry_boost = 1.0 if hist["dry_streak"] >= 2 and hist["total_precip"] > 0 else 0

    return {
        "daily_risks": risks,
        "avg_risk": round(min(avg_risk + dry_boost, 10), 1),
        "trend": trend,
        "peak_risk": max(risks) if risks else 0,
        "peak_day": risks.index(max(risks)) if risks else -1,
    }


def mosquito_level(risk):
    if risk >= 7:
        return "ALTO"
    if risk >= 4:
        return "MEDIO"
    if risk >= 2:
        return "BAJO"
    return "MUY BAJO"


def analyze_historical(data, ciudad):
    """Analyze historical observed data to detect trends/anomalies."""
    daily = data.get("daily", {})
    dates = daily.get("time", [])
    t_maxs = daily.get("temperature_2m_max", [])
    t_mins = daily.get("temperature_2m_min", [])
    precips = daily.get("precipitation_sum", [])

    t_mean_norm = ciudad["t_mean"]

    today = datetime.date.today()
    hist_maxs, hist_mins, hist_prec = [], [], []

    for d, tmax, tmin, prec in zip(dates, t_maxs, t_mins, precips):
        dt = datetime.date.fromisoformat(d)
        if dt < today and tmax is not None:
            hist_maxs.append(tmax)
            hist_mins.append(tmin)
            hist_prec.append(prec if prec else 0)

    n = len(hist_maxs)
    if n == 0:
        return {"mean_obs_max": None, "mean_obs_min": None, "mean_anomaly": None,
                "dry_days": 0, "total_precip": 0, "trend": "stable", "heatwave": False,
                "recent_maxes": [], "recent_mins": []}

    mean_obs_max = sum(hist_maxs) / n
    mean_obs_min = sum(hist_mins) / n
    mean_obs = (mean_obs_max + mean_obs_min) / 2
    anomaly = mean_obs - t_mean_norm
    total_precip = sum(hist_prec)

    dry_streak = 0
    for p in reversed(hist_prec):
        if p == 0:
            dry_streak += 1
        else:
            break

    if n >= 4:
        recent = hist_maxs[-3:]
        prior = hist_maxs[-4:-1]
        avg_recent = sum(recent) / 3
        avg_prior = sum(prior) / 3
        diff = avg_recent - avg_prior
        if diff > 1.5:
            trend = "rapid_warming"
        elif diff > 0.5:
            trend = "warming"
        elif diff < -1.5:
            trend = "rapid_cooling"
        elif diff < -0.5:
            trend = "cooling"
        else:
            trend = "stable"
    else:
        trend = "stable"

    heatwave = dry_streak >= 3 and anomaly > 3.0

    return {
        "mean_obs_max": mean_obs_max,
        "mean_obs_min": mean_obs_min,
        "mean_obs": mean_obs,
        "mean_normal": t_mean_norm,
        "mean_anomaly": anomaly,
        "dry_streak": dry_streak,
        "total_precip": total_precip,
        "trend": trend,
        "heatwave": heatwave,
        "n_days": n,
        "recent_maxes": hist_maxs,
        "recent_mins": hist_mins,
    }


def compute_daily_stats(hourly_data, date_str):
    """Compute daily min/max/mean stats from hourly data."""
    temps, humids, precips = [], [], []
    for h in hourly_data:
        if h["date"] == date_str:
            if h["temp"] is not None:
                temps.append(h["temp"])
            if h["humid"] is not None:
                humids.append(h["humid"])
            if h["precip"] is not None:
                precips.append(h["precip"])

    if not temps:
        return None

    t_min = min(temps)
    t_max = max(temps)
    t_mean = sum(temps) / len(temps)
    h_min = min(humids) if humids else 0
    h_max = max(humids) if humids else 0
    total_precip = sum(precips) if precips else 0

    t_min_hour = next((h["hour"] for h in hourly_data if h["date"] == date_str and h["temp"] == t_min), "??")
    t_max_hour = next((h["hour"] for h in hourly_data if h["date"] == date_str and h["temp"] == t_max), "??")
    h_min_hour = next((h["hour"] for h in hourly_data if h["date"] == date_str and h["humid"] == h_min), "??")
    h_max_hour = next((h["hour"] for h in hourly_data if h["date"] == date_str and h["humid"] == h_max), "??")

    conditions = [h["condition"] for h in hourly_data if h["date"] == date_str]
    dominant = Counter(conditions).most_common(1)[0][0] if conditions else "Despejado"

    if any("Tormenta" in c for c in conditions):
        cond_summary = "Tormentas aisladas"
    elif any("Lluvia" in c for c in conditions):
        cond_summary = "Lluvias, parcialmente nublado"
    elif any("Niebla" in c for c in conditions):
        cond_summary = "Niebla, nubosidad"
    elif any("Nublado" in c for c in conditions) and any("Parcialmente" in c for c in conditions):
        cond_summary = "Nubosidad variable"
    elif sum(1 for c in conditions if "Soleado" in c or "Despejado" in c) >= 18:
        cond_summary = "Soleado, despejado"
    elif any("Nublado" in c for c in conditions):
        cond_summary = "Mayormente nublado"
    else:
        cond_summary = dominant

    return {
        "t_min": t_min, "t_min_hour": f"{t_min_hour:02d}:00",
        "t_max": t_max, "t_max_hour": f"{t_max_hour:02d}:00",
        "t_mean": t_mean,
        "h_min": h_min, "h_min_hour": f"{h_min_hour:02d}:00",
        "h_max": h_max, "h_max_hour": f"{h_max_hour:02d}:00",
        "precip": total_precip,
        "condition": cond_summary,
        "dominant": dominant,
    }


def analyze_convective_risk(hourly_data, forecast_dates):
    """Analyze convective/thunderstorm risk from hourly weather codes.
    Returns per-day severity and a summary for the period.
    """
    storm_severity = {
        95: "moderada", 96: "fuerte", 97: "fuerte",
        99: "severa",
    }

    result = {}
    for fd in forecast_dates:
        date = fd["date"]
        hours = [h for h in hourly_data if h["date"] == date]
        max_severity = 0
        storm_hours = []
        total_conv_precip = 0
        for h in hours:
            code = None
            for k, v in storm_severity.items():
                if h.get("raw_code") == k:
                    code = k
                    break
            if code:
                max_severity = max(max_severity, code)
                storm_hours.append(h["hour"])
                total_conv_precip += h["precip"] if h["precip"] else 0

        night_storm = any(0 <= h <= 5 for h in storm_hours)

        if max_severity >= 99:
            sev_label = "severa"
        elif max_severity >= 96:
            sev_label = "fuerte"
        elif max_severity >= 95:
            sev_label = "moderada"
        elif max_severity >= 90:
            sev_label = "leve"
        else:
            sev_label = None

        result[date] = {
            "has_storm": sev_label is not None,
            "severity": sev_label,
            "hours": storm_hours,
            "night_storm": night_storm,
            "conv_precip": total_conv_precip,
        }

    has_any_storm = any(r["has_storm"] for r in result.values())
    severe_days = sum(1 for r in result.values() if r["severity"] in ("fuerte", "severa"))
    night_storm_days = sum(1 for r in result.values() if r["night_storm"])
    return {
        "daily": result,
        "has_any_storm": has_any_storm,
        "severe_days": severe_days,
        "night_storm_days": night_storm_days,
    }


def build_forecast(raw, days):
    """Build enhanced forecast from raw Open-Meteo data."""
    if not raw:
        return None

    hourly = raw.get("hourly", {})
    daily = raw.get("daily", {})

    times = hourly.get("time", [])
    temps = hourly.get("temperature_2m", [])
    humids = hourly.get("relative_humidity_2m", [])
    precip_prob = hourly.get("precipitation_probability", [])
    precip = hourly.get("precipitation", [])
    weather_codes = hourly.get("weather_code", [])
    pressure = hourly.get("surface_pressure", [])

    daily_dates = daily.get("time", [])
    daily_tmax = daily.get("temperature_2m_max", [])
    daily_tmin = daily.get("temperature_2m_min", [])
    daily_precip = daily.get("precipitation_sum", [])

    hourly_data = []
    for i in range(len(times)):
        dt = datetime.datetime.fromisoformat(times[i])
        code = weather_codes[i] if i < len(weather_codes) else None
        cond = weather_code_to_condition(code)
        hourly_data.append({
            "datetime": dt,
            "date": dt.strftime("%Y-%m-%d"),
            "hour": dt.hour,
            "iso": dt.strftime("%Y-%m-%dT%H:%M+02:00"),
            "temp": temps[i] if i < len(temps) else None,
            "humid": humids[i] if i < len(humids) else None,
            "precip_prob": precip_prob[i] if i < len(precip_prob) else 0,
            "precip": precip[i] if i < len(precip) else 0,
            "pressure": pressure[i] if i < len(pressure) else None,
            "condition": cond,
            "raw_code": weather_codes[i] if i < len(weather_codes) else None,
        })

    today = datetime.date.today()
    forecast_dates = []
    for d_str, tm, tn, pp in zip(daily_dates, daily_tmax, daily_tmin, daily_precip):
        dt = datetime.date.fromisoformat(d_str)
        if dt >= today:
            forecast_dates.append({
                "date": d_str,
                "t_max": tm,
                "t_min": tn,
                "precip_sum": pp if pp else 0,
                "dow": dt.strftime("%A"),
            })

    return {"hourly": hourly_data, "forecast_dates": forecast_dates}


def print_header(title):
    w = 78
    print()
    print("=" * w)
    print(f"  {title}")
    print("=" * w)


def today_report(ciudad_nombre):
    """Show today's hourly data with pressure, condition, trend and observations."""
    ciudad = CIUDADES[ciudad_nombre]

    raw = fetch_openmeteo(ciudad, 3)
    if not raw:
        print("  ERROR: No se pudieron obtener datos.", file=sys.stderr)
        return 1

    hist = analyze_historical(raw, ciudad)
    forecast = build_forecast(raw, 3)
    if not forecast:
        print("  ERROR: No se pudieron procesar los datos.", file=sys.stderr)
        return 1

    hourly_data = forecast["hourly"]
    today_str = datetime.date.today().isoformat()
    today_hours = [h for h in hourly_data if h["date"] == today_str]

    if not today_hours:
        print("  No hay datos para hoy.")
        return 1

    dow_map = {
        "Monday": "Lun", "Tuesday": "Mar", "Wednesday": "Mié",
        "Thursday": "Jue", "Friday": "Vie", "Saturday": "Sáb", "Sunday": "Dom"
    }
    dow = dow_map.get(datetime.date.today().strftime("%A"), "??")

    print()
    print(f"  ── {ciudad_display_name(ciudad_nombre)}, {dow} {datetime.date.today().day} {datetime.date.today().strftime('%b %Y')} ──")
    print()
    print(f"  {'Hora':>6}  {'Temp':>7}  {'Presión':>8}  {'Humedad':>7}  {'Lluvia':>7}  {'Estado':<25}")
    print(f"  {'─'*6}  {'─'*7}  {'─'*8}  {'─'*7}  {'─'*7}  {'─'*25}")

    temps = []
    humids = []
    pressures = []
    conditions = Counter()

    for h in today_hours:
        temps.append(h["temp"])
        humids.append(h["humid"])
        if h["pressure"] is not None:
            pressures.append(h["pressure"])
        conditions[h["condition"]] += 1

        pres = f"{h['pressure']:.0f} hPa" if h["pressure"] is not None else "N/A"
        prec = f"{h['precip']:.1f}mm" if h["precip"] and h["precip"] > 0 else "0.0mm"
        icon = condition_icon(h["condition"])
        print(f"  {h['hour']:>02d}:00  {h['temp']:>5.1f}°C  {pres:>8}  {h['humid']:>3.0f}%  {prec:>7}  {icon} {h['condition']:<23}")

    now_h = datetime.datetime.now().hour
    now_entry = next((h for h in today_hours if h["hour"] == now_h), today_hours[0] if today_hours else None)

    t_now = now_entry["temp"] if now_entry else 0
    h_now = now_entry["humid"] if now_entry else 0
    p_now = now_entry["pressure"] if now_entry and now_entry["pressure"] is not None else 0

    t_min = min(temps)
    t_max = max(temps)
    t_mean = sum(temps) / len(temps)
    h_min = min(humids)
    h_max = max(humids)
    h_mean = sum(humids) / len(humids)
    p_min = min(pressures) if pressures else 0
    p_max = max(pressures) if pressures else 0
    p_mean = sum(pressures) / len(pressures) if pressures else 0
    dominant = conditions.most_common(1)[0][0]

    t_min_h = next((h["hour"] for h in today_hours if h["temp"] == t_min), "??")
    t_max_h = next((h["hour"] for h in today_hours if h["temp"] == t_max), "??")
    h_min_h = next((h["hour"] for h in today_hours if h["humid"] == h_min), "??")
    h_max_h = next((h["hour"] for h in today_hours if h["humid"] == h_max), "??")
    p_min_h = next((h["hour"] for h in today_hours if h["pressure"] == p_min), "??")
    p_max_h = next((h["hour"] for h in today_hours if h["pressure"] == p_max), "??")

    trend_slope = (temps[-1] - temps[0]) / len(temps) if len(temps) > 1 else 0
    if trend_slope > 1.5:
        temp_trend = "CALENTAMIENTO RÁPIDO"
    elif trend_slope > 0.5:
        temp_trend = "CALENTAMIENTO"
    elif trend_slope < -1.5:
        temp_trend = "ENFRIAMIENTO RÁPIDO"
    elif trend_slope < -0.5:
        temp_trend = "ENFRIAMIENTO"
    else:
        temp_trend = "ESTABLE"

    print()
    print(f"  Resumen del día:")
    print(f"  ─────────────────────────────────────────────────")
    print(f"  Temperatura")
    print(f"    Actual:  {t_now:.1f}°C ({now_h:02d}:00)")
    print(f"    Máxima:  {t_max:.1f}°C ({t_max_h:02d}:00)")
    print(f"    Mínima:  {t_min:.1f}°C ({t_min_h:02d}:00)")
    print(f"    Media:   {t_mean:.1f}°C")
    print(f"  Humedad")
    print(f"    Actual:  {h_now:.0f}% ({now_h:02d}:00)")
    print(f"    Máxima:  {h_max:.0f}% ({h_max_h:02d}:00)")
    print(f"    Mínima:  {h_min:.0f}% ({h_min_h:02d}:00)")
    print(f"    Media:   {h_mean:.0f}%")
    if pressures:
        print(f"  Presión")
        print(f"    Actual:  {p_now:.0f} hPa ({now_h:02d}:00)")
        print(f"    Máxima:  {p_max:.0f} hPa ({p_max_h:02d}:00)")
        print(f"    Mínima:  {p_min:.0f} hPa ({p_min_h:02d}:00)")
        print(f"    Media:   {p_mean:.0f} hPa")
    print(f"  Estado dom.:  {condition_icon(dominant)} {dominant}")
    print(f"  Tendencia:    {temp_trend} ({trend_slope:+.2f}°C/h)")
    print(f"  Normal julio: {ciudad['t_mean']}°C | Anomalía: {t_mean - ciudad['t_mean']:+.1f}°C")

    if hist["heatwave"]:
        has_rain_today = any(h["precip"] and h["precip"] > 0 for h in today_hours)
        if has_rain_today:
            print(f"  ⚠ OLA DE CALOR INTERRUMPIDA — lluvias en el día de hoy")
        else:
            print(f"  ⚠ OLA DE CALOR: {hist['dry_streak']} días secos consecutivos")
    if any("Tormenta" in cond for cond in conditions):
        night_hours = [h["hour"] for h in today_hours if "Tormenta" in h["condition"] and 0 <= h["hour"] <= 5]
        if night_hours:
            print(f"  ⛈ Tormentas nocturnas (alta incertidumbre, verificar radar)")
        else:
            print(f"  ⛈ Posibles tormentas durante el día")
    if h_mean >= 70:
        print(f"  💧 Alta humedad: sensación de bochorno")
    if t_max >= 37:
        print(f"  🔥 Calor extremo: evitar sol directo 11-17h")
    if any(h["precip"] and h["precip"] > 0 for h in today_hours):
        print(f"  ☂ Lluvias previstas: llevar paraguas")

    print()
    print(f"  Fuente: Open-Meteo | Actualizado: {datetime.datetime.now().strftime('%H:%M')} CEST")
    print()
    return 0


def run(ciudad_nombre, days, show_detail=False, resumen=False):
    """Main analysis and forecast generator."""
    ciudad = CIUDADES[ciudad_nombre]
    t_mean_norm = ciudad["t_mean"]
    precip_norm = ciudad["precip"]

    dow_map = {
        "Monday": "Lun", "Tuesday": "Mar", "Wednesday": "Mié",
        "Thursday": "Jue", "Friday": "Vie", "Saturday": "Sáb", "Sunday": "Dom"
    }
    dow_map_long = {
        "Monday": "LUN", "Tuesday": "MAR", "Wednesday": "MIÉ",
        "Thursday": "JUE", "Friday": "VIE", "Saturday": "SÁB", "Sunday": "DOM"
    }

    print_header(f"PRONÓSTICO METEOROLÓGICO - {ciudad_display_name(ciudad_nombre)}")
    print(f"  Coordenadas: {ciudad['lat']:.2f}°{ 'N' if ciudad['lat']>0 else 'S' }, "
          f"{ciudad['lon']:.2f}°{ 'E' if ciudad['lon']>0 else 'O' } | "
          f"Altitud: {ciudad['alt']}m")
    print(f"  Generado: {datetime.datetime.now().strftime('%Y-%m-%d %H:%M')} CEST")
    print(f"  Período: {days} día(s) | Normal julio: {t_mean_norm}°C / {precip_norm}mm")
    print()

    raw = fetch_openmeteo(ciudad, days)
    if not raw:
        print("\n  ERROR: No se pudieron obtener datos. Verifique conexión.", file=sys.stderr)
        return 1

    hist = analyze_historical(raw, ciudad)
    forecast = build_forecast(raw, days)
    if not forecast:
        print("  ERROR: No se pudieron procesar los datos.", file=sys.stderr)
        return 1

    hourly_data = forecast["hourly"]
    forecast_dates = forecast["forecast_dates"]

    # =====================================================================
    # SECTION 1: Historical observed data
    # =====================================================================
    if not resumen:
        print_header("1. DATOS OBSERVADOS")

        if hist["n_days"] > 0:
            print(f"\n  Días con datos: {hist['n_days']}")
            print(f"  Temperatura media observada: {hist['mean_obs']:.1f}°C")
            print(f"  Temperatura normal climática: {hist['mean_normal']:.1f}°C")
            print(f"  Anomalía térmica: {hist['mean_anomaly']:+.1f}°C")
            print(f"  Precipitación total acumulada: {hist['total_precip']:.1f} mm")
            print()
            print(f"  Tendencia reciente: {hist['trend']}")
            print(f"  Rachas secas consecutivas: {hist['dry_streak']} día(s)")
            if hist["heatwave"]:
                print(f"  ⚠ OLA DE CALOR ACTIVA: {hist['dry_streak']} días secos + anomalía >+3°C")

            print()
            print(f"  {'Fecha':<14} {'T.Min':>7} {'T.Max':>7} {'T.Media':>8} {'Precip':>7} {'Desvío':>7}")
            print(f"  {'-'*14} {'-'*7} {'-'*7} {'-'*8} {'-'*7} {'-'*7}")

        daily = raw.get("daily", {})
        for i, d in enumerate(daily.get("time", [])):
            dt = datetime.date.fromisoformat(d)
            if dt >= datetime.date.today():
                break
            tm = daily["temperature_2m_min"][i]
            tx = daily["temperature_2m_max"][i]
            pp = daily["precipitation_sum"][i] if daily["precipitation_sum"][i] else 0
            tmean = (tm + tx) / 2 if tm and tx else 0
            anomaly = tmean - t_mean_norm
            print(f"  {d:<14} {tm if tm else 'N/A':>7} {tx if tx else 'N/A':>7} "
                  f"{tmean:>7.1f}°C  {pp if pp else 0:>6.1f}mm {anomaly:>+6.1f}°C")

    # =====================================================================
    # SECTION 2: Summary table
    # =====================================================================
    print_header(f"{'1' if resumen else '2'}. TABLA RESUMEN - PRÓXIMOS DÍAS")

    print(f"  {'Día':<14} {'T.Min (h)':<14} {'T.Max (h)':<14} "
          f"{'H.min (h)':<14} {'H.max (h)':<14} {'Lluvia':>8} {'Condición':<30}")
    print(f"  {'-'*14} {'-'*14} {'-'*14} {'-'*14} {'-'*14} {'-'*8} {'-'*30}")

    daily_stats = []
    for fd in forecast_dates:
        ds = compute_daily_stats(hourly_data, fd["date"])
        if ds:
            daily_stats.append((fd, ds))
            dt = datetime.date.fromisoformat(fd["date"])
            label = f"{dow_map.get(fd['dow'], fd['dow'][:3])} {dt.day} {dt.strftime('%b')}"
            icon = condition_icon(ds["condition"])
            flag = " 🔥" if ds["t_max"] >= 38 else ""
            print(f"  {label:<14} {ds['t_min']:>5.1f}°C ({ds['t_min_hour']})  "
                  f"{ds['t_max']:>5.1f}°C ({ds['t_max_hour']})  "
                  f"{ds['h_min']:>3.0f}% ({ds['h_min_hour']})  "
                  f"{ds['h_max']:>3.0f}% ({ds['h_max_hour']})  "
                  f"{ds['precip']:>6.2f}mm  {icon} {ds['condition']}{flag}")

    print()
    print(f"  🔥 = T ≥ 38°C")

    # =====================================================================
    # SECTION 3: Anomaly analysis
    # =====================================================================
    if not resumen:
        print_header("3. ANÁLISIS DE ANOMALÍAS Y TENDENCIA")

        if hist["n_days"] > 0:
            print(f"\n  Temperatura media observada (histórico): {hist['mean_obs']:.1f}°C")
            print(f"  Temperatura normal climática (julio):   {hist['mean_normal']:.1f}°C")
            print(f"  Anomalía:                              {hist['mean_anomaly']:+.1f}°C")

            if hist["heatwave"]:
                print(f"\n  ⚠ OLA DE CALOR ACTIVA: {hist['dry_streak']} días sin precipitación")
                print(f"     con anomalía sostenida >+3°C sobre la normal histórica")
            else:
                print(f"\n  Sin condiciones de ola de calor detectadas.")

            trend_labels = {
                "rapid_warming": "CALENTAMIENTO RÁPIDO (>1.5°C/3días)",
                "warming": "CALENTAMIENTO MODERADO",
                "cooling": "ENFRIAMIENTO",
                "rapid_cooling": "ENFRIAMIENTO RÁPIDO",
                "stable": "ESTABLE",
            }
            print(f"  Tendencia: {trend_labels.get(hist['trend'], hist['trend'])}")

        print(f"\n  Anomalías proyectadas por día (normal julio: {t_mean_norm}°C):")
        print(f"  {'Día':<14} {'T.Media':>9} {'Normal':>8} {'Anomalía':>9} {'Lluvia':>8}")
        print(f"  {'-'*14} {'-'*9} {'-'*8} {'-'*9} {'-'*8}")
        for fd, ds in daily_stats:
            dt = datetime.date.fromisoformat(fd["date"])
            label = f"{dow_map.get(fd['dow'], fd['dow'][:3])} {dt.day}"
            anomaly = ds["t_mean"] - t_mean_norm
            print(f"  {label:<14} {ds['t_mean']:>7.1f}°C  {t_mean_norm:>6.1f}°C  "
                  f"{anomaly:>+7.1f}°C  {fd['precip_sum']:>6.1f}mm")

    # =====================================================================
    # SECTION 4: Mosquito risk index
    # =====================================================================
    mr = compute_mosquito_risk(forecast_dates, daily_stats, hist)

    if not resumen:
        print_header("4. ÍNDICE DE RIESGO DE ZANCUDOS")

        level_labels = {"ALTO": "🔴 ALTO", "MEDIO": "🟡 MEDIO", "BAJO": "🟢 BAJO", "MUY BAJO": "⚪ MUY BAJO"}
        print(f"\n  Riesgo promedio período: {mr['avg_risk']:.1f}/10  ({level_labels.get(mosquito_level(mr['avg_risk']), '?')})")
        print(f"  Tendencia: {mr['trend']}")
        print(f"  Pico de riesgo: día {mr['peak_day'] + 1} ({mr['peak_risk']:.1f}/10)")

        print(f"\n  {'Día':<14} {'Riesgo':>8} {'Nivel':<14} {'T.Media':>8} {'H.Media':>8} {'Lluvia':>8}")
        print(f"  {'-'*14} {'-'*8} {'-'*14} {'-'*8} {'-'*8} {'-'*8}")
        for i, (fd, ds) in enumerate(daily_stats):
            risk = mr["daily_risks"][i]
            dt = datetime.date.fromisoformat(fd["date"])
            label = f"{dow_map.get(fd['dow'], fd['dow'][:3])} {dt.day}"
            h_mean = (ds["h_min"] + ds["h_max"]) / 2
            print(f"  {label:<14} {risk:>5.1f}/10  {level_labels.get(mosquito_level(risk), '?'):<14} "
                  f"{ds['t_mean']:>6.1f}°C  {h_mean:>5.0f}%  {fd['precip_sum']:>5.1f}mm")

        trend_icons = {"aumentando": "⬆", "disminuyendo": "⬇", "estable": "➡"}
        print(f"\n  {trend_icons.get(mr['trend'], '➡')} Tendencia: {mr['trend'].upper()}")
        print(f"  🦟 Riesgo basado en: temperatura (45%), humedad (35%), precipitación (20%)")
        print(f"  ℹ Ciclo óptimo: T 25-30°C, humedad >70%, agua estancada post-lluvia")

    # =====================================================================
    # SECTION 5: Storm risk analysis
    # =====================================================================
    sr = analyze_convective_risk(hourly_data, forecast_dates)

    if sr["has_any_storm"] and not resumen:
        print_header("5. ANÁLISIS DE TORMENTAS CONVECTIVAS")

        storm_icons = {"moderada": "⛈", "fuerte": "⛈", "severa": "🌪", "leve": "⛈"}
        for fd, ds in daily_stats:
            d = sr["daily"].get(fd["date"])
            if d and d["has_storm"]:
                icon = storm_icons.get(d["severity"], "⛈")
                hours_str = ", ".join(f"{h:02d}:00" for h in d["hours"])
                night_tag = " 🌙" if d["night_storm"] else ""
                print(f"  {icon} {fd['date']}: Tormenta {d['severity']} ({d['conv_precip']:.1f}mm) a las {hours_str}{night_tag}")

        if sr["night_storm_days"] > 0:
            print(f"\n  🌙 Tormentas nocturnas ({sr['night_storm_days']} día(s)): difíciles de predecir con >12h de antelación")
        if sr["severe_days"] > 0:
            print(f"  ⚠ Tormentas fuertes/severas: posible granizo, ráfagas y actividad eléctrica intensa")

        print(f"\n  ℹ Las tormentas convectivas son eventos localizados y de alta incertidumbre.")
        print(f"     El modelo puede subestimar su intensidad si faltan datos de CAPE/CIN.")
        print(f"     Se recomienda verificar fuentes locales (protección civil, radar meteorológico).")

    # =====================================================================
    # SECTION 6: Executive summary
    # =====================================================================
    print_header(f"{'2' if resumen else '6'}. RESUMEN EJECUTIVO")

    total_precip_proj = sum(fd["precip_sum"] for fd in forecast_dates)
    mean_temps = [ds["t_mean"] for _, ds in daily_stats]
    mean_forecast_temp = sum(mean_temps) / len(mean_temps) if mean_temps else 0
    max_temps = [ds["t_max"] for _, ds in daily_stats]
    overall_max = max(max_temps) if max_temps else 0
    min_temps = [ds["t_min"] for _, ds in daily_stats]
    overall_min = min(min_temps) if min_temps else 0

    print(f"\n  Ciudad: {ciudad_nombre} ({ciudad['lat']:.2f}°N, {ciudad['lon']:.2f}°E, {ciudad['alt']}m snm)")
    print(f"  Período: {days} día(s) | Generado: {datetime.datetime.now().strftime('%Y-%m-%d %H:%M')} CEST")
    print(f"  Fuentes: Open-Meteo (DWD/MeteoFrance/MetOffice), datos observados, normales climáticas")
    print(f"  Método: Integración multi-modelo + análisis estadístico de anomalías")

    print(f"\n  Temperatura promedio período:  {mean_forecast_temp:>5.1f}°C")
    print(f"  Temperatura máxima absoluta:   {overall_max:>5.1f}°C")
    print(f"  Temperatura mínima absoluta:   {overall_min:>5.1f}°C")
    print(f"  Precipitación total estimada:  {total_precip_proj:>5.1f} mm")

    if total_precip_proj == 0:
        print(f"  🏜 Sin precipitación prevista")
    elif total_precip_proj < 2:
        print(f"  🌦 Lluvias ligeras aisladas")
    elif total_precip_proj < 10:
        print(f"  🌧 Lluvias moderadas")
    else:
        print(f"  ⛈ Precipitación significativa")

    if hist["heatwave"] and total_precip_proj > 0:
        print(f"  ⚠ OLA DE CALOR INTERRUMPIDA ({hist['dry_streak']} días secos hasta ayer — lluvias previstas en el período)")
    elif hist["heatwave"]:
        print(f"  ⚠ OLA DE CALOR activa ({hist['dry_streak']} días secos)")

    trend_icons = {"aumentando": "⬆", "disminuyendo": "⬇", "estable": "➡"}
    mosq_label = f"{mosquito_level(mr['avg_risk'])} {trend_icons.get(mr['trend'], '➡')}"
    print(f"  🦟 Riesgo zancudos: {mr['avg_risk']:.1f}/10 ({mosq_label})")
    if sr["has_any_storm"]:
        severe_tag = " 🌩" if sr["severe_days"] > 0 else ""
        night_tag = " 🌙" if sr["night_storm_days"] > 0 else ""
        print(f"  ⛈ Tormentas previstas ({sr['severe_days']} fuerte(s), {sr['night_storm_days']} nocturna(s)){severe_tag}{night_tag}")

    print(f"\n  Recomendaciones:")
    if sr["has_any_storm"]:
        print(f"    ⛈ Tormentas: evitar exteriores durante actividad eléctrica, buscar refugio")
        if sr["night_storm_days"] > 0:
            print(f"    🌙 Tormentas nocturnas: asegurar objetos sueltos en balcones/terrazas")
    if any(ds["t_max"] >= 37 for _, ds in daily_stats):
        print(f"    🔥 Días de calor extremo: mantenerse hidratado, evitar sol directo 11-17h")
    if total_precip_proj > 0:
        print(f"    ☂  Llevar paraguas día(s) con precipitación prevista")
    if any(ds["t_max"] >= 35 for _, ds in daily_stats):
        print(f"    🧴 Protección solar SPF 50+ obligatoria, sombrero, gafas de sol")
    if any(ds["h_max"] >= 80 for _, ds in daily_stats):
        print(f"    💧 Alta humedad: posible sensación de bochorno")
    if mr["avg_risk"] >= 4:
        print(f"    🦟 Riesgo zancudos MEDIO+ : usar repelente, evitar agua estancada")
    print(f"    🌬 Ventilar durante la noche/madrugada para refrescar viviendas")

    hottest = max(daily_stats, key=lambda x: x[1]["t_max"]) if daily_stats else None
    if hottest:
        fd, ds = hottest
        print(f"\n  ⚠ Pico de calor: {ds['t_max']:.0f}°C el {fd['date']} a las {ds['t_max_hour']}")

    print(f"\n  {'─' * 78}")
    print(f"  Datos: Open-Meteo API (libre, sin API key) | Modelos: ECMWF/GFS/ICON")
    print(f"  Próxima actualización recomendada: en +6h para mejor precisión")

    # =====================================================================
    # SECTION 7: Hourly detail (optional, with --detalle)
    # =====================================================================
    if show_detail:
        print_header("7. PREDICCIÓN HORARIA DETALLADA (--extendido / -e)")

        current_date = None
        row_count = 0
        for h in hourly_data:
            if h["date"] < datetime.date.today().isoformat():
                continue
            if row_count >= days * 24:
                break

            if h["date"] != current_date:
                current_date = h["date"]
                dt = datetime.date.fromisoformat(current_date)
                weekday = dow_map_long.get(dt.strftime("%A"), dt.strftime("%A").upper())
                print(f"\n  ── {weekday} {dt.day} {dt.strftime('%b %Y')} ──")

            precip_val = h["precip"] if h["precip"] else 0
            if precip_val >= 0.1:
                precip_str = f"{precip_val:.1f}"
            elif h["precip_prob"] and h["precip_prob"] >= 50:
                precip_str = f"~{precip_val:.1f}"
            else:
                precip_str = "0.0"

            icon = condition_icon(h["condition"])
            print(f"  {h['iso']}  {h['temp']:>5.1f}°C  {h['humid']:>3.0f}%  "
                  f"{precip_str:>5}mm  {icon} {h['condition']:<22}")
            row_count += 1

        print(f"\n  Total: {row_count} registros horarios")

    print()
    return 0


def main():
    parser = argparse.ArgumentParser(
        description="Pronóstico meteorológico para ciudades de Italia",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Ejemplos:
  python forecast.py                              # Torino 5 días
  python forecast.py -c ROMA                     # Roma 5 días
  python forecast.py --today                      # hoy Torino (hora por hora)
  python forecast.py -t -c NAPOLI                # hoy Napoli
  python forecast.py -c MILANO -d 10             # Milano 10 días
  python forecast.py -c NAPOLI -e               # Napoli + detalle horario
   python forecast.py -l                           # listar ciudades
   python forecast.py -r                           # solo resumen
   python forecast.py -r -c MILANO -d 10          # resumen Milano 10 días
        """,
    )
    parser.add_argument("--ciudad", "-c", type=str, default=CIUDAD_DEFAULT,
                        choices=sorted(CIUDADES.keys()),
                        help=f"Ciudad (default: {CIUDAD_DEFAULT}). Ver -l / --list")
    parser.add_argument("--dias", "-d", type=int, default=5, help="Días de pronóstico (default: 5, máx: 16)")
    parser.add_argument("--extendido", "-e", action="store_true", help="Incluir predicción horaria detallada")
    parser.add_argument("--list", "-l", action="store_true", help="Listar todas las ciudades disponibles y salir")
    parser.add_argument("--today", "-t", action="store_true", help="Mostrar datos de hoy (temperatura, presión, estado, tendencia)")
    parser.add_argument("--resumen", "-r", action="store_true", help="Mostrar solo tabla resumen y resumen ejecutivo")
    args = parser.parse_args()

    if args.list:
        list_ciudades()
        sys.exit(0)

    if args.today:
        ret = today_report(args.ciudad.upper())
        sys.exit(ret)

    if args.dias < 1 or args.dias > 16:
        print("ERROR: dias debe estar entre 1 y 16 (límite Open-Meteo gratuito)")
        sys.exit(1)

    ret = run(args.ciudad.upper(), args.dias, show_detail=args.extendido, resumen=args.resumen)
    sys.exit(ret)


if __name__ == "__main__":
    main()
