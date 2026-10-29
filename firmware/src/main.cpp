#include <Arduino.h>
#include <math.h>

#include "api.h"
#include "config.h"
#include "env_sensor.h"
#include "timekeeping.h"
#include "wifi_link.h"

EnvSensor sensor;
uint32_t lastReadingAt = 0;

// The DHT22 resolves 0.1; rounding also keeps float noise like 24.700001 out of the JSON.
double round1(float value) { return round(value * 10.0) / 10.0; }

void takeAndSendReading() {
  Measurement m;
  if (!sensor.read(m)) {
    return;  // logged by the sensor
  }
  time_t now;
  if (!timekeeping::now(now)) {
    Serial.printf("[reading] temperature=%.1f C  humidity=%.1f %%  not sent: clock not set\n",
                  m.temperature, m.humidity);
    return;
  }
  char ts[21];
  timekeeping::formatIso(now, ts, sizeof ts);
  Serial.printf("[%s] temperature=%.1f C  humidity=%.1f %%\n", ts, m.temperature, m.humidity);
  if (!wifi_link::connected()) {
    Serial.println("[reading] not sent: WiFi down");
    return;
  }
  const api::Reading batch[] = {
      {"temperature", round1(m.temperature), now},
      {"humidity", round1(m.humidity), now},
  };
  const api::PostResult result = api::postReadings(batch, 2);
  if (result.status == 200) {
    Serial.printf("[api] POST /api/readings: accepted=%d duplicates=%d\n", result.accepted,
                  result.duplicates);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\nenv-monitor " STATION_ID " starting");
  sensor.begin();
  timekeeping::begin();
  wifi_link::begin();
}

void loop() {
  wifi_link::loop();
  timekeeping::loop();

  if (millis() - lastReadingAt >= READING_INTERVAL_MS) {
    lastReadingAt = millis();
    takeAndSendReading();
  }
  delay(10);
}
