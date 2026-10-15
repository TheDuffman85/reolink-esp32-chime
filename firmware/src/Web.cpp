#include "Web.h"
#include <Update.h>
#include <WiFi.h>
#include "App.h"
#include "Band.h"
#include "ChimeService.h"
#include "Identity.h"
#include "LinkService.h"
#include "MqttService.h"
#include "Version.h"
#include "WebAuth.h"
#include "WebEncoding.h"
#include "WebUiGzip.h"
#include "SetupSound.h"
#include <cstddef>

WebServer server(80);

namespace {
bool otaAuthorized = false;
String otaError;
uint32_t otaStartedAt = 0;
bool setupVisible = true;
bool setupRequired = true;
constexpr uint32_t WebIdleMs = 60000;
bool webActive = false;
uint32_t webActivityAt = 0;
wifi_ps_type_t idleWifiSleep = WIFI_PS_MIN_MODEM;

// Validate types/ranges before changing any live or persisted setting.
bool integerField(JsonObjectConst obj, const char *name, int64_t min, int64_t max, int64_t &value) {
  if (!obj.containsKey(name)) return true;
  if (!obj[name].is<int64_t>()) return false;
  value = obj[name].as<int64_t>(); return value >= min && value <= max;
}
bool stringField(JsonObjectConst obj, const char *name, size_t max) {
  return !obj.containsKey(name) || (obj[name].is<const char *>() &&
    strlen(obj[name].as<const char *>()) <= max &&
    obj[name].as<JsonString>().size() == strlen(obj[name].as<const char *>()));
}
bool booleanField(JsonObjectConst obj, const char *name) {
  return !obj.containsKey(name) || obj[name].is<bool>();
}
bool updateSettings(JsonObjectConst obj, Settings &next) {
  int64_t hz = next.radio.hz, trim = next.radio.trim, polarity = uint8_t(next.radio.polarity);
  int64_t melody = next.gong.melody, volume = next.gong.volume, pulse = next.gong.relayMs;
  if (!integerField(obj,"frequency_hz",chime::Eu868Min,chime::Ism915Max,hz) ||
      !chime::validFrequency(uint32_t(hz)) ||
      !integerField(obj,"trim",-128,127,trim) || !integerField(obj,"polarity",0,2,polarity) ||
      !integerField(obj,"melody",0,chime::Melodies-1,melody) || !integerField(obj,"volume",0,100,volume) ||
      !integerField(obj,"relay_ms",50,2000,pulse)) return false;
  for (const char *key : {"relay", "open_wifi", "clear_profile"})
    if (!booleanField(obj,key)) return false;
  if (!stringField(obj,"ssid",32) || !stringField(obj,"wifi_password",63) ||
      !stringField(obj,"key",32) || !stringField(obj,"source",8) ||
      !stringField(obj,"destination",8) || !stringField(obj,"pair",4)) return false;
  if (next.radio.hz != hz || next.radio.trim != trim ||
      obj.containsKey("polarity")) next.learnedPolarity = 0;
  next.radio.hz = hz; next.radio.trim = trim;
  next.radio.polarity = static_cast<chime::Polarity>(polarity);
  next.gong.melody = melody; next.gong.volume = volume; next.gong.relayMs = pulse;
  if (obj.containsKey("relay")) next.gong.relay = obj["relay"].as<bool>();
  if (obj.containsKey("ssid")) strlcpy(next.ssid, obj["ssid"], sizeof(next.ssid));
  const String password = obj["wifi_password"] | "";
  if (obj["open_wifi"] == true) next.wifiPassword[0] = 0;
  else if (password.length()) {
    if (password.length() < 8) return false;
    password.toCharArray(next.wifiPassword, sizeof(next.wifiPassword));
  }
  if (obj["clear_profile"] == true) {
    next.profile = false; memset(next.key,0,sizeof(next.key));
    next.source = 0; next.destination = 0; next.pair = 0; next.paired = false;
  } else {
    uint8_t raw[4];
    const String source = obj["source"] | "", destination = obj["destination"] | "", pair = obj["pair"] | "";
    const auto number = [](const uint8_t *bytes, size_t count) {
      uint32_t value = 0; for (size_t i=0; i<count; ++i) value = (value<<8)|bytes[i]; return value;
    };
    if (source.length()) { if (!parseHex(source,raw,4)) return false; next.source=number(raw,4); }
    if (destination.length()) { if (!parseHex(destination,raw,4)) return false; next.destination=number(raw,4); }
    if (pair.length()) { if (!parseHex(pair,raw,2)) return false; next.pair=number(raw,2); }
    const String key = obj["key"] | "";
    if (key.length()) {
      if (!parseHex(key,next.key,16) || source.length()!=8 || destination.length()!=8 || pair.length()!=4) return false;
      next.profile = true; next.paired = false;
    }
    if (next.source != settings.source || next.destination != settings.destination ||
        next.pair != settings.pair) next.paired = false;
  }
  return validSettings(next);
}
}

void noteWebActivity() {
  webActivityAt = millis();
  if (webActive) return;
  idleWifiSleep = WiFi.getSleep();
  WiFi.setSleep(false);
  webActive = true;
}

void serviceWeb() {
  server.handleClient();
  // OTA owns the connection until completion/abort, even if it takes longer
  // than the activity window. Unsigned subtraction also handles millis wrap.
  if (webActive && !otaStarted && uint32_t(millis() - webActivityAt) >= WebIdleMs) {
    WiFi.setSleep(idleWifiSleep);
    webActive = false;
  }
}

void jsonReply(int status, const JsonDocument &doc) {
  String body; serializeJson(doc, body);
  server.sendHeader("Cache-Control", "no-store");
  server.send(status, "application/json", body);
}
void errorReply(int status, const char *message) {
  StaticJsonDocument<256> doc; doc["error"] = message; jsonReply(status, doc);
}
void messageReply(const char *message) {
  StaticJsonDocument<256> doc; doc["message"] = message; jsonReply(200, doc);
}

