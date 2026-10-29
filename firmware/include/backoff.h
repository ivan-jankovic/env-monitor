// Exponential backoff: each failure doubles the wait, up to a maximum.
// Plain C++, so it can be tested on the computer.
#pragma once

#include <stdint.h>

class Backoff {
 public:
  Backoff(uint32_t minMs, uint32_t maxMs) : min_(minMs), max_(maxMs), next_(minMs) {}

  // Record a failure and return how long to wait before the next attempt.
  uint32_t fail() {
    const uint32_t wait = next_;
    next_ = next_ > max_ / 2 ? max_ : next_ * 2;
    return wait;
  }

  void reset() { next_ = min_; }

 private:
  uint32_t min_;
  uint32_t max_;
  uint32_t next_;
};
