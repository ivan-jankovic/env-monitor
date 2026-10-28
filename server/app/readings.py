import math
from datetime import UTC, datetime, timedelta

from flask import Blueprint, request

from .auth import require_token
from .db import get_db

bp = Blueprint("readings", __name__)

SENSORS = {"temperature", "humidity"}
MAX_BATCH = 1000
MAX_CLOCK_SKEW = timedelta(minutes=5)
# Anything older means the device clock was never set (NTP failed, clock starts at 1970).
EARLIEST_TS = datetime(2020, 1, 1, tzinfo=UTC)


def parse_ts(raw, now):
    """Parse an ISO 8601 timestamp with a timezone and return it in UTC."""
    if not isinstance(raw, str):
        raise ValueError("must be an ISO 8601 string, e.g. 2026-10-26T14:03:10Z")
    try:
        ts = datetime.fromisoformat(raw)
    except ValueError:
        raise ValueError("must be an ISO 8601 string, e.g. 2026-10-26T14:03:10Z") from None
    if ts.tzinfo is None:
        raise ValueError("must include a timezone, e.g. a trailing Z")
    ts = ts.astimezone(UTC)
    if ts < EARLIEST_TS:
        raise ValueError("is before 2020, the device clock is probably not set")
    if ts > now + MAX_CLOCK_SKEW:
        raise ValueError("is in the future")
    return ts


def parse_value(raw):
    # bool is a subclass of int, so reject it explicitly
    if isinstance(raw, bool) or not isinstance(raw, (int, float)):
        raise ValueError("must be a number")
    try:
        value = float(raw)
    except OverflowError:
        raise ValueError("must be a finite number") from None
    if not math.isfinite(value):
        raise ValueError("must be a finite number")
    return value


def validate_batch(body, now):
    """Return (station_id, [(sensor, value, ts), ...], errors)."""
    if not isinstance(body, dict):
        return None, [], ["body must be a JSON object (Content-Type: application/json)"]

    errors = []
    station_id = body.get("station_id")
    if not isinstance(station_id, str) or not station_id.strip():
        errors.append("station_id: must be a non-empty string")

    readings = body.get("readings")
    if not isinstance(readings, list) or not readings:
        return station_id, [], errors + ["readings: must be a non-empty list"]
    if len(readings) > MAX_BATCH:
        return station_id, [], errors + [f"readings: at most {MAX_BATCH} per batch"]

    parsed = []
    for i, reading in enumerate(readings):
        where = f"readings[{i}]"
        errors_before = len(errors)
        if not isinstance(reading, dict):
            errors.append(f"{where}: must be an object")
            continue

        sensor = reading.get("sensor")
        if not isinstance(sensor, str) or sensor not in SENSORS:
            errors.append(f"{where}.sensor: must be one of {', '.join(sorted(SENSORS))}")

        try:
            value = parse_value(reading.get("value"))
        except ValueError as e:
            errors.append(f"{where}.value: {e}")

        try:
            ts = parse_ts(reading.get("ts"), now)
        except ValueError as e:
            errors.append(f"{where}.ts: {e}")

        if len(errors) == errors_before:
            parsed.append((sensor, value, ts))

    return station_id, parsed, errors


@bp.post("/api/readings")
@require_token
def post_readings():
    station_id, readings, errors = validate_batch(
        request.get_json(silent=True), datetime.now(UTC)
    )
    if errors:
        return {"error": "invalid batch", "details": errors}, 400

    # Oldest first: buffered batches arrive late, and alarm evaluation will depend on order.
    readings.sort(key=lambda r: r[2])

    conn = get_db()
    with conn.transaction():
        cur = conn.execute(
            "UPDATE stations SET last_seen = now() WHERE id = %s", (station_id,)
        )
        if cur.rowcount == 0:
            return {"error": f"unknown station_id: {station_id}"}, 422

        with conn.cursor() as cur:
            cur.executemany(
                "INSERT INTO readings (station_id, sensor, value, ts)"
                " VALUES (%s, %s, %s, %s) ON CONFLICT DO NOTHING",
                [(station_id, *r) for r in readings],
            )
            accepted = cur.rowcount

    return {"accepted": accepted, "duplicates": len(readings) - accepted}
