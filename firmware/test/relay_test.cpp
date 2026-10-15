// Host tests for the experimental range extender: which frames are relayed,
// unchanged on-air bytes, repeat suppression, quiet wait, expiry, queue and
// rate limits, and the list of other chimes.
#include "Relay.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

using namespace chime;
using Bytes = std::vector<uint8_t>;

namespace {
constexpr uint32_t Node = 0x1A2B3C4D, Other = 0x0C0FFEE5, Third = 0x0BEEF123;
constexpr uint32_t Front = 0x11223344, Stranger = 0x55667788;
const uint32_t Paired[] = {Front};

Packet decode(const AirFrame &air, bool inverted = false) {
  Decoder decoder;
  Packet result;
  unsigned packets = 0;
  for (size_t i = 0; i < air.size; ++i) {
    const uint8_t byte = inverted ? uint8_t(~air.bytes[i]) : air.bytes[i];
    for (int bit = 7; bit >= 0; --bit)
      if (decoder.feedBit((byte >> bit) & 1, result)) ++packets;
  }
  assert(packets == 1);
  return result;
}
Packet packet(Bytes mac) {
  mac[0] = static_cast<uint8_t>(mac.size());
  AirFrame air;
  assert(encode(mac.data(), mac.size(), air));
  const Packet p = decode(air);
  assert(validPacket(p));
  return p;
}
// Type 2-4 header with the chime ID at +4 and the doorbell ID at +8.
Packet frame(uint8_t type, size_t length, uint32_t chimeId, uint32_t doorbell, uint32_t sequence = 1) {
  Bytes m(length);
  m[1] = type; write16le(m.data() + 2, 0xBEEF);
  write32le(m.data() + 4, chimeId); write32le(m.data() + 8, doorbell);
  if (length >= 16) write32le(m.data() + 12, sequence);
  for (size_t i = 16; i < length; ++i) m[i] = uint8_t(0xA0 + i);
  return packet(m);
}
Packet ring(uint32_t chimeId, uint32_t doorbell, uint32_t sequence = 1) {
  return frame(3, 20, chimeId, doorbell, sequence);
}
Packet discovery(uint32_t chimeId) {
  Bytes m(8); m[1] = 0; write32le(m.data() + 4, chimeId);
  return packet(m);
}
// Type-1 key response (opcode 0) or the chime's echo (opcode 1).
Packet pairFrame(uint32_t chimeId, uint32_t doorbell, uint8_t opcode) {
  Bytes m(32); m[1] = 1; write32le(m.data() + 4, chimeId);
  m[8] = opcode; write32le(m.data() + 12, doorbell);
  for (size_t i = 16; i < 32; ++i) m[i] = uint8_t(i);
  return packet(m);
}
Relay::Verdict offer(Relay &r, const Packet &p, uint32_t now) {
  return r.offer(p, Node, Paired, 1, now);
}

void testSelection() {
  Relay r;
  uint32_t chime, doorbell;
  assert(Relay::eligible(ring(Other, Front), Node, Paired, 1, false, chime, doorbell));
  assert(chime == Other && doorbell == Front);
  // Frames to this chime are handled here, not relayed.
  assert(offer(r, ring(Node, Front), 0) == Relay::Verdict::Ignored);
  // Doorbells that are not paired with this chime are left alone.
  assert(offer(r, ring(Other, Stranger), 0) == Relay::Verdict::Ignored);
  assert(r.offer(ring(Other, Front), Node, nullptr, 0, 0) == Relay::Verdict::Ignored);
  // Join request/response lengths, data and disconnect frames.
  assert(Relay::eligible(frame(2, 20, Other, Front), Node, Paired, 1, false, chime, doorbell));
  assert(Relay::eligible(frame(2, 24, Other, Front), Node, Paired, 1, false, chime, doorbell));
  assert(!Relay::eligible(frame(2, 22, Other, Front), Node, Paired, 1, false, chime, doorbell));
  assert(Relay::eligible(frame(3, 17, Other, Front), Node, Paired, 1, false, chime, doorbell));
  assert(!Relay::eligible(frame(3, 16, Other, Front), Node, Paired, 1, false, chime, doorbell));
  assert(Relay::eligible(frame(4, 16, Other, Front), Node, Paired, 1, false, chime, doorbell));
  assert(!Relay::eligible(frame(4, 12, Other, Front), Node, Paired, 1, false, chime, doorbell));
  // Plaintext discovery and pairing only inside the pairing window.
  assert(!Relay::eligible(discovery(Other), Node, Paired, 1, false, chime, doorbell));
  assert(!Relay::eligible(pairFrame(Other, Front, 0), Node, Paired, 1, false, chime, doorbell));
  // Zero or self-referencing IDs.
  assert(!Relay::eligible(ring(0, Front), Node, Paired, 1, false, chime, doorbell));
  // A corrupted CRC.
  Packet bad = ring(Other, Front); bad.bytes[bad.size - 1] ^= 1; bad.crc = Crc::Bad;
  assert(!Relay::eligible(bad, Node, Paired, 1, false, chime, doorbell));
  assert(r.queued == 0 && r.chimes() == 0);
}

void testUnchangedAfterQuiet() {
  Relay r;
  const Packet p = ring(Other, Front);
  assert(offer(r, p, 1000) == Relay::Verdict::Queued && r.pending() == 1);
  // Not while frames keep arriving (a retry burst).
  assert(!r.peek(1010, 1000));
  assert(!r.peek(1029, 1000));
  const RelayFrame *f = r.peek(1030, 1000);
  assert(f && f->doorbell == Front && !f->inverted);
  const Packet again = decode(f->air);
  assert(again.size == p.size && !std::memcmp(again.bytes, p.bytes, p.size) && validPacket(again));
  r.consume(true);
  assert(r.sent == 1 && r.pending() == 0 && !r.peek(2000, 1000));
  assert(r.chimes() == 1 && r.chime(0).id == Other && r.chime(0).doorbell == Front && r.chime(0).frames == 1);
  // Inverted reception is relayed inverted.
  Packet inverted = ring(Other, Front, 2); inverted.inverted = true;
  assert(offer(r, inverted, 3000) == Relay::Verdict::Queued);
  f = r.peek(3100, 3000);
  assert(f && f->inverted);
  const Packet flipped = decode(f->air, true);
  assert(!std::memcmp(flipped.bytes, inverted.bytes, inverted.size));
  r.consume(false);
  assert(r.failed == 1);
}

void testRepeats() {
  Relay r;
  const Packet p = ring(Other, Front, 7);
  assert(offer(r, p, 0) == Relay::Verdict::Queued);
  // Retry rounds 20 ms apart within RepeatMs stay one relay.
  for (uint32_t t = 20; t < Relay::RepeatMs; t += 20) assert(offer(r, p, t) == Relay::Verdict::Repeat);
  // Copies that keep coming are relayed once per RepeatMs, not suppressed for good.
  assert(offer(r, p, Relay::RepeatMs) == Relay::Verdict::Queued);
  assert(offer(r, p, Relay::RepeatMs + 500) == Relay::Verdict::Repeat);
  // A later retry (e.g. a join request every 2 s) is relayed again.
  assert(offer(r, p, 2 * Relay::RepeatMs + 500) == Relay::Verdict::Queued);
  // Another sequence is another frame.
  assert(offer(r, ring(Other, Front, 8), 2600) == Relay::Verdict::Queued);
  assert(r.queued == 4 && r.chime(0).frames == 4);
}

void testWaitExpiryAndCapacity() {
  Relay r;
  assert(offer(r, ring(Other, Front, 1), 0) == Relay::Verdict::Queued);
  // Constant reception delays the frame at most WaitMs.
  assert(!r.peek(Relay::WaitMs - 1, Relay::WaitMs - 1));
  assert(r.peek(Relay::WaitMs, Relay::WaitMs));
  // Not sent in time: dropped.
  assert(!r.peek(Relay::ExpireMs, Relay::ExpireMs));
  assert(r.expired == 1 && r.pending() == 0);
  for (uint32_t i = 0; i < Relay::Capacity; ++i)
    assert(offer(r, ring(Other, Front, 10 + i), 2000) == Relay::Verdict::Queued);
  assert(offer(r, ring(Other, Front, 99), 2000) == Relay::Verdict::Full && r.full == 1);
  // A full queue does not remember the frame: its next round can still go.
  r.consume(true);
  assert(offer(r, ring(Other, Front, 99), 2020) == Relay::Verdict::Queued);
}

void testRateLimit() {
  Relay r;
  uint32_t now = 0, sequence = 0;
  for (unsigned i = 0; i < Relay::Burst; ++i) {
    assert(offer(r, ring(Other, Front, ++sequence), now) == Relay::Verdict::Queued);
    assert(r.peek(now + Relay::QuietMs, now)); r.consume(true);
    now += 10;
  }
  assert(offer(r, ring(Other, Front, ++sequence), now) == Relay::Verdict::Limited && r.limited == 1);
  // Its retry rounds are repeats, not further refusals.
  assert(offer(r, ring(Other, Front, sequence), now + 20) == Relay::Verdict::Repeat && r.limited == 1);
  // One token per RefillMs from the first one spent.
  assert(offer(r, ring(Other, Front, ++sequence), Relay::RefillMs - 1) == Relay::Verdict::Limited);
  assert(offer(r, ring(Other, Front, ++sequence), Relay::RefillMs) == Relay::Verdict::Queued);
  assert(offer(r, ring(Other, Front, ++sequence), Relay::RefillMs + 1) == Relay::Verdict::Limited);
  // A long pause refills the whole burst, not more.
  r.reset(); now = 100000;
  for (unsigned i = 0; i < Relay::Burst; ++i) {
    assert(offer(r, ring(Other, Front, ++sequence), now) == Relay::Verdict::Queued);
    assert(r.peek(now + Relay::QuietMs, now)); r.consume(true);
  }
  now += 1000 * Relay::RefillMs;
  for (unsigned i = 0; i < Relay::Burst; ++i) {
    assert(offer(r, ring(Other, Front, ++sequence), now) == Relay::Verdict::Queued);
    assert(r.peek(now + Relay::QuietMs, now)); r.consume(true);
  }
  assert(offer(r, ring(Other, Front, ++sequence), now) == Relay::Verdict::Limited);
}

void testPairingWindow() {
  Relay r;
  uint32_t chime, doorbell;
  assert(offer(r, discovery(Other), 0) == Relay::Verdict::Ignored);
  assert(offer(r, pairFrame(Other, Front, 0), 0) == Relay::Verdict::Ignored);
  r.openPairing(1000);
  assert(r.pairing(1000) && r.pairingRemaining(1000) == Relay::PairingMs);
  // Discovery names no doorbell; a key exchange must name a paired one.
  assert(Relay::eligible(discovery(Other), Node, Paired, 1, true, chime, doorbell) && chime == Other && !doorbell);
  assert(!Relay::eligible(discovery(Node), Node, Paired, 1, true, chime, doorbell));
  assert(Relay::eligible(pairFrame(Other, Front, 0), Node, Paired, 1, true, chime, doorbell) && doorbell == Front);
  assert(Relay::eligible(pairFrame(Other, Front, 1), Node, Paired, 1, true, chime, doorbell));
  assert(!Relay::eligible(pairFrame(Other, Front, 2), Node, Paired, 1, true, chime, doorbell));
  assert(!Relay::eligible(pairFrame(Other, Stranger, 0), Node, Paired, 1, true, chime, doorbell));
  assert(!Relay::eligible(pairFrame(Node, Front, 0), Node, Paired, 1, true, chime, doorbell));
  // Discovery every 500 ms: one relay a second. It is not listed as a chime.
  uint32_t now = 1000, relayed = 0;
  for (int i = 0; i < 8; ++i, now += 500)
    if (offer(r, discovery(Other), now) == Relay::Verdict::Queued) { ++relayed; assert(r.peek(now + 100, now)); r.consume(true); }
  assert(relayed == 4 && r.chimes() == 0);
  // The exchange: response, echo, then join under the normal rules.
  assert(offer(r, pairFrame(Other, Front, 0), now) == Relay::Verdict::Queued);
  assert(offer(r, pairFrame(Other, Front, 1), now + 50) == Relay::Verdict::Queued);
  assert(offer(r, frame(2, 20, Other, Front), now + 100) == Relay::Verdict::Queued);
  assert(r.pairingQueued == 6 && r.chimes() == 1 && r.chime(0).frames == 3);
  while (r.peek(now + 200, now)) r.consume(true);
  // Pairing frames do not use the rate limit: drain it, discovery still goes.
  uint32_t sequence = 100;
  while (offer(r, ring(Other, Front, ++sequence), now) == Relay::Verdict::Queued) {
    assert(r.peek(now + 100, now)); r.consume(true);
  }
  assert(offer(r, discovery(Other), now + Relay::RepeatMs) == Relay::Verdict::Queued);
  // The window closes by itself or on request.
  assert(!r.pairing(1000 + Relay::PairingMs) && !r.pairingRemaining(1000 + Relay::PairingMs));
  assert(offer(r, discovery(Third), 1000 + Relay::PairingMs) == Relay::Verdict::Ignored);
  r.openPairing(400000); assert(r.pairing(400001));
  r.closePairing(); assert(!r.pairing(400002));
  r.openPairing(500000); r.reset(); assert(!r.pairing(500001));
}

void testChimesAndSeveralDoorbells() {
  Relay r;
  const uint32_t both[] = {Front, Stranger};
  assert(r.offer(ring(Other, Front, 1), Node, both, 2, 0) == Relay::Verdict::Queued);
  r.consume(true);
  assert(r.offer(ring(Third, Stranger, 1), Node, both, 2, 100) == Relay::Verdict::Queued);
  r.consume(true);
  assert(r.chimes() == 2 && r.chime(1).id == Third && r.chime(1).doorbell == Stranger);
  // The oldest entry makes room once MaxChimes are listed.
  for (uint32_t i = 0; i < Relay::MaxChimes; ++i) {
    assert(r.offer(ring(0x01000000 + i, Front, 1), Node, both, 2, 200 + i) == Relay::Verdict::Queued);
    r.consume(true);
  }
  assert(r.chimes() == Relay::MaxChimes);
  for (size_t i = 0; i < r.chimes(); ++i) assert(r.chime(i).id != Other && r.chime(i).id != Third);
  r.reset();
  assert(r.chimes() == 0 && r.queued == 0 && r.sent == 0 && !r.peek(0, 0));
}
}

int main() {
  testSelection();
  testUnchangedAfterQuiet();
  testRepeats();
  testWaitExpiryAndCapacity();
  testRateLimit();
  testPairingWindow();
  testChimesAndSeveralDoorbells();
  std::cout << "Relay tests passed: selection, unchanged bytes, quiet wait, repeats, expiry, queue and rate limits, pairing window, chime list\n";
}
