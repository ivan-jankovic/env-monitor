// UTC wall clock for reading timestamps. NTP is preferred; when it does not
// answer, the server's clock (server_time from GET /api/limits) is used, so
// readings keep valid timestamps on a LAN without internet access.
#pragma once

#include <stddef.h>
#include <time.h>

namespace timekeeping {

enum class Source { None, Server, Ntp };

void begin();
void loop();  // call on every loop() iteration
bool valid();
Source source();
const char* sourceName();

// Current UTC time. Returns false while the clock is not set.
bool now(time_t& out);

// Format as "2026-10-28T14:03:10Z". buf needs at least 21 bytes.
void formatIso(time_t t, char* buf, size_t len);

// formatIso(now). Returns false while the clock is not set.
bool isoNow(char* buf, size_t len);

}  // namespace timekeeping
