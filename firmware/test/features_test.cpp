// Host tests for the portable parts of sign-in, the ring log and the
// remote-safe chime settings used by MQTT/Home Assistant.
#include "Auth.h"
#include "ChimeControl.h"
#include "Audio.h"
#include "Protocol.h"
#include "SetupSound.h"
#include "RingLog.h"
#include "WifiConfig.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <openssl/hmac.h>

using namespace chime;

bool hmac(const uint8_t *key, size_t keyLength, const uint8_t *data, size_t length, uint8_t out[32]) {
  unsigned size = 32;
  return HMAC(EVP_sha256(), key, int(keyLength), data, length, out, &size) && size == 32;
}

const char *apply(ChimeState &state, const char *json) {
  DynamicJsonDocument doc(512);
  assert(!deserializeJson(doc, json));
  return applyChime(doc.as<JsonObjectConst>(), state);
}

void authTests() {
  uint8_t key[32], other[32], nonce[8] = {1, 2, 3, 4, 5, 6, 7, 8};
  for (int i = 0; i < 32; ++i) { key[i] = uint8_t(i); other[i] = uint8_t(i + 1); }
  char token[TokenLength + 1];
  assert(makeToken(key, 2000000000, nonce, hmac, token));
  assert(strlen(token) == TokenLength && token[8] == '.' && token[25] == '.');
  assert(!strncmp(token, "77359400.0102030405060708.", 26));
  assert(checkToken(key, token, TokenLength, 0, hmac));          // clock unknown
  assert(checkToken(key, token, TokenLength, 1999999999, hmac)); // before expiry
  assert(!checkToken(key, token, TokenLength, 2000000000, hmac));
  assert(!checkToken(other, token, TokenLength, 0, hmac));       // password changed
  char tampered[TokenLength + 1]; memcpy(tampered, token, sizeof(tampered));
  tampered[0] = '0'; // extending the expiry invalidates the MAC
  assert(!checkToken(key, tampered, TokenLength, 0, hmac));
  memcpy(tampered, token, sizeof(tampered)); tampered[40] = tampered[40] == 'a' ? 'b' : 'a';
  assert(!checkToken(key, tampered, TokenLength, 0, hmac));
  memcpy(tampered, token, sizeof(tampered)); tampered[30] = 'G';
  assert(!checkToken(key, tampered, TokenLength, 0, hmac));
  assert(!checkToken(key, token, TokenLength - 1, 0, hmac));
  assert(!checkToken(key, nullptr, 0, 0, hmac));
  assert(makeToken(key, 0, nonce, hmac, token) && checkToken(key, token, TokenLength, 4000000000u, hmac));

  assert(sameSecret("correct horse", "correct horse"));
  assert(!sameSecret("correct horse", "correct horsf"));
  assert(!sameSecret("correct", "correct horse") && !sameSecret("", ""));

  LoginThrottle throttle; uint32_t wait;
  const uint32_t a = 0x0A00000A, b = 0x0A00000B;
  for (int i = 0; i < 3; ++i) { assert(throttle.allowed(a, 1000, wait)); throttle.failure(a, 1000); }
  assert(throttle.allowed(a, 1000, wait) && wait == 0);
  throttle.failure(a, 1000);                      // 4th failure: 1 s
  assert(!throttle.allowed(a, 1500, wait) && wait == 500);
  assert(throttle.allowed(b, 1500, wait));        // other client unaffected
  assert(throttle.allowed(a, 2000, wait));
  throttle.failure(a, 2000);                      // 5th failure: 2 s
  assert(!throttle.allowed(a, 3999, wait) && wait == 1);
  for (int i = 0; i < 20; ++i) throttle.failure(a, 10000);
  assert(!throttle.allowed(a, 10000, wait) && wait == 300000); // capped at 5 min
  throttle.success(a);
  assert(throttle.allowed(a, 10001, wait));
  // Rollover: lock-out spanning millis() wrap still applies.
  for (int i = 0; i < 4; ++i) throttle.failure(b, 0xFFFFFF00u);
  assert(!throttle.allowed(b, 0x00000010u, wait) && wait == 1000 - 0x110);
  // Many addresses evict the least recently active slot without crashing.
  for (uint32_t ip = 100; ip < 120; ++ip) throttle.failure(ip, ip);
}

