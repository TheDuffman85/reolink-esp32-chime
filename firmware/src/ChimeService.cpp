#include "ChimeService.h"
#include <Preferences.h>
#include "App.h"
#include "Audio.h"
#include "LinkService.h"
#include "MqttService.h"

Gong gong;
bool gongReady = false;
GongExtra gongx;
uint8_t gongAudioOutput = 0;
chime::AppConfig chimeApp;
bool muteForever = false, muteTimed = false;
chime::RingLog ringLog;

namespace {
uint32_t muteUntil = 0;
bool ringLogDirty = false;
uint32_t ringLogChangedAt = 0;

// Persists only the groups that changed. Flash writes pause RF reception.
bool storeChime(const chime::ChimeState &next) {
  Settings nextSettings = settings;
  nextSettings.gong.melody = next.melody; nextSettings.gong.volume = next.level;
  nextSettings.gong.relay = next.extraRelay; nextSettings.gong.relayMs = next.extraRelayMs;
  GongExtra nextGong = gongx;
  nextGong.type = next.type; nextGong.pattern = next.pattern; nextGong.pulseMs = next.pulseMs; nextGong.gapMs = next.gapMs;
  chime::AppConfig nextApp = chimeApp;
  memcpy(nextApp.name, next.name, sizeof(nextApp.name)); nextApp.volume = next.volume; nextApp.led = next.led;
  const bool settingsChanged = nextSettings.gong.melody != settings.gong.melody ||
    nextSettings.gong.volume != settings.gong.volume || nextSettings.gong.relay != settings.gong.relay ||
    nextSettings.gong.relayMs != settings.gong.relayMs;
  const bool gongChanged = nextGong.type != gongx.type || nextGong.pattern != gongx.pattern ||
    nextGong.pulseMs != gongx.pulseMs || nextGong.gapMs != gongx.gapMs || next.audioOutput != gongAudioOutput;
  const bool appChanged = strcmp(nextApp.name, chimeApp.name) || nextApp.volume != chimeApp.volume ||
    nextApp.led != chimeApp.led;
  // A timed mute needs no flash write; minutes left only change when requested.
  const uint16_t minutesLeft = uint16_t((muteRemainingMs() + 59999) / 60000);
  if (next.muteMinutes != minutesLeft) {
    muteTimed = next.muteMinutes > 0; muteUntil = millis() + uint32_t(next.muteMinutes) * 60000;
  }
  const bool muteChanged = next.mute != muteForever;
  if (!settingsChanged && !gongChanged && !appChanged && !muteChanged) return true;
  radio.pause();
  bool ok = true;
  if (muteChanged) {
    Preferences prefs; ok = prefs.begin("mute", false);
    if (ok) { ok = prefs.putUChar("on", next.mute ? 1 : 0) == 1; prefs.end(); }
    if (ok) muteForever = next.mute;
  }
  if (ok && settingsChanged) { ok = saveSettings(nextSettings); storageOk = ok; if (ok) settings = nextSettings; }
  if (ok && gongChanged) {
    Preferences prefs; ok = prefs.begin("gongx", false);
    if (ok) {
      ok = prefs.putUChar("audioout", next.audioOutput) == 1 &&
        prefs.putBytes("gong", &nextGong, sizeof(nextGong)) == sizeof(nextGong);
      prefs.end();
    }
    if (ok) { gongx = nextGong; gongAudioOutput = next.audioOutput; }
  }
  if (ok && appChanged) {
    Preferences prefs; ok = prefs.begin("chimeapp", false);
    if (ok) {
      ok = prefs.putBytes("name", nextApp.name, sizeof(nextApp.name) - 1) == sizeof(nextApp.name) - 1 &&
        prefs.putUChar("volume", nextApp.volume) && prefs.putUChar("led", nextApp.led);
      prefs.end();
    }
    if (ok) chimeApp = nextApp;
  }
  restartRadio();
  return ok;
}

void saveRingLog() {
  static uint8_t image[chime::RingLog::ImageSize];
  const size_t length = ringLog.serialize(image, sizeof(image));
  radio.pause();
  Preferences prefs; bool ok = prefs.begin("ringlog", false);
  if (ok) { ok = prefs.putBytes("log", image, length) == length; prefs.end(); }
  restartRadio();
  ringLogDirty = false;
  if (!ok) logf("Ring log could not be saved\n");
}
}

void applyPinsLive(const PinMap &map) {
  const bool radioChanged=memcmp(&pins,&map,offsetof(PinMap,audio))!=0;
  gong.inhibit(true); radio.pause();
  if (pins.relay>=0) { digitalWrite(pins.relay,pins.relayActiveLow?HIGH:LOW); pinMode(pins.relay,INPUT); }
  if (pins.audio>=0) { digitalWrite(pins.audio,LOW); pinMode(pins.audio,INPUT); }
  if (pins.led>=0) { digitalWrite(pins.led,LOW); pinMode(pins.led,INPUT); }
  if (pins.button>=0) pinMode(pins.button,INPUT);
  radio.remapPins(map);
  gong.configurePins();
  if (pins.led>=0) { digitalWrite(pins.led,LOW); pinMode(pins.led,OUTPUT); }
  if (pins.button>=0) pinMode(pins.button,INPUT_PULLUP);
  ++pinMapRevision;
  restartRadio();
  if (radioChanged) initLink(radio.healthy);
  gong.inhibit(false);
  logf("GPIO map applied live; radio %s\n",radio.healthy?"ready":"unavailable");
}

