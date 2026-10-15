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

// One receive trim for several doorbells, whose crystals differ slightly.
// Each doorbell (by ID) has its own tracker and keeps its latest estimate.
// The receive target is the midpoint of those estimates, so the doorbells
// furthest apart are both off by half their spread. Transmissions can use
// each doorbell's own value (known()). With one doorbell this is exactly
// TrimTracker. ID 0 collects frames before any doorbell is known.
class TrimSet {
 public:
  static constexpr size_t Capacity = 6;  // Four doorbells, a pairing candidate, ID 0
  // Beyond this spread (about 25 kHz) the midpoint leaves a doorbell more
  // than 8 steps off, outside the 81.25 kHz filter's margin for the ~55 kHz
  // signal: reception should stay wide.
  static constexpr int MaxSpread = 16;
  void reset();          // Forgets everything, e.g. after a frequency change.
  void restart();        // New samples needed; estimates are kept.
  void remove(uint32_t id);
  // Keeps only these IDs (the paired doorbells and a pairing candidate).
  void retain(const uint32_t *ids, size_t count);
  // A stored estimate, e.g. from before a restart; a measured one wins.
  void seed(uint32_t id, int8_t trim);
  void add(uint32_t id, int8_t trimAtReceive, int8_t freqest);
  // Latest median estimate of one doorbell (kept across restart()).
  bool estimate(uint32_t id, int8_t &trim) const;
  // Best transmit trim for one doorbell: the current median, else the kept
  // estimate, else its newest single sample.
  bool known(uint32_t id, int8_t &trim) const;
  // Midpoint of all estimates and their spread (0 with fewer than two).
  bool midpoint(int8_t &trim) const;
  int spread() const;
  // Like TrimTracker::decide for the midpoint, once some doorbell has
  // MinSamples new frames since the last restart().
  bool decide(int8_t current, bool calibrated, int8_t &target) const;
 private:
  struct Entry { uint32_t id = 0; bool used = false, hasEstimate = false, fresh = false;
                 int8_t estimate = 0, last = 0; TrimTracker tracker; };
  bool range(int &lo, int &hi) const;
  const Entry *find(uint32_t id) const;
  Entry *entry(uint32_t id); // Finds or allocates.
  Entry entries_[Capacity];
};
}
