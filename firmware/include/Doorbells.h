#pragma once
#include "Link.h"

namespace chime {
// Slot 0 is the first doorbell (stored with Settings); further slots are the
// experimental additional doorbells.
constexpr size_t MaxDoorbells = 4;
constexpr size_t DoorbellNameSize = 25; // Local label, 24 bytes and a NUL.

// Links to several doorbells at once. Slots 0..count()-1 hold a doorbell;
// slot count() may be pairing a new one. Every link keeps its own key,
// session, heartbeat and retries. Frames carry the chime and doorbell IDs,
// so each link only accepts its own; one radio sends for all of them in turn.
class Doorbells {
 public:
  void init(uint32_t nodeId, uint32_t bootWord, LinkCrypto crypto, void *context,
            const LinkProfile *profiles, size_t count);
  size_t count() const { return count_; }
  // Occupied slots plus a doorbell that is being added.
  size_t active() const { return count_ + (adding() ? 1 : 0); }
  Link &link(size_t slot) { return links_[slot]; }
  const Link &link(size_t slot) const { return links_[slot]; }
  const char *name(size_t slot) const { return names_[slot]; }
  void setName(size_t slot, const char *name);
  // Slot of the latest pairing; its link reports the pairing progress.
  size_t pairSlot() const { return pairSlot_; }
  // Pairs a replacement for an occupied slot, or a new doorbell with
  // slot == count(). One pairing at a time; other links keep running.
  bool startPair(size_t slot, uint32_t now);
  bool startJoin(size_t slot, uint32_t now);
  // Rejoins every paired doorbell; returns how many started.
  size_t startJoinAll(uint32_t now);
  // Ends pairing and join attempts. Joined links stay joined.
  void cancel(uint32_t now);
  void tick(uint32_t now);
  // Every link sees the frame. Returns the first result, busy links first,
  // and the slot that produced it. Each doorbell is paired at most once: a
  // pairing response from a doorbell in another slot fails that pairing
  // ("doorbell already paired") without acknowledging its key.
  const char *receive(const Packet &packet, uint32_t now, size_t &slot);
  // The next frame to send; links with pending frames take turns.
  const AirFrame *peek(uint32_t now, size_t &slot) const;
  // A completed join in the pairing slot adds it when it was a new slot.
  void consume(size_t slot, bool success, uint32_t now);
  // Removes a doorbell; later slots move down.
  bool remove(size_t slot);
  // Occupied slot of a doorbell ID, or -1.
  int find(uint32_t doorbell) const;
  bool busy() const;
  bool pairing() const;
  // Saved profiles changed: a join completed or a doorbell was removed.
  bool dirty() const { return dirty_; }
  void clearDirty();
 private:
  bool adding() const;
  size_t limit() const { return count_ < MaxDoorbells ? count_ + 1 : MaxDoorbells; }
  void reset(size_t slot);
  Link links_[MaxDoorbells];
  char names_[MaxDoorbells][DoorbellNameSize]{};
  uint32_t nodeId_ = 0, bootWord_ = 0;
  LinkCrypto crypto_ = nullptr;
  void *context_ = nullptr;
  size_t count_ = 0, pairSlot_ = 0;
  size_t next_ = 0;
  bool dirty_ = false;
};
}
