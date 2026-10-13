#pragma once
#include <cstddef>
#include <cstdint>

namespace chime {
// Automatic frequency correction from the CC1101 FREQEST register. FREQEST
// and the FSCTRL0 trim share one unit (26 MHz / 2^14, about 1.587 kHz), and
// TI's permanent compensation adds FREQEST to the trim. Each sample is the
// trim a frame implies: trim at reception + FREQEST. A median over recent
// frames ignores single outliers, e.g. another doorbell nearby.
class TrimTracker {
 public:
  static constexpr int Limit = 40;      // About ±63 kHz, ±73 ppm at 868 MHz, ±69 at 915
  static constexpr int Hysteresis = 2;  // Ignore drift below about 3.2 kHz
  static constexpr size_t Window = 5, MinSamples = 3;
  void reset() { count_ = 0; next_ = 0; }
  void add(int8_t trimAtReceive, int8_t freqest);
  size_t count() const { return count_; }
  // Median implied trim once MinSamples frames were added.
  bool estimate(int8_t &trim) const;
  // True when the estimate should replace current: always for the first
  // calibration, afterwards only beyond the hysteresis.
  bool decide(int8_t current, bool calibrated, int8_t &target) const;
 private:
  int8_t samples_[Window]{};
  size_t count_ = 0, next_ = 0;
};
}
