// Calls to the Flask API in server/. Every request sends the bearer token.
#pragma once

#include <stddef.h>
#include <time.h>

namespace api {

struct Reading {
  const char* sensor;  // "temperature" or "humidity", as the server expects
  double value;
  time_t ts;  // UTC, when the value was measured
};

struct PostResult {
  int status;  // HTTP status, or a negative HTTPClient error (no connection)
  int accepted;
  int duplicates;
};

// POST one batch to /api/readings. Failures are logged here.
PostResult postReadings(const Reading* readings, size_t count);

// GET /api/limits and return its server_time (UTC). Used as the clock
// fallback when NTP is unreachable but the server is.
bool fetchServerTime(time_t& out);

}  // namespace api
