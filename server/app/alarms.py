"""Alarm rule with hysteresis. The firmware's red LED follows the same rule."""

from .limits import load_limits


def is_out_of_range(value, limit):
    """Raise when strictly above max_val or below min_val."""
    max_val, min_val = limit["max_val"], limit["min_val"]
    return (max_val is not None and value > max_val) or (
        min_val is not None and value < min_val
    )


def is_clear(value, limit):
    """Clear only when back inside both limits by at least the hysteresis."""
    max_val, min_val, h = limit["max_val"], limit["min_val"], limit["hysteresis"]
    return (max_val is None or value <= max_val - h) and (
        min_val is None or value >= min_val + h
    )


def evaluate(conn, station_id, readings):
    """Raise or clear alarms for newly stored readings, given oldest first.

    Runs inside the caller's transaction, which holds the station row lock,
    so two batches from the same station cannot interleave here.
    """
    limits = load_limits(conn)
    open_since = dict(
        conn.execute(
            "SELECT sensor, ts FROM alarms WHERE station_id = %s AND cleared_at IS NULL",
            (station_id,),
        ).fetchall()
    )

    for sensor, value, ts in readings:
        limit = limits.get(sensor)
        if limit is None:
            continue
        if sensor not in open_since:
            if is_out_of_range(value, limit):
                conn.execute(
                    "INSERT INTO alarms (station_id, sensor, value, ts)"
                    " VALUES (%s, %s, %s, %s)",
                    (station_id, sensor, value, ts),
                )
                open_since[sensor] = ts
        # A late reading from before the alarm was raised must not clear it.
        elif ts >= open_since[sensor] and is_clear(value, limit):
            conn.execute(
                "UPDATE alarms SET cleared_at = %s"
                " WHERE station_id = %s AND sensor = %s AND cleared_at IS NULL",
                (ts, station_id, sensor),
            )
            del open_since[sensor]
