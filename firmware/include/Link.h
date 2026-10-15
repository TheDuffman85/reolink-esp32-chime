#pragma once
#include "Protocol.h"

namespace chime {
struct LinkProfile {
  bool paired = false;
  uint32_t source = 0, destination = 0;
  uint16_t pair = 0;
  uint8_t key[16]{};
  uint8_t heartbeatMode = 0;
};

// This callback applies AES-128 CTR in place with the all-zero counter.
using LinkCrypto = bool (*)(void *, const uint8_t[16], uint8_t *, size_t);
class Link {
 public:
  void init(uint32_t nodeId, uint32_t bootWord, LinkCrypto crypto, void *context,
            const LinkProfile &profile);
  bool startPair(uint32_t now);
  bool startJoin(uint32_t now);
  void cancel(uint32_t now);
  // Ends a pairing attempt with this reason as its state; no effect otherwise.
  void abortPairing(const char *reason) { if (pairing_) fail(reason); }
  void tick(uint32_t now);
  // Queues an encrypted type-3 application frame (e.g. a reply to a doorbell
  // request). Returns false while another frame is pending or not joined.
  bool sendData(const uint8_t *plain, size_t length);
  const char *receive(const Packet &packet, uint32_t now);
  const AirFrame *peek(uint32_t now) const {
    return pending_ && (!retry_ || uint32_t(now - lastAttempt_) >= 200) ? &air_ : nullptr;
  }
  void consume(bool success, uint32_t now);
  const LinkProfile &profile() const { return profile_; }
  // Doorbell this link talks to: the candidate during pairing/join (0 until
  // a pairing response names it), otherwise the saved profile's doorbell.
  uint32_t peer() const { return busy() ? candidate_.source : profile_.source; }
  bool profileDirty() const { return dirty_; }
  void clearProfileDirty() { dirty_ = false; }
  bool profileReady() const { return state_ == State::Joined; }
  bool pairing() const { return pairing_; }
  bool busy() const {
    return state_ == State::Discovering || state_ == State::PairAck ||
      state_ == State::Joining || state_ == State::JoinAck;
  }
  const char *state() const;
  uint32_t transmitted() const { return transmitted_; }
  uint32_t transmitFailures() const { return failures_; }
 private:
  enum class State : uint8_t { Idle, Discovering, PairAck, Joining, JoinAck, Joined, Failed };
  enum class Tx : uint8_t { Discovery, PairAck, PairReplay, JoinRequest, JoinAck, Heartbeat, Data };
  bool queue(uint8_t type, const uint8_t *plain, size_t length, Tx kind);
  bool queuePairAck(Tx kind);
  void fail(const char *reason = "timed out");
  State state_ = State::Idle;
  Tx tx_ = Tx::Discovery;
  LinkCrypto crypto_ = nullptr;
  const char *failure_ = "timed out";
  void *context_ = nullptr;
  LinkProfile profile_{}, candidate_{};
  AirFrame air_{};
  uint8_t pairReply_[24]{};
  uint8_t joinReply_[12]{};
  uint32_t nodeId_ = 0, bootWord_ = 0, started_ = 0, lastTx_ = 0;
  uint32_t sequence_ = 0, transmitted_ = 0, failures_ = 0;
  uint32_t lastAttempt_ = 0;
  uint32_t receiveSequence_ = 0;
  uint8_t attempts_ = 0, txFailures_ = 0;
  bool pending_ = false, dirty_ = false, pairing_ = false, first_ = false;
  bool retry_ = false;
};
}
