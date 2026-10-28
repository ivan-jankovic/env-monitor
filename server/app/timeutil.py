from datetime import UTC


def iso_utc(dt):
    """Format as 2026-10-26T14:03:10Z, the project's timestamp convention."""
    return dt.astimezone(UTC).isoformat(timespec="seconds").replace("+00:00", "Z")