void ringLogTests() {
  RingLog log;
  assert(log.size() == 0 && log.lastId() == 0);
  for (int i = 0; i < 60; ++i) {
    RingEvent e; e.ms = uint32_t(i * 1000); e.tone = uint16_t(i % 11);
    e.source = uint8_t(i % RingSources); e.outcome = uint8_t(i % RingOutcomes); // includes chime_muted
    e.rssi = int8_t(-60 - i % 5); e.level = 35; e.volume = 4;
    const RingEvent &stored = log.add(e);
    assert(stored.id == uint32_t(i + 1));
  }
  assert(log.size() == RingLog::Capacity && log.lastId() == 60);
  assert(log.newest(0).id == 60 && log.newest(RingLog::Capacity - 1).id == 11);
  uint8_t image[RingLog::ImageSize];
  const size_t length = log.serialize(image, sizeof(image));
  assert(length == RingLog::ImageSize);
  assert(log.serialize(image, sizeof(image) - 1) == 0);
  RingLog copy;
  assert(copy.restore(image, length));
  assert(copy.size() == log.size() && copy.lastId() == 60);
  for (size_t i = 0; i < copy.size(); ++i) {
    const RingEvent &x = copy.newest(i), &y = log.newest(i);
    assert(x.id == y.id && x.ms == y.ms && x.tone == y.tone && x.source == y.source &&
           x.outcome == y.outcome && x.rssi == y.rssi && x.level == 35 && x.volume == 4);
    assert(x.flags & RingPreviousBoot);
  }
  RingEvent next; next.source = 1;
  assert(copy.add(next).id == 61 && copy.newest(0).id == 61 && !(copy.newest(0).flags & RingPreviousBoot));
  assert(copy.newest(1).id == 60);
  // Invalid images leave the log untouched.
  RingLog small; small.add(RingEvent{});
  assert(!small.restore(image, length - 1));
  image[2] = 2; assert(!small.restore(image, length)); image[2] = 1;
  image[RingLog::HeaderSize + 14] = RingSources; assert(!small.restore(image, length));
  assert(small.size() == 1 && small.lastId() == 1);
  // Partial log and clear.
  RingLog part; part.add(RingEvent{}); part.add(RingEvent{});
  const size_t partLength = part.serialize(image, sizeof(image));
  assert(partLength == RingLog::HeaderSize + 2 * RingLog::EntrySize);
  RingLog restored; assert(restored.restore(image, partLength) && restored.size() == 2 && restored.newest(0).id == 2);
  restored.add(RingEvent{}); assert(restored.newest(0).id == 3 && restored.newest(2).id == 1);
  restored.clear(); assert(restored.size() == 0 && restored.add(RingEvent{}).id == 4);
}

bool same(const ChimeState &a, const ChimeState &b) {
  return a.type == b.type && a.audioOutput == b.audioOutput && a.melody == b.melody && a.level == b.level && a.volume == b.volume &&
    a.pattern == b.pattern && a.led == b.led && a.extraRelay == b.extraRelay && a.overrideRingtone == b.overrideRingtone &&
    a.extraRelayMs == b.extraRelayMs && a.pulseMs == b.pulseMs && a.gapMs == b.gapMs &&
    !strcmp(a.name, b.name) && a.mute == b.mute && a.muteMinutes == b.muteMinutes;
}

