#include "Afc.h"
#include <algorithm>

namespace chime {
// C++11: std::min/max bind references, so the constants need definitions.
constexpr int TrimTracker::Limit, TrimTracker::Hysteresis;
constexpr size_t TrimTracker::Window, TrimTracker::MinSamples;
constexpr size_t TrimSet::Capacity;
constexpr int TrimSet::MaxSpread;
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

void TrimSet::reset() { for (auto &e : entries_) e = Entry{}; }
void TrimSet::restart() {
  for (auto &e : entries_) { e.tracker.reset(); e.fresh = false; }
}
const TrimSet::Entry *TrimSet::find(uint32_t id) const {
  for (const auto &e : entries_) if (e.used && e.id == id) return &e;
  return nullptr;
}
TrimSet::Entry *TrimSet::entry(uint32_t id) {
  for (auto &e : entries_) if (e.used && e.id == id) return &e;
  for (auto &e : entries_) if (!e.used) { e = Entry{}; e.used = true; e.id = id; return &e; }
  return nullptr; // Full: callers retain() the current doorbells.
}
void TrimSet::remove(uint32_t id) {
  for (auto &e : entries_) if (e.used && e.id == id) e = Entry{};
}
void TrimSet::retain(const uint32_t *ids, size_t count) {
  for (auto &e : entries_) {
    if (!e.used) continue;
    bool keep = false;
    for (size_t i = 0; i < count; ++i) if (ids[i] == e.id) keep = true;
    if (!keep) e = Entry{};
  }
}
void TrimSet::seed(uint32_t id, int8_t trim) {
  Entry *e = entry(id);
  if (e && !e->hasEstimate) {
    e->estimate = int8_t(std::max(-TrimTracker::Limit, std::min(TrimTracker::Limit, int(trim))));
    e->hasEstimate = true;
  }
}
void TrimSet::add(uint32_t id, int8_t trimAtReceive, int8_t freqest) {
  Entry *e = entry(id);
  if (!e) return;
  e->tracker.add(trimAtReceive, freqest);
  e->last = int8_t(std::max(-TrimTracker::Limit, std::min(TrimTracker::Limit, int(trimAtReceive) + int(freqest))));
  int8_t median;
  if (e->tracker.estimate(median)) { e->estimate = median; e->hasEstimate = true; e->fresh = true; }
}
bool TrimSet::estimate(uint32_t id, int8_t &trim) const {
  const Entry *e = find(id);
  if (!e || !e->hasEstimate) return false;
  trim = e->estimate;
  return true;
}
bool TrimSet::known(uint32_t id, int8_t &trim) const {
  const Entry *e = find(id);
  if (!e) return false;
  if (e->hasEstimate) trim = e->estimate;
  else if (e->tracker.count()) trim = e->last;
  else return false;
  return true;
}
bool TrimSet::range(int &lo, int &hi) const {
  bool any = false;
  for (const auto &e : entries_) {
    if (!e.used || !e.hasEstimate) continue;
    if (!any || e.estimate < lo) lo = e.estimate;
    if (!any || e.estimate > hi) hi = e.estimate;
    any = true;
  }
  return any;
}
bool TrimSet::midpoint(int8_t &trim) const {
  int lo = 0, hi = 0;
  if (!range(lo, hi)) return false;
  // Integer division rounds a half step towards zero: deterministic.
  trim = int8_t((lo + hi) / 2);
  return true;
}
int TrimSet::spread() const {
  int lo = 0, hi = 0;
  return range(lo, hi) ? hi - lo : 0;
}
bool TrimSet::decide(int8_t current, bool calibrated, int8_t &target) const {
  bool fresh = false;
  for (const auto &e : entries_) if (e.used && e.fresh) fresh = true;
  int8_t middle;
  if (!fresh || !midpoint(middle)) return false;
  const int change = int(middle) - int(current);
  if (calibrated && change < TrimTracker::Hysteresis && change > -TrimTracker::Hysteresis) return false;
  target = middle;
  return true;
}
}
