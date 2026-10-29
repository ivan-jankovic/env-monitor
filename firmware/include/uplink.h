// Gets readings to the server reliably: every sample goes into a RAM buffer,
// which is drained oldest first. Failed sends are retried with exponential
// backoff; the server ignores duplicates, so resending is safe.
#pragma once

#include <stddef.h>
#include <time.h>

namespace uplink {

void add(time_t ts, double temperature, double humidity);
void loop();  // call on every loop() iteration
size_t pending();

}  // namespace uplink
