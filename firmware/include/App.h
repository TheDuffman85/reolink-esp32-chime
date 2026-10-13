#pragma once
#include <Arduino.h>
#include "Settings.h"

// Core runtime shared by the firmware modules, defined in src/main.cpp:
// diagnostic log, persisted settings, WiFi/setup hotspot, clock and restart.
// LinkService (RF and doorbell link), ChimeService (playback, mute, ring log),
// WebAuth (sign-in), Web (HTTP API/OTA) and MqttService (Home Assistant)
// build on it; main.cpp's setup()/loop() drive them in a fixed order.

// Diagnostic log: every line goes to USB serial and to a RAM ring buffer that
// GET /api/log serves, so the chime can be debugged over WiFi on battery power.
constexpr size_t LogCapacity = 32768;
extern char logRing[LogCapacity];
extern uint32_t logTotal; // Bytes ever written; position = logTotal % LogCapacity.
void logf(const char *format, ...);

extern Settings settings;
extern bool storageOk;
// An OTA image is being written: RF, pairing and chime changes stay idle.
extern bool otaStarted;
extern uint32_t restartAt; // millis() of a requested restart; 0 if none.

// wifiConfig is the saved WiFi mode/hotspot; wifiMode is the mode running
// now (a saved change applies after a restart).
extern chime::WifiConfig wifiConfig;
extern chime::WifiMode wifiMode;
extern String apName, deviceId; // apName: the hotspot's current name.
extern bool apActive;
extern uint32_t stationStarted;
String defaultApName();
// Opens the setup hotspot, or in access-point mode the configured one.
void startAp();
// Physical-access recovery (button 8 s, serial 'w'): back to client mode with
// the setup hotspot open; `forget` also clears the saved network.
void recoverWifi(bool forget);

// UTC seconds, or 0 until SNTP has set the clock.
uint32_t epochNow();
String isoTime(uint32_t epoch);
void startClock();
