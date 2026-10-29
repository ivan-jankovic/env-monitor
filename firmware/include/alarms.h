// Alarm state per sensor on the device, using the same raise and clear rule
// as the server (server/app/alarms.py). It works offline too; the red LED
// shows anyOpen().
#pragma once

namespace alarms {

// Evaluate one measurement. Does nothing until the limits are loaded.
void evaluate(double temperature, double humidity);
bool anyOpen();

}  // namespace alarms
