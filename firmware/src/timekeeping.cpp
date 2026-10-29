#include "timekeeping.h"

#include <Arduino.h>
#include <esp_sntp.h>
#include <sys/time.h>
#include <time.h>

#include "api.h"
#include "config.h"
#include "wifi_link.h"

namespace timekeeping {
namespace {

volatile bool ntpSynced = false;  // set from the SNTP task
Source current = Source::None;
bool sntpStarted = false;
uint32_t sntpStartedAt = 0;
bool fallbackAnnounced = false;
bool serverAttempted = false;
uint32_t lastServerAttemptAt = 0;

void onNtpSync(struct timeval*) { ntpSynced = true; }

void logNow(const char* message) {
  char ts[21];
  isoNow(ts, sizeof ts);
  Serial.printf("[time] %s: %s\n", message, ts);
}

void syncFromServer() {
  serverAttempted = true;
  lastServerAttemptAt = millis();
  time_t serverTime;
  if (!api::fetchServerTime(serverTime) || ntpSynced) {
    return;  // failed (logged by api), or NTP answered in the meantime
  }
  const timeval tv = {serverTime, 0};
  settimeofday(&tv, nullptr);
  current = Source::Server;
  logNow("set from server");
}

}  // namespace

void begin() { sntp_set_time_sync_notification_cb(onNtpSync); }

void loop() {
  if (!wifi_link::connected()) {
    return;
  }
  if (!sntpStarted) {
    // SNTP keeps running in the background and re-syncs about every hour.
    configTime(0, 0, NTP_SERVER_1, NTP_SERVER_2);
    sntpStarted = true;
    sntpStartedAt = millis();
    Serial.println("[time] waiting for NTP");
  }
  if (ntpSynced) {
    if (current != Source::Ntp) {
      current = Source::Ntp;  // NTP always wins over the server's clock
      logNow("NTP sync");
    }
    return;
  }
  if (millis() - sntpStartedAt < NTP_WAIT_MS) {
    return;
  }
  if (!fallbackAnnounced) {
    fallbackAnnounced = true;
    Serial.printf("[time] no NTP answer after %lu s, using the server's clock\n",
                  static_cast<unsigned long>(NTP_WAIT_MS / 1000));
  }
  const uint32_t wait = current == Source::Server ? SERVER_TIME_REFRESH_MS : SERVER_TIME_RETRY_MS;
  if (!serverAttempted || millis() - lastServerAttemptAt >= wait) {
    syncFromServer();
  }
}

bool valid() { return current != Source::None; }

Source source() { return current; }

const char* sourceName() {
  switch (current) {
    case Source::Ntp: return "NTP";
    case Source::Server: return "server";
    default: return "not set";
  }
}

bool isoNow(char* buf, size_t len) {
  if (!valid()) {
    return false;
  }
  const time_t now = time(nullptr);
  struct tm utc;
  gmtime_r(&now, &utc);
  strftime(buf, len, "%Y-%m-%dT%H:%M:%SZ", &utc);
  return true;
}

}  // namespace timekeeping
