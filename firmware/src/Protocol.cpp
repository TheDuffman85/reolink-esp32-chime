#include "Protocol.h"
#include <cstring>

namespace chime {
uint8_t pn9Mask(uint16_t &state) {
  const uint8_t result = state & 0xFF;
  for (int i = 0; i < 8; ++i) {
    const uint16_t feedback = ((state >> 5) ^ state) & 1;
    state = (state >> 1) | (feedback << 8);
  }
  return result;
}
uint16_t crc16(const uint8_t *bytes, size_t count, uint16_t polynomial) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < count; ++i) {
    crc ^= uint16_t(bytes[i]) << 8;
    for (int j = 0; j < 8; ++j)
      crc = (crc & 0x8000) ? uint16_t((crc << 1) ^ polynomial) : uint16_t(crc << 1);
  }
  return crc;
}
uint32_t read32le(const uint8_t *p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
         (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
void write16le(uint8_t *p, uint16_t value) { p[0] = value; p[1] = value >> 8; }
void write32le(uint8_t *p, uint32_t value) {
  for (size_t i = 0; i < 4; ++i) p[i] = value >> (8 * i);
}
bool validPacket(const Packet &packet) {
  return packet.crc == Crc::Ti8005 && packet.size >= 5 &&
    packet.size <= MaxMac + 3 && packet.bytes[0] >= 2 &&
    size_t(packet.bytes[0]) + 3 == packet.size && packet.bytes[1] == packet.bytes[0] &&
    crc16(packet.bytes, packet.size - 2, 0x8005) ==
      ((uint16_t(packet.bytes[packet.size - 2]) << 8) | packet.bytes[packet.size - 1]);
}
bool encode(const uint8_t *mac, size_t size, AirFrame &out) {
  out = AirFrame{};
  if (!mac || size < 2 || size + 11 > MaxAir || mac[0] != size) return false;
  for (size_t i = 0; i < 4; ++i) out.bytes[i] = 0x55;
  out.bytes[4] = 0x93; out.bytes[5] = 0x0B; out.bytes[6] = 0x51; out.bytes[7] = 0xDE;
  out.bytes[8] = size;
  for (size_t i = 0; i < size; ++i) out.bytes[9 + i] = mac[i];
  const uint16_t crc = crc16(out.bytes + 8, size + 1, 0x8005);
  out.bytes[9 + size] = crc >> 8; out.bytes[10 + size] = crc;
  uint16_t pn9 = 0x1FF;
  out.size = size + 11;
  for (size_t i = 8; i < out.size; ++i) out.bytes[i] ^= pn9Mask(pn9);
  return true;
}
bool ringHeader(const Packet &packet, Ring &ring) {
  if (packet.crc != Crc::Ti8005 || packet.size != 23 ||
      packet.bytes[0] != 20 || packet.bytes[1] != 20 ||
      (packet.bytes[2] & 0x3F) != 3) return false;
  const auto *mac = packet.bytes + 1;
  ring.pair = mac[2] | (uint16_t(mac[3]) << 8);
  ring.destination = read32le(mac + 4);
  ring.source = read32le(mac + 8);
  ring.sequence = read32le(mac + 12);
  return true;
}
bool ringPayload(const uint8_t plain[4], Ring &ring) {
  if (plain[0] != 9) return false;
  // Byte 1 is unspecified in the vendor's ring routine.
  ring.parameter = plain[2] | (uint16_t(plain[3]) << 8);
  return true;
}
size_t dataPayload(const Packet &packet, uint32_t destination, uint32_t source,
                   uint16_t pair, uint8_t *out, size_t capacity) {
  if (!validPacket(packet) || (packet.bytes[2] & 0x3F) != 3 || packet.bytes[1] <= 16 ||
      read32le(packet.bytes + 5) != destination || read32le(packet.bytes + 9) != source ||
      (packet.bytes[3] | (uint16_t(packet.bytes[4]) << 8)) != pair) return 0;
  const size_t count = packet.bytes[1] - 16;
  if (!out || count > capacity) return 0;
  std::memcpy(out, packet.bytes + 17, count);
  return count;
}
void Decoder::reset() {
  shift_ = 0; syncBits_ = 0; octet_ = 0; bits_ = 0;
  pn9_ = 0x1FF; collecting_ = false; expected_ = 0;
  current_ = Packet{};
}
bool Decoder::feedBit(bool value, Packet &out) {
  if (!collecting_) {
    shift_ = (shift_ << 1) | uint32_t(value);
    if (syncBits_ < 32) ++syncBits_;
    if (syncBits_ != 32) return false;
    const bool normal = shift_ == Sync && polarity_ != Polarity::Inverted;
    const bool inverse = shift_ == ~Sync && polarity_ != Polarity::Normal;
    if (!normal && !inverse) return false;
    collecting_ = true; current_ = Packet{}; current_.inverted = inverse;
    bits_ = 0; octet_ = 0; pn9_ = 0x1FF; expected_ = 0;
    return false;
  }
  octet_ = (octet_ << 1) | uint8_t(value ^ current_.inverted);
  if (++bits_ != 8) return false;
  bits_ = 0;
  const uint8_t byte = octet_ ^ pn9Mask(pn9_);
  octet_ = 0;
  if (current_.size == 0) {
    if (byte < 2 || byte > MaxMac) { ++invalidLengths; reset(); return false; }
    expected_ = size_t(byte) + 3;
  }
  current_.bytes[current_.size++] = byte;
  if (current_.size != expected_) return false;
  const size_t body = current_.size - 2;
  const uint16_t received = (uint16_t(current_.bytes[body]) << 8) | current_.bytes[body + 1];
  if (crc16(current_.bytes, body, 0x8005) == received) current_.crc = Crc::Ti8005;
  else if (crc16(current_.bytes, body, 0x1021) == received) current_.crc = Crc::CcittDiagnostic;
  out = current_;
  reset();
  return true;
}
bool Duplicates::seen(const Ring &ring, uint32_t now) {
  for (const auto &entry : entries_)
    if (entry.used && entry.source == ring.source && entry.pair == ring.pair &&
        entry.sequence == ring.sequence && uint32_t(now - entry.time) < 300000)
      return true;
  auto &entry = entries_[next_];
  entry.source = ring.source; entry.sequence = ring.sequence;
  entry.time = now; entry.pair = ring.pair; entry.used = true;
  next_ = (next_ + 1) % 32;
  return false;
}
void Duplicates::reset() { for (auto &entry : entries_) entry = Entry{}; next_ = 0; }
void Duplicates::forget(uint32_t source) {
  for (auto &entry : entries_) if (entry.source == source) entry = Entry{};
}
}
