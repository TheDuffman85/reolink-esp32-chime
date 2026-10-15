// Host tests for automatic trim: median, hysteresis, clamping and the
// absolute (trim + FREQEST) estimate across trim changes.
#include "Afc.h"
#include <cassert>
#include <iostream>

using namespace chime;

void trimSetTests() {
  constexpr uint32_t Front = 0x11223344, Back = 0x55667788;
  TrimSet set; int8_t target = 0, trim = 0;
  // One doorbell behaves like TrimTracker.
  set.add(Front, 0, 9); set.add(Front, 0, 8);
  assert(!set.decide(0, false, target) && !set.estimate(Front, trim));
  assert(set.known(Front, trim) && trim == 8);   // Newest sample until a median exists
  set.add(Front, 0, 9);
  assert(set.decide(0, false, target) && target == 9 && set.spread() == 0);
  assert(set.estimate(Front, trim) && trim == 9 && set.known(Front, trim) && trim == 9);
  // A second doorbell 10 steps higher: reception meets in the middle.
  for (int i = 0; i < 3; ++i) set.add(Back, 9, 10);
  assert(set.midpoint(target) && target == 14 && set.spread() == 10);
  assert(set.decide(9, true, target) && target == 14);
  assert(set.known(Back, trim) && trim == 19 && set.known(Front, trim) && trim == 9);
  // Applying restarts the trackers: no decision without new frames, but the
  // estimates (and so the midpoint and transmit trims) stay.
  set.restart();
  assert(!set.decide(9, false, target) && set.midpoint(target) && target == 14);
  assert(set.known(Front, trim) && trim == 9);
  // New frames from one doorbell move only its estimate.
  for (int i = 0; i < 3; ++i) set.add(Front, 14, -3);  // Front now 11
  assert(set.decide(14, true, target) == false);       // Midpoint 15: within the hysteresis
  for (int i = 0; i < 3; ++i) set.add(Back, 14, 9);    // Back now 23
  assert(set.decide(14, true, target) && target == 17 && set.spread() == 12);
  // Odd sums round towards zero, also below zero.
  TrimSet negative;
  for (int i = 0; i < 3; ++i) { negative.add(1, 0, -3); negative.add(2, 0, -6); }
  assert(negative.midpoint(target) && target == -4 && negative.spread() == 3);
  // A seed does not replace a measured estimate, but fills a missing one.
  set.seed(Front, -30); assert(set.estimate(Front, trim) && trim == 11);
  set.seed(0x0C0FFEE0, 100); assert(set.estimate(0x0C0FFEE0, trim) && trim == TrimTracker::Limit);
  assert(set.spread() == TrimTracker::Limit - 11 && set.spread() > TrimSet::MaxSpread);
  // retain()/remove() drop forgotten doorbells and failed candidates.
  const uint32_t keep[] = {Front, Back};
  set.retain(keep, 2);
  assert(!set.estimate(0x0C0FFEE0, trim) && set.spread() == 12);
  set.remove(Back);
  assert(!set.known(Back, trim) && set.midpoint(target) && target == 11 && set.spread() == 0);
  // A full table ignores further IDs instead of evicting a doorbell.
  TrimSet full;
  for (uint32_t id = 1; id <= TrimSet::Capacity + 2; ++id) full.add(id, 0, int8_t(id));
  assert(full.known(1, trim) && trim == 1 && !full.known(TrimSet::Capacity + 1, trim));
  full.reset(); assert(!full.known(1, trim) && !full.midpoint(target));
}

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
  trimSetTests();
  std::cout << "AFC tests passed: median, hysteresis, first calibration, outliers, drift, clamping, "
               "doorbell midpoint, spread, kept estimates\n";
}
