#include "api.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

#include "config.h"
#include "credentials.h"
#include "timekeeping.h"

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

bool open(HTTPClient& http, const char* path) {
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);
  if (!http.begin(String(API_BASE_URL) + path)) {
    return false;
  }
  http.addHeader("Authorization", "Bearer " API_TOKEN);
  return true;
}

// GET a JSON endpoint. Returns the HTTP status, or a negative HTTPClient error.
int getJson(const char* path, JsonDocument& doc) {
  HTTPClient http;
  if (!open(http, path)) {
    return HTTPC_ERROR_CONNECTION_REFUSED;
  }
  const int status = http.GET();
  if (status == HTTP_CODE_OK && deserializeJson(doc, http.getString())) {
    doc.clear();  // body was not valid JSON; callers see missing fields
  }
  http.end();
  return status;
}

// The server's error bodies are short JSON messages such as
// {"error": "invalid batch", "details": [...]}, safe to print.
void logFailure(const char* what, int status, const String& body = String()) {
  if (status < 0) {
    Serial.printf("[api] %s failed: %s\n", what, HTTPClient::errorToString(status).c_str());
    return;
  }
  const char* hint = status == 401 ? " (check API_TOKEN in secrets.h)" : "";
  Serial.printf("[api] %s failed: HTTP %d%s %.300s\n", what, status, hint, body.c_str());
}

}  // namespace

PostResult postReadings(const Reading* readings, size_t count) {
  JsonDocument doc;
  doc["station_id"] = STATION_ID;
  JsonArray items = doc["readings"].to<JsonArray>();
  char ts[21];
  for (size_t i = 0; i < count; i++) {
    JsonObject item = items.add<JsonObject>();
    item["sensor"] = readings[i].sensor;
    item["value"] = readings[i].value;
    timekeeping::formatIso(readings[i].ts, ts, sizeof ts);
    item["ts"] = ts;  // char array, so ArduinoJson copies it
  }
  String body;
  serializeJson(doc, body);

  PostResult result = {HTTPC_ERROR_CONNECTION_REFUSED, 0, 0};
  HTTPClient http;
  if (!open(http, "/api/readings")) {
    logFailure("POST /api/readings", result.status);
    return result;
  }
  http.addHeader("Content-Type", "application/json");
  result.status = http.POST(body);
  const String response = result.status > 0 ? http.getString() : String();
  http.end();

  if (result.status != HTTP_CODE_OK) {
    logFailure("POST /api/readings", result.status, response);
    return result;
  }
  JsonDocument reply;
  if (!deserializeJson(reply, response)) {
    result.accepted = reply["accepted"] | 0;
    result.duplicates = reply["duplicates"] | 0;
  }
  return result;
}

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
