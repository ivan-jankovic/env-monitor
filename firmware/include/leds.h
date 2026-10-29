// Status LEDs (pins in config.h): green = last send succeeded and
// no alarm, yellow = WiFi down, sends failing or sensor fault, red = at least
// one alarm is open.
#pragma once

namespace leds {

void begin();  // includes a short self-test, one LED after another
void show(bool green, bool yellow, bool red);

}  // namespace leds
