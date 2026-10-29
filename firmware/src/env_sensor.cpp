#include "env_sensor.h"

#include <Arduino.h>
#include <DHT.h>

#include "config.h"

namespace {

DHT dht(DHT_PIN, DHT22);

constexpr int READ_ATTEMPTS = 3;          // first read plus two retries
constexpr uint32_t DHT_MIN_GAP_MS = 2100;  // the DHT22 needs at least 2 s between reads

// A glitch on the data line can pass the checksum; reject values the DHT22
// cannot produce.
bool plausible(float t, float h) {
  return !isnan(t) && !isnan(h) && t >= -40 && t <= 80 && h >= 0 && h <= 100;
}

}  // namespace

void EnvSensor::begin() { dht.begin(); }

bool EnvSensor::read(Measurement& out) {
  for (int attempt = 1; attempt <= READ_ATTEMPTS; attempt++) {
    const uint32_t sinceLast = millis() - lastReadAt_;
    if (hasRead_ && sinceLast < DHT_MIN_GAP_MS) {
      delay(DHT_MIN_GAP_MS - sinceLast);
    }
    hasRead_ = true;
    lastReadAt_ = millis();
    const float t = dht.readTemperature(false, true);  // force a fresh read
    const float h = dht.readHumidity();                // same read, cached by the library
    if (plausible(t, h)) {
      out = {t, h};
      if (failures_ >= SENSOR_FAULT_AFTER) {
        Serial.println("[sensor] reading again after a fault");
      }
      failures_ = 0;
      return true;
    }
  }
  failures_++;
  Serial.printf("[sensor] read failed %d times, sample skipped (%lu in a row)\n", READ_ATTEMPTS,
                static_cast<unsigned long>(failures_));
  if (failures_ == SENSOR_FAULT_AFTER) {
    Serial.println("[sensor] fault: not responding, check the wiring");
  }
  return false;
}

bool EnvSensor::faulty() const { return failures_ >= SENSOR_FAULT_AFTER; }
