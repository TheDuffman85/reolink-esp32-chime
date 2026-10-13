#include "Link.h"
#include <openssl/evp.h>
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

using namespace chime;
using Bytes = std::vector<uint8_t>;

namespace {
constexpr uint32_t Node = 0xA1B2C3D4, Ap = 0x11223344, Boot = 0x89ABCDEF;
constexpr uint16_t Pair = 0x4567;
const uint8_t Key[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};

bool aes(void *, const uint8_t key[16], uint8_t *bytes, size_t length) {
  EVP_CIPHER_CTX *context = EVP_CIPHER_CTX_new();
  if (!context) return false;
  uint8_t counter[16]{};
  int count = 0, final = 0;
  const bool ok = EVP_EncryptInit_ex(context, EVP_aes_128_ctr(), nullptr, key, counter) == 1 &&
    EVP_EncryptUpdate(context, bytes, &count, bytes, static_cast<int>(length)) == 1 &&
    EVP_EncryptFinal_ex(context, bytes + count, &final) == 1 &&
    size_t(count + final) == length;
  EVP_CIPHER_CTX_free(context);
  return ok;
}

Packet decode(const AirFrame &air) {
  Decoder decoder;
  Packet result;
  unsigned packets = 0;
  // Exercise the transmitted sync, whitening, PHY length and CRC together.
  for (size_t i = 0; i < air.size; ++i)
    for (int bit = 7; bit >= 0; --bit)
      if (decoder.feedBit((air.bytes[i] >> bit) & 1, result)) ++packets;
  assert(packets == 1 && validPacket(result));
  return result;
}

Packet packet(Bytes mac) {
  assert(mac.size() >= 2);
  mac[0] = static_cast<uint8_t>(mac.size());
  AirFrame air;
  assert(encode(mac.data(), mac.size(), air));
  return decode(air);
}

Bytes mac(const Packet &p) { return Bytes(p.bytes + 1, p.bytes + p.size - 2); }
Bytes outgoing(const Link &link, uint32_t now = 0) { assert(link.peek(now)); return mac(decode(*link.peek(now))); }
Bytes decrypt(Bytes m, size_t offset, const uint8_t key[16] = Key) {
  assert(aes(nullptr, key, m.data() + offset, m.size() - offset));
  return Bytes(m.begin() + offset, m.end());
}

LinkProfile profile(uint16_t pair = Pair) {
  LinkProfile p;
  p.paired = true; p.source = Ap; p.destination = Node; p.pair = pair;
  std::memcpy(p.key, Key, sizeof(Key));
  return p;
}

Packet pairResponse(uint32_t destination = Node) {
  Bytes m(32);
  m[1] = 1; write32le(m.data() + 4, destination);
  m[8] = 0; m[9] = 0xA9; m[10] = 0xBA; m[11] = 0xCB;
  write32le(m.data() + 12, Ap); std::memcpy(m.data() + 16, Key, sizeof(Key));
  return packet(m);
}

Packet joinResponse(uint16_t pair = Pair, uint32_t destination = Node,
                    uint8_t opcode = 3, uint8_t status = 6, uint32_t reserved = 0) {
  Bytes m(24);
  m[1] = 2; write32le(m.data() + 4, destination); write32le(m.data() + 8, Ap);
  m[12] = opcode; m[13] = status; write16le(m.data() + 14, pair);
  write32le(m.data() + 16, reserved); write32le(m.data() + 20, 0x12345678);
  assert(aes(nullptr, Key, m.data() + 12, 12));
  return packet(m);
}

void init(Link &link, const LinkProfile &p = LinkProfile{}) {
  link.init(Node, Boot, aes, nullptr, p);
}
void accepted(Link &link, const Packet &p, uint32_t now) { assert(link.receive(p, now)); }
void completeJoin(Link &link, uint32_t now) {
  link.tick(now);
  const Bytes request = outgoing(link);
  assert(request.size() == 20 && request[1] == 2);
  assert(read32le(request.data() + 4) == Node && read32le(request.data() + 8) == Ap);
  const Bytes plain = decrypt(request, 12);
  assert(plain == Bytes({2,0,0,0,0xEF,0xCD,0xAB,0x89}));
  link.consume(true, now);
  accepted(link, joinResponse(), now + 1);
  assert(!link.profileReady());
  const Bytes ack = outgoing(link);
  assert(ack.size() == 24 && ack[1] == 2);
  assert(read32le(ack.data() + 4) == Node && read32le(ack.data() + 8) == Ap);
  assert(decrypt(ack, 12) == Bytes({4,6,0x67,0x45,0,0,0,0,0xEF,0xCD,0xAB,0x89}));
  link.consume(true, now + 2);
  assert(link.profileReady() && !link.pairing());
  assert(link.profile().paired && link.profile().source == Ap && link.profile().destination == Node);
  assert(link.profile().pair == Pair && std::memcmp(link.profile().key, Key, 16) == 0);
}

void cryptoVector() {
  // Published AES-128 zero-key/zero-block vector checks the crypto callback
  // independently of the synthetic AP fixtures and encode/decode round trips.
  uint8_t zeroKey[16]{}, block[16]{};
  const uint8_t expected[16] = {0x66,0xE9,0x4B,0xD4,0xEF,0x8A,0x2C,0x3B,
                              0x88,0x4C,0xFA,0x59,0xCA,0x34,0x2B,0x2E};
  assert(aes(nullptr, zeroKey, block, sizeof(block)));
  assert(std::memcmp(block, expected, 16) == 0);
  assert(aes(nullptr, zeroKey, block, sizeof(block)));
  for (uint8_t byte : block) assert(byte == 0);
}

void pairingAndCommit() {
  Link link; init(link);
  assert(link.startPair(100) && link.pairing());
  link.tick(100);
  Bytes discovery = outgoing(link);
  assert(discovery.size() == 8 && discovery[1] == 0 && read32le(discovery.data() + 4) == Node);
  assert(!link.profile().paired && !link.profileDirty());
  link.consume(true, 100);
  link.tick(2099); assert(!link.peek(0));
  link.tick(2100); assert(outgoing(link) == discovery);
  // An AP reply supersedes a queued discovery retry.
  accepted(link, pairResponse(), 2101);
  Bytes pairAck = outgoing(link);
  Bytes expected = mac(pairResponse()); expected[8] = 1; expected[9] = 0; expected[10] = 0;
  assert(pairAck == expected);
  assert(!link.profile().paired && !link.profileReady() && !link.profileDirty());
  accepted(link, pairResponse(), 2102); assert(outgoing(link) == pairAck);
  link.consume(true, 2103);
  // A repeated pairing response can be answered without restarting the join.
  accepted(link, pairResponse(), 2104); assert(outgoing(link) == pairAck);
  link.consume(true, 2104);
  completeJoin(link, 4104);
  assert(link.profileDirty()); link.clearProfileDirty(); assert(!link.profileDirty());
  assert(link.profile().heartbeatMode == 0xA9);
  // Duplicated join responses get ACKs without resetting the session or NVS.
  accepted(link, joinResponse(), 4110); link.consume(true, 4110);
  assert(link.profileReady() && !link.profileDirty());
  assert(link.receive(joinResponse(Pair + 1), 4111)); assert(!link.peek(0));
  // Vendor pairing flags can disable the otherwise periodic heartbeat.
  link.tick(30000); assert(!link.peek(30000));
}

void rejectionAndPassiveMode() {
  Link link; init(link, profile());
  for (uint32_t now : {0U,2000U,25000U,60000U}) {
    link.tick(now); assert(!link.peek(0));
    assert(!link.receive(pairResponse(), now));
    assert(!link.receive(joinResponse(), now));
    assert(!link.peek(0));
  }
  assert(!link.profileReady() && !link.profileDirty());
  assert(link.startPair(0)); link.tick(0); const Bytes discovery = outgoing(link);
  assert(!link.receive(pairResponse(Node + 1), 1)); assert(outgoing(link) == discovery);
  auto invalid = pairResponse(); invalid.bytes[18] ^= 1;
  assert(!link.receive(invalid, 2)); assert(outgoing(link) == discovery);
  invalid = pairResponse(); invalid.crc = Crc::CcittDiagnostic;
  assert(!link.receive(invalid, 3)); assert(outgoing(link) == discovery);
  auto shortened = mac(pairResponse()); shortened.pop_back();
  assert(!link.receive(packet(shortened), 4)); assert(outgoing(link) == discovery);
  accepted(link, pairResponse(), 5); link.consume(true, 5); link.tick(6);
  const Bytes request = outgoing(link);
  assert(!link.receive(joinResponse(Pair, Node + 1), 7)); assert(outgoing(link) == request);
  Bytes wrongKeyReply = mac(joinResponse());
  uint8_t wrongKey[16]; std::memset(wrongKey, 0xAB, sizeof(wrongKey));
  assert(aes(nullptr, Key, wrongKeyReply.data() + 12, 12));
  assert(aes(nullptr, wrongKey, wrongKeyReply.data() + 12, 12));
  assert(link.receive(packet(wrongKeyReply), 8)); assert(outgoing(link) == request);
  for (const Packet &p : {joinResponse(Pair, Node, 4),
                          joinResponse(Pair, Node, 3, 7), joinResponse(Pair, Node, 3, 6, 1)}) {
    assert(link.receive(p, 8)); assert(outgoing(link) == request);
    assert(!link.profileDirty() && !link.profileReady());
  }
  invalid = joinResponse(); invalid.bytes[20] ^= 0x80;
  assert(!link.receive(invalid, 9)); assert(outgoing(link) == request);
  shortened = mac(joinResponse()); shortened.push_back(0);
  assert(!link.receive(packet(shortened), 10)); assert(outgoing(link) == request);
  link.cancel(11); assert(!link.peek(0) && !link.profileReady() && !link.pairing());
  assert(!link.receive(joinResponse(), 12));
  assert(link.profile().paired && link.profile().pair == Pair);
}

void bootRejoinHeartbeatAndDisconnect() {
  Link link; init(link, profile(0x9999));
  assert(link.startJoin(100)); completeJoin(link, 100);
  assert(link.profileDirty()); link.clearProfileDirty();
  link.tick(25101); assert(!link.peek(0));
  link.tick(25102);
  Bytes heartbeat = outgoing(link);
  assert(heartbeat.size() == 17 && heartbeat[1] == 3);
  assert(heartbeat[2] == 0x67 && heartbeat[3] == 0x45);
  assert(read32le(heartbeat.data() + 4) == Node && read32le(heartbeat.data() + 8) == Ap);
  assert(read32le(heartbeat.data() + 12) == 1 && decrypt(heartbeat, 16) == Bytes({13}));
  link.consume(true, 25102); link.tick(50102);
  heartbeat = outgoing(link); assert(read32le(heartbeat.data() + 12) == 2);
  link.consume(true, 50102);
  accepted(link, joinResponse(), 50103); link.consume(true, 50103);
  assert(!link.profileDirty());
  link.tick(75103);
  assert(read32le(outgoing(link).data() + 12) == 3);
  link.consume(true, 75103);
  Bytes ring(20); ring[1] = 3; write16le(ring.data() + 2, Pair);
  write32le(ring.data() + 4, Node); write32le(ring.data() + 8, Ap);
  write32le(ring.data() + 12, 99); ring[16] = 9; ring[18] = 5;
  assert(aes(nullptr, Key, ring.data() + 16, 4));
  assert(!link.receive(packet(ring), 75104)); assert(!link.peek(0));
  Bytes disconnect(16); disconnect[1] = 4;
  write32le(disconnect.data() + 4, Node + 1); write32le(disconnect.data() + 8, Ap);
  assert(!link.receive(packet(disconnect), 75105)); assert(link.profileReady());
  write32le(disconnect.data() + 4, Node);
  accepted(link, packet(disconnect), 75106);
  assert(!link.profileReady() && !link.profileDirty() && link.profile().paired);
  completeJoin(link, 75106); assert(link.profileDirty());

  // Stale and unrelated encrypted data cannot disconnect the active session.
  link.clearProfileDirty();
  write32le(ring.data() + 12, 99);
  assert(!link.receive(packet(ring), 75109));
  ring[16] = 22; ring[17] = 0; ring[18] = 0; ring[19] = 0;
  write32le(ring.data() + 12, 98);
  assert(aes(nullptr, Key, ring.data() + 16, 4));
  assert(link.receive(packet(ring), 75110)); assert(link.profileReady());
  write32le(ring.data() + 12, 100); write16le(ring.data() + 2, Pair + 1);
  assert(!link.receive(packet(ring), 75111)); assert(link.profileReady());
  write16le(ring.data() + 2, Pair); write32le(ring.data() + 8, Ap + 1);
  assert(!link.receive(packet(ring), 75112)); assert(link.profileReady());
  // The encrypted data disconnect opcode also initiates a fresh join.
  write32le(ring.data() + 8, Ap);
  accepted(link, packet(ring), 75113);
  assert(!link.profileReady() && link.profile().paired && !link.profileDirty());
}

void transmitFifoBounds() {
  AirFrame air;
  Bytes limit(MaxMac); limit[0] = MaxMac;
  assert(encode(limit.data(), limit.size(), air) && air.size == MaxAir);
  assert(mac(decode(air)) == limit);
  limit.push_back(0); limit[0] = MaxMac + 1;
  assert(!encode(limit.data(), limit.size(), air) && air.size == 0);
}

void boundedRadioRetriesAndZeroSession() {
  Link link; init(link, profile());
  const uint32_t start = UINT32_MAX - 100;
  assert(link.startJoin(start));
  assert(!link.startJoin(start + 1) && !link.startPair(start + 1));
  link.tick(start);
  const Bytes request = outgoing(link, start);
  for (unsigned attempt = 0; attempt < 3; ++attempt) {
    const uint32_t now = start + attempt * 200;
    assert(outgoing(link, now) == request);
    link.consume(false, now);
    assert(!link.peek(now) && !link.profileDirty() && !link.profileReady());
    if (attempt < 2) assert(!link.peek(now + 199));
  }
  link.tick(start + 10000);
  assert(!link.peek(start + 10000) && link.transmitFailures() == 3);
  assert(std::strcmp(link.state(), "radio transmission failed") == 0);
  assert(link.profile().paired && link.profile().pair == Pair);

  // Zero is a legitimate 16-bit session identifier, not an unpaired marker.
  assert(link.startJoin(start + 10001));
  accepted(link, joinResponse(0), start + 10002);
  const Bytes ack = decrypt(outgoing(link), 12);
  assert(ack[2] == 0 && ack[3] == 0);
  assert(!link.profileReady() && !link.profileDirty());
  link.consume(true, start + 10003);
  assert(link.profileReady() && link.profile().paired && link.profile().pair == 0);
  assert(link.profileDirty());
}

void failuresTimeoutCancelAndRollover() {
  // A failed final TX must not commit a replacement credential or pair ID.
  LinkProfile original = profile(0x9999); original.source = Ap + 123;
  std::memset(original.key, 0x55, sizeof(original.key));
  Link link; init(link, original);
  assert(link.startPair(0)); accepted(link, pairResponse(), 1);
  link.consume(false, 2); assert(link.transmitFailures() == 1);
  assert(!link.profileDirty() && link.profile().pair == 0x9999);
  assert(link.profile().source == original.source && std::memcmp(link.profile().key, original.key, 16) == 0);
  assert(!link.peek(201));
  link.tick(1000); assert(link.peek(1000)); link.consume(true, 1000);
  link.tick(1001); link.consume(true, 1001); accepted(link, joinResponse(), 1002);
  const Bytes ack = outgoing(link);
  link.consume(false, 1003);
  assert(!link.profileReady() && !link.profileDirty() && link.profile().pair == 0x9999);
  assert(link.profile().source == original.source && std::memcmp(link.profile().key, original.key, 16) == 0);
  // AP retries must preserve the queued ACK's radio retry budget/backoff.
  accepted(link, joinResponse(), 1004);
  assert(!link.peek(1004));
  assert(!link.peek(1202));
  link.tick(2000); assert(outgoing(link, 2000) == ack); link.consume(true, 2000);
  assert(link.profileReady() && link.profileDirty() && link.profile().pair == Pair);
  assert(link.profile().source == Ap && std::memcmp(link.profile().key, Key, 16) == 0);

  Link cancelled; init(cancelled, profile());
  assert(cancelled.startJoin(0)); accepted(cancelled, joinResponse(), 1);
  cancelled.cancel(2); cancelled.consume(true, 3); cancelled.tick(30000);
  assert(!cancelled.peek(0) && !cancelled.profileReady() && !cancelled.profileDirty());
  assert(cancelled.profile().paired && cancelled.profile().pair == Pair);

  Link timeout; init(timeout);
  const uint32_t start = UINT32_MAX - 500;
  assert(timeout.startPair(start)); timeout.tick(start); timeout.consume(true, start);
  timeout.tick(start + 1999); assert(!timeout.peek(0));
  timeout.tick(start + 2000); assert(timeout.peek(0)); timeout.consume(true, start + 2000);
  timeout.tick(start + 299999); assert(timeout.pairing());
  timeout.tick(start + 300000); assert(!timeout.peek(0) && !timeout.pairing());
  assert(std::strcmp(timeout.state(), "timed out") == 0 && !timeout.profileDirty());

  Link retries; init(retries, profile()); assert(retries.startJoin(start));
  for (uint32_t attempt = 0; attempt < 10; ++attempt) {
    const uint32_t now = start + attempt * 2000;
    retries.tick(now); assert(retries.peek(0)); retries.consume(true, now);
    retries.tick(now + 1999); assert(!retries.peek(0));
  }
  retries.tick(start + 20000);
  assert(!retries.peek(0) && !retries.profileReady() && !retries.profileDirty());
  assert(std::strcmp(retries.state(), "timed out") == 0 && retries.profile().paired);

  Link stalled; init(stalled, profile()); assert(stalled.startJoin(start));
  accepted(stalled, joinResponse(), start + 1);
  stalled.tick(start + 22000);
  assert(!stalled.peek(0) && !stalled.profileReady() && !stalled.profileDirty());
}
}

int main() {
  cryptoVector();
  pairingAndCommit();
  rejectionAndPassiveMode();
  bootRejoinHeartbeatAndDisconnect();
  boundedRadioRetriesAndZeroSession();
  failuresTimeoutCancelAndRollover();
  transmitFifoBounds();
  std::cout << "Link tests passed: AES vector, AP pairing/join/ACK simulation, commit gating, "
               "packet rejection, duplicates, heartbeat, disconnect, retry/timeout/cancel/rollover\n";
}
