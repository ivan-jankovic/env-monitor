import re
from datetime import UTC, datetime


def test_requires_token(client):
    assert client.get("/api/limits").status_code == 401


def test_returns_limits_from_database(client, auth, db):
    db.execute("UPDATE limits SET max_val = 28 WHERE sensor = 'temperature'")

    response = client.get("/api/limits", headers=auth)

    assert response.status_code == 200
    assert response.get_json()["limits"] == {
        "humidity": {"min_val": 10, "max_val": 50, "hysteresis": 2},
        "temperature": {"min_val": 15, "max_val": 28, "hysteresis": 1},
    }


def test_returns_server_time_in_utc(client, auth, db):
    before = datetime.now(UTC).replace(microsecond=0)
    server_time = client.get("/api/limits", headers=auth).get_json()["server_time"]
    after = datetime.now(UTC)

    assert re.fullmatch(r"\d{4}-\d\d-\d\dT\d\d:\d\d:\d\dZ", server_time)
    assert before <= datetime.fromisoformat(server_time) <= after
