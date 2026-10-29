#include "api.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

#include "config.h"
#include "credentials.h"

namespace api {
namespace {

// Parse "2026-10-28T14:03:10Z" to epoch seconds without depending on the TZ
// setting (days-from-civil algorithm by Howard Hinnant).
bool parseIsoUtc(const char* text, time_t& out) {
  int y, mo, d, h, mi, s;
  char z;
  if (sscanf(text, "%4d-%2d-%2dT%2d:%2d:%2d%c", &y, &mo, &d, &h, &mi, &s, &z) != 7 || z != 'Z') {
    return false;
  }
  y -= mo <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const long yoe = y - era * 400;
  const long doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  const long days = era * 146097 + doe - 719468;
  out = static_cast<time_t>(days) * 86400 + h * 3600 + mi * 60 + s;
  return true;
}

// GET a JSON endpoint. Returns the HTTP status, or a negative HTTPClient error.
int getJson(const char* path, JsonDocument& doc) {
  HTTPClient http;
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);
  if (!http.begin(String(API_BASE_URL) + path)) {
    return HTTPC_ERROR_CONNECTION_REFUSED;
  }
  http.addHeader("Authorization", "Bearer " API_TOKEN);
  const int status = http.GET();
  if (status == HTTP_CODE_OK && deserializeJson(doc, http.getString())) {
    doc.clear();  // body was not valid JSON; callers see missing fields
  }
  http.end();
  return status;
}

void logFailure(const char* what, int status) {
  if (status < 0) {
    Serial.printf("[api] %s failed: %s\n", what, HTTPClient::errorToString(status).c_str());
  } else {
    Serial.printf("[api] %s failed: HTTP %d\n", what, status);
  }
}

}  // namespace

bool fetchServerTime(time_t& out) {
  JsonDocument doc;
  const int status = getJson("/api/limits", doc);
  if (status != HTTP_CODE_OK) {
    logFailure("GET /api/limits", status);
    return false;
  }
  const char* serverTime = doc["server_time"];
  if (!serverTime || !parseIsoUtc(serverTime, out)) {
    Serial.println("[api] GET /api/limits: missing or invalid server_time");
    return false;
  }
  return true;
}

}  // namespace api
