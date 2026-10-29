#include <Arduino.h>
#include <DHT.h>

#include "config.h"
#include "timekeeping.h"
#include "wifi_link.h"

DHT dht(DHT_PIN, DHT22);
uint32_t lastReadingAt = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("\nenv-monitor " STATION_ID " starting");
  dht.begin();
  timekeeping::begin();
  wifi_link::begin();
}

void loop() {
  wifi_link::loop();
  timekeeping::loop();

  if (millis() - lastReadingAt >= READING_INTERVAL_MS) {
    lastReadingAt = millis();
    char ts[21];
    if (!timekeeping::isoNow(ts, sizeof ts)) {
      strcpy(ts, "time not set");
    }
    const float h = dht.readHumidity();
    const float t = dht.readTemperature();
    if (isnan(h) || isnan(t)) {
      Serial.printf("[%s] read failed\n", ts);
    } else {
      Serial.printf("[%s] temperature=%.1f C  humidity=%.1f %%  (clock: %s)\n",
                    ts, t, h, timekeeping::sourceName());
    }
  }
  delay(10);
}
