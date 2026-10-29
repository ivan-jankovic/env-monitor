// Pushbutton between its GPIO and GND, read with the internal pull-up.
#pragma once

namespace button {

void begin();

// Poll on every loop() iteration. True once per press, after debouncing.
// A press shorter than a blocking operation (sensor retry, HTTP timeout) can
// be missed.
bool pressed();

}  // namespace button