void registerWeb() {
  static bool registered = false;
  if (registered) return;
  registered = true;
  // Preserve first-run setup until Finish, even after pairing or a restart.
  // Migrate hidden wizards as complete; older paired installations stay hidden.
  Preferences uiPrefs;
  radio.pause();
  if (uiPrefs.begin("webui", false)) {
    setupVisible = uiPrefs.getBool("setup", !settings.paired);
    if (!uiPrefs.isKey("setup") && !uiPrefs.putBool("setup", setupVisible)) storageOk = false;
    setupRequired = !uiPrefs.getBool("init_done", !setupVisible);
    if (!uiPrefs.isKey("init_done") && !uiPrefs.putBool("init_done", !setupRequired)) storageOk = false;
    if (setupRequired) setupVisible = true;
    uiPrefs.end();
  } else storageOk = false;
  restartRadio();
  const char *headers[] = {"X-CSRF-Token", "Cookie", "Accept-Encoding", "X-WebUI-Active"}; server.collectHeaders(headers, 4);
  const auto page = [](const uint8_t *compressed, size_t size) {
    noteWebActivity(); // Wake before sending the large HTML response.
    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader("Vary", "Accept-Encoding");
    server.sendHeader("X-Content-Type-Options", "nosniff");
    server.sendHeader("X-Frame-Options", "DENY");
    if (!chime::acceptsGzip(server.header("Accept-Encoding").c_str())) {
      server.send(406,"text/plain","This page requires gzip support. Use a modern browser or curl --compressed.");
      return;
    }
    server.sendHeader("Content-Encoding", "gzip");
    server.send_P(200,"text/html",reinterpret_cast<const char *>(compressed),size);
  };
  const auto redirect = [](const char *location) {
    server.sendHeader("Location", location, true); server.send(302, "text/plain", "");
  };
  server.on("/", HTTP_GET, [page, redirect]() {
    if (!credentialsValid()) { redirect("/login"); return; }
    page(WebUiGzip, sizeof(WebUiGzip));
  });
  server.on("/login", HTTP_GET, [page, redirect]() {
    if (!auth.enabled || sessionValid()) { redirect("/"); return; }
    page(LoginUiGzip, sizeof(LoginUiGzip));
  });
  registerAuthRoutes();
  server.on("/api/setup", HTTP_POST, []() {
    if (!mutationAuthorized()) return;
    StaticJsonDocument<128> doc;
    if (deserializeJson(doc,server.arg("plain")) || !doc.is<JsonObject>() ||
        doc.size()!=1 || !doc["visible"].is<bool>()) {
      errorReply(400,"Expected {visible: true or false}"); return;
    }
    const bool visible = doc["visible"].as<bool>();
    if (visible != setupVisible || (!visible && setupRequired)) {
      if (otaStarted || restartAt || rfLink.busy()) { errorReply(409,"Wait for the update or radio handshake to finish"); return; }
      Preferences prefs;
      radio.pause();
      const bool opened = prefs.begin("webui", false);
      const bool saved = opened && prefs.putBool("setup", visible)==1 &&
        (visible || !setupRequired || prefs.putBool("init_done", true)==1);
      if (opened) prefs.end();
      restartRadio();
      if (!saved) { storageOk=false; errorReply(500,"Could not save Setup visibility"); return; }
      setupVisible=visible;
      if (!visible) setupRequired=false;
    }
    doc["visible"]=setupVisible; jsonReply(200,doc);
  });
  server.on("/api/status", HTTP_GET, []() {
    if (!authenticated()) return;
    StaticJsonDocument<3072> doc;
    doc["csrf"]=csrf; doc["uptime_ms"]=millis(); doc["wifi_connected"]=WiFi.status()==WL_CONNECTED;
    doc["ssid"]=WiFi.SSID(); doc["ip"]=WiFi.status()==WL_CONNECTED?WiFi.localIP().toString():WiFi.softAPIP().toString();
    doc["ap_active"]=apActive; doc["ap_name"]=apName; doc["wifi_mode"]=chime::WifiModeNames[uint8_t(wifiMode)];
    doc["radio_ok"]=radio.healthy; doc["radio_error"]=radio.lastError();
    doc["radio_part"]=radio.part; doc["radio_version"]=radio.version; doc["marcstate"]=radio.state();
    doc["rf_frequency_hz"]=int64_t(settings.radio.hz)+int64_t(sweepTrim)*26000000/16384;
    doc["rf_trim"]=sweepTrim; doc["rssi_dbm"]=radio.rssi();
    doc["auto_trim"]=radioOptions.autoTrim; doc["trim_calibrated"]=radioOptions.calibrated && !afcAcquiring;
    doc["rx_bandwidth_khz"]=radio.wide()?162.5:81.25; doc["rx_mode"]=uint8_t(radio.mode());
    doc["tx_polarity"]=txPolarityKnown?(txInverted?"inverted (locked)":"normal (locked)"):
      sweepCombo!=UINT32_MAX?(lastTxInverted?"inverted":"normal"):"normal";
    if (sweepCombo!=UINT32_MAX) doc["sweep_step"]=sweepCombo+1; else doc["sweep_step"]=nullptr;
    doc["sweep_steps"]=22;
    doc["pairing"]=rfLink.state(); doc["profile"]=settings.profile;
    doc["paired"]=settings.paired; doc["joined"]=rfLink.profileReady();
    doc["setup_visible"]=setupVisible;
    doc["setup_required"]=setupRequired;
    doc["node_id"]=hex32(settings.nodeId);
    doc["identity_compatible"]=chime::compatibleNodeId(settings.nodeId);
    doc["transmitted"]=transmitted; doc["transmit_errors"]=transmitErrors;
    doc["packets"]=packetCount; doc["crc_ti"]=crcTi; doc["crc_bad"]=crcBad; doc["crc_ccitt"]=crcCcitt;
    doc["dropped"]=Radio::dropped; doc["invalid_lengths"]=Radio::invalidLengths+decoder.invalidLengths;
    doc["false_syncs"]=Radio::falseSyncs;
    doc["rings"]=rings; if (rings) doc["last_ring_ms"]=lastRing; else doc["last_ring_ms"]=nullptr;
    doc["gong_active"]=gong.active(); doc["gong_ready"]=gongReady;
    doc["storage_ok"]=storageOk; doc["learned_polarity"]=settings.learnedPolarity;
    doc["wifi_configured"]=strlen(settings.ssid)>0; doc["wifi_rssi"]=WiFi.status()==WL_CONNECTED?WiFi.RSSI():0;
    doc["hostname"]="reolink-chime.local"; doc["chime_name"]=chimeApp.name;
    doc["doorbell_id"]=hex32(settings.source); doc["free_heap"]=ESP.getFreeHeap();
    doc["version"]=FIRMWARE_VERSION; doc["firmware"]=__DATE__ " " __TIME__; doc["radio_resets"]=radioResets;
    doc["ring_seq"]=ringLog.lastId(); doc["auth_enabled"]=auth.enabled; doc["auth_user"]=auth.enabled?auth.user:"";
    JsonObject mute=doc.createNestedObject("mute");
    mute["active"]=chimeMuted(); mute["permanent"]=muteForever; mute["remaining_s"]=(muteRemainingMs()+999)/1000;
    const uint32_t epoch=epochNow(); doc["time_synced"]=epoch!=0; if (epoch) doc["epoch"]=epoch; else doc["epoch"]=nullptr;
    doc["mqtt"]=integrations.mqtt?mqtt.state():String("off");
    jsonReply(200,doc);
  });
  server.on("/api/settings", HTTP_GET, []() {
    if (!authenticated()) return;
    StaticJsonDocument<1024> doc;
    doc["frequency_hz"]=settings.radio.hz; doc["trim"]=settings.radio.trim; doc["auto_trim"]=radioOptions.autoTrim;
    doc["rx_mode"]=uint8_t(radioOptions.rxMode);
    doc["polarity"]=uint8_t(settings.radio.polarity); doc["melody"]=settings.gong.melody;
    doc["volume"]=settings.gong.volume; doc["relay"]=settings.gong.relay; doc["relay_ms"]=settings.gong.relayMs;
    doc["ssid"]=settings.ssid; doc["wifi_password_set"]=strlen(settings.wifiPassword)>0;
    doc["profile"]=settings.profile; doc["source"]=hex32(settings.source);
    doc["paired"]=settings.paired; doc["node_id"]=hex32(settings.nodeId);
    doc["destination"]=hex32(settings.destination); doc["pair"]=hex16(settings.pair);
    jsonReply(200,doc); // Secrets are never returned.
  });
  server.on("/api/settings", HTTP_POST, []() {
    if (!mutationAuthorized()) return;
    if (server.arg("plain").length()>2048) { errorReply(413,"Settings body too large"); return; }
    DynamicJsonDocument doc(4096);
    if (deserializeJson(doc,server.arg("plain")) || !doc.is<JsonObject>()) {
      errorReply(400,"Expected a JSON object"); return;
    }
    // Forgetting the doorbell ends any pairing or rejoin attempt with it.
    if (rfLink.busy() && doc["clear_profile"]==true) { rfLink.cancel(millis()); logf("[link] cancelled to forget doorbell\n"); }
    if (rfLink.busy()) { errorReply(409,"Cancel pairing/join before changing settings"); return; }
    Settings next = settings;
    if (!updateSettings(doc.as<JsonObjectConst>(),next) || !booleanField(doc.as<JsonObjectConst>(),"auto_trim") ||
        (doc.containsKey("rx_mode") && !(doc["rx_mode"].is<int>() && (doc["rx_mode"]==0 || doc["rx_mode"]==1)))) {
      errorReply(400,"Invalid settings, hex IDs/key, or WiFi password"); return;
    }
    // Switching automatic trim on, or a new frequency, starts a fresh
    // measurement with the wide acquisition bandwidth.
    RadioOptions nextOptions = radioOptions;
    if (doc.containsKey("auto_trim")) nextOptions.autoTrim = doc["auto_trim"].as<bool>();
    if (doc.containsKey("rx_mode")) nextOptions.rxMode = doc["rx_mode"]==1 ? RxMode::Esp32 : RxMode::PacketEngine;
    if ((nextOptions.autoTrim && !radioOptions.autoTrim) || next.radio.hz != settings.radio.hz) nextOptions.calibrated = false;
    radio.pause(); storageOk=saveSettings(next);
    if (storageOk && (nextOptions.autoTrim != radioOptions.autoTrim || nextOptions.calibrated != radioOptions.calibrated ||
        nextOptions.rxMode != radioOptions.rxMode))
      storageOk=saveRadioOptions(nextOptions);
    if (!storageOk) { restartRadio(); errorReply(500,"NVS write failed; settings were not applied"); return; }
    radioOptions=nextOptions; resetAfc();
    const bool linkChanged = next.paired != settings.paired || next.source != settings.source ||
      next.destination != settings.destination || next.pair != settings.pair ||
      memcmp(next.key, settings.key, sizeof(next.key)) != 0 ||
      next.radio.hz != settings.radio.hz || next.radio.trim != settings.radio.trim;
    settings=next; learnPending=false; duplicates.reset(); restartRadio();
    if (linkChanged) initLink(true);
    messageReply("Settings saved; reboot for WiFi changes");
  });
  server.on("/api/chime", HTTP_GET, []() {
    if (!authenticated()) return;
    StaticJsonDocument<512> doc;
    doc["name"]=chimeApp.name; doc["volume"]=chimeApp.volume; doc["led"]=chimeApp.led;
    doc["joined"]=rfLink.profileReady(); doc["doorbell_id"]=hex32(settings.source);
    doc["node_id"]=hex32(settings.nodeId); doc["session"]=hex16(rfLink.profile().pair);
    doc["requests"]=appRequests; doc["last_opcode"]=lastAppOpcode;
    if (appRequests) doc["last_request_ms"]=lastAppAt; else doc["last_request_ms"]=nullptr;
    doc["uptime_ms"]=millis();
    jsonReply(200,doc);
  });
  server.on("/api/chime", HTTP_POST, []() {
    if (!mutationAuthorized()) return;
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc,server.arg("plain")) || !doc.is<JsonObject>()) { errorReply(400,"Expected a JSON object"); return; }
    for (JsonPairConst pair : doc.as<JsonObjectConst>())
      if (strcmp(pair.key().c_str(),"name") && strcmp(pair.key().c_str(),"volume") && strcmp(pair.key().c_str(),"led")) {
        errorReply(400,"Expected name, volume or led"); return;
      }
    if (const char *error=updateChime(doc.as<JsonObjectConst>(),"web")) { errorReply(400,error); return; }
    messageReply("Chime settings saved; the doorbell reads them at its next config request");
  });
  server.on("/api/log", HTTP_GET, []() {
    if (!authenticated()) return;
    // ?from=<byte offset> returns only newer text; X-Log-Next is the next offset.
    uint32_t from = server.hasArg("from") ? strtoul(server.arg("from").c_str(), nullptr, 10) : 0;
    const uint32_t end = logTotal;
    if (from > end) from = end;
    if (end - from > LogCapacity) from = end - LogCapacity;
    String body; body.reserve(end - from);
    for (uint32_t i = from; i < end; ++i) body += logRing[i % LogCapacity];
    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader("X-Log-Next", String(end));
    server.send(200, "text/plain", body);
  });
  server.on("/api/packets", HTTP_GET, []() {
    if (!authenticated()) return;
    DynamicJsonDocument doc(20000); JsonArray array=doc.createNestedArray("packets");
    for (size_t i=0;i<packetLogSize;++i) {
      const auto &entry=packetLog[(packetLogNext+PacketLogSize-packetLogSize+i)%PacketLogSize];
      JsonObject obj=array.createNestedObject();
      obj["id"]=entry.id; obj["ms"]=entry.ms; obj["rssi_dbm"]=entry.rssi; obj["inverted"]=entry.inverted;
      if (entry.offsetValid) obj["offset_khz"]=roundf(entry.offset*26000.0f/16384.0f*10)/10;
      else obj["offset_khz"]=nullptr; // Esp32 receive mode cannot measure it.
      if (entry.epoch) { obj["epoch"]=entry.epoch; obj["epoch_ms"]=entry.epochMs; } else obj["epoch"]=nullptr;
      obj["crc"]=entry.crc==chime::Crc::Ti8005?"TI-8005 candidate":
        entry.crc==chime::Crc::CcittDiagnostic?"CCITT diagnostic only":"bad";
      obj["hex"]=entry.hex; obj["result"]=entry.result;
    }
    if (doc.overflowed()) { errorReply(503,"Packet log allocation failed"); return; }
    jsonReply(200,doc);
  });
  server.on("/api/gong/test", HTTP_POST, []() {
    if (!mutationAuthorized()) return;
    // "sound" (0-9 or a name) requests a slot like the doorbell's music ID.
    // Other overrides preview a sound without saving it: {"type":0-2,"audio_output":0-1,
    // "melody":0-9,"volume":0-100,"pattern":0-9,"pulse_ms":50-2000,"gap_ms":100-3000}.
    uint8_t flags = 0, mode = gongx.type, output = gongAudioOutput;
    StaticJsonDocument<256> doc;
    const auto in = [&](const char *key, int lo, int hi) {
      return doc[key].is<int>() && doc[key].as<int>() >= lo && doc[key].as<int>() <= hi;
    };
    const bool options = !deserializeJson(doc,server.arg("plain")) && doc.is<JsonObject>();
    int slot = 0;
    if (options && doc.containsKey("sound") && (slot = chime::soundSlot(doc["sound"])) < 0) {
      errorReply(400,"sound: 0 to 9 or a melody or pattern name"); return;
    }
    GongRequest request = gongRequest(4, uint8_t(slot));
    if (options && doc.size() > (doc.containsKey("sound") ? 1u : 0u)) {
      flags = chime::RingPreview;
      if (in("type",0,chime::GongKinds-1)) mode=doc["type"].as<int>();
      if (in("audio_output",0,chime::AudioOutputs-1)) output=doc["audio_output"].as<int>();
      if (in("melody",0,chime::Melodies-1)) request.tone.melody=doc["melody"].as<int>();
      if (in("volume",0,100)) request.tone.volume=doc["volume"].as<int>();
      if (doc["extra_relay"].is<bool>()) request.tone.relay=doc["extra_relay"].as<bool>();
      if (in("extra_relay_ms",50,2000)) request.tone.relayMs=doc["extra_relay_ms"].as<int>();
      if (in("pattern",0,RelayPatterns-1)) request.pattern=doc["pattern"].as<int>();
      if (in("pulse_ms",50,2000)) request.pulseMs=doc["pulse_ms"].as<int>();
      if (in("gap_ms",100,3000)) request.gapMs=doc["gap_ms"].as<int>();
    }
    request.type = outputType(mode, output);
    if (const char *error = outputError(request.type, pins)) { errorReply(409,error); return; }
    if ((request.type==GongType::Pwm || request.type==GongType::Dac) && request.tone.relay && pins.relay<0) { errorReply(409,"Assign a relay GPIO for the extra pulse"); return; }
    const auto outcome = playChime(chime::RingSource::Web, request, 4, flags);
    if (outcome == chime::RingOutcome::Busy) { errorReply(503,"Gong busy/unavailable"); return; }
    messageReply(outcome == chime::RingOutcome::Silent ? "Gong type is None: logged without sound" : "Playing test sound");
  });
  server.on("/api/gong", HTTP_GET, []() {
    if (!authenticated()) return;
    StaticJsonDocument<256> doc;
    doc["type"]=gongx.type; doc["audio_output"]=gongAudioOutput; doc["pattern"]=gongx.pattern; doc["pulse_ms"]=gongx.pulseMs; doc["gap_ms"]=gongx.gapMs;
    doc["override_ringtone"]=overrideRingtone;
    doc["relay_available"]=pins.relay>=0; doc["pwm_available"]=pins.audio>=0;
    doc["dac_available"]=pins.audio==25 || pins.audio==26;
    jsonReply(200,doc);
  });
  server.on("/api/gong", HTTP_POST, []() {
    if (!mutationAuthorized()) return;
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc,server.arg("plain")) || !doc.is<JsonObject>()) { errorReply(400,"Expected a JSON object"); return; }
    for (JsonPairConst pair : doc.as<JsonObjectConst>()) {
      const char *key=pair.key().c_str();
      if (!strcmp(key,"override_ringtone") ? !pair.value().is<bool>() :
          (strcmp(key,"type") && strcmp(key,"audio_output") && strcmp(key,"pattern") && strcmp(key,"pulse_ms") && strcmp(key,"gap_ms")) ||
          !pair.value().is<int>()) { errorReply(400,"Invalid gong settings"); return; }
    }
    chime::ChimeState preview = currentChime();
    if (const char *error = chime::applyChime(doc.as<JsonObjectConst>(), preview)) { errorReply(400,error); return; }
    if (const char *error = outputError(outputType(preview.type, preview.audioOutput), pins)) { errorReply(409,error); return; }
    if (const char *error=updateChime(doc.as<JsonObjectConst>(),"web")) { errorReply(400,error); return; }
    messageReply("Gong settings saved");
  });
  // Validate the selected sound together with its planned output GPIOs. Pins
  // apply only on Finish; playback is inhibited during the live change.
  const auto setupSound = [](bool save) {
    if (!mutationAuthorized()) return;
    if (otaStarted || restartAt || (save && rfLink.busy())) { errorReply(409,"Wait for the update or radio handshake to finish"); return; }
    StaticJsonDocument<768> doc;
    if (deserializeJson(doc,server.arg("plain")) || !doc.is<JsonObject>()) { errorReply(400,"Expected a JSON object"); return; }
    PinMap savedPins; loadPins(savedPins);
    chime::SetupOutputPins outputs; outputs.audio=savedPins.audio; outputs.relay=savedPins.relay;
    outputs.relayActiveLow=savedPins.relayActiveLow;
    chime::ChimeState next = currentChime();
    if (const char *error=chime::applySetupSound(doc.as<JsonObjectConst>(),next,outputs)) { errorReply(400,error); return; }
    PinMap proposed=savedPins; proposed.audio=outputs.audio; proposed.relay=outputs.relay;
    proposed.relayActiveLow=outputs.relayActiveLow;
    DynamicJsonDocument reply(3072); JsonArray issues=reply.createNestedArray("issues");
    bool ok=checkPins(proposed,[](void *ctx,const char *role,PinIssue level,const char *text) {
      JsonObject i=static_cast<JsonArray *>(ctx)->createNestedObject();
      i["role"]=role; i["level"]=level==PinIssue::Error?"error":level==PinIssue::Warning?"warning":"info"; i["text"]=text;
    },&issues);
    const char *outputIssue=outputError(outputType(next.type,next.audioOutput),proposed);
    if (!outputIssue && next.type==0 && next.extraRelay && proposed.relay<0) outputIssue="Assign a relay GPIO for the extra pulse";
    if (outputIssue) {
      JsonObject i=issues.createNestedObject(); i["role"]=next.type==1 || (next.extraRelay && proposed.relay<0)?"relay":"audio";
      i["level"]="error"; i["text"]=outputIssue; ok=false;
    }
    const bool changed=memcmp(&proposed,&pins,offsetof(PinMap,relayActiveLow)) || proposed.relayActiveLow!=pins.relayActiveLow;
    reply["ok"]=ok; reply["pins_changed"]=changed; reply["restart_required"]=false;
    if (!save) { jsonReply(200,reply); return; }
    if (!ok) { reply["error"]="Fix the wiring errors first"; jsonReply(400,reply); return; }
    if (changed) {
      gong.inhibit(true); radio.pause(); const bool stored=savePins(proposed); restartRadio();
      if (!stored) { gong.inhibit(false); errorReply(500,"Could not save output wiring"); return; }
    }
    if (const char *error=updateChime(doc["chime"].as<JsonObjectConst>(),"setup",&proposed)) {
      if (changed) { radio.pause(); savePins(savedPins); restartRadio(); gong.inhibit(false); }
      errorReply(500,error); return;
    }
    if (changed) applyPinsLive(proposed);
    reply["message"]="Sound and wiring saved and applied";
    jsonReply(200,reply);
  };
  server.on("/api/setup/sound/check",HTTP_POST,[setupSound]() { setupSound(false); });
  server.on("/api/setup/sound",HTTP_POST,[setupSound]() { setupSound(true); });
  server.on("/api/pins", HTTP_GET, []() {
    if (!authenticated()) return;
    StaticJsonDocument<1024> doc; const PinMap defaults;
    const auto put = [](JsonObject o, const PinMap &m) {
      o["sck"]=m.sck; o["miso"]=m.miso; o["mosi"]=m.mosi; o["cs"]=m.cs; o["clock"]=m.clock; o["data"]=m.data;
      o["audio"]=m.audio; o["relay"]=m.relay; o["led"]=m.led; o["button"]=m.button;
      o["relay_active_low"]=m.relayActiveLow;
    };
    put(doc.createNestedObject("pins"),pins); put(doc.createNestedObject("defaults"),defaults);
    PinMap stored; loadPins(stored); put(doc.createNestedObject("saved"),stored);
    jsonReply(200,doc);
  });
  // Validates (and with /api/pins saves) a GPIO map; issues explain each finding.
  const auto pinRequest = [](bool save) {
    if (!mutationAuthorized()) return;
    if (save && (otaStarted || restartAt || rfLink.busy())) { errorReply(409,"Wait for the update or radio handshake to finish"); return; }
    StaticJsonDocument<512> doc;
    if (deserializeJson(doc,server.arg("plain")) || !doc.is<JsonObject>()) { errorReply(400,"Expected a JSON object"); return; }
    PinMap next=pins;
    struct { const char *key; int8_t PinMap::*pin; } keys[] = {{"sck",&PinMap::sck},{"miso",&PinMap::miso},
      {"mosi",&PinMap::mosi},{"cs",&PinMap::cs},{"clock",&PinMap::clock},{"data",&PinMap::data},
      {"audio",&PinMap::audio},{"relay",&PinMap::relay},{"led",&PinMap::led},{"button",&PinMap::button}};
    for (const auto &k : keys) if (doc.containsKey(k.key)) {
      if (!doc[k.key].is<int>() || doc[k.key].as<int>() < -1 || doc[k.key].as<int>() > 39) { errorReply(400,"GPIO numbers are -1 to 39"); return; }
      next.*(k.pin)=doc[k.key].as<int>();
    }
    if (doc.containsKey("relay_active_low")) {
      if (!doc["relay_active_low"].is<bool>()) { errorReply(400,"relay_active_low must be true or false"); return; }
      next.relayActiveLow=doc["relay_active_low"].as<bool>();
    }
    DynamicJsonDocument reply(3072); JsonArray issues=reply.createNestedArray("issues");
    bool ok=checkPins(next,[](void *ctx, const char *role, PinIssue level, const char *text) {
      JsonObject i=static_cast<JsonArray *>(ctx)->createNestedObject();
      i["role"]=role; i["level"]=level==PinIssue::Error?"error":level==PinIssue::Warning?"warning":"info"; i["text"]=text;
    },&issues);
    if (const char *error = outputError(outputType(gongx.type, gongAudioOutput), next)) {
      JsonObject i=issues.createNestedObject(); i["role"]=gongx.type==1 ? "relay" : "audio";
      const bool invalidDac = gongx.type==0 && gongAudioOutput==1 && next.audio>=0;
      i["level"]=invalidDac ? "error" : "warning";
      if (invalidDac) ok=false;
      i["text"]=error;
    }
    reply["ok"]=ok;
    if (save) {
      if (!ok) { reply["error"]="Fix the GPIO errors first"; jsonReply(400,reply); return; }
      radio.pause(); const bool saved=savePins(next); restartRadio();
      if (!saved) { errorReply(500,"Could not save GPIO settings"); return; }
      applyPinsLive(next);
      logf("GPIO map saved and applied\n");
      reply["message"]="GPIO settings saved and applied.";
    }
    jsonReply(200,reply);
  };
  server.on("/api/pins/check", HTTP_POST, [pinRequest]() { pinRequest(false); });
  server.on("/api/pins", HTTP_POST, [pinRequest]() { pinRequest(true); });
  server.on("/api/pins/reset", HTTP_POST, []() {
    if (!mutationAuthorized()) return;
    if (otaStarted || restartAt || rfLink.busy()) { errorReply(409,"Wait for the update or radio handshake to finish"); return; }
    radio.pause(); const bool saved=savePins(PinMap{}); restartRadio();
    if (!saved) { errorReply(500,"Could not save GPIO settings"); return; }
    applyPinsLive(PinMap{});
    messageReply("Default GPIO settings restored and applied.");
  });
  server.on("/api/wifi/scan", HTTP_GET, []() {
    if (!authenticated()) return;
    const int16_t found = WiFi.scanComplete();
    DynamicJsonDocument doc(4096);
    if (found == WIFI_SCAN_RUNNING) { doc["scanning"]=true; jsonReply(200,doc); return; }
    if (found < 0 || server.hasArg("refresh")) {
      WiFi.scanDelete(); WiFi.scanNetworks(true); doc["scanning"]=true; jsonReply(200,doc); return;
    }
    doc["scanning"]=false; JsonArray list=doc.createNestedArray("networks");
    for (int i=0;i<found && list.size()<24;++i) {
      const String ssid=WiFi.SSID(i); if (!ssid.length()) continue;
      bool duplicate=false; for (JsonObject n : list) if (ssid==n["ssid"].as<const char *>()) { duplicate=true; break; }
      if (duplicate) continue; // Strongest first: results are sorted by RSSI.
      JsonObject n=list.createNestedObject(); n["ssid"]=ssid; n["rssi"]=WiFi.RSSI(i);
      n["secure"]=WiFi.encryptionType(i)!=WIFI_AUTH_OPEN;
    }
    jsonReply(200,doc);
  });
  server.on("/api/wifi", HTTP_GET, []() {
    if (!authenticated()) return;
    StaticJsonDocument<384> doc;
    doc["mode"]=chime::WifiModeNames[uint8_t(wifiConfig.mode)]; doc["active_mode"]=chime::WifiModeNames[uint8_t(wifiMode)];
    doc["ap_ssid"]=wifiConfig.apSsid; doc["ap_password_set"]=wifiConfig.apPassword[0]!=0;
    doc["default_ap_ssid"]=defaultApName(); doc["button"]=pins.button>=0;
    jsonReply(200,doc); // The hotspot password is never returned.
  });
  // {mode, ap_ssid, ap_password, clear_ap_password}; applies after a restart.
  server.on("/api/wifi", HTTP_POST, []() {
    if (!mutationAuthorized()) return;
    StaticJsonDocument<384> doc;
    if (deserializeJson(doc,server.arg("plain")) || !doc.is<JsonObject>()) { errorReply(400,"Expected a JSON object"); return; }
    chime::WifiConfig next=wifiConfig;
    if (const char *error=chime::applyWifi(doc.as<JsonObjectConst>(),next)) { errorReply(400,error); return; }
    radio.pause(); const bool ok=saveWifiConfig(next); restartRadio();
    if (!ok) { errorReply(500,"Could not save the WiFi settings"); return; }
    wifiConfig=next;
    logf("WiFi mode '%s' saved; applies after a restart\n", chime::WifiModeNames[uint8_t(next.mode)]);
    messageReply(next.mode==wifiMode?"Saved. Restart the chime to apply the hotspot settings.":"Saved. Restart the chime to switch the WiFi mode.");
  });
  server.on("/api/wifi/connect", HTTP_POST, []() {
    if (!mutationAuthorized()) return;
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc,server.arg("plain")) || !doc["ssid"].is<const char *>()) { errorReply(400,"Expected {ssid, password}"); return; }
    const char *ssid=doc["ssid"]; const char *password=doc["password"] | "";
    if (!strlen(ssid) || strlen(ssid)>32 || strlen(password)>63 || (strlen(password) && strlen(password)<8)) {
      errorReply(400,"Network name 1-32 characters; password empty or 8-63 characters"); return;
    }
    Settings next=settings; strlcpy(next.ssid,ssid,sizeof(next.ssid)); strlcpy(next.wifiPassword,password,sizeof(next.wifiPassword));
    // Joining a network switches an access point to client mode.
    chime::WifiConfig nextWifi=wifiConfig; nextWifi.mode=chime::WifiMode::Client;
    radio.pause(); storageOk=saveSettings(next);
    const bool wifiSaved=storageOk && (nextWifi.mode==wifiConfig.mode || saveWifiConfig(nextWifi));
    restartRadio();
    if (!storageOk || !wifiSaved) { errorReply(500,"Could not save WiFi settings"); return; }
    settings=next; wifiConfig=nextWifi;
    if (wifiMode!=chime::WifiMode::Client) { wifiMode=chime::WifiMode::Client; WiFi.mode(WIFI_AP_STA); WiFi.setAutoReconnect(true); }
    // Keep the setup AP up so the browser stays connected while joining.
    if (!apActive) startAp();
    WiFi.disconnect(false,false); WiFi.begin(settings.ssid,settings.wifiPassword); stationStarted=millis();
    logf("WiFi: connecting to %s\n", settings.ssid);
    messageReply("Connecting");
  });
  server.on("/api/pair/start", HTTP_POST, []() {
    if (!mutationAuthorized()) return;
    if (!storageOk) { errorReply(503,"Settings storage unavailable; pairing needs persistent credentials"); return; }
    if (!startPairing()) { errorReply(409,"Pairing busy or radio unavailable"); return; }
    messageReply("Pairing started for 5 minutes");
  });
  server.on("/api/pair/cancel", HTTP_POST, []() {
    if (!mutationAuthorized()) return;
    rfLink.cancel(millis()); messageReply("Pairing/join attempt cancelled");
  });
  server.on("/api/join/start", HTTP_POST, []() {
    if (!mutationAuthorized()) return;
    if (!radio.healthy || otaStarted || restartAt || !rfLink.startJoin(millis())) {
      errorReply(409,"No saved pairing, link busy, or radio unavailable"); return;
    }
    duplicates.reset(); messageReply("Rejoin started");
  });
  server.on("/api/reboot", HTTP_POST, []() {
    if (!mutationAuthorized()) return;
    messageReply("Rebooting"); gong.inhibit(true); restartAt=millis()+500;
  });
  server.on("/api/update", HTTP_POST, []() {
    if (!otaAuthorized) {
      if (!mutationAuthorized()) return;
      errorReply(400,"Expected multipart firmware upload"); return;
    }
    otaAuthorized=false;
    noteWebActivity();
    if (!otaStarted || otaError.length() || Update.hasError()) {
      errorReply(400,otaError.length()?otaError.c_str():"Firmware update failed");
      otaStarted=false; gong.inhibit(false); restartRadio(); initLink(true); return;
    }
    messageReply("Firmware updated; rebooting"); restartAt=millis()+500;
  }, []() {
    HTTPUpload &upload=server.upload();
    if (upload.status==UPLOAD_FILE_START) {
      otaAuthorized=credentialsValid() && server.header("X-CSRF-Token")==csrf;
      otaStarted=false; otaError="";
      if (!otaAuthorized) return;
      noteWebActivity();
      rfLink.cancel(millis()); gong.inhibit(true); radio.pause();
      if (!upload.filename.endsWith(".bin")) { otaError="Upload a firmware .bin file"; return; }
      otaStarted=Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH); otaStartedAt=millis();
      if (!otaStarted) otaError=String("Could not open OTA partition: ")+Update.errorString();
    } else if (upload.status==UPLOAD_FILE_WRITE && otaAuthorized && otaStarted && !otaError.length()) {
      if (Update.write(upload.buf,upload.currentSize)!=upload.currentSize) otaError=String("Firmware write failed: ")+Update.errorString();
    } else if (upload.status==UPLOAD_FILE_END && otaAuthorized && otaStarted) {
      if (otaError.length()) Update.abort();
      else if (!Update.end(true)) otaError=String("Update library rejected the firmware image: ")+Update.errorString();
      logf("OTA: %u bytes in %.1f s%s%s\n", unsigned(upload.totalSize), (millis()-otaStartedAt)/1000.0,
        otaError.length()?", ":"", otaError.c_str());
    } else if (upload.status==UPLOAD_FILE_ABORTED && otaAuthorized) {
      Update.abort(); otaAuthorized=false; otaStarted=false; otaError="Upload aborted";
      noteWebActivity(); gong.inhibit(false); restartRadio(); initLink(true);
    }
  });
  // {"mute":true|false} or {"mute_minutes":1-1440 (0 ends a timed mute)}.
  server.on("/api/mute", HTTP_GET, []() {
    if (!authenticated()) return;
    StaticJsonDocument<128> doc;
    doc["active"]=chimeMuted(); doc["permanent"]=muteForever; doc["remaining_s"]=(muteRemainingMs()+999)/1000;
    jsonReply(200,doc);
  });
  server.on("/api/mute", HTTP_POST, []() {
    if (!mutationAuthorized()) return;
    StaticJsonDocument<128> doc;
    if (deserializeJson(doc,server.arg("plain")) || !doc.is<JsonObject>() || !doc.size()) { errorReply(400,"Expected {mute} or {mute_minutes}"); return; }
    for (JsonPairConst pair : doc.as<JsonObjectConst>())
      if (strcmp(pair.key().c_str(),"mute") && strcmp(pair.key().c_str(),"mute_minutes")) { errorReply(400,"Expected mute or mute_minutes"); return; }
    if (const char *error=updateChime(doc.as<JsonObjectConst>(),"web")) { errorReply(400,error); return; }
    messageReply(muteForever?"Chime muted until you unmute it":muteTimed?"Chime muted":"Chime unmuted");
  });
  server.on("/api/rings", HTTP_GET, []() {
    if (!authenticated()) return;
    DynamicJsonDocument doc(12288);
    doc["uptime_ms"]=millis(); const uint32_t epoch=epochNow(); if (epoch) doc["epoch"]=epoch; else doc["epoch"]=nullptr;
    doc["last_id"]=ringLog.lastId(); doc["rings_since_start"]=rings;
    JsonArray list=doc.createNestedArray("rings");
    for (size_t i=0;i<ringLog.size();++i) ringJson(ringLog.newest(i),list.createNestedObject(),false);
    if (doc.overflowed()) { errorReply(503,"Ring log allocation failed"); return; }
    jsonReply(200,doc);
  });
  server.on("/api/rings/clear", HTTP_POST, []() {
    if (!mutationAuthorized()) return;
    clearRingLog();
    messageReply("Ring log cleared");
  });
  server.on("/api/integrations", HTTP_GET, []() {
    if (!authenticated()) return;
    StaticJsonDocument<768> doc;
    doc["enabled"]=integrations.mqtt; doc["host"]=integrations.host; doc["port"]=integrations.port;
    doc["username"]=integrations.user; doc["password_set"]=integrations.password[0]!=0;
    doc["topic"]=integrations.topic; doc["default_topic"]=defaultMqttBase();
    doc["discovery"]=integrations.discovery; doc["prefix"]=integrations.prefix;
    doc["state"]=integrations.mqtt?mqtt.state():String("off"); doc["dropped"]=mqtt.dropped();
    doc["ntp"]=integrations.ntp; const uint32_t epoch=epochNow();
    doc["time_synced"]=epoch!=0; if (epoch) doc["epoch"]=epoch; else doc["epoch"]=nullptr;
    jsonReply(200,doc); // The broker password is never returned.
  });
  server.on("/api/integrations", HTTP_POST, []() {
    if (!mutationAuthorized()) return;
    StaticJsonDocument<768> doc;
    if (deserializeJson(doc,server.arg("plain")) || !doc.is<JsonObject>()) { errorReply(400,"Expected a JSON object"); return; }
    JsonObjectConst in=doc.as<JsonObjectConst>();
    IntegrationSettings next=integrations; int64_t port=next.port;
    for (const char *key : {"enabled","discovery","clear_password"}) if (!booleanField(in,key)) { errorReply(400,"Expected true or false"); return; }
    if (!integerField(in,"port",1,65535,port) || !stringField(in,"host",64) || !stringField(in,"username",64) ||
        !stringField(in,"password",64) || !stringField(in,"topic",64) || !stringField(in,"prefix",32) ||
        !stringField(in,"ntp",64)) { errorReply(400,"Invalid value or text too long"); return; }
    if (in.containsKey("enabled")) next.mqtt=in["enabled"];
    if (in.containsKey("discovery")) next.discovery=in["discovery"];
    next.port=port;
    if (in.containsKey("host")) strlcpy(next.host,in["host"],sizeof(next.host));
    if (in.containsKey("username")) strlcpy(next.user,in["username"],sizeof(next.user));
    if (in["clear_password"]==true) next.password[0]=0;
    else if (strlen(in["password"] | "")) strlcpy(next.password,in["password"],sizeof(next.password));
    if (in.containsKey("topic")) strlcpy(next.topic,in["topic"],sizeof(next.topic));
    if (in.containsKey("prefix")) strlcpy(next.prefix,in["prefix"],sizeof(next.prefix));
    if (in.containsKey("ntp")) strlcpy(next.ntp,in["ntp"],sizeof(next.ntp));
    if (next.mqtt && !next.host[0]) { errorReply(400,"Enter the broker address"); return; }
    if (!validIntegrations(next)) { errorReply(400,"Invalid host, topic (no + # or empty levels) or time server"); return; }
    radio.pause(); const bool ok=saveIntegrations(next); restartRadio();
    if (!ok) { errorReply(500,"Could not save the integration settings"); return; }
    const bool clockChanged=strcmp(next.ntp,integrations.ntp)!=0;
    integrations=next;
    if (clockChanged && WiFi.status()==WL_CONNECTED) startClock();
    mqtt.stop();
    if (integrations.mqtt && WiFi.status()==WL_CONNECTED) startMqtt();
    messageReply(integrations.mqtt?"Saved; connecting to the MQTT broker":"Saved");
  });
  server.onNotFound([]() {
    if (server.uri().startsWith("/api/")) { if (authenticated()) errorReply(404,"Unknown API route"); return; }
    // Captive-portal probes lead to the authenticated setup UI.
    server.sendHeader("Location",apActive?"http://192.168.4.1/":"/",true);
    server.send(302,"text/plain","");
  });
  server.begin();
}
