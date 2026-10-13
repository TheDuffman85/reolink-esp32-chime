#include "WifiConfig.h"
#include <cstring>
#include "ChimeControl.h"

namespace chime {
const char *const WifiModeNames[WifiModes] = {"client", "ap", "off"};

namespace {
bool printable(const char *text, size_t capacity) {
  if (!memchr(text, 0, capacity)) return false;
  for (const char *c = text; *c; ++c) if (*c < 0x20 || *c > 0x7E) return false;
  return true;
}
// A JSON string without embedded NUL, at most `max` characters.
bool text(JsonVariantConst value, size_t max) {
  return value.is<const char *>() && value.as<JsonString>().size() == strlen(value.as<const char *>()) &&
    strlen(value.as<const char *>()) <= max;
}
}

bool validWifiConfig(const WifiConfig &c) {
  const size_t password = strnlen(c.apPassword, sizeof(c.apPassword));
  return uint8_t(c.mode) < WifiModes && printable(c.apSsid, sizeof(c.apSsid)) &&
    printable(c.apPassword, sizeof(c.apPassword)) && (!password || password >= 8);
}

const char *applyWifi(JsonObjectConst update, WifiConfig &config) {
  if (update.isNull()) return "Expected a JSON object";
  WifiConfig next = config;
  for (JsonPairConst pair : update) {
    const char *key = pair.key().c_str();
    if (!strcmp(key, "mode")) {
      const int mode = chimeEnum(pair.value(), WifiModeNames, WifiModes);
      if (mode < 0) return "mode: client, ap or off";
      next.mode = WifiMode(mode);
    } else if (!strcmp(key, "ap_ssid")) {
      if (!text(pair.value(), 32)) return "Hotspot name: up to 32 characters";
      strncpy(next.apSsid, pair.value().as<const char *>(), sizeof(next.apSsid) - 1);
    } else if (!strcmp(key, "ap_password")) {
      if (!text(pair.value(), 63)) return "Hotspot password: 8-63 characters";
      const char *password = pair.value().as<const char *>();
      if (password[0]) {
        if (strlen(password) < 8) return "Hotspot password: 8-63 characters";
        strncpy(next.apPassword, password, sizeof(next.apPassword) - 1);
      }
    } else if (strcmp(key, "clear_ap_password")) return "Expected mode, ap_ssid, ap_password or clear_ap_password";
  }
  if (update.containsKey("clear_ap_password")) {
    if (!update["clear_ap_password"].is<bool>()) return "clear_ap_password: true or false";
    if (update["clear_ap_password"].as<bool>()) memset(next.apPassword, 0, sizeof(next.apPassword));
  }
  if (!validWifiConfig(next)) return "Use printable characters (no umlauts) for the hotspot name and password";
  config = next;
  return nullptr;
}
}
