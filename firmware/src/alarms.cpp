#include "alarms.h"

#include <Arduino.h>

#include "alarm_limits.h"

namespace alarms {
namespace {

struct State {
  const char* name;
  const char* unit;
  bool open;
};

State temperatureAlarm = {"temperature", "C", false};
State humidityAlarm = {"humidity", "%", false};

// Raise when strictly above max_val or below min_val.
bool isOutOfRange(double value, const Limit& l) {
  return (l.hasMax && value > l.maxVal) || (l.hasMin && value < l.minVal);
}

// Clear only when back inside both limits by at least the hysteresis.
bool isClear(double value, const Limit& l) {
  return (!l.hasMax || value <= l.maxVal - l.hysteresis) &&
         (!l.hasMin || value >= l.minVal + l.hysteresis);
}

void update(State& state, double value, const Limit& limit) {
  if (!state.open && isOutOfRange(value, limit)) {
    state.open = true;
    Serial.printf("[alarm] %s raised: %.1f %s\n", state.name, value, state.unit);
  } else if (state.open && isClear(value, limit)) {
    state.open = false;
    Serial.printf("[alarm] %s cleared: %.1f %s\n", state.name, value, state.unit);
  }
}

}  // namespace

void evaluate(double temperature, double humidity) {
  if (!limits::loaded()) {
    return;
  }
  update(temperatureAlarm, temperature, limits::temperature());
  update(humidityAlarm, humidity, limits::humidity());
}

bool anyOpen() { return temperatureAlarm.open || humidityAlarm.open; }

}  // namespace alarms
