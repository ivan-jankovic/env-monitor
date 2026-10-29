#include "alarm_limits.h"

#include <Arduino.h>

#include "api.h"
#include "config.h"
#include "wifi_link.h"

namespace limits {
namespace {

Limit temperatureLimit = {};
Limit humidityLimit = {};
bool isLoaded = false;
bool attempted = false;
uint32_t lastAttemptAt = 0;

bool same(const Limit& a, const Limit& b) {
  return a.hasMin == b.hasMin && a.minVal == b.minVal && a.hasMax == b.hasMax &&
         a.maxVal == b.maxVal && a.hysteresis == b.hysteresis;
}

void formatBound(char* buf, size_t len, bool has, double value) {
  if (has) {
    snprintf(buf, len, "%.1f", value);
  } else {
    snprintf(buf, len, "none");
  }
}

void log(const char* name, const char* unit, const Limit& l) {
  char lo[12], hi[12], clearLo[12], clearHi[12];
  formatBound(lo, sizeof lo, l.hasMin, l.minVal);
  formatBound(hi, sizeof hi, l.hasMax, l.maxVal);
  formatBound(clearLo, sizeof clearLo, l.hasMin, l.minVal + l.hysteresis);
  formatBound(clearHi, sizeof clearHi, l.hasMax, l.maxVal - l.hysteresis);
  Serial.printf("[limits] %s: %s..%s %s, alarm clears at %s..%s\n", name, lo, hi, unit, clearLo,
                clearHi);
}

void fetch() {
  attempted = true;
  lastAttemptAt = millis();
  api::LimitsResponse fetched;
  if (!api::fetchLimits(fetched)) {
    if (isLoaded) {
      Serial.println("[limits] keeping the last known limits");
    }
    return;
  }
  const bool changed = !isLoaded || !same(fetched.temperature, temperatureLimit) ||
                       !same(fetched.humidity, humidityLimit);
  temperatureLimit = fetched.temperature;
  humidityLimit = fetched.humidity;
  isLoaded = true;
  if (changed) {
    log("temperature", "C", temperatureLimit);
    log("humidity", "%", humidityLimit);
  }
}

}  // namespace

void loop() {
  if (!wifi_link::connected()) {
    return;
  }
  const uint32_t wait = isLoaded ? LIMITS_REFRESH_MS : LIMITS_RETRY_MS;
  if (!attempted || millis() - lastAttemptAt >= wait) {
    fetch();
  }
}

bool loaded() { return isLoaded; }

const Limit& temperature() { return temperatureLimit; }

const Limit& humidity() { return humidityLimit; }

}  // namespace limits
