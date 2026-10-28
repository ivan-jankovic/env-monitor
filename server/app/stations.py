from datetime import UTC, datetime, timedelta

from flask import Blueprint, current_app

from .auth import require_token
from .db import get_db
from .timeutil import iso_utc

bp = Blueprint("stations", __name__)

# A station is offline after missing this many readings in a row, so a single
# dropped send does not flag it.
OFFLINE_AFTER_MISSED = 3


def list_stations(conn):
    """Return stations as dicts with id, name, last_seen (datetime or None) and online."""
    now = datetime.now(UTC)
    offline_after = timedelta(
        seconds=OFFLINE_AFTER_MISSED * current_app.config["READING_INTERVAL_SECONDS"]
    )
    rows = conn.execute("SELECT id, name, last_seen FROM stations ORDER BY id").fetchall()
    return [
        {
            "id": station_id,
            "name": name,
            "last_seen": last_seen,
            "online": last_seen is not None and now - last_seen <= offline_after,
        }
        for station_id, name, last_seen in rows
    ]


@bp.get("/api/stations")
@require_token
def get_stations():
    return {
        "stations": [
            {**s, "last_seen": iso_utc(s["last_seen"]) if s["last_seen"] else None}
            for s in list_stations(get_db())
        ]
    }
