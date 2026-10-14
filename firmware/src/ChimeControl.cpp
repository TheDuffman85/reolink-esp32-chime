#include "ChimeControl.h"
#include <cmath>
#include <cstring>

namespace chime {
const char *const GongKindNames[GongKinds] = {"Audio", "Gong", "None"};
const char *const AudioOutputNames[AudioOutputs] = {"PWM", "DAC"};
const char *const MelodyNames[Melodies] = {"Ding dong", "Ascending", "Double beep"};
const char *const PatternNames[Patterns] = {"Ding-dong", "Double", "Triple", "Long ring", "Insistent"};

namespace {
// Lower-case letters and digits only, so "Ding dong", "ding_dong" and
// "DING-DONG" compare equal.
void normalize(const char *in, char *out, size_t capacity) {
  size_t n = 0;
  for (; *in && n + 1 < capacity; ++in) {
    char c = *in;
    if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) out[n++] = c;
  }
  out[n] = 0;
}
bool integer(JsonVariantConst value, long lo, long hi, long &out) {
  if (value.is<long>()) out = value.as<long>();
  else if (value.is<double>()) {
    const double d = value.as<double>();
    if (!std::isfinite(d) || std::floor(d) != d || d < lo || d > hi) return false;
    out = long(d);
  } else return false;
  return out >= lo && out <= hi;
}
bool boolean(JsonVariantConst value, bool &out) {
  if (value.is<bool>()) { out = value.as<bool>(); return true; }
  long number;
  if (integer(value, 0, 1, number)) { out = number == 1; return true; }
  if (!value.is<const char *>()) return false;
  char text[8]; normalize(value.as<const char *>(), text, sizeof(text));
  if (!strcmp(text, "on") || !strcmp(text, "true")) { out = true; return true; }
  if (!strcmp(text, "off") || !strcmp(text, "false")) { out = false; return true; }
  return false;
}
}

int chimeEnum(JsonVariantConst value, const char *const *names, uint8_t count) {
  long index;
  if (integer(value, 0, count - 1, index)) return int(index);
  if (!value.is<const char *>()) return -1;
  char wanted[40], candidate[40];
  normalize(value.as<const char *>(), wanted, sizeof(wanted));
  if (!wanted[0]) return -1;
  for (uint8_t i = 0; i < count; ++i) {
    normalize(names[i], candidate, sizeof(candidate));
    if (!strcmp(wanted, candidate)) return i;
  }
  return -1;
}

