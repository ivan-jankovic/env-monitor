# Design

How env-monitor works in detail. For setup, wiring and an overview, see the [README](../README.md).

## Data flow

1. Every 10 seconds the firmware reads the DHT22, rounds both values to 0.1 (the sensor's resolution), and evaluates the alarm limits locally.
2. If the clock is set, the reading gets a UTC timestamp and goes into a RAM buffer.
3. The buffer is sent oldest first, up to 50 samples (100 readings) per request, to `POST /api/readings`. Normally that is a single sample, sent right away.
4. The API validates the batch, sorts it by timestamp, inserts it with `ON CONFLICT DO NOTHING`, updates the station's `last_seen`, and evaluates alarms for the newly inserted readings, all in one transaction.
5. The dashboard reads the database directly when it renders the page and fetches the page again every 10 seconds.

All timestamps are UTC, ISO 8601 with a `Z`, e.g. `2026-10-26T14:03:10Z`. The device's measurement time (`ts`) and the server's arrival time (`received_at`) are stored separately.

The reading interval appears in two places that must agree: `READING_INTERVAL_MS` in `firmware/include/config.h` and `READING_INTERVAL_SECONDS` in `docker-compose.yml`. The server uses it for the offline timeout and the dashboard's chart resolution.

## API

Every `/api/` endpoint except `/api/health` requires `Authorization: Bearer <API_TOKEN>`. Errors are JSON objects with an `error` field. Request bodies are limited to 1 MB.

### `POST /api/readings`

```json
{
  "station_id": "esp32-01",
  "readings": [
    {"sensor": "temperature", "value": 22.4, "ts": "2026-10-26T14:03:10Z"},
    {"sensor": "humidity", "value": 41.2, "ts": "2026-10-26T14:03:10Z"}
  ]
}
```

Validation:

- `station_id`: a non-empty string.
- `readings`: a list of 1 to 1000 readings.
- `sensor`: `temperature` or `humidity`.
- `value`: a finite number. Booleans are rejected.
- `ts`: an ISO 8601 string with a timezone, converted to UTC. It must not be before 2020: such a timestamp means the device clock was never set. It must not be more than 5 minutes in the future.

| Status | Body | When |
|---|---|---|
| 200 | `{"accepted": 2, "duplicates": 0}` | stored; duplicates were already in the database and were ignored |
| 400 | `{"error": "invalid batch", "details": ["readings[1].ts: is in the future"]}` | validation failed; nothing is stored |
| 401 | `{"error": "missing or invalid bearer token"}` | no token or a wrong one |
| 413 | `{"error": "..."}` | body over 1 MB |
| 422 | `{"error": "unknown station_id: esp32-99"}` | the station is not in the `stations` table |

### `GET /api/limits`

```json
{
  "limits": {
    "humidity": {"min_val": 10.0, "max_val": 50.0, "hysteresis": 2.0},
    "temperature": {"min_val": 15.0, "max_val": 30.0, "hysteresis": 1.0}
  },
  "server_time": "2026-10-29T11:05:18Z"
}
```

`min_val` and `max_val` may be `null`, which means no limit on that side. `server_time` is the firmware's clock fallback when NTP is unreachable (see [Clock](#clock)).

### `GET /api/stations`

```json
{"stations": [{"id": "esp32-01", "name": "Storage room demo",
               "last_seen": "2026-10-29T11:11:50Z", "online": true}]}
```

A station is offline after 3 missed readings: no request for 3 × `READING_INTERVAL_SECONDS` (30 s). `last_seen` is `null` until the first batch arrives.

### `GET /api/health`

`{"status": "ok"}`, without a token. Docker's healthcheck uses it.

### `GET /` (dashboard)

Open and read-only, so it never contains the API token. `?hours=1`, `6` or `24` selects the chart range; any other value returns 400. Longer ranges are averaged into time buckets of at least the reading interval, so each line has at most 720 points. Averaging can make short spikes look smaller on the 24 h chart, but they still appear in the alarm list. A gap of more than 3 buckets breaks the line instead of drawing across it. The page shows the last 10 alarms.

An earlier plan also included `GET /api/readings` for the charts. The dashboard renders its data on the server, so that endpoint was not built.

## Database

`server/db/init.sql` creates four tables and seeds one station and the default limits. It runs only when the database volume is first created. After changing it, `docker compose down -v` recreates the database, which deletes all data.

| Table | Purpose |
|---|---|
| `stations` | known stations; `last_seen` is updated on every accepted batch |
| `readings` | one row per sensor value; `UNIQUE (station_id, sensor, ts)` makes resends idempotent |
| `limits` | `min_val`, `max_val` and `hysteresis` per sensor; edit these rows to change the alarm limits |
| `alarms` | `ts` is when the alarm was raised, `cleared_at` is null while it is open; a partial unique index allows at most one open alarm per station and sensor |

All SQL is parameterized. Inserting a batch updates the station row first, which locks it for the transaction, so two batches from the same station cannot evaluate alarms at the same time.

## Alarm rule

Each sensor has `min_val`, `max_val` and `hysteresis`:

- **Raise** when a value is strictly above `max_val` or below `min_val`, and no alarm is open for that station and sensor.
- **Clear** only when the value is back inside both limits by at least the hysteresis: at or below `max_val − hysteresis` and at or above `min_val + hysteresis`. Inside that band an open alarm stays open, so a value hovering at the limit does not flip the alarm on and off.

Example, humidity with the seeded `max_val` 50 and `hysteresis` 2: 50.4 raises, 49 keeps the alarm open, 48 clears it.

The server evaluates only newly inserted readings, oldest first, so a resent duplicate cannot re-raise a cleared alarm. An alarm is cleared at the time of the reading that cleared it, and only by a reading newer than the alarm.

The firmware applies the same rule (`firmware/src/alarms.cpp`, mirroring `server/app/alarms.py`) to drive the red LED, also while offline. A comparison of both implementations over 2,804 values and four limit sets gave identical results.

## Firmware

PlatformIO project, Arduino framework, for a classic ESP32 (`esp32dev`). Libraries: Adafruit DHT sensor library, Adafruit Unified Sensor and ArduinoJson, with pinned versions in `platformio.ini`. All tunable values are in `include/config.h`.

| Module | Responsibility |
|---|---|
| `main.cpp` | schedule: a reading every 10 s or on a button press; LED state |
| `env_sensor` | the only code that knows the sensor is a DHT22: reads with retries, keeps 2 s between reads, counts failures |
| `wifi_link` | connects and retries, logging the WiFi stack's reason for each failure |
| `timekeeping` | UTC clock: NTP, with the server's clock as a fallback |
| `alarm_limits` | fetches the limits from the API and keeps the last known ones |
| `alarms` | the alarm rule, per sensor, on the device |
| `uplink` | the RAM buffer, batching and backoff |
| `api` | HTTP requests with the bearer token, JSON encoding and decoding |
| `sample_buffer.h`, `backoff.h` | the ring buffer and the backoff, plain C++ with no Arduino dependencies |
| `leds`, `button` | status LEDs and the debounced button |
| `credentials.h` | includes `secrets.h` and stops the build if a value is missing |

The serial log (115200 baud) prefixes every line with its module, e.g. `[wifi]`, `[uplink]` or `[alarm]`, and never prints the WiFi password, the server URL or the token.

### Sensor

A failed read is retried twice, 2.1 s apart, because the DHT22 needs at least 2 s between reads. Values outside the sensor's range (−40 to 80 °C, 0 to 100 %) count as failed reads. If all three attempts fail, the sample is skipped. After 3 skipped samples in a row, the sensor counts as faulty, and the yellow LED turns on until a read succeeds again.

### Clock

The ESP32 has no battery-backed clock, so the time starts at 1970 on every boot. Once WiFi is up, the firmware starts NTP (`pool.ntp.org`, `time.google.com`). If NTP has not answered after 15 s, it sets the clock from `server_time` in `GET /api/limits`. It retries every 30 s if that fails and re-syncs every hour while NTP stays unreachable. NTP takes over as soon as it answers.

This matters when the LAN works but the internet does not: NTP is the only part of the data path that needs the internet. Without a clock the server would reject every reading, because the timestamps would be before 2020.

### Buffer and backoff

Every reading goes into a ring buffer of 4,320 samples, which is 12 hours at the 10 s interval. Each sample is 8 bytes: a 32-bit timestamp plus both values in tenths, about 35 KB in total. When the buffer is full, the oldest sample is dropped, so the most recent data survives.

The buffer is sent oldest first, up to 50 samples per request, one request per loop pass, so readings continue while a backlog drains. After a failed send, the firmware waits 5 s, then 10, 20, 40, 80 and at most 120 s. The wait resets after a successful send, and when WiFi reconnects, so recovery does not wait out an old pause.

| Response | Action |
|---|---|
| 200 | remove the sent samples from the buffer |
| no connection, 5xx, 401, 408, 422, 429 | keep the samples and retry with backoff; these can be fixed without losing data, e.g. by correcting the token or adding the station |
| other 4xx (e.g. 400) | drop the batch and log it; it would fail forever and block everything behind it |

Resending is safe because the server ignores duplicates: if a response is lost after the server stored the batch, the resend is reported as `duplicates`.

### Limits

The limits are fetched once WiFi is up and then every 60 s; the first fetch is retried every 30 s until it succeeds. If a fetch fails, the last known limits are kept. Until the first fetch succeeds, the device evaluates no alarms. When the limits change, both sensors' limits are logged with the band where an alarm clears.

### LEDs and button

- **Green:** WiFi up, the last send succeeded, no retry pending, no sensor fault, and no open alarm.
- **Yellow:** any of those conditions fails, except the alarm. Yellow is also on from boot until the first successful send.
- **Red:** at least one sensor has an open alarm on the device.

The button reads `LOW` when pressed (internal pull-up) and is debounced for 30 ms. A press logs `[check]` with the values and the open alarms, and sends the reading like any other. The regular schedule then restarts 10 s from the press.

## Failure handling

The first six scenarios were tested on the hardware, with the database checked afterwards. The last two follow from the design; duplicate handling is covered by the server tests.

| Failure | Device | Server and dashboard |
|---|---|---|
| WiFi down (hotspot off) | retries every 15 s and logs the reason (e.g. `NO_AP_FOUND`); keeps reading and buffering; yellow LED | station goes offline after 30 s; the backlog arrives right after the reconnect, with the original timestamps |
| API down (`docker compose stop api`) | backoff 5 s up to 2 min; yellow LED; keeps the last limits | after recovery the backlog arrives in batches of up to 50 samples; the chart has no gap |
| Alarm while offline | red and yellow LEDs; raises and clears locally | records the alarm with the device's timestamps once the backlog arrives |
| No internet, LAN up (NTP unreachable) | clock from `server_time` after 15 s | readings accepted as usual |
| NTP and API both unreachable at boot | readings skipped (`clock not set`) until a clock source answers | nothing arrives for that period |
| Sensor unplugged | samples skipped; fault reported after 3 in a row; yellow LED | gap in the chart; no fault is reported to the server |
| Response lost after the server stored a batch | resends the batch | reported as duplicates; nothing is stored twice |
| Device reset during an outage | the RAM buffer is lost | readings from the outage are missing |
