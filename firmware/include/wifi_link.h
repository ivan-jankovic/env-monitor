// Keeps the WiFi connection up: connects at boot and retries after a failed
// attempt or a dropped connection.
#pragma once

namespace wifi_link {

void begin();
void loop();  // call on every loop() iteration
bool connected();

}  // namespace wifi_link
