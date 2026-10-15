// Host tests for several doorbells sharing one chime: pairing an additional
// doorbell beside a joined one, frame routing by ID, transmit turns,
// replacing/duplicate slots, removal and failure paths. Real AES-CTR.
#include "Doorbells.h"
#include <openssl/evp.h>
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

using namespace chime;
using Bytes = std::vector<uint8_t>;

namespace {
constexpr uint32_t Node = 0x1A2B3C4D, Boot = 0x32413030;
constexpr uint32_t Front = 0x11223344, Back = 0x55667788, Side = 0x0BADCAFE;
const uint8_t FrontKey[16] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
const uint8_t BackKey[16] = {9,9,9,9,8,8,8,8,7,7,7,7,6,6,6,6};
const uint8_t SideKey[16] = {0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF};

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
  for (size_t i = 0; i < air.size; ++i)
    for (int bit = 7; bit >= 0; --bit)
      if (decoder.feedBit((air.bytes[i] >> bit) & 1, result)) ++packets;
  assert(packets == 1 && validPacket(result));
  return result;
}
Packet packet(Bytes mac) {
  mac[0] = static_cast<uint8_t>(mac.size());
  AirFrame air;
  assert(encode(mac.data(), mac.size(), air));
  return decode(air);
}
Bytes mac(const Packet &p) { return Bytes(p.bytes + 1, p.bytes + p.size - 2); }
// The next frame and the slot sending it.
Bytes outgoing(const Doorbells &d, size_t &slot, uint32_t now = 0) {
  const AirFrame *air = d.peek(now, slot);
  assert(air);
  return mac(decode(*air));
}
Packet pairResponse(uint32_t ap, const uint8_t key[16]) {
  Bytes m(32);
  m[1] = 1; write32le(m.data() + 4, Node);
  m[8] = 0; m[9] = 0; m[10] = 0xBA; m[11] = 0xCB;
  write32le(m.data() + 12, ap); std::memcpy(m.data() + 16, key, 16);
  return packet(m);
}
Packet joinResponse(uint32_t ap, const uint8_t key[16], uint16_t pair) {
  Bytes m(24);
  m[1] = 2; write32le(m.data() + 4, Node); write32le(m.data() + 8, ap);
  m[12] = 3; m[13] = 6; write16le(m.data() + 14, pair);
  write32le(m.data() + 20, 0x12345678);
  assert(aes(nullptr, key, m.data() + 12, 12));
  return packet(m);
}
// Type-3 data frame from a doorbell; plain is encrypted with key.
Packet data(uint32_t ap, const uint8_t key[16], uint16_t pair, uint32_t sequence, Bytes plain) {
  Bytes m(16);
  m[1] = 3; write16le(m.data() + 2, pair); write32le(m.data() + 4, Node);
  write32le(m.data() + 8, ap); write32le(m.data() + 12, sequence);
  assert(aes(nullptr, key, plain.data(), plain.size()));
  m.insert(m.end(), plain.begin(), plain.end());
  return packet(m);
}
LinkProfile profile(uint32_t ap, const uint8_t key[16], uint16_t pair) {
  LinkProfile p;
  p.paired = true; p.source = ap; p.destination = Node; p.pair = pair;
  std::memcpy(p.key, key, 16);
  return p;
}
void init(Doorbells &d, const std::vector<LinkProfile> &profiles) {
  d.init(Node, Boot, aes, nullptr, profiles.data(), profiles.size());
}
// Sends every queued frame (all must come from slot), then answers the join.
void join(Doorbells &d, size_t expected, uint32_t ap, const uint8_t key[16], uint16_t pair, uint32_t now) {
  d.tick(now);
  size_t slot = 99;
  Bytes request = outgoing(d, slot, now);
  assert(slot == expected && request[1] == 2 && read32le(request.data() + 8) == ap);
  d.consume(slot, true, now);
  size_t from = 99;
  assert(d.receive(joinResponse(ap, key, pair), now + 1, from) && from == expected);
  const Bytes ack = outgoing(d, slot, now + 1);
  assert(slot == expected && ack[1] == 2);
  d.consume(slot, true, now + 2);
  // A duplicate slot of the same doorbell may have moved this one down.
  const int joined = d.find(ap);
  assert(joined >= 0 && d.link(joined).profileReady() && d.link(joined).profile().pair == pair);
}
// Discovery, pairing response and acknowledgement in the pairing slot.
void pairKey(Doorbells &d, size_t expected, uint32_t ap, const uint8_t key[16], uint32_t now) {
  d.tick(now);
  size_t slot = 99;
  const Bytes discovery = outgoing(d, slot, now);
  assert(slot == expected && discovery.size() == 8 && discovery[1] == 0);
  d.consume(slot, true, now);
  size_t from = 99;
  assert(!std::strcmp(d.receive(pairResponse(ap, key), now + 1, from), "pairing response accepted") &&
         from == expected);
  const Bytes ack = outgoing(d, slot, now + 1);
  assert(slot == expected && ack[1] == 1 && !std::memcmp(ack.data() + 16, key, 16));
  d.consume(slot, true, now + 1);
}