void setupSoundTests() {
  ChimeState state; SetupOutputPins pins;
  const auto apply = [&](const char *json) {
    StaticJsonDocument<768> doc; assert(!deserializeJson(doc,json));
    return applySetupSound(doc.as<JsonObjectConst>(),state,pins);
  };
  assert(!apply(R"({"chime":{"type":"Audio","audio_output":"DAC","melody":2},"pins":{"audio":26,"relay":-1}})"));
  assert(state.type==0 && state.audioOutput==1 && state.melody==2 && pins.audio==26 && pins.relay==-1);
  assert(!apply(R"({"chime":{"type":"Gong"},"pins":{"relay":27,"relay_active_low":true}})"));
  assert(state.type==1 && pins.relay==27 && pins.relayActiveLow);
  const ChimeState before=state; const SetupOutputPins beforePins=pins;
  const char *bad[]={R"({})", R"({"chime":[],"pins":{}})",
    R"({"chime":{"name":"New"},"pins":{"audio":"25"}})",
    R"({"chime":{"type":0},"pins":{"relay_active_low":1}})",
    R"({"chime":{"ssid":"x"},"pins":{}})", R"({"chime":{},"pins":{"sck":18}})",
    R"({"chime":{"audio_output":2},"pins":{"relay":25}})",
    R"({"chime":{},"pins":{"audio":40}})", R"({"chime":{},"pins":{"relay":-2}})",
    R"({"chime":{},"pins":{},"restart":true})"};
  for (const char *json:bad) {
    assert(apply(json)); assert(same(state,before));
    assert(pins.audio==beforePins.audio && pins.relay==beforePins.relay && pins.relayActiveLow==beforePins.relayActiveLow);
  }
}

