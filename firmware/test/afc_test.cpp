// Host tests for automatic trim: median, hysteresis, clamping and the
// absolute (trim + FREQEST) estimate across trim changes.
#include "Afc.h"
#include <cassert>
#include <iostream>

using namespace chime;

int main() {
  TrimTracker t; int8_t target = 0;
  // Owner bench, 2026-10-13: FREQEST 8..9 at trim 0 (+12.7..+14.3 kHz).
  t.add(0, 9); t.add(0, 8);
  assert(!t.decide(0, false, target));          // needs three frames
  t.add(0, 9);
  assert(t.decide(0, false, target) && target == 9);
  // After correction residuals are small: no change within the hysteresis,
  // but the first calibration is always confirmed.
  t.reset(); t.add(9, 0); t.add(9, 1); t.add(9, -1);
  assert(!t.decide(9, true, target));
  assert(t.decide(9, false, target) && target == 9);
  // Samples taken at different trims agree on the same absolute value.
  t.reset(); t.add(0, 12); t.add(5, 7); t.add(12, 0);
  assert(t.decide(0, true, target) && target == 12);
  // One outlier (e.g. another doorbell) does not move the median.
  t.reset(); t.add(0, 9); t.add(0, -30); t.add(0, 9); t.add(0, 10); t.add(0, 9);
  assert(t.decide(0, true, target) && target == 9);
  // Window keeps the five newest samples: slow drift is followed.
  for (int i = 0; i < 5; ++i) t.add(0, 12);
  assert(t.decide(9, true, target) && target == 12);
  // Large crystal errors are clamped to the safe range.
  t.reset(); for (int i = 0; i < 3; ++i) t.add(30, 100);
  assert(t.decide(0, true, target) && target == TrimTracker::Limit);
  t.reset(); for (int i = 0; i < 3; ++i) t.add(-30, -100);
  assert(t.decide(0, true, target) && target == -TrimTracker::Limit);
  // Even sample counts use the lower middle.
  t.reset(); t.add(0, 4); t.add(0, 8); t.add(0, 6); t.add(0, 10);
  assert(t.estimate(target) && target == 6);
  std::cout << "AFC tests passed: median, hysteresis, first calibration, outliers, drift, clamping\n";
}