void addSecondDoorbell() {
  Doorbells d; init(d, {profile(Front, FrontKey, 0x1111)});
  assert(d.count() == 1 && d.active() == 1 && !d.busy());
  assert(d.startJoinAll(0) == 1);
  join(d, 0, Front, FrontKey, 0x2222, 0);
  assert(d.dirty()); d.clearDirty(); assert(!d.dirty());
  d.setName(0, "Front door");
  // Adding: slot 1 pairs while the front door keeps its session.
  assert(!d.startPair(2, 10) && d.startPair(1, 10));
  assert(d.pairing() && d.pairSlot() == 1 && d.count() == 1 && d.active() == 2);
  assert(d.link(0).profileReady() && !d.startPair(1, 11));
  assert(d.link(1).peer() == 0);
  pairKey(d, 1, Back, BackKey, 100);
  assert(d.link(1).peer() == Back && d.count() == 1 && !d.dirty());
  join(d, 1, Back, BackKey, 0x3333, 2100);
  assert(d.count() == 2 && d.active() == 2 && d.dirty() && !d.pairing());
  assert(d.link(1).profileDirty() && !d.link(0).profileDirty());
  assert(d.link(0).profileReady() && d.link(0).profile().pair == 0x2222);
  assert(d.find(Front) == 0 && d.find(Back) == 1 && d.find(Side) == -1 && d.find(0) == -1);
  assert(!std::strcmp(d.name(0), "Front door") && !d.name(1)[0]);
  d.clearDirty();
  // Frames reach only the doorbell they are addressed to.
  size_t from = 99;
  assert(!d.receive(joinResponse(Side, SideKey, 1), 2200, from));
  assert(!std::strcmp(d.receive(data(Back, BackKey, 0x3333, 5, {22}), 2201, from), "doorbell requested rejoin") &&
         from == 1);
  assert(d.link(1).busy() && d.link(0).profileReady() && d.busy() && !d.pairing());
  join(d, 1, Back, BackKey, 0x4444, 2300);
  // A frame encrypted for the front door's session does not touch the back door.
  assert(!d.receive(data(Front, FrontKey, 0x3333, 9, {13}), 2400, from));
  assert(d.link(1).profileReady() && d.link(1).profile().pair == 0x4444);
  // Heartbeats: both links queue one and take turns on the radio.
  d.tick(40000);
  size_t first = 99, second = 99;
  const Bytes a = outgoing(d, first, 40000);
  d.consume(first, true, 40000);
  const Bytes b = outgoing(d, second, 40000);
  d.consume(second, true, 40000);
  assert(first != second && a[1] == 3 && b[1] == 3);
  assert(read32le(a.data() + 8) == (first ? Back : Front) && read32le(b.data() + 8) == (second ? Back : Front));
  assert(!d.peek(40000, first));
  // Cancel ends a rejoin only; joined links stay joined.
  assert(d.startJoin(0, 41000) && d.busy());
  d.cancel(41000);
  assert(!d.busy() && !d.link(0).profileReady() && d.link(1).profileReady());
  assert(!std::strcmp(d.link(0).state(), "idle"));
}

void addingTheSameDoorbellAgain() {
  Doorbells d; init(d, {profile(Front, FrontKey, 1), profile(Back, BackKey, 2)});
  d.setName(0, "Front door");
  assert(d.startJoin(0, 0)); join(d, 0, Front, FrontKey, 3, 0);
  assert(d.startJoin(1, 10)); join(d, 1, Back, BackKey, 4, 10);
  d.clearDirty();
  // Adding: the front door answers with a fresh key. The pairing fails without
  // acknowledging it, so the doorbell keeps the key the chime already holds.
  assert(d.startPair(2, 100));
  d.tick(100);
  size_t slot = 99;
  outgoing(d, slot, 100); d.consume(slot, true, 100);
  uint8_t fresh[16]; std::memcpy(fresh, FrontKey, 16); fresh[0] ^= 0xFF;
  size_t from = 99;
  assert(!std::strcmp(d.receive(pairResponse(Front, fresh), 101, from), "doorbell already paired") && from == 2);
  assert(!d.pairing() && !d.peek(101, slot) && !std::strcmp(d.link(2).state(), "doorbell already paired"));
  assert(d.count() == 2 && d.active() == 2 && !d.dirty() && d.pairSlot() == 2);
  assert(d.link(0).profileReady() && !std::memcmp(d.link(0).profile().key, FrontKey, 16));
  assert(d.link(1).profileReady() && !std::strcmp(d.name(0), "Front door"));
  // Replacing slot 0 with the doorbell held by slot 1 is refused too.
  assert(d.startPair(0, 200));
  d.tick(200); outgoing(d, slot, 200); d.consume(slot, true, 200);
  assert(!std::strcmp(d.receive(pairResponse(Back, fresh), 201, from), "doorbell already paired") && from == 0);
  assert(d.find(Front) == 0 && d.find(Back) == 1 && d.link(1).profileReady() && !d.dirty());
  // Pairing a doorbell again into its own slot is allowed.
  assert(d.startPair(1, 300));
  pairKey(d, 1, Back, fresh, 300);
  join(d, 1, Back, fresh, 5, 2300);
  assert(d.count() == 2 && d.dirty() && !std::memcmp(d.link(1).profile().key, fresh, 16));
}

