#include "RingLog.h"
#include "Protocol.h"

namespace chime {
const char *const RingSourceNames[RingSources] = {"doorbell", "web", "button", "mqtt", "serial"};
const char *const RingOutcomeNames[RingOutcomes] = {"played", "muted", "silent", "busy", "chime_muted"};

const RingEvent &RingLog::add(RingEvent event) {
  event.id = ++lastId_;
  if (!event.id) event.id = ++lastId_; // Zero means "no ring" to API clients.
  events_[next_] = event;
  const RingEvent &stored = events_[next_];
  next_ = (next_ + 1) % Capacity;
  if (size_ < Capacity) ++size_;
  return stored;
}
const RingEvent &RingLog::newest(size_t index) const {
  return events_[(next_ + Capacity - 1 - index) % Capacity];
}
void RingLog::clear() { next_ = 0; size_ = 0; } // IDs keep increasing.

// Image: 'R' 'L' version count, u32 last ID, then entries oldest first.
size_t RingLog::serialize(uint8_t *out, size_t capacity) const {
  const size_t total = HeaderSize + size_ * EntrySize;
  if (!out || capacity < total) return 0;
  out[0] = 'R'; out[1] = 'L'; out[2] = 1; out[3] = uint8_t(size_);
  write32le(out + 4, lastId_);
  for (size_t i = 0; i < size_; ++i) {
    const RingEvent &e = newest(size_ - 1 - i);
    uint8_t *p = out + HeaderSize + i * EntrySize;
    write32le(p, e.id); write32le(p + 4, e.epoch); write32le(p + 8, e.ms);
    write16le(p + 12, e.tone);
    p[14] = e.source; p[15] = e.outcome; p[16] = e.type; p[17] = e.sound;
    p[18] = e.volume; p[19] = e.level; p[20] = uint8_t(e.rssi); p[21] = e.flags;
  }
  return total;
}
bool RingLog::restore(const uint8_t *in, size_t length) {
  if (!in || length < HeaderSize || in[0] != 'R' || in[1] != 'L' || in[2] != 1) return false;
  const size_t count = in[3];
  if (count > Capacity || length != HeaderSize + count * EntrySize) return false;
  RingEvent events[Capacity];
  for (size_t i = 0; i < count; ++i) {
    const uint8_t *p = in + HeaderSize + i * EntrySize;
    RingEvent &e = events[i];
    e.id = read32le(p); e.epoch = read32le(p + 4); e.ms = read32le(p + 8);
    e.tone = uint16_t(p[12] | (p[13] << 8));
    e.source = p[14]; e.outcome = p[15]; e.type = p[16]; e.sound = p[17];
    e.volume = p[18]; e.level = p[19]; e.rssi = int8_t(p[20]);
    e.flags = uint8_t(p[21] | RingPreviousBoot);
    if (e.source >= RingSources || e.outcome >= RingOutcomes) return false;
  }
  for (size_t i = 0; i < count; ++i) events_[i] = events[i];
  size_ = count; next_ = count % Capacity; lastId_ = read32le(in + 4);
  return true;
}
}
