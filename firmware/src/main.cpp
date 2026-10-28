#include <Arduino.h>
#include <DHT.h>

const int DHT_PIN = 4;
DHT dht(DHT_PIN, DHT22);

void setup() {
  Serial.begin(115200);
  dht.begin();
}

void loop() {
  delay(5000);                       // the DHT22 needs at least 2 s between reads
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t)) {
    Serial.println("read failed");
    return;
  }
  Serial.printf("temperature=%.1f C  humidity=%.1f %%\n", t, h);
}

/*
#include <Arduino.h>

const int LED_PIN = 2;  // onboard LED on most DevKit V1 boards

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  Serial.println("ESP32 alive");
  digitalWrite(LED_PIN, LOW);   // LED off, and it stays off
}

void loop() {
  // digitalWrite(LED_PIN, HIGH);
  // Serial.println("LED on");
  // delay(1000);
  // digitalWrite(LED_PIN, LOW);
  // Serial.println("LED off");
  // delay(1000);
}

*/