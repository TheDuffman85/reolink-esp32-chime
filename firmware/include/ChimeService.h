#pragma once
#include <ArduinoJson.h>
#include "AppProtocol.h"
#include "ChimeControl.h"
#include "Hardware.h"
#include "RingLog.h"

// Playback and the owner-facing chime settings shared by RF, web and MQTT:
// gong type and pattern, app name/volume/LED, mute and the ring history.
extern Gong gong;
extern bool gongReady;
// Gong type and relay pattern, stored separately from the Settings blob
// (namespace "gongx") so existing pairings survive this firmware update.
struct GongExtra { uint8_t type = 1, pattern = 0; uint16_t pulseMs = 300, gapMs = 700; };
extern GongExtra gongx;
// Stored under their own keys in gongx; keeps the existing GongExtra blob layout.
extern uint8_t gongAudioOutput;
extern bool overrideRingtone;
// Applies a validated saved map without recreating tasks or rebooting.
void applyPinsLive(const PinMap &map);
void loadGongExtra();
// nullptr when the selected output has a usable GPIO.
const char *outputError(GongType type, const PinMap &map);
// Chime settings the doorbell app reads and writes (vendor opcodes 0x0B, 0x0C,
// 0x0E, 0x13, 0x15). Volume 0..4 scales the gong volume; stored in NVS.
extern chime::AppConfig chimeApp;
void loadChimeApp();
void saveChimeApp();
// The saved output for a requested sound slot (0..9; the doorbell's music ID,
// MQTT/API "sound", otherwise 0). With the ringtone override on, the saved
// melody/pattern plays instead. appVolume 0..4 scales audio; 0 silences the
// relay too.
GongRequest gongRequest(uint8_t appVolume, uint8_t slot = 0);

// Permanent mute is saved (NVS "mute"); a timed mute lives in RAM, so a
// restart ends it and the chime never stays silent after a power cut.
extern bool muteForever, muteTimed;
uint32_t muteRemainingMs();
bool chimeMuted();
void loadMute();
void serviceMute();

chime::ChimeState currentChime();
// Validates and saves a partial chime update; nullptr on success.
// Setup validates against proposed pins before applying them live.
const char *updateChime(JsonObjectConst update, const char *origin, const PinMap *validationPins = nullptr);

// Ring history (NVS "ringlog"), saved a few seconds after the last change.
extern chime::RingLog ringLog;
void loadRingLog();
void clearRingLog();
// Writes a changed ring log once reception and playback are quiet.
void serviceRingLog();
// labels: names for MQTT/Home Assistant; otherwise indexes for the web UI.
void ringJson(const chime::RingEvent &e, JsonObject o, bool labels);
// Plays (or deliberately does not play) a chime and records it. The audio
// tone volume in `request` is already scaled; appVolume is the 0..4 chime
// volume. An extra relay pulse is kept even at zero audio level.
// The owner's mute silences doorbell rings and MQTT plays (unless forced);
// local tests still play so the sound can be checked. doorbell: the ringing
// doorbell's ID.
chime::RingOutcome playChime(chime::RingSource source, const GongRequest &request, uint8_t appVolume,
                             uint8_t flags = 0, uint16_t tone = 0, int8_t rssi = 0, bool force = false,
                             uint32_t doorbell = 0);
