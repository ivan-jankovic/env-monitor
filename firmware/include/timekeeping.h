// UTC wall clock for reading timestamps. NTP is preferred; when it does not
// answer, the server's clock (server_time from GET /api/limits) is used, so
// readings keep valid timestamps on a LAN without internet access.
#pragma once

#include <stddef.h>

namespace timekeeping {

enum class Source { None, Server, Ntp };

void begin();
void loop();  // call on every loop() iteration
bool valid();
Source source();
const char* sourceName();

// Current UTC time as "2026-10-28T14:03:10Z". Returns false while the clock is
// not set. buf needs at least 21 bytes.
bool isoNow(char* buf, size_t len);

}  // namespace timekeeping
