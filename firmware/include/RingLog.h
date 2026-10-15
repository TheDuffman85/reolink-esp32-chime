#pragma once
#include <cstddef>
#include <cstdint>

namespace chime {
// Who asked the chime to play. Doorbell rings arrive over RF; the others
// are local tests or remote requests.
enum class RingSource : uint8_t { Doorbell, Web, Button, Mqtt, Serial };
constexpr uint8_t RingSources = 5;
// What the chime did: played, muted by chime volume 0, silent because the
// gong type is None, skipped because the output was busy/unavailable, or
// muted by the owner (permanent or timed mute).
enum class RingOutcome : uint8_t { Played, Muted, Silent, Busy, ChimeMuted };
constexpr uint8_t RingOutcomes = 5;
extern const char *const RingSourceNames[RingSources];
extern const char *const RingOutcomeNames[RingOutcomes];
constexpr uint8_t RingPreview = 1;      // Played with preview overrides.
constexpr uint8_t RingPreviousBoot = 2; // Restored from flash after a restart.

struct RingEvent {
  uint32_t id = 0;
  uint32_t epoch = 0;  // UTC seconds, 0 while the clock is not synchronised.
  uint32_t ms = 0;     // Uptime of the boot that recorded it.
  uint16_t tone = 0;   // Doorbell music ID (ringtone chosen in the Reolink app).
  uint8_t source = 0, outcome = 0;
  uint8_t type = 0, sound = 0; // Gong type and melody/pattern index.
  uint8_t volume = 0;          // Chime volume 0..4 used.
  uint8_t level = 0;           // Audio level 0..100.
  int8_t rssi = 0;             // Doorbell frames only, dBm.
  uint8_t flags = 0;
  uint32_t doorbell = 0;       // Doorbell ID; 0 for other sources and version-1 images.
};

// Fixed-size history, newest first. Its compact image survives restarts in NVS.
class RingLog {
 public:
  static constexpr size_t Capacity = 50;
  // Version 2 entries append the doorbell ID; version-1 images still restore.
  static constexpr size_t EntrySize = 26, LegacyEntrySize = 22, HeaderSize = 8;
  static constexpr size_t ImageSize = HeaderSize + Capacity * EntrySize;
  // Assigns the next ID and returns the stored copy.
  const RingEvent &add(RingEvent event);
  size_t size() const { return size_; }
  // index 0 is the newest event; index must be below size().
  const RingEvent &newest(size_t index) const;
  uint32_t lastId() const { return lastId_; }
  void clear();
  size_t serialize(uint8_t *out, size_t capacity) const;
  // Replaces the log with an image; restored entries are marked previous
  // boot. Returns false (and leaves the log unchanged) for invalid images.
  bool restore(const uint8_t *in, size_t length);
 private:
  RingEvent events_[Capacity];
  size_t next_ = 0, size_ = 0;
  uint32_t lastId_ = 0;
};
}
