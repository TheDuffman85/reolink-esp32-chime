#pragma once
#include <ArduinoJson.h>
#include <cstddef>
#include <cstdint>
#include "Sounds.h"

namespace chime {
// User-facing modes are independent of the audio output method. Legacy NVS
// and ring history use type 3 for DAC; migrate it to Audio with DAC output.
enum class GongKind : uint8_t { Audio = 0, Relay = 1, None = 2 };
enum class AudioOutput : uint8_t { Pwm = 0, Dac = 1 };
constexpr uint8_t GongKinds = 3, AudioOutputs = 2;
constexpr uint8_t modeFromStoredType(uint8_t type) { return type == 3 ? 0 : type; }
constexpr uint8_t audioFromStoredType(uint8_t type) { return type == 3 ? 1 : 0; }
constexpr uint16_t MaxMuteMinutes = 1440;
extern const char *const GongKindNames[GongKinds];
extern const char *const AudioOutputNames[AudioOutputs];
extern const char *const MelodyNames[Melodies];
extern const char *const PatternNames[Patterns];

// Everything that changes how the chime sounds or presents itself. It
// deliberately excludes administrative settings (WiFi, login, radio, GPIO,
// pairing), so remote interfaces built on it, such as MQTT, cannot reach them.
struct ChimeState {
  uint8_t type = 1, audioOutput = 0, melody = 0, level = 35, volume = 4, pattern = 0;
  bool led = true, extraRelay = false;
  // Off: every ring plays the slot it requests. On: always melody/pattern.
  bool overrideRingtone = false;
  uint16_t extraRelayMs = 300, pulseMs = 300, gapMs = 700;
  char name[33] = "Reolink Chime";
  // Mute: permanent (saved) or for muteMinutes (runtime); never both.
  bool mute = false;
  uint16_t muteMinutes = 0;
};

// Applies a partial update such as {"melody":"Ascending","volume":3}.
// Enumerations accept an index or a name (case, spaces, '-' and '_' are
// ignored); booleans accept true/false, 0/1 and "ON"/"OFF". Unknown keys and
// invalid values reject the whole update; `state` changes only on success.
// "mute": true mutes permanently, false unmutes (also ending a timed mute);
// "mute_minutes": 1..1440 mutes for that long instead, 0 ends a timed mute.
// "mute" is applied before "mute_minutes" regardless of key order.
// Returns nullptr on success or a short error text.
const char *applyChime(JsonObjectConst update, ChimeState &state);
// Writes all fields with enumerations as display names. "mute" is true while
// muted for any reason; "mute_minutes" is the time left of a timed mute.
void writeChime(const ChimeState &state, JsonObject out);
// Builds {"<field>": value} from a bare payload ("3", "ON", "Ding dong").
// Returns false for an empty field name or a document allocation failure.
bool chimeField(const char *field, const char *payload, size_t length, JsonDocument &out);
// Index of an enumeration name/index in `names`, or -1.
int chimeEnum(JsonVariantConst value, const char *const *names, uint8_t count);
// Sound slot 0..9 from an index or a melody or pattern name, or -1.
int soundSlot(JsonVariantConst value);
}
