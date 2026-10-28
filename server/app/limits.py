from flask import Blueprint

from .auth import require_token
from .db import get_db

bp = Blueprint("limits", __name__)


def load_limits(conn):
    """Return {sensor: {"min_val", "max_val", "hysteresis"}}. min/max may be None."""
    rows = conn.execute(
        "SELECT sensor, min_val, max_val, hysteresis FROM limits ORDER BY sensor"
    ).fetchall()
    return {
        sensor: {"min_val": min_val, "max_val": max_val, "hysteresis": hysteresis}
        for sensor, min_val, max_val, hysteresis in rows
    }


@bp.get("/api/limits")
@require_token
def get_limits():
    return {"limits": load_limits(get_db())}
