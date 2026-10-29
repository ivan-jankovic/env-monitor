#include <Arduino.h>
#include <math.h>

#include "alarm_limits.h"
#include "alarms.h"
#include "button.h"
#include "config.h"
#include "env_sensor.h"
#include "leds.h"
#include "timekeeping.h"
#include "uplink.h"
#include "wifi_link.h"

EnvSensor sensor;
uint32_t lastReadingAt = 0;

// The DHT22 resolves 0.1; rounding also keeps float noise like 24.700001 out of the JSON.
double round1(float value) { return round(value * 10.0) / 10.0; }

// check: triggered by the button, so also log the result against the limits.
void takeReading(bool check) {
  Measurement m;
  if (!sensor.read(m)) {
    if (check) {
      Serial.println("[check] sensor read failed");
    }
    return;  // logged by the sensor
  }
  const double temperature = round1(m.temperature);
  const double humidity = round1(m.humidity);
  alarms::evaluate(temperature, humidity);  // works offline, unlike the server's alarms
  if (check) {
    Serial.printf("[check] temperature=%.1f C  humidity=%.1f %%  open alarms: %s%s\n",
                  temperature, humidity, alarms::describeOpen(),
                  limits::loaded() ? "" : " (limits not loaded yet)");
  }

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

void updateLeds() {
  const bool red = alarms::anyOpen();
  const bool yellow = !uplink::healthy() || sensor.faulty();
  const bool green = !yellow && !red;

  static int lastState = -1;  // log changes only
  const int state = green | yellow << 1 | red << 2;
  if (state != lastState) {
    lastState = state;
    Serial.printf("[leds] green=%s yellow=%s red=%s\n", green ? "on" : "off",
                  yellow ? "on" : "off", red ? "on" : "off");
  }
  leds::show(green, yellow, red);
}

void setup() {
  Serial.begin(115200);
  // The build time shows which firmware is actually running after an upload.
  Serial.println("\nenv-monitor " STATION_ID " starting, built " __DATE__ " " __TIME__);
  leds::begin();
  button::begin();
  sensor.begin();
  timekeeping::begin();
  wifi_link::begin();
}

void loop() {
  wifi_link::loop();
  timekeeping::loop();
  limits::loop();

  if (button::pressed()) {
    Serial.println("[button] check requested");
    takeReading(true);
    lastReadingAt = millis();  // restart the schedule from this reading
  } else if (millis() - lastReadingAt >= READING_INTERVAL_MS) {
    lastReadingAt = millis();
    takeReading(false);
  }
  uplink::loop();
  updateLeds();
  delay(10);
}
