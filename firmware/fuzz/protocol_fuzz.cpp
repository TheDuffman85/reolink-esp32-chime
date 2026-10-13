// Software receive path: demodulated bits -> Decoder -> frame parsers, plus
// an encode/decode round trip that must reproduce every valid MAC exactly.
#include "Protocol.h"
#include "FuzzInput.h"
#include <initializer_list>

using namespace chime;

namespace {
void parse(const Packet &packet, Duplicates &duplicates, uint32_t now) {
  FUZZ_CHECK(packet.size >= 5 && packet.size <= MaxMac + 3);
  FUZZ_CHECK(packet.size == size_t(packet.bytes[0]) + 3);
  const bool valid = validPacket(packet);
  FUZZ_CHECK(!valid || packet.crc == Crc::Ti8005);
  Ring ring;
  if (ringHeader(packet, ring)) {
    FUZZ_CHECK(packet.size == 23);
    uint8_t plain[4];
    std::memcpy(plain, packet.bytes + 17, sizeof(plain));
    if (ringPayload(plain, ring)) duplicates.seen(ring, now);
  }
  uint8_t data[MaxMac - 16];
  const size_t count = dataPayload(packet, read32le(packet.bytes + 5), read32le(packet.bytes + 9),
    packet.bytes[3] | (uint16_t(packet.bytes[4]) << 8), data, sizeof(data));
  FUZZ_CHECK(count <= sizeof(data));
  FUZZ_CHECK(!count || (valid && count == size_t(packet.bytes[1]) - 16 &&
    !std::memcmp(data, packet.bytes + 17, count)));
}

// Encoded frames decode to exactly one identical packet in either polarity.
void roundTrip(const uint8_t *data, size_t size, Duplicates &duplicates) {
  if (size < 2 || size > MaxMac) return;
  uint8_t mac[MaxMac];
  std::memcpy(mac, data, size);
  mac[0] = uint8_t(size);
  AirFrame air;
  FUZZ_CHECK(encode(mac, size, air) && air.size == size + 11);
  for (bool inverted : {false, true}) {
    Decoder decoder;
    Packet packet;
    unsigned packets = 0;
    for (size_t i = 0; i < air.size; ++i)
      for (int bit = 7; bit >= 0; --bit)
        if (decoder.feedBit(((air.bytes[i] >> bit) & 1) ^ inverted, packet)) ++packets;
    FUZZ_CHECK(packets == 1 && packet.inverted == inverted && validPacket(packet));
    FUZZ_CHECK(packet.size == size + 3 && !std::memcmp(packet.bytes + 1, mac, size));
    parse(packet, duplicates, size);
  }
}
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  Duplicates duplicates;
  roundTrip(data, size, duplicates);
  FuzzInput in(data, size);
  Decoder decoder;
  decoder.setPolarity(static_cast<Polarity>(in.u8() % 3));
  uint32_t now = in.u32();
  Packet packet;
  while (!in.empty()) {
    const uint8_t byte = in.u8();
    for (int bit = 7; bit >= 0; --bit)
      if (decoder.feedBit((byte >> bit) & 1, packet)) parse(packet, duplicates, now += 1000);
  }
  return 0;
}
