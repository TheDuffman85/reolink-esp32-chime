#include "Settings.h"
#include "Band.h"
#include "Identity.h"
#include <esp_system.h>

namespace {
struct LegacySettings {
  uint32_t schema;
  RadioSettings radio;
  GongSettings gong;
  char ssid[33], wifiPassword[65], adminPassword[25];
  bool profile;
  uint16_t pair;
  uint32_t source, destination;
  uint8_t key[16];
  uint8_t learnedPolarity;
};
uint32_t newNodeId() {
  uint32_t id;
  do { id = chime::nodeIdCandidate(esp_random()); } while (!chime::compatibleNodeId(id));
  return id;
}
}

bool validSettings(const Settings &s) {
  return s.schema == 2 && s.nodeId && s.nodeId != UINT32_MAX &&
    (!s.paired || (s.profile && s.source && s.destination == s.nodeId)) &&
    chime::validFrequency(s.radio.hz) &&
    uint8_t(s.radio.polarity) <= 2 && s.gong.melody < chime::Melodies && s.gong.volume <= 100 &&
    s.gong.relayMs >= 50 && s.gong.relayMs <= 2000 && s.learnedPolarity <= 2 &&
    memchr(s.ssid, 0, sizeof(s.ssid)) && memchr(s.wifiPassword, 0, sizeof(s.wifiPassword)) &&
    memchr(s.adminPassword, 0, sizeof(s.adminPassword)) && strlen(s.adminPassword) >= 12;
}
bool saveSettings(const Settings &settings) {
  Preferences prefs;
  if (!prefs.begin("chime", false)) return false;
  const bool ok = prefs.putBytes("settings", &settings, sizeof(settings)) == sizeof(settings);
  prefs.end(); return ok;
}
bool loadSettings(Settings &settings) {
  Preferences prefs;
  if (prefs.begin("chime", true)) {
    if (prefs.getBytesLength("settings") == sizeof(settings)) {
      Settings candidate;
      prefs.getBytes("settings", &candidate, sizeof(candidate));
      if (validSettings(candidate)) { settings = candidate; prefs.end(); return true; }
    }
    if (prefs.getBytesLength("settings") == sizeof(LegacySettings)) {
      LegacySettings old{};
      prefs.getBytes("settings", &old, sizeof(old));
      if (old.schema == 1) {
        Settings candidate;
        candidate.nodeId = newNodeId();
        candidate.radio = old.radio; candidate.gong = old.gong;
        memcpy(candidate.ssid, old.ssid, sizeof(old.ssid));
        memcpy(candidate.wifiPassword, old.wifiPassword, sizeof(old.wifiPassword));
        memcpy(candidate.adminPassword, old.adminPassword, sizeof(old.adminPassword));
        candidate.profile = old.profile; candidate.pair = old.pair;
        candidate.source = old.source; candidate.destination = old.destination;
        memcpy(candidate.key, old.key, sizeof(old.key));
        candidate.learnedPolarity = old.learnedPolarity;
        if (validSettings(candidate)) {
          settings = candidate; prefs.end(); return saveSettings(settings);
        }
      }
    }
    prefs.end();
  }
  settings = Settings{};
  settings.nodeId = newNodeId();
  uint8_t random[8]; esp_fill_random(random, sizeof(random));
  const String password = hexBytes(random, sizeof(random));
  password.toCharArray(settings.adminPassword, sizeof(settings.adminPassword));
  return saveSettings(settings);
}
bool loadRadioOptions(RadioOptions &options) {
  options = RadioOptions{};
  Preferences prefs;
  if (!prefs.begin("radio", true)) {
    // 1.0.8 kept automatic trim in namespace "afc"; carry it over.
    if (prefs.begin("afc", true)) {
      options.autoTrim = prefs.getBool("on", true); options.calibrated = prefs.getBool("cal", false);
      prefs.end();
    }
    return false; // Otherwise defaults: packet engine, automatic trim on.
  }
  options.autoTrim = prefs.getBool("autotrim", true);
  options.calibrated = prefs.getBool("cal", false);
  options.rxMode = prefs.getUChar("rxmode", 0) == 1 ? RxMode::Esp32 : RxMode::PacketEngine;
  prefs.end(); return true;
}
bool saveRadioOptions(const RadioOptions &options) {
  Preferences prefs;
  if (!prefs.begin("radio", false)) return false;
  const bool ok = prefs.putBool("autotrim", options.autoTrim) && prefs.putBool("cal", options.calibrated) &&
    prefs.putUChar("rxmode", uint8_t(options.rxMode));
  prefs.end(); return ok;
}
bool loadWifiConfig(chime::WifiConfig &config) {
  config = chime::WifiConfig{};
  Preferences prefs; chime::WifiConfig stored;
  if (!prefs.begin("wifi", true)) return false; // Saved before this option: client mode.
  const bool found = prefs.getBytesLength("cfg") == sizeof(stored) &&
    prefs.getBytes("cfg", &stored, sizeof(stored)) == sizeof(stored) && chime::validWifiConfig(stored);
  prefs.end();
  if (found) config = stored;
  return found;
}
bool saveWifiConfig(const chime::WifiConfig &config) {
  if (!chime::validWifiConfig(config)) return false;
  Preferences prefs;
  if (!prefs.begin("wifi", false)) return false;
  const bool ok = prefs.putBytes("cfg", &config, sizeof(config)) == sizeof(config);
  prefs.end(); return ok;
}
bool parseHex(const String &value, uint8_t *bytes, size_t size) {
  if (value.length() != size * 2) return false;
  const auto digit = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  };
  for (size_t i = 0; i < size; ++i) {
    const int hi = digit(value[2*i]), lo = digit(value[2*i+1]);
    if (hi < 0 || lo < 0) return false;
    bytes[i] = (hi << 4) | lo;
  }
  return true;
}
String hexBytes(const uint8_t *bytes, size_t count) {
  const char digits[] = "0123456789ABCDEF";
  String result; result.reserve(count * 2);
  for (size_t i = 0; i < count; ++i) { result += digits[bytes[i] >> 4]; result += digits[bytes[i] & 15]; }
  return result;
}
String hex32(uint32_t value) { char buffer[9]; snprintf(buffer, sizeof(buffer), "%08lX", (unsigned long)value); return buffer; }
String hex16(uint16_t value) { char buffer[5]; snprintf(buffer, sizeof(buffer), "%04X", value); return buffer; }
