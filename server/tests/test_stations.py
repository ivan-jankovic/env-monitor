from datetime import UTC, datetime, timedelta

from app import create_app


def get_station(client, auth):
    response = client.get("/api/stations", headers=auth)
    assert response.status_code == 200
    (station,) = response.get_json()["stations"]
    return station


def test_requires_token(client):
    assert client.get("/api/stations").status_code == 401


def test_never_seen_station_is_offline(client, auth, db):
    assert get_station(client, auth) == {
        "id": "esp32-01",
        "name": "Storage room demo",
        "last_seen": None,
        "online": False,
    }


def test_recently_seen_station_is_online(client, auth, db):
    # Default interval is 10 s, so offline after 30 s
    db.execute("UPDATE stations SET last_seen = now() - interval '20 seconds'")

    station = get_station(client, auth)

    assert station["online"] is True
    assert station["last_seen"].endswith("Z")
    last_seen = datetime.fromisoformat(station["last_seen"])
    assert datetime.now(UTC) - last_seen < timedelta(seconds=30)


def test_station_is_offline_after_three_missed_readings(client, auth, db):
    db.execute("UPDATE stations SET last_seen = now() - interval '40 seconds'")

    assert get_station(client, auth)["online"] is False


def test_offline_timeout_follows_reading_interval(app, auth, db):
    app.config["READING_INTERVAL_SECONDS"] = 60
    db.execute("UPDATE stations SET last_seen = now() - interval '2 minutes'")

    assert get_station(app.test_client(), auth)["online"] is True


def test_reading_interval_is_read_from_environment(monkeypatch):
    monkeypatch.setenv("READING_INTERVAL_SECONDS", "15")
    app = create_app({"API_TOKEN": "t", "DATABASE_URL": "postgresql://unused"})

    assert app.config["READING_INTERVAL_SECONDS"] == 15
