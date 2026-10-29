#include <Arduino.h>
#include <math.h>

#include "alarm_limits.h"
#include "alarms.h"
#include "config.h"
#include "env_sensor.h"
#include "timekeeping.h"
#include "uplink.h"
#include "wifi_link.h"

EnvSensor sensor;
uint32_t lastReadingAt = 0;

// The DHT22 resolves 0.1; rounding also keeps float noise like 24.700001 out of the JSON.
double round1(float value) { return round(value * 10.0) / 10.0; }

void takeReading() {
  Measurement m;
  if (!sensor.read(m)) {
    return;  // logged by the sensor
  }
  const double temperature = round1(m.temperature);
  const double humidity = round1(m.humidity);
  alarms::evaluate(temperature, humidity);  // works offline, unlike the server's alarms

  time_t now;
  if (!timekeeping::now(now)) {
    // The server rejects timestamps before 2020, so this can't be buffered either.
    Serial.printf("[reading] temperature=%.1f C  humidity=%.1f %%  skipped: clock not set\n",
                  m.temperature, m.humidity);
    return;
  }
  char ts[21];
  timekeeping::formatIso(now, ts, sizeof ts);
  Serial.printf("[%s] temperature=%.1f C  humidity=%.1f %%\n", ts, m.temperature, m.humidity);
  uplink::add(now, temperature, humidity);
}

void setup() {
  Serial.begin(115200);
  // The build time shows which firmware is actually running after an upload.
  Serial.println("\nenv-monitor " STATION_ID " starting, built " __DATE__ " " __TIME__);
  sensor.begin();
  timekeeping::begin();
  wifi_link::begin();
}

void loop() {
  wifi_link::loop();
  timekeeping::loop();
  limits::loop();

  if (millis() - lastReadingAt >= READING_INTERVAL_MS) {
    lastReadingAt = millis();
    takeReading();
  }
  uplink::loop();
  delay(10);
}
