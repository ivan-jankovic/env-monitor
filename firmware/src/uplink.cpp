#include "uplink.h"

#include <Arduino.h>
#include <math.h>

#include "api.h"
#include "backoff.h"
#include "config.h"
#include "sample_buffer.h"
#include "wifi_link.h"

namespace uplink {
namespace {

SampleBuffer<BUFFER_CAPACITY> buffer;
Backoff backoff(SEND_BACKOFF_MIN_MS, SEND_BACKOFF_MAX_MS);
bool waiting = false;  // in a backoff pause after a failed send
uint32_t retryAt = 0;
bool wasConnected = false;

// Static: too large for the loop task's stack.
Sample batch[MAX_BATCH_SAMPLES];
api::Reading readings[2 * MAX_BATCH_SAMPLES];

// Worth keeping the data and trying again later? A 400 means the batch itself
// is invalid and would fail forever, blocking everything behind it.
bool retryable(int status) {
  return status < 0 || status >= 500 || status == 401 || status == 408 || status == 422 ||
         status == 429;
}

void sendBatch() {
  const size_t n = buffer.peek(batch, MAX_BATCH_SAMPLES);
  for (size_t i = 0; i < n; i++) {
    readings[2 * i] = {"temperature", batch[i].temperature10 / 10.0, batch[i].ts};
    readings[2 * i + 1] = {"humidity", batch[i].humidity10 / 10.0, batch[i].ts};
  }
  const api::PostResult result = api::postReadings(readings, 2 * n);

  if (result.status == 200) {
    buffer.pop(n);
    backoff.reset();
    waiting = false;
    Serial.printf("[uplink] sent %u sample%s (accepted=%d duplicates=%d)", n, n == 1 ? "" : "s",
                  result.accepted, result.duplicates);
    if (!buffer.empty()) {
      Serial.printf(", still buffered: %u", buffer.size());
    }
    Serial.println();
  } else if (!retryable(result.status)) {
    buffer.pop(n);
    Serial.printf("[uplink] server rejected the batch, dropped %u samples\n", n);
  } else {
    const uint32_t wait = backoff.fail();
    waiting = true;
    retryAt = millis() + wait;
    Serial.printf("[uplink] retry in %lu s (buffered: %u)\n",
                  static_cast<unsigned long>(wait / 1000), buffer.size());
  }
}

}  // namespace

void add(time_t ts, double temperature, double humidity) {
  const Sample sample = {static_cast<uint32_t>(ts),
                         static_cast<int16_t>(lround(temperature * 10)),
                         static_cast<uint16_t>(lround(humidity * 10))};
  if (!buffer.push(sample)) {
    Serial.printf("[uplink] buffer full, dropped the oldest sample (%lu dropped so far)\n",
                  static_cast<unsigned long>(buffer.dropped()));
  }
  if (!wifi_link::connected() || waiting) {
    Serial.printf("[uplink] %s (buffered: %u)\n", waiting ? "waiting to retry" : "WiFi down",
                  buffer.size());
  }
}

void loop() {
  const bool connected = wifi_link::connected();
  if (connected && !wasConnected && waiting) {
    backoff.reset();  // fresh connection: try right away instead of finishing the pause
    waiting = false;
  }
  wasConnected = connected;
  if (!connected || buffer.empty()) {
    return;
  }
  if (waiting && static_cast<int32_t>(millis() - retryAt) < 0) {
    return;
  }
  sendBatch();
}

size_t pending() { return buffer.size(); }

}  // namespace uplink
