// Temperature and humidity sensor. This class is the only code that knows the
// sensor is a DHT22: to swap in an I2C sensor (GPIO 21/22 are kept free for
// it), reimplement env_sensor.cpp and keep this interface.
#pragma once

#include <stdint.h>

struct Measurement {
  float temperature;  // degrees C
  float humidity;     // % relative humidity
};

class EnvSensor {
 public:
  void begin();

  // Take one measurement, retrying a failed read. Blocks for up to about 4 s
  // while retrying, because the DHT22 needs 2 s between reads.
  bool read(Measurement& out);

  uint32_t consecutiveFailures() const { return failures_; }
  bool faulty() const;  // too many failed reads in a row

 private:
  uint32_t failures_ = 0;
};
