#pragma once
#include <ArduinoJson.h>
#include <cstdint>

namespace chime {
// Persisted WiFi mode (NVS "wifi"). Client joins the saved network and opens
// the setup hotspot when that fails; AccessPoint runs only the chime's own
// hotspot; Off keeps the radio off. Holding the button 8 s or sending 'w' on
// the serial console returns to Client.
enum class WifiMode : uint8_t { Client = 0, AccessPoint = 1, Off = 2 };
constexpr uint8_t WifiModes = 3;
extern const char *const WifiModeNames[WifiModes]; // "client", "ap", "off"

// Hotspot of access-point mode. An empty name uses "Reolink-Chime-<id>"; an
// empty password uses the sign-in password while sign-in is on, else the
// hotspot is open (the same rule as the setup hotspot).
struct WifiConfig {
  WifiMode mode = WifiMode::Client;
  char apSsid[33]{};
  char apPassword[64]{};
};
// Name 0-32 and password 0 or 8-63 printable ASCII characters.
bool validWifiConfig(const WifiConfig &config);
// {"mode":"client"|"ap"|"off"|0-2, "ap_ssid", "ap_password", "clear_ap_password"}.
// A blank/missing "ap_password" keeps the saved one. Unknown keys and invalid
// values reject the whole update; `config` changes only on success.
// Returns nullptr on success or a short error text.
const char *applyWifi(JsonObjectConst update, WifiConfig &config);
}