const char *applyChime(JsonObjectConst update, ChimeState &state) {
  if (update.isNull()) return "Expected a JSON object";
  ChimeState next = state;
  int mute = -1; long muteMinutes = -1;
  for (JsonPairConst pair : update) {
    const char *key = pair.key().c_str();
    JsonVariantConst value = pair.value();
    long number; bool flag; int pick;
    if (!strcmp(key, "type")) {
      if ((pick = chimeEnum(value, GongKindNames, GongKinds)) < 0) return "type: Audio, Gong or None";
      next.type = uint8_t(pick);
    } else if (!strcmp(key, "audio_output")) {
      if ((pick = chimeEnum(value, AudioOutputNames, AudioOutputs)) < 0) return "audio_output: PWM or DAC";
      next.audioOutput = uint8_t(pick);
    } else if (!strcmp(key, "melody")) {
      if ((pick = chimeEnum(value, MelodyNames, Melodies)) < 0) return "melody: Ding dong, Ascending or Double beep";
      next.melody = uint8_t(pick);
    } else if (!strcmp(key, "pattern")) {
      if ((pick = chimeEnum(value, PatternNames, Patterns)) < 0) return "pattern: Ding-dong, Double, Triple, Long ring or Insistent";
      next.pattern = uint8_t(pick);
    } else if (!strcmp(key, "audio_level")) {
      if (!integer(value, 0, 100, number)) return "audio_level: 0 to 100";
      next.level = uint8_t(number);
    } else if (!strcmp(key, "volume")) {
      if (!integer(value, 0, 4, number)) return "volume: 0 (mute) to 4";
      next.volume = uint8_t(number);
    } else if (!strcmp(key, "led")) {
      if (!boolean(value, flag)) return "led: true or false";
      next.led = flag;
    } else if (!strcmp(key, "extra_relay")) {
      if (!boolean(value, flag)) return "extra_relay: true or false";
      next.extraRelay = flag;
    } else if (!strcmp(key, "extra_relay_ms")) {
      if (!integer(value, 50, 2000, number)) return "extra_relay_ms: 50 to 2000";
      next.extraRelayMs = uint16_t(number);
    } else if (!strcmp(key, "pulse_ms")) {
      if (!integer(value, 50, 2000, number)) return "pulse_ms: 50 to 2000";
      next.pulseMs = uint16_t(number);
    } else if (!strcmp(key, "gap_ms")) {
      if (!integer(value, 100, 3000, number)) return "gap_ms: 100 to 3000";
      next.gapMs = uint16_t(number);
    } else if (!strcmp(key, "name")) {
      if (!value.is<const char *>()) return "name: text";
      const char *name = value.as<const char *>();
      const size_t length = strlen(name);
      if (!length || length >= sizeof(next.name) || value.as<JsonString>().size() != length)
        return "name: 1 to 32 bytes";
      for (size_t i = 0; i < length; ++i) if (uint8_t(name[i]) < 0x20) return "name: no control characters";
      memset(next.name, 0, sizeof(next.name)); memcpy(next.name, name, length);
    } else if (!strcmp(key, "mute")) {
      if (!boolean(value, flag)) return "mute: true or false";
      mute = flag;
    } else if (!strcmp(key, "mute_minutes")) {
      if (!integer(value, 0, MaxMuteMinutes, muteMinutes)) return "mute_minutes: 0 to 1440";
    } else {
      return "Unknown or protected setting";
    }
  }
  if (mute >= 0) { next.mute = mute == 1; next.muteMinutes = 0; }
  if (muteMinutes > 0) { next.mute = false; next.muteMinutes = uint16_t(muteMinutes); }
  else if (muteMinutes == 0) next.muteMinutes = 0;
  state = next;
  return nullptr;
}

void writeChime(const ChimeState &state, JsonObject out) {
  out["type"] = GongKindNames[state.type < GongKinds ? state.type : 0];
  out["audio_output"] = AudioOutputNames[state.audioOutput < AudioOutputs ? state.audioOutput : 0];
  out["melody"] = MelodyNames[state.melody < Melodies ? state.melody : 0];
  out["pattern"] = PatternNames[state.pattern < Patterns ? state.pattern : 0];
  out["audio_level"] = state.level; out["volume"] = state.volume;
  out["led"] = state.led; out["extra_relay"] = state.extraRelay;
  out["extra_relay_ms"] = state.extraRelayMs;
  out["pulse_ms"] = state.pulseMs; out["gap_ms"] = state.gapMs;
  out["name"] = state.name;
  out["mute"] = state.mute || state.muteMinutes > 0; out["mute_minutes"] = state.muteMinutes;
}

bool chimeField(const char *field, const char *payload, size_t length, JsonDocument &out) {
  if (!field || !*field) return false;
  out.clear();
  StaticJsonDocument<128> parsed;
  // A JSON scalar ("3", "true", "\"Ascending\"") keeps its type; anything
  // else is used as plain text, so "Ding dong" or "ON" work unquoted.
  const bool scalar = !deserializeJson(parsed, payload, length) &&
    !parsed.is<JsonObject>() && !parsed.is<JsonArray>() && !parsed.isNull();
  // Non-const char pointers make ArduinoJson copy the key and text.
  char key[24]; strncpy(key, field, sizeof(key) - 1); key[sizeof(key) - 1] = 0;
  if (scalar) out[key] = parsed.as<JsonVariantConst>();
  else {
    size_t start = 0, end = length;
    while (start < end && (payload[start] == ' ' || payload[start] == '\t')) ++start;
    while (end > start && (payload[end - 1] == ' ' || payload[end - 1] == '\t' ||
           payload[end - 1] == '\r' || payload[end - 1] == '\n')) --end;
    char text[64];
    if (end - start >= sizeof(text)) return false;
    memcpy(text, payload + start, end - start); text[end - start] = 0;
    out[key] = text;
  }
  return !out.overflowed();
}
}
