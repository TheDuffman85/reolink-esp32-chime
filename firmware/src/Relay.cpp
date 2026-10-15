#include "Relay.h"

namespace chime {
namespace {
// FNV-1a over the PHY length and MAC: identical frames, identical hash.
uint32_t frameHash(const Packet &packet) {
  uint32_t hash = 2166136261u;
  for (size_t i = 0; i + 2 < packet.size; ++i) { hash ^= packet.bytes[i]; hash *= 16777619u; }
  return hash;
}
}
void Relay::reset() { *this = Relay{}; }
bool Relay::eligible(const Packet &packet, uint32_t nodeId, const uint32_t *doorbells,
                     size_t count, bool pairing, uint32_t &chime, uint32_t &doorbell) {
  if (!validPacket(packet) || !doorbells) return false;
  const uint8_t *mac = packet.bytes + 1;
  const size_t length = mac[0];
  const uint8_t type = mac[1] & 0x3F;
  chime = read32le(mac + 4);
  if (!chime || chime == nodeId) return false;
  // Discovery: any chime, but only while pairing is relayed.
  if (type == 0) { doorbell = 0; return pairing && length == 8; }
  // Key response (opcode 0) and the chime's echo (opcode 1) name the doorbell
  // in their payload.
  if (type == 1) {
    if (!pairing || length != 32 || mac[8] > 1) return false;
    doorbell = read32le(mac + 12);
  } else if ((type == 2 && (length == 20 || length == 24)) || (type == 3 && length >= 17) ||
             (type == 4 && length >= 16)) {
    doorbell = read32le(mac + 8);
  } else return false;
  if (!doorbell || doorbell == chime) return false;
  for (size_t i = 0; i < count; ++i) if (doorbells[i] == doorbell) return true;
  return false;
}
void Relay::refill(uint32_t now) {
  if (tokens_ >= Burst) { refillAt_ = now; return; }
  const uint32_t steps = uint32_t(now - refillAt_) / RefillMs;
  if (!steps) return;
  tokens_ = steps >= uint32_t(Burst - tokens_) ? Burst : uint8_t(tokens_ + steps);
  refillAt_ += steps * RefillMs;
}
void Relay::noteChime(uint32_t id, uint32_t doorbell, uint32_t now, bool counted) {
  if (!doorbell) return; // Discovery: not yet a chime of a paired doorbell.
  Chime *slot = nullptr;
  for (auto &c : chimes_) if (c.id == id && c.doorbell == doorbell) slot = &c;
  for (auto &c : chimes_) if (!slot && !c.id) slot = &c;
  if (!slot) {
    slot = &chimes_[0];
    for (auto &c : chimes_) if (uint32_t(now - c.at) > uint32_t(now - slot->at)) slot = &c;
    *slot = Chime{};
  }
  slot->id = id; slot->doorbell = doorbell; slot->at = now;
  if (counted) ++slot->frames;
}
Relay::Verdict Relay::offer(const Packet &packet, uint32_t nodeId, const uint32_t *doorbells,
                            size_t count, uint32_t now) {
  uint32_t chime, doorbell;
  if (!eligible(packet, nodeId, doorbells, count, pairing(now), chime, doorbell)) return Verdict::Ignored;
  const bool pairingFrame = (packet.bytes[2] & 0x3F) <= 1;
  const uint32_t hash = frameHash(packet);
  for (const auto &s : seen_)
    if (s.used && s.hash == hash && uint32_t(now - s.at) < RepeatMs) {
      noteChime(chime, doorbell, now, false);
      return Verdict::Repeat;
    }
  if (count_ == Capacity) { ++full; noteChime(chime, doorbell, now, false); return Verdict::Full; }
  Seen &entry = seen_[nextSeen_]; nextSeen_ = (nextSeen_ + 1) % History;
  entry.hash = hash; entry.at = now; entry.used = true;
  refill(now);
  if (!pairingFrame && !tokens_) { ++limited; noteChime(chime, doorbell, now, false); return Verdict::Limited; }
  RelayFrame &frame = queue_[(head_ + count_) % Capacity];
  // The packet passed validPacket(), so it encodes back to the same bytes.
  if (!encode(packet.bytes + 1, packet.bytes[0], frame.air)) return Verdict::Ignored;
  if (pairingFrame) ++pairingQueued; else --tokens_;
  frame.doorbell = doorbell; frame.at = now; frame.inverted = packet.inverted;
  ++count_; ++queued; noteChime(chime, doorbell, now, true);
  return Verdict::Queued;
}
const RelayFrame *Relay::peek(uint32_t now, uint32_t lastReceive) {
  while (count_ && uint32_t(now - queue_[head_].at) >= ExpireMs) {
    head_ = (head_ + 1) % Capacity; --count_; ++expired;
  }
  if (!count_) return nullptr;
  const RelayFrame &frame = queue_[head_];
  const bool quiet = uint32_t(now - lastReceive) >= QuietMs;
  return quiet || uint32_t(now - frame.at) >= WaitMs ? &frame : nullptr;
}
void Relay::consume(bool success) {
  if (!count_) return;
  head_ = (head_ + 1) % Capacity; --count_;
  if (success) ++sent; else ++failed;
}
size_t Relay::chimes() const {
  size_t n = 0;
  for (const auto &c : chimes_) if (c.id) ++n;
  return n;
}
}
