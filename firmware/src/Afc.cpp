#include "Afc.h"
#include <algorithm>

namespace chime {
// C++11: std::min/max bind references, so the constants need definitions.
constexpr int TrimTracker::Limit, TrimTracker::Hysteresis;
constexpr size_t TrimTracker::Window, TrimTracker::MinSamples;
void TrimTracker::add(int8_t trimAtReceive, int8_t freqest) {
  const int implied = std::max(-Limit, std::min(Limit, int(trimAtReceive) + int(freqest)));
  samples_[next_] = int8_t(implied);
  next_ = (next_ + 1) % Window;
  if (count_ < Window) ++count_;
}
bool TrimTracker::estimate(int8_t &trim) const {
  if (count_ < MinSamples) return false;
  int8_t sorted[Window];
  std::copy(samples_, samples_ + count_, sorted);
  std::sort(sorted, sorted + count_);
  // Even counts take the lower middle: deterministic, no rounding.
  trim = sorted[(count_ - 1) / 2];
  return true;
}
bool TrimTracker::decide(int8_t current, bool calibrated, int8_t &target) const {
  int8_t median;
  if (!estimate(median)) return false;
  const int change = int(median) - int(current);
  if (calibrated && change < Hysteresis && change > -Hysteresis) return false;
  target = median;
  return true;
}
}
