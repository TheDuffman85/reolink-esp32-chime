#pragma once
#include <cstddef>
#include <cstdint>

namespace chime {
constexpr uint32_t Sync = 0x930B51DE;
constexpr size_t MaxMac = 96;
enum class Polarity : uint8_t { Auto, Normal, Inverted };
enum class Crc : uint8_t { Bad, Ti8005, CcittDiagnostic };
struct Packet {
  uint8_t bytes[MaxMac + 3]{}; // PHY length, MAC, CRC; de-whitened
  size_t size = 0;
  bool inverted = false;
  Crc crc = Crc::Bad;
};
uint8_t pn9Mask(uint16_t &state);
uint16_t crc16(const uint8_t *bytes, size_t count, uint16_t polynomial);
uint32_t read32le(const uint8_t *p);
void write16le(uint8_t *p, uint16_t value);
void write32le(uint8_t *p, uint32_t value);
// Complete software-generated on-air frame: preamble, sync, whitened
// PHY length, MAC and TI CRC. Frames above the 64-byte CC1101 TX FIFO are
// streamed by refilling the FIFO during transmission.
constexpr size_t MaxAir = MaxMac + 11;
struct AirFrame { uint8_t bytes[MaxAir]{}; size_t size = 0; };
bool encode(const uint8_t *mac, size_t size, AirFrame &out);
bool validPacket(const Packet &packet);
struct Ring {
  uint16_t pair, parameter;
  uint32_t source, destination, sequence;
};
bool ringHeader(const Packet &packet, Ring &ring);
bool ringPayload(const uint8_t plain[4], Ring &ring);
// Copies the ciphertext of a valid type-3 data frame addressed to this
// session (destination, source, pair) and returns its length; 0 otherwise,
// or when it exceeds capacity. Doorbell application requests use this form.
size_t dataPayload(const Packet &packet, uint32_t destination, uint32_t source,
                   uint16_t pair, uint8_t *out, size_t capacity);

// Feed demodulated bits MSB first; sync may occur at any bit alignment.
// Auto searches both polarities for every packet, without changing RF registers.
class Decoder {
 public:
  void reset();
  void setPolarity(Polarity value) { polarity_ = value; reset(); }
  bool feedBit(bool value, Packet &out);
  uint32_t invalidLengths = 0;
 private:
  Polarity polarity_ = Polarity::Auto;
  uint32_t shift_ = 0;
  uint8_t syncBits_ = 0, octet_ = 0, bits_ = 0;
  uint16_t pn9_ = 0x1FF;
  bool collecting_ = false;
  size_t expected_ = 0;
  Packet current_;
};

// The doorbell repeats each frame several times with identical bytes, and a
// weak link damages a few different bits in each copy. A bitwise majority of
// three damaged copies of one length often restores the frame; only a result
// that passes the TI CRC is returned, as a valid packet.
class Combiner {
 public:
  bool offer(const Packet &damaged, uint32_t now, Packet &out);
  // A valid frame of this length arrived: its damaged copies are not needed.
  void drop(size_t size);
  void reset();
 private:
  struct Entry { Packet packet; uint32_t at = 0; bool used = false; } entries_[6];
  size_t next_ = 0;
};

// Retries share a sequence. Keep several events, including interleaved nodes,
// for five minutes; unsigned subtraction handles millis() rollover.
class Duplicates {
 public:
  bool seen(const Ring &ring, uint32_t now);
  void reset();
  // After one doorbell rejoins; other doorbells keep their history.
  void forget(uint32_t source);
 private:
  struct Entry { uint32_t source = 0, sequence = 0, time = 0;
                 uint16_t pair = 0; bool used = false; } entries_[32];
  size_t next_ = 0;
};
}
