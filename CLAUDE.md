# CLAUDE.md

Persistent rules for this repository. Overview, setup and wiring: see `README.md`. API, schema, alarm rule and firmware design: see `docs/design.md`.

## What this project is

`env-monitor`: an ESP32 + DHT22 environment monitor that sends batched readings over WiFi to a Flask API backed by PostgreSQL, with a web dashboard, alarms and offline buffering. The scope is deliberately small: reliability and clarity matter more than feature count.

## Stack

- Server: Python 3.14, Flask, `psycopg` (v3) with plain parameterized SQL, pytest. Managed with **uv** (`pyproject.toml` + `uv.lock`, no requirements.txt).
- Database: PostgreSQL 17 via Docker Compose, schema in `server/db/init.sql`.
- Firmware: C++ (Arduino framework) on a classic ESP32 DevKit V1 (ESP32-WROOM-32), built with PlatformIO in `firmware/`.
- Dashboard: server-rendered Flask page with Chart.js.

## Commands

- Start everything: `docker compose up --build`
- Server tests: `./scripts/test.sh` (starts a throwaway Postgres on port 5433, runs pytest, removes the container; pytest args are passed through)
- Add a dependency: `cd server && uv add <pkg>` (dev: `uv add --dev <pkg>`)

## Conventions

- All timestamps are UTC, ISO 8601 (`2026-10-26T14:03:10Z`). Device time `ts` and server time `received_at` are stored separately.
- Readings are idempotent: `UNIQUE (station_id, sensor, ts)` and `INSERT ... ON CONFLICT DO NOTHING`.
- Never build SQL with string formatting. Always use parameters.
- API requires a token in the `Authorization: Bearer <token>` header. Validate all input and return clear JSON errors with proper status codes.
- Secrets live in `.env` (gitignored). Commit `.env.example` only.
- Alarm limits live in the database and are fetched by the firmware, not hardcoded.
- Sensor access in firmware is isolated behind one small class or function so the DHT22 can be swapped for an I2C sensor later.
- Small, meaningful commits with imperative messages (e.g. "Add readings endpoint with batch validation"). The Git history should read as a clear story of how the project was built.
- Write or update tests with every API change.

## Hardware safety rules (important)

- The ESP32 is 3.3 V only. Never connect 9 V, 5 V or anything above 3.3 V to a GPIO, the 3V3 pin or the DHT22.
- Power comes from USB only. The 9 V battery is deliberately not used.
- Use ADC1 pins (GPIO 32-39) for analog input, since ADC2 does not work while WiFi is active.
- Avoid GPIO 0, 2, 12, 15 for LEDs and buttons (boot-related).
- On the breadboard only one ESP32 pin row is reachable, the one with 3V3 (USB end first: 3V3, GND, D15, D2, D4, D16, D17, D5, D18, D19, D21, RX0, TX0, D22, D23). Use only these pins: 3V3, GND, D4 (DHT22), D16/D17/D18 (LEDs), D23 (button, at the far end of the row where there is room to wire). Keep D21 and D22 free for a possible future I2C sensor.
- The ESP32 supports 2.4 GHz WiFi only.
- No soldering anywhere in this project.

## Working agreements

- Wiring and pin assignments are documented in the README wiring table. Any hardware suggestion should name exact pins and voltage levels, with a one-line reason.
- Breadboard wiring and firmware flashing are done by hand on the physical board, so don't assume access to the device. Give exact pin numbers and wiring tables.
- Ask before large structural changes. Prefer simple solutions and avoid unneeded dependencies.
