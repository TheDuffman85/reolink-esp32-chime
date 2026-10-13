// Pairing/join/session state machine driven by an arbitrary sequence of
// local commands, clock steps, radio results and received frames. Received
// data frames then take the firmware's application-request path.
#include "Link.h"
#include "AppProtocol.h"
#include "FuzzInput.h"

using namespace chime;

namespace {
constexpr uint32_t Node = 0x6CBD97DD, Ap = 0x3383DEA5, Boot = 0x32413030;
bool cryptoFails = false;
// The cipher is the identity so the fuzzer can reach every decrypted field;
// test/link_test.cpp covers the AES-CTR callback contract with real vectors.
bool crypt(void *, const uint8_t[16], uint8_t *, size_t) { return !cryptoFails; }

// Every frame the link queues must survive the software receive path intact.
void checkAir(const AirFrame &air) {
  FUZZ_CHECK(air.size >= 13 && air.size <= MaxAir);
  Decoder decoder;
  decoder.setPolarity(Polarity::Normal);
  Packet packet;
  unsigned packets = 0;
  for (size_t i = 0; i < air.size; ++i)
    for (int bit = 7; bit >= 0; --bit)
      if (decoder.feedBit((air.bytes[i] >> bit) & 1, packet)) ++packets;
  FUZZ_CHECK(packets == 1 && validPacket(packet) && packet.size == air.size - 8);
}

// Raw packets test validation; framed ones carry a correct length and CRC,
// and optionally the addresses of this node, so deeper states are reachable.
Packet receivedPacket(FuzzInput &in, const Link &link) {
  const uint8_t flags = in.u8();
  Packet packet;
  if (!(flags & 1)) {
    packet.size = in.u8() % (sizeof(packet.bytes) + 1);
    in.bytes(packet.bytes, packet.size);
    packet.crc = static_cast<Crc>((flags >> 1) % 3);
    packet.inverted = flags & 8;
    return packet;
  }
  const size_t length = 2 + in.u8() % (MaxMac - 1);
  uint8_t *mac = packet.bytes + 1;
  in.bytes(mac, length);
  mac[0] = uint8_t(length);
  if ((flags & 2) && length >= 16) {
    const uint8_t type = mac[1] & 0x3F;
    if (type == 1) { write32le(mac + 4, Node); write32le(mac + 12, Ap); mac[8] = 0; }
    else {
      write32le(mac + 4, link.profile().destination); write32le(mac + 8, link.profile().source);
      if (type == 3) write16le(mac + 2, link.profile().pair);
    }
  }
  packet.bytes[0] = uint8_t(length);
  const uint16_t crc = crc16(packet.bytes, length + 1, 0x8005);
  packet.bytes[length + 1] = uint8_t(crc >> 8); packet.bytes[length + 2] = uint8_t(crc);
  packet.size = length + 3;
  packet.crc = Crc::Ti8005;
  return packet;
}

// Mirrors LinkService's onPacket(): only joined, addressed, decryptable
// type-3 frames reach the application handler; its reply is queued.
void applicationRequest(Link &link, AppConfig &config, const Packet &packet, const char *result) {
  if (!link.profileReady() || (result && !std::strcmp(result, "stale data frame"))) return;
  const LinkProfile &profile = link.profile();
  uint8_t plain[MaxMac];
  const size_t count = dataPayload(packet, profile.destination, profile.source, profile.pair,
    plain, sizeof(plain));
  if (!count || !crypt(nullptr, profile.key, plain, count)) return;
  uint8_t reply[MaxMac - 16];
  const AppResult app = appRequest(config, plain, count, reply, sizeof(reply), Boot);
  FUZZ_CHECK(app.replyLength <= sizeof(reply));
  if (app.replyLength) link.sendData(reply, app.replyLength);
}
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  FuzzInput in(data, size);
  cryptoFails = false;
  LinkProfile saved;
  const uint8_t setup = in.u8();
  saved.paired = setup & 1;
  if (setup & 2) { saved.source = Ap; saved.destination = Node; }
  saved.pair = in.u16(); saved.heartbeatMode = in.u8() % 2;
  in.bytes(saved.key, sizeof(saved.key));
  Link link;
  link.init(setup & 4 ? Node : 0, Boot, crypt, nullptr, saved);
  AppConfig config;
  uint32_t now = in.u32();
  while (!in.empty()) {
    switch (in.u8() % 9) {
      case 0: link.startPair(now); break;
      case 1: link.startJoin(now); break;
      case 2: link.cancel(now); break;
      case 3: now += uint32_t(in.u16()) * 10; link.tick(now); break;
      case 4:
        if (const AirFrame *air = link.peek(now)) {
          checkAir(*air);
          link.consume(in.u8() & 1, now);
        }
        break;
      case 5: {
        uint8_t plain[MaxMac];
        const size_t length = in.u8() % (sizeof(plain) + 1);
        in.bytes(plain, length);
        link.sendData(plain, length);
        break;
      }
      case 6: {
        const Packet packet = receivedPacket(in, link);
        const char *result = link.receive(packet, now);
        FUZZ_CHECK(!result || std::strlen(result) < 64);
        applicationRequest(link, config, packet, result);
        break;
      }
      case 7: cryptoFails = !cryptoFails; break;
      case 8: link.clearProfileDirty(); break;
    }
    FUZZ_CHECK(link.state() && link.state()[0]);
    if (link.profileReady())
      FUZZ_CHECK(link.profile().paired && link.profile().source && link.profile().destination);
  }
  return 0;
}