void loadGongExtra() {
  Preferences prefs; if (!prefs.begin("gongx", true)) return;
  GongExtra stored;
  if (prefs.getBytesLength("gong") == sizeof(stored) && prefs.getBytes("gong", &stored, sizeof(stored)) == sizeof(stored) &&
      stored.type <= 3 && stored.pattern < RelayPatterns && stored.pulseMs >= 50 && stored.pulseMs <= 2000 &&
      stored.gapMs >= 100 && stored.gapMs <= 3000) {
    // A separate method key also remembers DAC while Gong or None is selected.
    const uint8_t output = prefs.getUChar("audioout", chime::audioFromStoredType(stored.type));
    gongAudioOutput = output < chime::AudioOutputs ? output : chime::audioFromStoredType(stored.type);
    stored.type = chime::modeFromStoredType(stored.type);
    gongx = stored;
  }
  prefs.end();
}
const char *outputError(GongType type, const PinMap &map) {
  if (type == GongType::Relay && map.relay < 0) return "Assign a relay GPIO before choosing the gong";
  if ((type == GongType::Pwm || type == GongType::Dac) && map.audio < 0) return "Assign an audio GPIO before choosing Audio";
  if (type == GongType::Dac && !chime::dacPin(map.audio)) return "DAC audio requires GPIO25 or GPIO26";
  return nullptr;
}
GongRequest gongRequest(uint8_t appVolume) {
  GongRequest request;
  request.tone = settings.gong; request.tone.volume = request.tone.volume * appVolume / 4;
  request.type = outputType(gongx.type, gongAudioOutput);
  request.pattern = gongx.pattern; request.pulseMs = gongx.pulseMs; request.gapMs = gongx.gapMs;
  return request;
}
void loadChimeApp() {
  Preferences prefs; if (!prefs.begin("chimeapp", true)) return;
  prefs.getBytes("name", chimeApp.name, sizeof(chimeApp.name) - 1);
  chimeApp.volume = prefs.getUChar("volume", chimeApp.volume);
  chimeApp.led = prefs.getUChar("led", chimeApp.led);
  prefs.end();
  if (chimeApp.volume > 4) chimeApp.volume = 4;
  chimeApp.led = chimeApp.led ? 1 : 0;
}
void saveChimeApp() {
  radio.pause(); // Flash writes stall the cache; keep the RX ISR out of it.
  Preferences prefs;
  if (prefs.begin("chimeapp", false)) {
    prefs.putBytes("name", chimeApp.name, sizeof(chimeApp.name) - 1);
    prefs.putUChar("volume", chimeApp.volume); prefs.putUChar("led", chimeApp.led);
    prefs.end();
  }
  restartRadio();
}

uint32_t muteRemainingMs() {
  if (!muteTimed) return 0;
  const int32_t left = int32_t(muteUntil - millis());
  return left > 0 ? uint32_t(left) : 0;
}
bool chimeMuted() { return muteForever || muteRemainingMs() > 0; }
void loadMute() {
  Preferences prefs; if (!prefs.begin("mute", true)) return;
  muteForever = prefs.getUChar("on", 0) == 1; prefs.end();
}
void serviceMute() {
  if (muteTimed && !muteRemainingMs()) { muteTimed = false; logf("[mute] timed mute ended\n"); }
}

chime::ChimeState currentChime() {
  chime::ChimeState state;
  state.type = gongx.type; state.audioOutput = gongAudioOutput; state.melody = settings.gong.melody; state.level = settings.gong.volume;
  state.volume = chimeApp.volume; state.pattern = gongx.pattern; state.led = chimeApp.led;
  state.extraRelay = settings.gong.relay; state.extraRelayMs = settings.gong.relayMs;
  state.pulseMs = gongx.pulseMs; state.gapMs = gongx.gapMs;
  memcpy(state.name, chimeApp.name, sizeof(state.name));
  state.mute = muteForever; state.muteMinutes = uint16_t((muteRemainingMs() + 59999) / 60000);
  return state;
}
const char *updateChime(JsonObjectConst update, const char *origin, const PinMap *validationPins) {
  if (otaStarted) return "Firmware update in progress";
  chime::ChimeState next = currentChime();
  if (const char *error = chime::applyChime(update, next)) return error;
  const PinMap &map=validationPins ? *validationPins : pins;
  // Optional pins may be disconnected deliberately. Name, LED, volume and
  // mute remain editable; selecting an output still requires usable wiring.
  const bool selectingOutput=update.containsKey("type") || update.containsKey("audio_output");
  if (selectingOutput) if (const char *error=outputError(outputType(next.type,next.audioOutput),map)) return error;
  if (next.type==uint8_t(chime::GongKind::Audio) && next.extraRelay && map.relay<0 &&
      (selectingOutput || update.containsKey("extra_relay"))) return "Assign a relay GPIO for the extra pulse";
  if (!storeChime(next)) return "Could not save the chime settings";
  logf("[%s] chime: type=%s audio_output=%s melody=%u level=%u volume=%u pattern=%u led=%u mute=%s name='%s'\n", origin,
    chime::GongKindNames[gongx.type], chime::AudioOutputNames[gongAudioOutput], settings.gong.melody, settings.gong.volume, chimeApp.volume,
    gongx.pattern, chimeApp.led, muteForever ? "on" : muteTimed ? (String(next.muteMinutes) + " min").c_str() : "off",
    chimeApp.name);
  return nullptr;
}

