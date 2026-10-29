#include "button.h"

#include <Arduino.h>

#include "config.h"

namespace button {
namespace {

int stableLevel = HIGH;  // HIGH = released (pull-up), LOW = pressed
int lastLevel = HIGH;
uint32_t lastChangeAt = 0;

}  // namespace

void begin() { pinMode(BUTTON_PIN, INPUT_PULLUP); }

bool pressed() {
  const int level = digitalRead(BUTTON_PIN);
  if (level != lastLevel) {
    lastLevel = level;
    lastChangeAt = millis();
  }
  // Accept a new level only once the contacts have stopped bouncing.
  if (level != stableLevel && millis() - lastChangeAt >= BUTTON_DEBOUNCE_MS) {
    stableLevel = level;
    return stableLevel == LOW;
  }
  return false;
}

}  // namespace button
