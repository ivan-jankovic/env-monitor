from datetime import UTC, datetime

import pytest

from app.alarms import is_clear, is_out_of_range

TEMP = {"min_val": 15, "max_val": 30, "hysteresis": 1}


# --- the rule itself ---


@pytest.mark.parametrize(
    "value, expected",
    [(30.1, True), (30, False), (14.9, True), (15, False), (22, False)],
)
def test_raises_only_outside_the_limits(value, expected):
    assert is_out_of_range(value, TEMP) is expected


@pytest.mark.parametrize(
    "value, expected",
    [
        (30.5, False),  # still above max
        (29.5, False),  # inside the limit but within the hysteresis band
        (29, True),  # at max - hysteresis
        (15.5, False),  # band on the low side
        (16, True),  # at min + hysteresis
        (10, False),  # jumped from too high to too low
    ],
)
def test_clears_only_past_the_hysteresis_band(value, expected):
    assert is_clear(value, TEMP) is expected


def test_missing_limit_is_ignored():
    only_max = {"min_val": None, "max_val": 50, "hysteresis": 2}

    assert is_out_of_range(-100, only_max) is False
    assert is_clear(-100, only_max) is True


# --- through POST /api/readings ---


def post(client, auth, *readings):
    body = {
        "station_id": "esp32-01",
        "readings": [
            {"sensor": "humidity", "value": value, "ts": f"2026-10-26T14:00:{second:02}Z"}
            for second, value in readings
        ],
    }
    response = client.post("/api/readings", json=body, headers=auth)
    assert response.status_code == 200, response.get_json()
    return response.get_json()


def alarms(db):
    return db.execute(
        "SELECT sensor, value, ts, cleared_at FROM alarms ORDER BY id"
    ).fetchall()


def at(second):
    return datetime(2026, 10, 26, 14, 0, second, tzinfo=UTC)


def test_out_of_range_reading_raises_alarm(client, auth, db):
    post(client, auth, (10, 41), (20, 55))

    assert alarms(db) == [("humidity", 55, at(20), None)]


def test_alarm_stays_open_inside_hysteresis_band(client, auth, db):
    post(client, auth, (10, 55), (20, 49))

    assert alarms(db) == [("humidity", 55, at(10), None)]


def test_alarm_clears_past_hysteresis_band(client, auth, db):
    post(client, auth, (10, 55), (20, 49), (30, 48))

    assert alarms(db) == [("humidity", 55, at(10), at(30))]


def test_never_more_than_one_open_alarm(client, auth, db):
    post(client, auth, (10, 55), (20, 60))
    post(client, auth, (30, 70))

    assert alarms(db) == [("humidity", 55, at(10), None)]


def test_new_alarm_after_clear(client, auth, db):
    post(client, auth, (10, 55), (20, 40), (30, 51))

    assert alarms(db) == [
        ("humidity", 55, at(10), at(20)),
        ("humidity", 51, at(30), None),
    ]


def test_batch_is_processed_in_timestamp_order(client, auth, db):
    # Sent out of order. In ts order: raise at :10, band at :20, clear at :30.
    # In the order sent, :30 would come first and the alarm would stay open.
    post(client, auth, (30, 40), (10, 55), (20, 49))

    assert alarms(db) == [("humidity", 55, at(10), at(30))]


def test_resent_reading_does_not_reraise_cleared_alarm(client, auth, db):
    post(client, auth, (10, 55), (20, 40))
    result = post(client, auth, (10, 55))

    assert result == {"accepted": 0, "duplicates": 1}
    assert alarms(db) == [("humidity", 55, at(10), at(20))]


def test_late_reading_older_than_alarm_does_not_clear_it(client, auth, db):
    post(client, auth, (20, 55))
    post(client, auth, (10, 40))

    assert alarms(db) == [("humidity", 55, at(20), None)]


def test_sensors_have_independent_alarms(client, auth, db):
    body = {
        "station_id": "esp32-01",
        "readings": [
            {"sensor": "temperature", "value": 31, "ts": "2026-10-26T14:00:10Z"},
            {"sensor": "humidity", "value": 55, "ts": "2026-10-26T14:00:10Z"},
            {"sensor": "humidity", "value": 40, "ts": "2026-10-26T14:00:20Z"},
        ],
    }
    client.post("/api/readings", json=body, headers=auth)

    assert alarms(db) == [
        ("temperature", 31, at(10), None),
        ("humidity", 55, at(10), at(20)),
    ]