void ringJson(const chime::RingEvent &e, JsonObject o, bool labels) {
  o["id"] = e.id;
  o["source"] = chime::RingSourceNames[e.source]; o["outcome"] = chime::RingOutcomeNames[e.outcome];
  const bool relay = e.type == uint8_t(GongType::Relay);
  if (labels) {
    o["type"] = chime::GongKindNames[chime::modeFromStoredType(e.type) < chime::GongKinds ? chime::modeFromStoredType(e.type) : 0];
    if (e.type != uint8_t(GongType::None))
      o["sound"] = relay ? chime::PatternNames[e.sound % chime::Patterns] : chime::MelodyNames[e.sound % chime::Melodies];
    const String time = isoTime(e.epoch);
    if (time.length()) o["time"] = time; else o["time"] = nullptr;
  } else {
    o["type"] = chime::modeFromStoredType(e.type); o["sound"] = e.sound; o["ms"] = e.ms;
    if (e.epoch) o["epoch"] = e.epoch; else o["epoch"] = nullptr;
    o["previous_boot"] = bool(e.flags & chime::RingPreviousBoot);
  }
  if (e.type == uint8_t(GongType::Pwm) || e.type == uint8_t(GongType::Dac)) {
    const uint8_t output = chime::audioFromStoredType(e.type);
    if (labels) o["audio_output"] = chime::AudioOutputNames[output]; else o["audio_output"] = output;
  }
  o["volume"] = e.volume; o["audio_level"] = e.level; o["preview"] = bool(e.flags & chime::RingPreview);
  if (e.source == uint8_t(chime::RingSource::Doorbell)) { o["tone"] = e.tone; o["rssi_dbm"] = e.rssi; }
}
chime::RingOutcome playChime(chime::RingSource source, const GongRequest &request, uint8_t appVolume,
                             uint8_t flags, uint16_t tone, int8_t rssi, bool force) {
  chime::RingOutcome outcome;
  const bool mutable_ = source == chime::RingSource::Doorbell || source == chime::RingSource::Mqtt;
  if (mutable_ && !force && chimeMuted()) outcome = chime::RingOutcome::ChimeMuted;
  else if (request.type == GongType::None) outcome = chime::RingOutcome::Silent;
  else if (request.type == GongType::Relay && appVolume == 0) outcome = chime::RingOutcome::Muted;
  else if (!gongReady || !gong.play(request)) outcome = chime::RingOutcome::Busy;
  else outcome = appVolume ? chime::RingOutcome::Played : chime::RingOutcome::Muted;
  chime::RingEvent event;
  event.epoch = epochNow(); event.ms = millis(); event.tone = tone; event.rssi = rssi; event.flags = flags;
  event.source = uint8_t(source); event.outcome = uint8_t(outcome); event.type = uint8_t(request.type);
  event.sound = request.type == GongType::Relay ? request.pattern : request.tone.melody;
  event.volume = appVolume; event.level = request.tone.volume;
  const chime::RingEvent &stored = ringLog.add(event);
  ringLogDirty = true; ringLogChangedAt = millis();
  logf("[ring] #%u %s: %s\n", stored.id, chime::RingSourceNames[stored.source], chime::RingOutcomeNames[stored.outcome]);
  publishRing(stored);
  return outcome;
}
void loadRingLog() {
  static uint8_t image[chime::RingLog::ImageSize];
  Preferences prefs; if (!prefs.begin("ringlog", true)) return;
  const size_t length = prefs.getBytesLength("log");
  if (length && length <= sizeof(image) && prefs.getBytes("log", image, length) == length &&
      !ringLog.restore(image, length)) logf("Stored ring log ignored (invalid)\n");
  prefs.end();
}
void clearRingLog() {
  ringLog.clear(); ringLogDirty=true; ringLogChangedAt=millis()-10000; // Saved on the next quiet loop.
}
void serviceRingLog() {
  if (ringLogDirty && uint32_t(millis()-ringLogChangedAt)>3000 && uint32_t(millis()-lastPacketAt)>3000 &&
      !gong.active() && !otaStarted && !rfLink.busy()) saveRingLog();
}
