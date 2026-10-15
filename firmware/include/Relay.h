#pragma once
#include "Protocol.h"

namespace chime {
// Range extender (experimental). Frames between a doorbell paired with this
// chime and another chime are sent again unchanged, so a chime out of the
// doorbell's range can still hear it, and the other way round. No key is
// needed: the MAC header and CRC are in the clear and the encrypted payload is
// copied as is. Types 2-4 carry the chime ID at +4 and the doorbell ID at +8
// in both directions, so both directions of that conversation are relayed.
// Pairing is relayed only while the owner keeps a pairing window open:
// discovery (type 0) names no doorbell, and the type-1 key exchange carries
// the key in plaintext. Type-1 frames still need a paired doorbell at +12.
// Never relayed: unpaired doorbells, frames to or from this chime and
// anything without a valid CRC. One radio and one carrier: frames are stored
// and sent after reception has gone quiet, identical frames once, and a rate
// limit bounds the airtime.
struct RelayFrame { AirFrame air; uint32_t doorbell = 0, at = 0; bool inverted = false; };
class Relay {
 public:
  static constexpr size_t Capacity = 4;        // Frames waiting to be sent
  static constexpr size_t History = 16;        // Recently seen frames
  static constexpr size_t MaxChimes = 4;       // Other chimes listed in the status
  // An identical frame is relayed once per RepeatMs: one relay for the
  // doorbell's retry rounds (about 200 ms), each 2-second join retry, and at
  // most one discovery a second from a chime that sends them faster.
  static constexpr uint32_t RepeatMs = 1000;
  // Sent once nothing was received for QuietMs (the end of a retry burst),
  // at the latest after WaitMs; dropped unsent after ExpireMs.
  static constexpr uint32_t QuietMs = 30, WaitMs = 250, ExpireMs = 1000;
  // At most Burst frames at once, then one per RefillMs (20 a minute).
  // Pairing frames are bounded by the pairing window instead.
  static constexpr uint8_t Burst = 20;
  static constexpr uint32_t RefillMs = 3000;
  // Like the chime's own pairing window.
  static constexpr uint32_t PairingMs = 300000;
  enum class Verdict : uint8_t { Ignored, Queued, Repeat, Limited, Full };
  struct Chime { uint32_t id = 0, doorbell = 0, at = 0, frames = 0; };

  void reset();
  // The chime and doorbell of a frame that may be relayed: TI CRC, a nonzero
  // chime other than nodeId and, except for discovery, one of doorbells.
  // Type 2-4 with a plausible length; with pairing also type 0 discovery
  // (doorbell 0) and the 32-byte type-1 key response and echo.
  static bool eligible(const Packet &packet, uint32_t nodeId, const uint32_t *doorbells,
                       size_t count, bool pairing, uint32_t &chime, uint32_t &doorbell);
  Verdict offer(const Packet &packet, uint32_t nodeId, const uint32_t *doorbells,
                size_t count, uint32_t now);
  // The oldest frame due at now (lastReceive: time of the latest received
  // frame); expired frames are dropped first. consume() after sending it.
  const RelayFrame *peek(uint32_t now, uint32_t lastReceive);
  void consume(bool sent);
  // Relays pairing for PairingMs from now, or stops it.
  void openPairing(uint32_t now) { pairing_ = true; pairingAt_ = now; }
  void closePairing() { pairing_ = false; }
  bool pairing(uint32_t now) const { return pairing_ && uint32_t(now - pairingAt_) < PairingMs; }
  uint32_t pairingRemaining(uint32_t now) const { return pairing(now) ? PairingMs - (now - pairingAt_) : 0; }
  size_t pending() const { return count_; }
  size_t chimes() const;
  const Chime &chime(size_t index) const { return chimes_[index]; }
  // Since reset(): frames queued (pairing frames among them), sent, failed
  // to send, expired unsent, and refused by the rate limit or a full queue.
  uint32_t queued = 0, pairingQueued = 0, sent = 0, failed = 0, expired = 0, limited = 0, full = 0;
 private:
  struct Seen { uint32_t hash = 0, at = 0; bool used = false; };
  void refill(uint32_t now);
  void noteChime(uint32_t id, uint32_t doorbell, uint32_t now, bool queued);
  RelayFrame queue_[Capacity];
  size_t head_ = 0, count_ = 0, nextSeen_ = 0;
  Seen seen_[History];
  Chime chimes_[MaxChimes];
  uint8_t tokens_ = Burst;
  uint32_t refillAt_ = 0, pairingAt_ = 0;
  bool pairing_ = false;
};
}
