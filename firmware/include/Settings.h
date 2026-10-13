#pragma once
#include "Hardware.h"
#include <Preferences.h>
#include "WifiConfig.h"

struct Settings {
  uint32_t schema = 2;
  RadioSettings radio;
  GongSettings gong;
  char ssid[33]{};
  char wifiPassword[65]{};
  char adminPassword[25]{};
  bool profile = false;
  uint16_t pair = 0;
  uint32_t source = 0, destination = 0;
  uint8_t key[16]{};
  // Auto locks after three TI CRC matches; saving radio settings clears this.
  uint8_t learnedPolarity = 0;
  // Manual profiles stay passive. Only completed owner-initiated pairing
  // enables join requests and protocol replies on subsequent boots.
  bool paired = false;
  uint8_t heartbeatMode = 0;
  uint32_t nodeId = 0;
};
// Radio options added after Settings' layout was fixed; own NVS namespace.
// autoTrim: automatic trim (Afc.h). calibrated: a trim was measured; until
// then packet-engine RX uses the wide bandwidth. rxMode: RxMode, packet
// engine by default.
struct RadioOptions { bool autoTrim = true; bool calibrated = false;
                      RxMode rxMode = RxMode::PacketEngine; };
bool loadRadioOptions(RadioOptions &options);
bool saveRadioOptions(const RadioOptions &options);
// WiFi mode and access-point hotspot, NVS namespace "wifi".
bool loadWifiConfig(chime::WifiConfig &config);
bool saveWifiConfig(const chime::WifiConfig &config);
bool validSettings(const Settings &settings);
bool loadSettings(Settings &settings);
bool saveSettings(const Settings &settings);
bool parseHex(const String &value, uint8_t *bytes, size_t size);
String hexBytes(const uint8_t *bytes, size_t count);
String hex32(uint32_t value);
String hex16(uint16_t value);
