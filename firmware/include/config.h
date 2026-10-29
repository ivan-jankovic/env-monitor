// Non-secret settings. Credentials live in secrets.h (see secrets.h.example).
#pragma once

#include <stddef.h>
#include <stdint.h>

#define STATION_ID "esp32-01"

// Keep in sync with READING_INTERVAL_SECONDS in docker-compose.yml: the server
// marks the station offline after 3 missed readings.
constexpr uint32_t READING_INTERVAL_MS = 10000;

constexpr uint8_t DHT_PIN = 4;
// Consecutive skipped samples before the sensor counts as faulty.
constexpr uint32_t SENSOR_FAULT_AFTER = 3;

// Each LED goes GPIO -> LED anode, cathode -> 220 Ohm -> GND.
constexpr uint8_t LED_GREEN_PIN = 16;
constexpr uint8_t LED_YELLOW_PIN = 17;
constexpr uint8_t LED_RED_PIN = 18;
// Button between GPIO 23 and GND (internal pull-up, so pressed reads LOW).
// GPIO 23 is the last pin on the reachable row, where there is room to wire.
constexpr uint8_t BUTTON_PIN = 23;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 30;

// How long one WiFi connection attempt may take before the next one starts.
constexpr uint32_t WIFI_ATTEMPT_MS = 15000;

// Overridable with build flags, e.g. to test the server-time fallback:
//   -DNTP_SERVER_1='"ntp.invalid"' -DNTP_SERVER_2='"ntp.invalid"'
#ifndef NTP_SERVER_1
#define NTP_SERVER_1 "pool.ntp.org"
#endif
#ifndef NTP_SERVER_2
#define NTP_SERVER_2 "time.google.com"
#endif

// Without an NTP answer after this long, take the time from GET /api/limits.
constexpr uint32_t NTP_WAIT_MS = 15000;
constexpr uint32_t SERVER_TIME_RETRY_MS = 30000;      // after a failed fallback request
constexpr uint32_t SERVER_TIME_REFRESH_MS = 3600000;  // re-sync while NTP stays unreachable

// Alarm limits: re-fetched this often, so database changes apply within a minute.
constexpr uint32_t LIMITS_REFRESH_MS = 60000;
constexpr uint32_t LIMITS_RETRY_MS = 30000;  // until the first fetch succeeds

constexpr uint16_t HTTP_TIMEOUT_MS = 5000;

// Offline buffer: 12 h of samples at the 10 s interval, 8 bytes each (~35 KB).
constexpr size_t BUFFER_CAPACITY = 12 * 3600 / (READING_INTERVAL_MS / 1000);
// Samples per POST (2 readings each); the server accepts up to 1000 readings.
constexpr size_t MAX_BATCH_SAMPLES = 50;
// After a failed send, wait 5 s, then 10, 20, ... up to 2 min.
constexpr uint32_t SEND_BACKOFF_MIN_MS = 5000;
constexpr uint32_t SEND_BACKOFF_MAX_MS = 120000;
