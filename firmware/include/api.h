// Calls to the Flask API in server/. Every request sends the bearer token.
#pragma once

#include <time.h>

namespace api {

// GET /api/limits and return its server_time (UTC). Used as the clock
// fallback when NTP is unreachable but the server is.
bool fetchServerTime(time_t& out);

}  // namespace api
