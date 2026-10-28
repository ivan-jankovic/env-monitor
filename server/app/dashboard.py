from datetime import UTC, datetime, timedelta

from flask import Blueprint, abort, current_app, render_template, request

from .db import get_db
from .limits import load_limits
from .stations import list_stations
from .timeutil import iso_utc

# Open, read-only page: it never receives the API token, and the /api/*
# routes stay token-protected. Authentication is a documented production gap.
bp = Blueprint("dashboard", __name__)

SENSORS = {"temperature": ("Temperature", "°C"), "humidity": ("Humidity", "%")}
RANGES_HOURS = (1, 6, 24)
MAX_POINTS = 720  # per line; longer ranges are averaged into time buckets
RECENT_ALARMS = 10


@bp.app_template_filter("iso_utc")
def iso_utc_filter(dt):
    return iso_utc(dt)


def epoch_ms(dt):
    return int(dt.timestamp() * 1000)


def latest_values(conn):
    """{(station_id, sensor): (value, ts)}, one index lookup per pair."""
    rows = conn.execute(
        """
        SELECT st.id, se.sensor, r.value, r.ts
        FROM stations st
        CROSS JOIN unnest(%s::text[]) AS se(sensor)
        CROSS JOIN LATERAL (
            SELECT value, ts FROM readings
            WHERE station_id = st.id AND sensor = se.sensor
            ORDER BY ts DESC LIMIT 1
        ) r
        """,
        (list(SENSORS),),
    ).fetchall()
    return {(station_id, sensor): (value, ts) for station_id, sensor, value, ts in rows}


def series(conn, station_ids, since, bucket):
    """{sensor: {station_id: [[epoch_ms, avg_value], ...]}} averaged per bucket."""
    rows = conn.execute(
        """
        SELECT station_id, sensor,
               date_bin(%s, ts, TIMESTAMPTZ '2000-01-01 00:00Z') AS t, avg(value)
        FROM readings
        WHERE station_id = ANY(%s) AND sensor = ANY(%s) AND ts >= %s
        GROUP BY station_id, sensor, t
        ORDER BY t
        """,
        (bucket, station_ids, list(SENSORS), since),
    ).fetchall()
    result = {sensor: {} for sensor in SENSORS}
    for station_id, sensor, t, value in rows:
        result[sensor].setdefault(station_id, []).append([epoch_ms(t), round(value, 2)])
    return result


@bp.get("/")
def dashboard():
    allowed = {str(h): h for h in RANGES_HOURS}
    hours = allowed.get(request.args.get("hours", "1"))
    if hours is None:
        abort(400, f"hours must be one of {', '.join(allowed)}")

    conn = get_db()
    now = datetime.now(UTC)
    since = now - timedelta(hours=hours)
    interval = timedelta(seconds=current_app.config["READING_INTERVAL_SECONDS"])
    bucket = max(timedelta(hours=hours) / MAX_POINTS, interval)

    stations = list_stations(conn)
    limits = load_limits(conn)
    open_alarms = set(
        conn.execute(
            "SELECT station_id, sensor FROM alarms WHERE cleared_at IS NULL"
        ).fetchall()
    )
    recent_alarms = conn.execute(
        "SELECT station_id, sensor, value, ts, cleared_at FROM alarms"
        " ORDER BY ts DESC LIMIT %s",
        (RECENT_ALARMS,),
    ).fetchall()

    chart_data = {
        "since": epoch_ms(since),
        "until": epoch_ms(now),
        "bucket_ms": int(bucket.total_seconds() * 1000),
        "sensors": {
            sensor: {"label": label, "unit": unit, "limits": limits.get(sensor)}
            for sensor, (label, unit) in SENSORS.items()
        },
        "series": series(conn, [s["id"] for s in stations], since, bucket),
    }

    return render_template(
        "dashboard.html",
        hours=hours,
        ranges=RANGES_HOURS,
        now=now,
        refresh_seconds=current_app.config["READING_INTERVAL_SECONDS"],
        sensors=SENSORS,
        stations=stations,
        latest=latest_values(conn),
        open_alarms=open_alarms,
        recent_alarms=recent_alarms,
        chart_data=chart_data,
    )
