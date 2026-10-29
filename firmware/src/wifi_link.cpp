#include "wifi_link.h"

#include <WiFi.h>

#include "config.h"
#include "credentials.h"

namespace wifi_link {
namespace {

uint32_t attemptStartedAt = 0;
uint32_t attempts = 0;
bool wasConnected = false;
// Why the WiFi stack last dropped or rejected us; set from the WiFi event task.
volatile uint8_t lastReason = 0;

void onDisconnected(WiFiEvent_t, WiFiEventInfo_t info) {
  const uint8_t reason = info.wifi_sta_disconnected.reason;
  if (reason != WIFI_REASON_ASSOC_LEAVE) {  // that one is our own WiFi.disconnect()
    lastReason = reason;
  }
}

void logReason(const char* what) {
  const uint8_t reason = lastReason;
  if (reason == 0) {
    Serial.printf("[wifi] %s: no answer from the network\n", what);
  } else {
    // e.g. NO_AP_FOUND (wrong SSID or 5 GHz only), AUTH_FAIL (wrong password)
    Serial.printf("[wifi] %s: %s (reason %u)\n", what,
                  WiFi.disconnectReasonName(static_cast<wifi_err_reason_t>(reason)), reason);
  }
}

void startAttempt() {
  attempts++;
  attemptStartedAt = millis();
  lastReason = 0;
  Serial.printf("[wifi] connecting, attempt %lu\n", static_cast<unsigned long>(attempts));
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

}  // namespace

void begin() {
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);        // don't write the credentials to flash on every begin()
  WiFi.setAutoReconnect(false);  // loop() does the retrying, so it can log it
  WiFi.onEvent(onDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  startAttempt();
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!wasConnected) {
      wasConnected = true;
      attempts = 0;
      Serial.printf("[wifi] connected, ip=%s rssi=%d dBm\n",
                    WiFi.localIP().toString().c_str(), WiFi.RSSI());
    }
    return;
  }
  if (wasConnected) {
    wasConnected = false;
    logReason("connection dropped");
    startAttempt();
  } else if (millis() - attemptStartedAt >= WIFI_ATTEMPT_MS) {
    logReason("attempt failed");
    startAttempt();
  }
}

bool connected() { return WiFi.status() == WL_CONNECTED; }

}  // namespace wifi_link