void chimeTests() {
  assert(dacPin(25) && dacPin(26) && !dacPin(27) && !dacPin(-1));
  assert(audioPhaseStep(0) == 0 && audioPhaseStep(1000) == (uint32_t(1) << 28));
  uint32_t phase = 0; uint16_t lo = 65535, hi = 0;
  for (int i = 0; i < 32; ++i) {
    const uint16_t sample = dacSample(phase, 100);
    assert((sample & 255) == 0 && dacSample(phase, 0) == 32768);
    if (sample < lo) lo = sample;
    if (sample > hi) hi = sample;
    const int full = int(sample >> 8) - 128;
    const int half = int(dacSample(phase, 50) >> 8) - 128;
    assert(half == full / 2);
    phase += uint32_t(1) << 27;
  }
  assert(lo == 256 && hi == 65280 && phase == 0);

  ChimeState state;
  assert(state.type == uint8_t(GongKind::Relay));
  assert(!apply(state, R"({"type":"Audio","audio_output":"PWM","audio_level":0})"));
  assert(state.type == uint8_t(GongKind::Audio) && state.audioOutput == 0 && state.level == 0);
  assert(!apply(state, R"({"audio_output":"dac","audio_level":100})"));
  assert(state.type == uint8_t(GongKind::Audio) && state.audioOutput == 1 && state.level == 100);
  assert(!apply(state, R"({"type":"Gong"})") && state.audioOutput == 1);
  assert(!apply(state, R"({"type":"None"})") && state.audioOutput == 1);
  assert(!apply(state, R"({"type":"Audio"})") && state.audioOutput == 1);
  assert(!apply(state, R"({"audio_output":0})") && state.audioOutput == 0);
  assert(!apply(state, R"({"audio_output":1})") && state.audioOutput == 1);
  // Upgrade mappings preserve the mode and audio method from legacy NVS/logs.
  for (uint8_t legacy = 0; legacy < 4; ++legacy) {
    assert(modeFromStoredType(legacy) == (legacy == 3 ? 0 : legacy));
    assert(audioFromStoredType(legacy) == (legacy == 3 ? 1 : 0));
  }
  assert(!apply(state, R"({"melody":"Ascending","volume":3,"led":"OFF","type":"gong"})"));
  assert(state.melody == 1 && state.volume == 3 && !state.led && state.type == 1);
  assert(!apply(state, R"({"melody":"double_beep","pattern":"LONG-RING","type":2})"));
  assert(state.melody == 2 && state.pattern == 3 && state.type == 2);
  assert(!apply(state, R"({"audio_level":55.0,"extra_relay":1,"extra_relay_ms":800,"pulse_ms":250,"gap_ms":400})"));
  assert(state.level == 55 && state.extraRelay && state.extraRelayMs == 800 && state.pulseMs == 250 && state.gapMs == 400);
  assert(!apply(state, R"({"name":"Hallway"})") && !strcmp(state.name, "Hallway"));
  // The ringtone override is off by default and accepts the usual booleans.
  assert(!ChimeState().overrideRingtone);
  assert(!apply(state, R"({"override_ringtone":"ON"})") && state.overrideRingtone);
  assert(!apply(state, R"({"override_ringtone":false})") && !state.overrideRingtone);
  // Mute: permanent, timed, order-independent combination and unmute.
  assert(!apply(state, R"({"mute":"ON"})") && state.mute && state.muteMinutes == 0);
  assert(!apply(state, R"({"mute_minutes":30})") && !state.mute && state.muteMinutes == 30);
  assert(!apply(state, R"({"mute_minutes":0})") && !state.mute && state.muteMinutes == 0);
  assert(!apply(state, R"({"mute_minutes":15,"mute":true})") && !state.mute && state.muteMinutes == 15);
  assert(!apply(state, R"({"mute":true})") && state.mute && state.muteMinutes == 0);
  assert(!apply(state, R"({"mute_minutes":0})") && state.mute); // ends only a timed mute
  assert(!apply(state, R"({"mute_minutes":1440})") && state.muteMinutes == 1440);
  assert(!apply(state, R"({"mute":false})") && !state.mute && state.muteMinutes == 0);
  // Atomic rejection: one bad field leaves everything unchanged.
  const ChimeState before = state;
  const char *bad[] = {
    R"({"volume":5})", R"({"volume":2.5})", R"({"volume":"3"})", R"({"audio_level":-1})",
    R"({"type":"Buzzer"})", R"({"type":3})", R"({"type":"PWM"})", R"({"type":"DAC"})",
    R"({"audio_output":2})", R"({"audio_output":-1})", R"({"audio_output":"I2S"})",
    R"({"type":"Audio","audio_output":0.5})", R"({"audio_level":101})",
    R"({"audio_level":0.5})", R"({"buzzer_level":50})", R"({"melody":"Siren"})", R"({"melody":10})", R"({"pattern":10})", R"({"type":"relayx"})", R"({"pattern":""})",
    R"({"led":"maybe"})", R"({"led":2})", R"({"override_ringtone":"always"})", R"({"sound":3})", R"({"pulse_ms":49})", R"({"gap_ms":3001})",
    R"({"extra_relay_ms":2001})", R"({"mute_minutes":1441})", R"({"mute_minutes":-1})", R"({"mute":"soon"})", R"({"name":""})", R"({"name":"123456789012345678901234567890123"})",
    R"({"name":"a\u0001b"})", R"({"name":5})",
    // Administrative settings are not part of the chime state.
    R"({"melody":0,"ssid":"evil"})", R"({"password":"x"})", R"({"frequency_hz":868000000})",
    R"({"wifi_password":"x"})", R"({"trim":3})", R"({"relay":true})", R"({"key":"00"})",
  };
  for (const char *json : bad) {
    assert(apply(state, json) != nullptr);
    assert(same(state, before));
  }
  assert(applyChime(JsonObjectConst(), state) != nullptr);

  StaticJsonDocument<512> out;
  writeChime(state, out.to<JsonObject>());
  assert(!strcmp(out["type"], "None") && !strcmp(out["melody"], "Double beep") &&
         !strcmp(out["pattern"], "Long ring") && out["volume"] == 3 && out["led"] == false && out["override_ringtone"] == false &&
         !strcmp(out["name"], "Hallway"));
  assert(out["mute"] == false && out["mute_minutes"] == 0 && out["audio_output"] == "DAC");
  state.muteMinutes = 20;
  writeChime(state, out.to<JsonObject>());
  assert(out["mute"] == true && out["mute_minutes"] == 20);
  // The published state can be fed back unchanged.
  ChimeState again; assert(!applyChime(out.as<JsonObjectConst>(), again));
  assert(same(again, state));

  DynamicJsonDocument field(256);
  const char *payloads[][3] = {{"volume", "2", nullptr}, {"led", "ON", nullptr}, {"melody", " Ding dong \n", nullptr},
                               {"melody", "\"Ascending\"", nullptr}, {"extra_relay", "false", nullptr}};
  for (auto &p : payloads) {
    assert(chimeField(p[0], p[1], strlen(p[1]), field));
    assert(!applyChime(field.as<JsonObjectConst>(), state));
  }
  assert(state.volume == 2 && state.led && state.melody == 1 && !state.extraRelay);
  assert(chimeField("name", "Front door", 10, field) && !applyChime(field.as<JsonObjectConst>(), state));
  assert(!strcmp(state.name, "Front door"));
  assert(chimeField("ssid", "x", 1, field) && applyChime(field.as<JsonObjectConst>(), state));
  assert(!chimeField("", "1", 1, field));
  char longText[80]; memset(longText, 'x', sizeof(longText));
  assert(!chimeField("name", longText, sizeof(longText), field));

  // Every doorbell slot is selectable and survives named MQTT state round trips.
  for (uint8_t i = 0; i < DoorbellSounds; ++i) {
    out.clear(); out["melody"] = i; out["pattern"] = i;
    assert(!applyChime(out.as<JsonObjectConst>(), state));
    assert(state.melody == i && state.pattern == i);
    writeChime(state, out.to<JsonObject>());
    ChimeState restored; assert(!applyChime(out.as<JsonObjectConst>(), restored));
    assert(same(restored, state));
  }
}

