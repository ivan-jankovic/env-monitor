// Fixed-size ring buffer of samples waiting to be sent. Lives in RAM, so it
// is lost on reset (flash-backed buffering is a later option). Plain C++, no
// Arduino dependencies, so it can be tested on the computer.
#pragma once

#include <stddef.h>
#include <stdint.h>

// One reading of both sensors, packed into 8 bytes. Values are in tenths,
// matching the DHT22's 0.1 resolution.
struct Sample {
  uint32_t ts;            // UTC epoch seconds (fits until 2106)
  int16_t temperature10;  // -400..800 for -40.0..80.0 C
  uint16_t humidity10;    // 0..1000 for 0.0..100.0 %
};

template <size_t Capacity>
class SampleBuffer {
 public:
  // Append a sample. When full, the oldest sample is dropped to make room;
  // returns false in that case.
  bool push(const Sample& sample) {
    bool kept = true;
    if (count_ == Capacity) {
      head_ = (head_ + 1) % Capacity;
      count_--;
      dropped_++;
      kept = false;
    }
    items_[(head_ + count_) % Capacity] = sample;
    count_++;
    return kept;
  }

  // Copy up to max of the oldest samples into out, oldest first.
  size_t peek(Sample* out, size_t max) const {
    const size_t n = max < count_ ? max : count_;
    for (size_t i = 0; i < n; i++) {
      out[i] = items_[(head_ + i) % Capacity];
    }
    return n;
  }

  // Remove the n oldest samples (after they were sent).
  void pop(size_t n) {
    if (n > count_) {
      n = count_;
    }
    head_ = (head_ + n) % Capacity;
    count_ -= n;
  }

  size_t size() const { return count_; }
  bool empty() const { return count_ == 0; }
  uint32_t dropped() const { return dropped_; }

 private:
  Sample items_[Capacity];
  size_t head_ = 0;  // index of the oldest sample
  size_t count_ = 0;
  uint32_t dropped_ = 0;
};
