#include "Doorbells.h"
#include <cstring>

namespace chime {
void Doorbells::init(uint32_t nodeId, uint32_t bootWord, LinkCrypto crypto, void *context,
                     const LinkProfile *profiles, size_t count) {
  nodeId_ = nodeId; bootWord_ = bootWord; crypto_ = crypto; context_ = context;
  count_ = profiles && count <= MaxDoorbells ? count : 0;
  pairSlot_ = 0; next_ = 0; dirty_ = false;
  std::memset(names_, 0, sizeof(names_));
  for (size_t i = 0; i < MaxDoorbells; ++i)
    links_[i].init(nodeId, bootWord, crypto, context, i < count_ ? profiles[i] : LinkProfile{});
}
void Doorbells::reset(size_t slot) {
  links_[slot].init(nodeId_, bootWord_, crypto_, context_, LinkProfile{});
  names_[slot][0] = 0;
}
void Doorbells::setName(size_t slot, const char *name) {
  if (slot >= MaxDoorbells) return;
  std::strncpy(names_[slot], name ? name : "", DoorbellNameSize - 1);
  names_[slot][DoorbellNameSize - 1] = 0;
}
bool Doorbells::adding() const {
  return count_ < MaxDoorbells && links_[count_].busy();
}
bool Doorbells::startPair(size_t slot, uint32_t now) {
  if (slot > count_ || slot >= MaxDoorbells || pairing()) return false;
  // A rejoin with this slot's old doorbell must not block pairing its replacement.
  if (links_[slot].busy()) links_[slot].cancel(now);
  if (slot == count_) reset(slot); // A fresh slot, not an earlier failed attempt.
  if (!links_[slot].startPair(now)) return false;
  pairSlot_ = slot;
  return true;
}
bool Doorbells::startJoin(size_t slot, uint32_t now) {
  return slot < count_ && links_[slot].startJoin(now);
}
size_t Doorbells::startJoinAll(uint32_t now) {
  size_t started = 0;
  for (size_t i = 0; i < count_; ++i)
    if (!links_[i].pairing() && links_[i].startJoin(now)) ++started;
  return started;
}
void Doorbells::cancel(uint32_t now) {
  for (size_t i = 0; i < limit(); ++i)
    if (links_[i].busy()) links_[i].cancel(now);
}
void Doorbells::tick(uint32_t now) {
  for (size_t i = 0; i < limit(); ++i) links_[i].tick(now);
}
const char *Doorbells::receive(const Packet &packet, uint32_t now, size_t &slot) {
  const char *result = nullptr;
  // A pairing response from a doorbell that another slot already holds ends
  // that pairing unacknowledged, so the doorbell keeps its existing key.
  size_t refused = MaxDoorbells;
  const uint8_t *mac = packet.bytes + 1;
  if (validPacket(packet) && mac[0] == 32 && (mac[1] & 0x3F) == 1 && mac[8] == 0 &&
      read32le(mac + 4) == nodeId_) {
    const int holder = find(read32le(mac + 12));
    for (size_t i = 0; i < limit(); ++i)
      if (links_[i].pairing() && !links_[i].peer() && holder >= 0 && size_t(holder) != i) {
        links_[i].abortPairing("doorbell already paired");
        refused = i; result = links_[i].state(); slot = i;
      }
  }
  bool busyLinks[MaxDoorbells];
  for (size_t i = 0; i < limit(); ++i) busyLinks[i] = links_[i].busy();
  for (int pass = 0; pass < 2; ++pass)
    for (size_t i = 0; i < limit(); ++i) {
      if (i == refused || busyLinks[i] != (pass == 0)) continue;
      const char *answer = links_[i].receive(packet, now);
      if (answer && !result) { result = answer; slot = i; }
    }
  return result;
}
const AirFrame *Doorbells::peek(uint32_t now, size_t &slot) const {
  const size_t n = limit();
  for (size_t k = 0; k < n; ++k) {
    const size_t i = (next_ + k) % n;
    if (const AirFrame *frame = links_[i].peek(now)) { slot = i; return frame; }
  }
  return nullptr;
}
void Doorbells::consume(size_t slot, bool success, uint32_t now) {
  if (slot >= limit()) return;
  const bool wasDirty = links_[slot].profileDirty();
  links_[slot].consume(success, now);
  if (success) next_ = slot + 1;
  if (wasDirty || !links_[slot].profileDirty()) return;
  dirty_ = true;
  if (slot == count_) ++count_;
}
bool Doorbells::remove(size_t slot) {
  if (slot >= count_) return false;
  const size_t end = limit();
  for (size_t i = slot; i + 1 < end; ++i) {
    links_[i] = links_[i + 1];
    std::memcpy(names_[i], names_[i + 1], DoorbellNameSize);
  }
  reset(end - 1);
  --count_;
  if (pairSlot_ > slot) --pairSlot_;
  else if (pairSlot_ == slot) pairSlot_ = 0;
  next_ = 0; dirty_ = true;
  return true;
}
int Doorbells::find(uint32_t doorbell) const {
  if (!doorbell) return -1;
  for (size_t i = 0; i < count_; ++i)
    if (links_[i].profile().source == doorbell) return int(i);
  return -1;
}
bool Doorbells::busy() const {
  for (size_t i = 0; i < limit(); ++i) if (links_[i].busy()) return true;
  return false;
}
bool Doorbells::pairing() const {
  for (size_t i = 0; i < limit(); ++i) if (links_[i].pairing()) return true;
  return false;
}
void Doorbells::clearDirty() {
  dirty_ = false;
  for (auto &link : links_) link.clearProfileDirty();
}
}
