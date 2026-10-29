// Alarm limits from GET /api/limits, fetched once WiFi is up and then
// periodically, so a change in the database takes effect without reflashing.
#pragma once

struct Limit {
  bool hasMin;  // min_val and max_val may be null on the server
  double minVal;
  bool hasMax;
  double maxVal;
  double hysteresis;
};

namespace limits {

void loop();  // call on every loop() iteration
bool loaded();
const Limit& temperature();
const Limit& humidity();

}  // namespace limits
