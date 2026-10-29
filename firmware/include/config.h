// Non-secret settings. Credentials live in secrets.h (see secrets.h.example).
#pragma once

#include <stdint.h>

#define STATION_ID "esp32-01"

// Keep in sync with READING_INTERVAL_SECONDS in docker-compose.yml: the server
// marks the station offline after 3 missed readings.
constexpr uint32_t READING_INTERVAL_MS = 10000;

constexpr uint8_t DHT_PIN = 4;
// Consecutive skipped samples before the sensor counts as faulty.
constexpr uint32_t SENSOR_FAULT_AFTER = 3;

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

constexpr uint16_t HTTP_TIMEOUT_MS = 5000;