void soundTests() {
  static_assert(Melodies == 10 && Patterns == 10, "V2 has ten ringtone slots");
  // Parse actual plaintext ring commands before selecting the sound; includes
  // the complete u16 range so an upper byte never aliases a valid slot.
  // The override keeps the saved choices but still rejects unknown IDs.
  for (uint32_t id = 0; id <= UINT16_MAX; ++id) {
    uint8_t plain[] = {9, 0xA4, uint8_t(id), uint8_t(id >> 8)};
    Ring ring{}; assert(ringPayload(plain, ring));
    for (bool override : {false, true}) {
      uint8_t melody = 2, pattern = 4;
      assert(selectSound(ring.parameter, override, melody, pattern) == (id < DoorbellSounds));
      if (id < DoorbellSounds && !override) assert(melody == id && pattern == id);
      else assert(melody == 2 && pattern == 4);
    }
  }
  // MQTT/API "sound": an index or either list's name for the same slot.
  StaticJsonDocument<64> slot;
  const struct { const char *json; int slot; } slots[] = {{"0", 0}, {"9", 9}, {"\"Ding dong\"", 0},
    {"\"ding-dong\"", 0}, {"\"Descending\"", 3}, {"\"Long ring\"", 3}, {"\"three_pairs\"", 9},
    {"10", -1}, {"-1", -1}, {"2.5", -1}, {"\"\"", -1}, {"\"Siren\"", -1}, {"true", -1}, {"null", -1}};
  for (const auto &s : slots) { assert(!deserializeJson(slot, s.json)); assert(soundSlot(slot.as<JsonVariantConst>()) == s.slot); }
  for (uint8_t i = 0; i < Melodies; ++i) {
    const Note *notes = melodyScore(i);
    assert(notes && notes[0].hz && notes[0].ms);
    uint32_t duration = 0;
    for (uint8_t n = 0; n < MaxNotes && notes[n].ms; ++n) {
      assert(notes[n].hz < AudioSampleRate / 2);
      duration += notes[n].ms;
    }
    assert(duration >= 400 && duration <= 3000);
  }
  assert(!melodyScore(Melodies) && !melodyScore(255));
  // Legacy timing and grouped rhythms; protect long/quick pulse limits.
  assert(strikeScore(0,300,700).count == 1);
  assert(strikeScore(3,2000,700).onMs == 3000);
  auto quick = strikeScore(4,50,100);
  assert(quick.count == 5 && quick.onMs == 50 && quick.gaps[0] == 100);
  assert(strikeScore(6,300,3000).gaps[0] == 3000);
  for (uint8_t i = 0; i < Patterns; ++i) {
    auto score = strikeScore(i,300,700);
    assert(score.count >= 1 && score.count <= 6 && score.onMs >= 50 && score.onMs <= 3000);
    for (uint8_t g = 0; g + 1 < score.count; ++g)
      assert(score.gaps[g] >= 100 && score.gaps[g] <= 3000);
  }
  auto pairs = strikeScore(9,300,700);
  assert(pairs.count == 6 && pairs.gaps[0] < pairs.gaps[1] && pairs.gaps[2] == pairs.gaps[0]);
}

