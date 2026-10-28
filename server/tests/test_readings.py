from datetime import UTC, datetime, timedelta

import pytest


def batch(*readings, station_id="esp32-01"):
    return {"station_id": station_id, "readings": list(readings)}


def reading(sensor="temperature", value=22.4, ts="2026-10-26T14:03:10Z"):
    return {"sensor": sensor, "value": value, "ts": ts}


# --- auth (no database needed: checked before any query) ---


@pytest.mark.parametrize(
    "headers",
    [
        {},
        {"Authorization": "Bearer wrong-token"},
        {"Authorization": "test-token"},
        {"Authorization": "Basic test-token"},
    ],
)
def test_rejects_missing_or_wrong_token(client, headers):
    response = client.post("/api/readings", json=batch(reading()), headers=headers)

    assert response.status_code == 401
    assert response.headers["WWW-Authenticate"] == "Bearer"
    assert "error" in response.get_json()


# --- validation (no database needed: checked before any query) ---


def test_rejects_non_json_body(client, auth):
    response = client.post("/api/readings", data="hello", headers=auth)

    assert response.status_code == 400
    assert "JSON object" in response.get_json()["details"][0]


@pytest.mark.parametrize(
    "body, expected",
    [
        ([], "body must be a JSON object"),
        ({"readings": [reading()]}, "station_id: must be a non-empty string"),
        (batch(), "readings: must be a non-empty list"),
        ({"station_id": "esp32-01", "readings": "x"}, "readings: must be a non-empty list"),
        (batch("x"), "readings[0]: must be an object"),
        (batch(reading(sensor="pressure")), "readings[0].sensor: must be one of"),
        (batch(reading(sensor=["temperature"])), "readings[0].sensor: must be one of"),
        (batch(reading(value="22.4")), "readings[0].value: must be a number"),
        (batch(reading(value=True)), "readings[0].value: must be a number"),
        (batch(reading(value=None)), "readings[0].value: must be a number"),
        (batch(reading(ts="yesterday")), "readings[0].ts: must be an ISO 8601"),
        (batch(reading(ts=1761487390)), "readings[0].ts: must be an ISO 8601"),
        (batch(reading(ts="2026-10-26T14:03:10")), "readings[0].ts: must include a timezone"),
        (batch(reading(ts="1970-01-01T00:00:12Z")), "readings[0].ts: is before 2020"),
    ],
)
def test_rejects_invalid_batch(client, auth, body, expected):
    response = client.post("/api/readings", json=body, headers=auth)

    assert response.status_code == 400
    data = response.get_json()
    assert data["error"] == "invalid batch"
    assert any(d.startswith(expected) for d in data["details"]), data["details"]


def test_rejects_non_finite_value(client, auth):
    # Python's JSON parser accepts NaN, so it must be caught explicitly
    body = '{"station_id": "esp32-01", "readings": [{"sensor": "humidity", "value": NaN, "ts": "2026-10-26T14:03:10Z"}]}'
    response = client.post(
        "/api/readings", data=body, content_type="application/json", headers=auth
    )

    assert response.status_code == 400
    assert response.get_json()["details"] == ["readings[0].value: must be a finite number"]


def test_rejects_timestamp_in_the_future(client, auth):
    future = (datetime.now(UTC) + timedelta(hours=1)).isoformat()
    response = client.post("/api/readings", json=batch(reading(ts=future)), headers=auth)

    assert response.status_code == 400
    assert response.get_json()["details"] == ["readings[0].ts: is in the future"]


def test_reports_every_invalid_reading(client, auth):
    body = batch(reading(), reading(sensor="pressure"), reading(value="x", ts="nope"))
    response = client.post("/api/readings", json=body, headers=auth)

    assert response.get_json()["details"] == [
        "readings[1].sensor: must be one of humidity, temperature",
        "readings[2].value: must be a number",
        "readings[2].ts: must be an ISO 8601 string, e.g. 2026-10-26T14:03:10Z",
    ]


def test_rejects_too_large_batch(client, auth):
    response = client.post("/api/readings", json=batch(*[reading()] * 1001), headers=auth)

    assert response.status_code == 400
    assert response.get_json()["details"] == ["readings: at most 1000 per batch"]


# --- storing readings (needs the test database) ---


def test_accepts_batch_and_stores_readings(client, auth, db):
    body = batch(reading("temperature", 22.4), reading("humidity", 41.2))
    response = client.post("/api/readings", json=body, headers=auth)

    assert response.status_code == 200
    assert response.get_json() == {"accepted": 2, "duplicates": 0}
    rows = db.execute(
        "SELECT sensor, value, ts FROM readings ORDER BY sensor"
    ).fetchall()
    ts = datetime(2026, 10, 26, 14, 3, 10, tzinfo=UTC)
    assert rows == [("humidity", 41.2, ts), ("temperature", 22.4, ts)]


def test_resent_batch_counts_duplicates(client, auth, db):
    first = batch(reading("temperature"), reading("humidity"))
    client.post("/api/readings", json=first, headers=auth)

    second = batch(reading("temperature"), reading("humidity"), reading(ts="2026-10-26T14:03:20Z"))
    response = client.post("/api/readings", json=second, headers=auth)

    assert response.get_json() == {"accepted": 1, "duplicates": 2}
    assert db.execute("SELECT count(*) FROM readings").fetchone() == (3,)


def test_duplicates_within_one_batch_are_stored_once(client, auth, db):
    response = client.post("/api/readings", json=batch(reading(), reading()), headers=auth)

    assert response.get_json() == {"accepted": 1, "duplicates": 1}


def test_timestamp_with_offset_is_stored_as_utc(client, auth, db):
    client.post("/api/readings", json=batch(reading(ts="2026-10-26T15:03:10+01:00")), headers=auth)

    (ts,) = db.execute("SELECT ts FROM readings").fetchone()
    assert ts == datetime(2026, 10, 26, 14, 3, 10, tzinfo=UTC)


def test_updates_station_last_seen(client, auth, db):
    before = datetime.now(UTC)
    client.post("/api/readings", json=batch(reading()), headers=auth)

    (last_seen,) = db.execute(
        "SELECT last_seen FROM stations WHERE id = 'esp32-01'"
    ).fetchone()
    assert last_seen >= before - timedelta(seconds=5)


def test_rejects_unknown_station(client, auth, db):
    response = client.post(
        "/api/readings", json=batch(reading(), station_id="esp32-99"), headers=auth
    )

    assert response.status_code == 422
    assert response.get_json() == {"error": "unknown station_id: esp32-99"}
    assert db.execute("SELECT count(*) FROM readings").fetchone() == (0,)
