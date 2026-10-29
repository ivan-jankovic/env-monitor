#include "leds.h"

#include <Arduino.h>

#include "config.h"

namespace leds {

void begin() {
  const uint8_t pins[] = {LED_GREEN_PIN, LED_YELLOW_PIN, LED_RED_PIN};
  for (uint8_t pin : pins) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }
  Serial.println("[leds] self-test: green, yellow, red");
  for (uint8_t pin : pins) {
    digitalWrite(pin, HIGH);
    delay(300);
    digitalWrite(pin, LOW);
  }
}

void show(bool green, bool yellow, bool red) {
  digitalWrite(LED_GREEN_PIN, green ? HIGH : LOW);
  digitalWrite(LED_YELLOW_PIN, yellow ? HIGH : LOW);
  digitalWrite(LED_RED_PIN, red ? HIGH : LOW);
}

}  // namespace leds