const char *wifi(WifiConfig &config, const char *json) {
  DynamicJsonDocument doc(512);
  assert(!deserializeJson(doc, json));
  return applyWifi(doc.as<JsonObjectConst>(), config);
}

void wifiTests() {
  WifiConfig config;
  assert(validWifiConfig(config) && config.mode == WifiMode::Client);
  assert(!wifi(config, R"({"mode":"ap","ap_ssid":"Chime","ap_password":"secret123"})"));
  assert(config.mode == WifiMode::AccessPoint && !strcmp(config.apSsid, "Chime") && !strcmp(config.apPassword, "secret123"));
  assert(!wifi(config, R"({"mode":"OFF","ap_password":""})")); // blank keeps the password
  assert(config.mode == WifiMode::Off && !strcmp(config.apPassword, "secret123"));
  assert(!wifi(config, R"({"mode":0,"ap_password":"other-secret"})"));
  assert(config.mode == WifiMode::Client && !strcmp(config.apPassword, "other-secret"));
  assert(!wifi(config, R"({"ap_password":"12345678"})") && !strcmp(config.apPassword, "12345678"));
  assert(!memchr(config.apPassword + 8, 1, sizeof(config.apPassword) - 8)); // shorter password leaves no tail
  const WifiConfig before = config;
  const auto unchanged = [&]() { return !memcmp(&config, &before, sizeof(config)); };
  assert(wifi(config, R"({"mode":"mesh"})") && unchanged());
  assert(wifi(config, R"({"mode":3})") && unchanged());
  assert(wifi(config, R"({"ap_password":"short"})") && unchanged());
  assert(wifi(config, R"({"ap_password":"0123456789012345678901234567890123456789012345678901234567890123"})") && unchanged());
  assert(wifi(config, R"({"ap_ssid":"012345678901234567890123456789012"})") && unchanged());
  assert(wifi(config, R"({"ap_ssid":"Klingel\u00e4"})") && unchanged());
  assert(wifi(config, R"({"ap_ssid":"a\u0000b"})") && unchanged());
  assert(wifi(config, R"({"ap_ssid":5})") && unchanged());
  assert(wifi(config, R"({"mode":"ap","ssid":"x"})") && unchanged()); // unknown key rejects all
  assert(wifi(config, R"({"clear_ap_password":"yes"})") && unchanged());
  assert(!wifi(config, R"({"clear_ap_password":true,"ap_ssid":""})"));
  assert(!config.apPassword[0] && !config.apSsid[0] && validWifiConfig(config));
  config.apPassword[0] = 'x'; assert(!validWifiConfig(config)); // 1-7 characters
  config.apPassword[0] = 0; config.mode = WifiMode(7); assert(!validWifiConfig(config));
}

int main() {
  authTests();
  ringLogTests();
  chimeTests(); setupSoundTests(); soundTests();
  wifiTests();
  std::cout << "Feature tests passed: session tokens, sign-in throttle, ring log image, MQTT chime settings, WiFi mode\n";
}
