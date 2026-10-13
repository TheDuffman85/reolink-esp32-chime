#include "Link.h"
#include <cstring>

namespace chime {
namespace {
uint16_t word(const uint8_t *p) { return p[0] | (uint16_t(p[1]) << 8); }
constexpr uint32_t PairTimeout = 300000, JoinTimeout = 22000;
constexpr uint32_t RequestInterval = 2000, HeartbeatInterval = 25000;
}
void Link::init(uint32_t nodeId, uint32_t bootWord, LinkCrypto crypto, void *context,
                const LinkProfile &profile) {
  *this = Link{};
  nodeId_ = nodeId; bootWord_ = bootWord; crypto_ = crypto; context_ = context;
  profile_ = profile;
}
bool Link::startPair(uint32_t now) {
  if (state_ == State::Discovering || state_ == State::PairAck ||
      state_ == State::Joining || state_ == State::JoinAck) return false;
  if (!nodeId_ || !crypto_) return false;
  pending_ = false; pairing_ = true; first_ = true; started_ = now;
  attempts_ = 0; candidate_ = LinkProfile{};
  state_ = State::Discovering;
  return true;
}
bool Link::startJoin(uint32_t now) {
  if (state_ == State::Discovering || state_ == State::PairAck ||
      state_ == State::Joining || state_ == State::JoinAck) return false;
  if (!profile_.paired || !profile_.source || !profile_.destination || !crypto_) return false;
  pending_ = false; pairing_ = false; first_ = true; started_ = now;
  candidate_ = profile_; candidate_.pair = 0; attempts_ = 0;
  state_ = State::Joining;
  return true;
}
void Link::cancel(uint32_t) {
  pending_ = false; pairing_ = false; candidate_ = LinkProfile{};
  // A previous session cannot safely resume after an interrupted join.
  state_ = State::Idle;
}
void Link::fail(const char *reason) {
  pending_ = false; pairing_ = false; candidate_ = LinkProfile{};
  failure_ = reason; state_ = State::Failed;
}
bool Link::queue(uint8_t type, const uint8_t *plain, size_t length, Tx kind) {
  if (pending_) return false;
  uint8_t mac[MaxMac]{};
  const size_t header = type == 0 || type == 1 ? 8 : type == 2 ? 12 : 16;
  if (header + length > sizeof(mac)) return false;
  mac[0] = header + length; mac[1] = type;
  const bool joined = state_ == State::Joined;
  const auto &p = joined ? profile_ : candidate_;
  if (type <= 1) write32le(mac + 4, nodeId_);
  else {
    // Node-originated encrypted frames place source before destination;
    // AP-originated frames place destination before source.
    write32le(mac + 4, p.destination); write32le(mac + 8, p.source);
    if (type == 3) {
      write16le(mac + 2, p.pair); write32le(mac + 12, sequence_ + 1);
    }
  }
  if (length) std::memcpy(mac + header, plain, length);
  if (type >= 2 && (!crypto_ || !crypto_(context_, p.key, mac + header, length))) return false;
  if (!encode(mac, header + length, air_)) return false;
  tx_ = kind; pending_ = true; retry_ = false; txFailures_ = 0;
  return true;
}
bool Link::queuePairAck(Tx kind) {
  uint8_t reply[24]; std::memcpy(reply, pairReply_, sizeof(reply));
  reply[0] = 1; reply[1] = 0; reply[2] = 0;
  return queue(1, reply, sizeof(reply), kind);
}
void Link::tick(uint32_t now) {
  if (pairing_ && uint32_t(now - started_) >= PairTimeout) { fail(); return; }
  if (!pairing_ && (state_ == State::Joining || state_ == State::JoinAck) &&
      uint32_t(now - started_) >= JoinTimeout) { fail(); return; }
  if (pending_) return;
  if (state_ == State::Discovering && (first_ || uint32_t(now - lastTx_) >= RequestInterval))
    queue(0, nullptr, 0, Tx::Discovery);
  else if (state_ == State::Joining && (first_ || uint32_t(now - lastTx_) >= RequestInterval)) {
    if (attempts_ >= 10) { fail(); return; }
    uint8_t request[8]{}; request[0] = 2; write32le(request + 4, bootWord_);
    queue(2, request, sizeof(request), Tx::JoinRequest);
  } else if (state_ == State::Joined && profile_.heartbeatMode == 0 &&
             uint32_t(now - lastTx_) >= HeartbeatInterval) {
    const uint8_t heartbeat = 13;
    queue(3, &heartbeat, 1, Tx::Heartbeat);
  }
}
bool Link::sendData(const uint8_t *plain, size_t length) {
  if (state_ != State::Joined || !plain || !length) return false;
  return queue(3, plain, length, Tx::Data);
}
void Link::consume(bool success, uint32_t now) {
  if (!pending_) return;
  if (!success) {
    ++failures_; lastAttempt_ = now; retry_ = true;
    if (++txFailures_ >= 3) fail("radio transmission failed");
    return;
  }
  pending_ = false; ++transmitted_; lastTx_ = now; first_ = false;
  switch (tx_) {
    case Tx::Discovery: break;
    case Tx::PairAck:
      state_ = State::Joining; first_ = true; attempts_ = 0; break;
    case Tx::PairReplay: break;
    case Tx::JoinRequest: ++attempts_; break;
    case Tx::JoinAck:
      if (state_ == State::JoinAck) {
        profile_ = candidate_; profile_.paired = true;
        // Every newly completed join resets receive duplicate history even if
        // the AP randomly issues the same 16-bit session identifier again.
        dirty_ = true; pairing_ = false;
        sequence_ = 0; receiveSequence_ = 0; state_ = State::Joined;
      }
      break;
    case Tx::Heartbeat: case Tx::Data: ++sequence_; break;
  }
}
const char *Link::receive(const Packet &packet, uint32_t now) {
  if (!validPacket(packet)) return nullptr;
  const uint8_t *mac = packet.bytes + 1;
  const size_t length = mac[0];
  const uint8_t type = mac[1] & 0x3F;
  if (type == 1) {
    if (length != 32 || read32le(mac + 4) != nodeId_ || mac[8] != 0) return nullptr;
    const uint32_t ap = read32le(mac + 12);
    if (!ap || ap == nodeId_) return "invalid pairing response";
    if (state_ == State::Discovering) {
      candidate_ = LinkProfile{}; candidate_.paired = true;
      candidate_.source = ap; candidate_.destination = nodeId_;
      candidate_.heartbeatMode = mac[9];
      std::memcpy(candidate_.key, mac + 16, 16);
      std::memcpy(pairReply_, mac + 8, 24);
      pending_ = false; state_ = State::PairAck;
      if (!queuePairAck(Tx::PairAck)) { fail("pair acknowledgement failed"); return "pair acknowledgement failed"; }
      return "pairing response accepted";
    }
    const auto &p = state_ == State::Joined ? profile_ : candidate_;
    if ((state_ == State::PairAck || state_ == State::Joining || state_ == State::Joined) &&
        p.paired && ap == p.source && !std::memcmp(mac + 16, p.key, 16)) {
      if (!pending_) {
        std::memcpy(pairReply_, mac + 8, 24);
        queuePairAck(state_ == State::PairAck ? Tx::PairAck : Tx::PairReplay);
      }
      return "pairing response repeated";
    }
    return nullptr;
  }
  if (type == 2) {
    if (length != 24 || (state_ != State::Joining && state_ != State::JoinAck && state_ != State::Joined))
      return nullptr;
    const auto &p = state_ == State::Joined ? profile_ : candidate_;
    if (read32le(mac + 4) != p.destination || read32le(mac + 8) != p.source) return nullptr;
    uint8_t plain[12]; std::memcpy(plain, mac + 12, sizeof(plain));
    if (!crypto_(context_, p.key, plain, sizeof(plain))) return "join decryption failed";
    if (plain[0] != 3 || plain[1] != 6 || read32le(plain + 4) != 0)
      return "invalid join response";
    const uint16_t pair = word(plain + 2);
    if (state_ == State::Joined && pair != profile_.pair) return "stale join response";
    if (state_ == State::JoinAck && pair != candidate_.pair) return "stale join response";
    // AP retries must not restart the same radio retry budget or bypass its
    // backoff while the acknowledgement is still waiting to transmit.
    if (pending_ && tx_ == Tx::JoinAck) return "join response repeated";
    if (state_ == State::Joining) {
      candidate_.pair = pair; state_ = State::JoinAck;
    }
    std::memcpy(joinReply_, plain, sizeof(plain));
    joinReply_[0] = 4; write32le(joinReply_ + 8, bootWord_);
    // An acknowledgement takes priority over an unsent request or heartbeat.
    pending_ = false;
    if (!queue(2, joinReply_, sizeof(joinReply_), Tx::JoinAck)) return "join acknowledgement failed";
    return "join response accepted";
  }
  if (type == 4 && state_ == State::Joined && length >= 16 &&
      read32le(mac + 4) == profile_.destination && read32le(mac + 8) == profile_.source) {
    // Vendor maps addressed type-4 frames to DATA_DIS_CONNECT (22), then
    // clears the session and joins again without removing the pairing key.
    startJoin(now);
    return "doorbell requested rejoin";
  }
  if (type == 3 && state_ == State::Joined && length >= 17 &&
      read32le(mac + 4) == profile_.destination && read32le(mac + 8) == profile_.source &&
      word(mac + 2) == profile_.pair) {
    const uint32_t sequence = read32le(mac + 12);
    if (sequence < receiveSequence_) return "stale data frame";
    uint8_t plain[MaxMac - 16];
    std::memcpy(plain, mac + 16, length - 16);
    if (!crypto_(context_, profile_.key, plain, length - 16)) return "data decryption failed";
    receiveSequence_ = sequence;
    if (plain[0] == 22) {
      startJoin(now);
      return "doorbell requested rejoin";
    }
  }
  return nullptr;
}
const char *Link::state() const {
  switch (state_) {
    case State::Idle: return "idle";
    case State::Discovering: return "discovering";
    case State::PairAck: return "pair acknowledgement";
    case State::Joining: return "joining";
    case State::JoinAck: return "join acknowledgement";
    case State::Joined: return "joined";
    case State::Failed: return failure_;
  }
  return "idle";
}
}