void replacingAndRemoving() {
  Doorbells d; init(d, {profile(Front, FrontKey, 1), profile(Back, BackKey, 2)});
  d.setName(0, "Front door"); d.setName(1, "Back door");
  // Pairing a replacement into slot 0 keeps the back door.
  assert(d.startPair(0, 0) && d.pairSlot() == 0 && d.active() == 2);
  pairKey(d, 0, Side, SideKey, 0);
  join(d, 0, Side, SideKey, 9, 2000);
  assert(d.count() == 2 && d.find(Side) == 0 && d.find(Back) == 1 && d.find(Front) == -1);
  assert(!std::strcmp(d.name(0), "Front door")); // The slot (e.g. a new front door) keeps its label.
  d.clearDirty();
  // Removing slot 0 moves the back door up; the vacated slot is empty.
  assert(d.remove(0) && d.count() == 1 && d.dirty() && d.pairSlot() == 0);
  assert(d.find(Back) == 0 && !std::strcmp(d.name(0), "Back door"));
  assert(!d.link(1).profile().source && !d.name(1)[0]);
  assert(!d.remove(1) && d.remove(0) && d.count() == 0);
  // With no doorbell the first pairing uses slot 0.
  assert(d.startPair(0, 0) && d.active() == 1 && d.count() == 0);
}

void failedAddAndLimits() {
  Doorbells d; init(d, {profile(Front, FrontKey, 1)});
  assert(d.startJoinAll(0) == 1);
  join(d, 0, Front, FrontKey, 2, 0);
  d.clearDirty();
  assert(d.startPair(1, 0));
  // No answer: after five minutes the attempt fails, the front door is unaffected.
  d.tick(300000);
  assert(!d.pairing() && d.count() == 1 && d.active() == 1 && d.pairSlot() == 1);
  assert(!std::strcmp(d.link(1).state(), "timed out") && d.link(0).profileReady() && !d.dirty());
  // The next attempt starts from an empty slot.
  assert(d.startPair(1, 300001) && !d.link(1).profile().source);
  d.cancel(300002);
  assert(!d.pairing() && d.link(0).profileReady());
  // All slots in use: no further slot to add.
  Doorbells full;
  init(full, {profile(Front, FrontKey, 1), profile(Back, BackKey, 2), profile(Side, SideKey, 3),
              profile(0x0C0FFEE0, SideKey, 4)});
  assert(full.count() == MaxDoorbells && !full.startPair(MaxDoorbells, 0) && full.startPair(3, 0));
  // A failed radio transmission is retried without blocking other links.
  full.cancel(0);
  assert(full.startJoinAll(0) == 4);
  full.tick(0);
  size_t slot = 99;
  outgoing(full, slot, 0);
  full.consume(slot, false, 0);
  size_t next = 99;
  outgoing(full, next, 0);
  assert(next != slot);
  // Out-of-range consume is ignored.
  full.consume(MaxDoorbells, true, 0);
}

void names() {
  Doorbells d; init(d, {profile(Front, FrontKey, 1)});
  d.setName(0, "A name that is far too long for the label");
  assert(std::strlen(d.name(0)) == DoorbellNameSize - 1);
  d.setName(0, nullptr); assert(!d.name(0)[0]);
  d.setName(MaxDoorbells, "ignored");
  // Invalid init counts give no doorbells.
  Doorbells none; none.init(Node, Boot, aes, nullptr, nullptr, 3);
  assert(none.count() == 0);
}
}

int main() {
  addSecondDoorbell();
  addingTheSameDoorbellAgain();
  replacingAndRemoving();
  failedAddAndLimits();
  names();
  std::cout << "Doorbell tests passed: second doorbell, routing, turns, no duplicate pairing, replace, remove, failures\n";
}
