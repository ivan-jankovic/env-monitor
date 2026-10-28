import json
import re

import pytest


def chart_data(response):
    match = re.search(
        r'<script id="chart-data" type="application/json">(.*?)</script>',
        response.get_data(as_text=True),
        re.S,
    )
    return json.loads(match.group(1))


def add_reading(db, sensor, value, age):
    db.execute(
        "INSERT INTO readings (station_id, sensor, value, ts)"
        " VALUES ('esp32-01', %s, %s, now() - %s::interval)",
        (sensor, value, age),
    )


def test_is_open_without_token(client, db):
    response = client.get("/")

    assert response.status_code == 200
    assert response.mimetype == "text/html"
    assert "Storage room demo" in response.get_data(as_text=True)


def test_never_receives_the_api_token(client, db):
    assert "test-token" not in client.get("/").get_data(as_text=True)


@pytest.mark.parametrize("hours", ["2", "abc", ""])
def test_rejects_unknown_range(client, db, hours):
    response = client.get(f"/?hours={hours}")

    assert response.status_code == 400
    assert response.get_json() == {"error": "hours must be one of 1, 6, 24"}


def test_shows_latest_value_per_sensor(client, db):
    add_reading(db, "temperature", 21.0, "30 seconds")
    add_reading(db, "temperature", 22.46, "10 seconds")

    html = client.get("/").get_data(as_text=True)

    assert "22.5 °C" in html
    assert "21.0 °C" not in html
    assert "No data" in html  # humidity has no readings yet


def test_marks_sensor_with_open_alarm(client, db):
    db.execute(
        "INSERT INTO alarms (station_id, sensor, value, ts)"
        " VALUES ('esp32-01', 'humidity', 55, now())"
    )

    html = client.get("/").get_data(as_text=True)

    assert 'class="card alarm"' in html
    assert "Humidity · alarm" in html
    assert "<strong>open</strong>" in html


def test_lists_only_recent_alarms(client, db):
    for minutes in range(12):
        db.execute(
            "INSERT INTO alarms (station_id, sensor, value, ts, cleared_at)"
            " VALUES ('esp32-01', 'humidity', %s, now() - %s::interval, now())",
            (50 + minutes, f"{minutes} minutes"),
        )

    html = client.get("/").get_data(as_text=True)

    assert html.count("<td><code>esp32-01</code></td>") == 10
    assert "50.0 %" in html  # newest
    assert "61.0 %" not in html  # oldest two are left out


def test_escapes_station_name(client, db):
    db.execute("UPDATE stations SET name = '<script>alert(1)</script>'")

    html = client.get("/").get_data(as_text=True)

    assert "<script>alert(1)</script>" not in html
    assert "&lt;script&gt;alert(1)&lt;/script&gt;" in html


def test_chart_data_includes_series_and_limits(client, db):
    add_reading(db, "humidity", 40.0, "5 minutes")

    data = chart_data(client.get("/"))

    assert data["sensors"]["humidity"] == {
        "label": "Humidity",
        "unit": "%",
        "limits": {"min_val": 10, "max_val": 50, "hysteresis": 2},
    }
    assert [p[1] for p in data["series"]["humidity"]["esp32-01"]] == [40.0]
    assert data["series"]["temperature"] == {}
    assert data["until"] - data["since"] == 3600 * 1000


def test_long_range_is_averaged_into_buckets(client, db):
    # 24 h / 720 points = 2 minute buckets. Readings 10 s apart, aligned to one bucket.
    db.execute(
        "INSERT INTO readings (station_id, sensor, value, ts) VALUES"
        " ('esp32-01', 'humidity', 40, date_bin('2 minutes', now() - interval '1 hour', TIMESTAMPTZ '2000-01-01 00:00Z')),"
        " ('esp32-01', 'humidity', 42, date_bin('2 minutes', now() - interval '1 hour', TIMESTAMPTZ '2000-01-01 00:00Z') + interval '10 seconds')"
    )

    data = chart_data(client.get("/?hours=24"))

    assert data["bucket_ms"] == 120_000
    assert [p[1] for p in data["series"]["humidity"]["esp32-01"]] == [41.0]


def test_short_range_keeps_every_reading(client, db):
    add_reading(db, "humidity", 40.0, "30 seconds")
    add_reading(db, "humidity", 42.0, "20 seconds")

    data = chart_data(client.get("/?hours=1"))

    assert data["bucket_ms"] == 10_000  # never finer than the reading interval
    assert [p[1] for p in data["series"]["humidity"]["esp32-01"]] == [40.0, 42.0]


def test_serves_vendored_chartjs(client):
    response = client.get("/static/vendor/chart-4.5.1.umd.min.js")

    assert response.status_code == 200
    assert b"Chart.js v4.5.1" in response.data[:100]
    response.close()
