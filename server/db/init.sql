-- Runs only when the database volume is first created.
-- After changing it: docker compose down -v (deletes all data).

CREATE TABLE stations (
    id         TEXT PRIMARY KEY,
    name       TEXT NOT NULL,
    last_seen  TIMESTAMPTZ
);

CREATE TABLE readings (
    id          BIGSERIAL PRIMARY KEY,
    station_id  TEXT NOT NULL REFERENCES stations(id),
    sensor      TEXT NOT NULL,          -- 'temperature', 'humidity'
    value       DOUBLE PRECISION NOT NULL,
    ts          TIMESTAMPTZ NOT NULL,   -- time of measurement (from the device)
    received_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    UNIQUE (station_id, sensor, ts)     -- makes retries idempotent
);
CREATE INDEX idx_readings_lookup ON readings (station_id, sensor, ts DESC);

CREATE TABLE limits (
    sensor      TEXT PRIMARY KEY,
    min_val     DOUBLE PRECISION,
    max_val     DOUBLE PRECISION,
    hysteresis  DOUBLE PRECISION NOT NULL DEFAULT 0   -- alarm clears only this far inside the limit
);

CREATE TABLE alarms (
    id          BIGSERIAL PRIMARY KEY,
    station_id  TEXT NOT NULL REFERENCES stations(id),
    sensor      TEXT NOT NULL,
    value       DOUBLE PRECISION NOT NULL,
    ts          TIMESTAMPTZ NOT NULL,
    cleared_at  TIMESTAMPTZ
);
-- at most one open alarm per station and sensor
CREATE UNIQUE INDEX one_open_alarm ON alarms (station_id, sensor) WHERE cleared_at IS NULL;

INSERT INTO stations VALUES ('esp32-01', 'Storage room demo', NULL);
INSERT INTO limits VALUES ('temperature', 15, 30, 1), ('humidity', 10, 50, 2);
