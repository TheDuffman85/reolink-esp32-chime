#include "MqttService.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include "App.h"
#include "ChimeService.h"
#include "LinkService.h"
#include "Version.h"

IntegrationSettings integrations;
Mqtt mqtt;

namespace {
String mqttBase, lastMqttState, lastMqttStatus, announcedName;
uint32_t mqttStatusAt = 0;

// Home Assistant MQTT discovery: one device with event, controls and
// diagnostics. With discovery disabled, empty retained configs remove them.
void announceMqtt() {
  const String node = "reolink_chime_" + deviceId;
  const bool remove = !integrations.discovery;
  DynamicJsonDocument doc(1536);
  const auto entity = [&](const char *id, const char *name, const char *icon, const char *category) {
    doc.clear(); JsonObject o = doc.to<JsonObject>();
    o["~"] = mqttBase; o["name"] = name; o["uniq_id"] = node + "_" + id;
    o["avty_t"] = "~/availability"; if (icon) o["ic"] = icon; if (category) o["ent_cat"] = category;
    JsonObject dev = o.createNestedObject("dev");
    dev.createNestedArray("ids").add(node); dev["name"] = chimeApp.name;
    dev["mf"] = "DIY"; dev["mdl"] = "ESP32 + CC1101 chime for Reolink doorbells"; dev["sw"] = FIRMWARE_VERSION;
    if (WiFi.status() == WL_CONNECTED) dev["cu"] = "http://" + WiFi.localIP().toString() + "/";
    o.createNestedObject("o")["name"] = "reolink-chime";
    return o;
  };
  const auto send = [&](const char *component, const char *id) {
    const String topic = String(integrations.prefix) + "/" + component + "/" + node + "/" + id + "/config";
    String body; if (!remove) serializeJson(doc, body);
    mqtt.publish(topic, body, true);
  };
  // overrideOnly: unavailable while rings play the requested ringtone slot.
  const auto select = [&](const char *id, const char *name, const char *icon, const char *field,
                          const char *const *names, uint8_t count, bool overrideOnly) {
    JsonObject o = entity(id, name, icon, nullptr);
    if (overrideOnly) {
      o.remove("avty_t"); o["avty_mode"] = "all";
      JsonArray avty = o.createNestedArray("avty");
      avty.createNestedObject()["t"] = "~/availability";
      JsonObject state = avty.createNestedObject();
      state["t"] = "~/state"; state["val_tpl"] = "{{ 'online' if value_json.override_ringtone else 'offline' }}";
    }
    o["stat_t"] = "~/state"; o["val_tpl"] = String("{{ value_json.") + field + " }}";
    o["cmd_t"] = "~/set"; o["cmd_tpl"] = String("{\"") + field + "\": {{ value | tojson }}}";
    JsonArray options = o.createNestedArray("ops"); for (uint8_t i = 0; i < count; ++i) options.add(names[i]);
    send("select", id);
  };
  const auto number = [&](const char *id, const char *name, const char *icon, const char *field,
                          int min, int max, int step, const char *unit, const char *category) {
    JsonObject o = entity(id, name, icon, category);
    o["stat_t"] = "~/state"; o["val_tpl"] = String("{{ value_json.") + field + " }}";
    o["cmd_t"] = "~/set"; o["cmd_tpl"] = String("{\"") + field + "\": {{ value | int }}}";
    o["min"] = min; o["max"] = max; o["step"] = step; o["mode"] = "slider";
    if (unit) o["unit_of_meas"] = unit;
    send("number", id);
  };
  const auto toggle = [&](const char *id, const char *name, const char *icon, const char *field, const char *category) {
    JsonObject o = entity(id, name, icon, category);
    o["stat_t"] = "~/state"; o["val_tpl"] = String("{{ 'ON' if value_json.") + field + " else 'OFF' }}";
    o["cmd_t"] = "~/set"; o["stat_on"] = "ON"; o["stat_off"] = "OFF";
    o["pl_on"] = String("{\"") + field + "\": true}"; o["pl_off"] = String("{\"") + field + "\": false}";
    send("switch", id);
  };
  { JsonObject o = entity("ring", "Doorbell", "mdi:doorbell", nullptr);
    o["stat_t"] = "~/event"; o["dev_cla"] = "doorbell";
    JsonArray types = o.createNestedArray("evt_typ"); types.add("ring"); types.add("test");
    send("event", "ring"); }
  { JsonObject o = entity("ringing", "Ringing", "mdi:bell-ring", nullptr);
    o["stat_t"] = "~/doorbell"; o["pl_on"] = "ON"; o["off_dly"] = 5;
    send("binary_sensor", "ringing"); }
  { JsonObject o = entity("play", "Play chime", "mdi:play", nullptr);
    o["cmd_t"] = "~/play"; o["pl_prs"] = "{}";
    send("button", "play"); }
  const char *const outputChoices[] = {"Gong", "Audio", "None"};
  select("gong_type", "Sound mode", "mdi:bell-cog", "type", outputChoices, chime::GongKinds, false);
  select("audio_output", "Audio output", "mdi:audio-input-rca", "audio_output", chime::AudioOutputNames, chime::AudioOutputs, false);
  toggle("override_ringtone", "Override ringtone", "mdi:playlist-music", "override_ringtone", nullptr);
  select("melody", "Melody", "mdi:music", "melody", chime::MelodyNames, chime::Melodies, true);
  select("pattern", "Gong pattern", "mdi:bell-ring-outline", "pattern", chime::PatternNames, chime::Patterns, true);
  number("volume", "Chime volume", "mdi:volume-high", "volume", 0, 4, 1, nullptr, nullptr);
  mqtt.publish(String(integrations.prefix) + "/number/" + node + "/buzzer_level/config", "", true);
  number("audio_level", "Audio level", "mdi:volume-source", "audio_level", 0, 100, 5, "%", "config");
  number("extra_relay_ms", "Extra relay pulse length", "mdi:timer-outline", "extra_relay_ms", 50, 2000, 50, "ms", "config");
  number("pulse_ms", "Strike length", "mdi:timer-outline", "pulse_ms", 50, 2000, 25, "ms", "config");
  number("gap_ms", "Strike pause", "mdi:timer-pause-outline", "gap_ms", 100, 3000, 50, "ms", "config");
  toggle("mute", "Mute", "mdi:bell-off", "mute", nullptr);
  { JsonObject o = entity("mute_minutes", "Mute for", "mdi:timer-sand", nullptr);
    o["stat_t"] = "~/state"; o["val_tpl"] = "{{ value_json.mute_minutes }}";
    o["cmd_t"] = "~/set"; o["cmd_tpl"] = "{\"mute_minutes\": {{ value | int }}}";
    o["min"] = 0; o["max"] = chime::MaxMuteMinutes; o["step"] = 1; o["mode"] = "box"; o["unit_of_meas"] = "min";
    send("number", "mute_minutes"); }
  { JsonObject o = entity("muted_until", "Muted until", "mdi:bell-sleep", nullptr);
    o["stat_t"] = "~/status"; o["val_tpl"] = "{{ value_json.mute_until }}"; o["dev_cla"] = "timestamp";
    send("sensor", "muted_until"); }
  toggle("led", "Status LED", "mdi:led-on", "led", "config");
  toggle("extra_relay", "Extra relay pulse", "mdi:electric-switch", "extra_relay", "config");
  { JsonObject o = entity("name", "Chime name", "mdi:rename", "config");
    o["stat_t"] = "~/state"; o["val_tpl"] = "{{ value_json.name }}";
    o["cmd_t"] = "~/set"; o["cmd_tpl"] = "{\"name\": {{ value | tojson }}}"; o["min"] = 1; o["max"] = 32;
    send("text", "name"); }
  { JsonObject o = entity("rings", "Rings since restart", "mdi:counter", "diagnostic");
    o["stat_t"] = "~/status"; o["val_tpl"] = "{{ value_json.rings }}"; o["stat_cla"] = "total_increasing";
    send("sensor", "rings"); }
  { JsonObject o = entity("last_ring", "Last ring", nullptr, nullptr);
    o["stat_t"] = "~/status"; o["val_tpl"] = "{{ value_json.last_ring }}"; o["dev_cla"] = "timestamp";
    send("sensor", "last_ring"); }
  { JsonObject o = entity("link", "Doorbell link", nullptr, "diagnostic");
    o["stat_t"] = "~/status"; o["val_tpl"] = "{{ 'ON' if value_json.linked else 'OFF' }}"; o["dev_cla"] = "connectivity";
    send("binary_sensor", "link"); }
  { JsonObject o = entity("wifi_rssi", "WiFi signal", nullptr, "diagnostic");
    o["stat_t"] = "~/status"; o["val_tpl"] = "{{ value_json.wifi_rssi }}"; o["dev_cla"] = "signal_strength";
    o["unit_of_meas"] = "dBm"; o["stat_cla"] = "measurement"; o["en"] = false;
    send("sensor", "wifi_rssi"); }
  announcedName = chimeApp.name;
  logf("[mqtt] %s Home Assistant discovery\n", remove ? "removed" : "published");
}
void mqttResult(const char *error) {
  if (error) logf("[mqtt] command rejected: %s\n", error);
  StaticJsonDocument<192> doc; doc["ok"] = !error; if (error) doc["error"] = error;
  String body; serializeJson(doc, body); mqtt.publish(mqttBase + "/result", body, false);
}
// Commands: <base>/set (JSON), <base>/set/<field> (bare value), <base>/play
// (optional JSON overrides) and <base>/get. Only chime settings are reachable.
void handleMqtt(const MqttMessage &message) {
  const String topic = message.topic, set = mqttBase + "/set";
  DynamicJsonDocument doc(768);
  if (topic == set || topic.startsWith(set + "/")) {
    if (topic == set) {
      if (deserializeJson(doc, message.payload, message.length) || !doc.is<JsonObject>()) { mqttResult("Expected a JSON object"); return; }
    } else if (!chime::chimeField(message.topic + set.length() + 1, message.payload, message.length, doc)) {
      mqttResult("Invalid value"); return;
    }
    mqttResult(updateChime(doc.as<JsonObjectConst>(), "mqtt"));
    lastMqttState = ""; // Confirm the (possibly unchanged) state to optimistic clients.
  } else if (topic == mqttBase + "/play") {
    uint8_t volume = chimeApp.volume, mode = gongx.type, output = gongAudioOutput; uint8_t flags = 0; bool force = false;
    const bool options = message.length && !deserializeJson(doc, message.payload, message.length) && doc.is<JsonObject>();
    // "sound" requests a slot like the doorbell's music ID; it is not a preview.
    int slot = 0;
    if (options && doc.containsKey("sound") && (slot = chime::soundSlot(doc["sound"])) < 0) {
      mqttResult("sound: 0 to 9 or a melody or pattern name"); return;
    }
    GongRequest request = gongRequest(volume, uint8_t(slot));
    if (options) {
      int pick;
      if (doc.containsKey("force")) {
        if (!doc["force"].is<bool>()) { mqttResult("force: true or false"); return; }
        force = doc["force"];
      }
      if (doc.containsKey("type")) {
        if ((pick = chime::chimeEnum(doc["type"], chime::GongKindNames, chime::GongKinds)) < 0) { mqttResult("type: Audio, Gong or None"); return; }
        mode = pick; flags = chime::RingPreview;
      }
      if (doc.containsKey("audio_output")) {
        if ((pick = chime::chimeEnum(doc["audio_output"], chime::AudioOutputNames, chime::AudioOutputs)) < 0) { mqttResult("audio_output: PWM or DAC"); return; }
        output = pick; flags = chime::RingPreview;
      }
      if (doc.containsKey("melody")) {
        if ((pick = chime::chimeEnum(doc["melody"], chime::MelodyNames, chime::Melodies)) < 0) { mqttResult("melody: unknown"); return; }
        request.tone.melody = pick; flags = chime::RingPreview;
      }
      if (doc.containsKey("pattern")) {
        if ((pick = chime::chimeEnum(doc["pattern"], chime::PatternNames, chime::Patterns)) < 0) { mqttResult("pattern: unknown"); return; }
        request.pattern = pick; flags = chime::RingPreview;
      }
      if (doc.containsKey("volume")) {
        if (!doc["volume"].is<int>() || doc["volume"].as<int>() < 0 || doc["volume"].as<int>() > 4) { mqttResult("volume: 0 to 4"); return; }
        volume = doc["volume"].as<int>(); request.tone.volume = settings.gong.volume * volume / 4; flags = chime::RingPreview;
      }
    }
    request.type = outputType(mode, output);
    if (const char *error = outputError(request.type, pins)) { mqttResult(error); return; }
    const auto outcome = playChime(chime::RingSource::Mqtt, request, volume, flags, 0, 0, force);
    mqttResult(outcome == chime::RingOutcome::Busy ? "Gong busy or unavailable" :
      outcome == chime::RingOutcome::ChimeMuted ? "Chime is muted; send {\"force\":true} to play anyway" : nullptr);
  } else if (topic == mqttBase + "/get") {
    lastMqttState = ""; lastMqttStatus = "";
  }
}
}

String defaultMqttBase() { return "reolink_chime/" + deviceId; }
void startMqtt() {
  mqttBase = integrations.topic[0] ? String(integrations.topic) : defaultMqttBase();
  lastMqttState = ""; lastMqttStatus = "";
  mqtt.start(integrations, "reolink-chime-" + deviceId, mqttBase);
  if (integrations.mqtt) logf("[mqtt] connecting to %s:%u, topic %s\n", integrations.host, integrations.port, mqttBase.c_str());
}
void publishRing(const chime::RingEvent &e) {
  if (!mqtt.connected()) return;
  StaticJsonDocument<512> doc; JsonObject o = doc.to<JsonObject>();
  const bool doorbell = e.source == uint8_t(chime::RingSource::Doorbell);
  o["event_type"] = doorbell ? "ring" : "test"; // Home Assistant event entity
  ringJson(e, o, true);
  String body; serializeJson(doc, body);
  mqtt.publish(mqttBase + "/event", body, false);
  if (doorbell) mqtt.publish(mqttBase + "/doorbell", "ON", false);
}
void serviceMqtt() {
  if (!mqtt.running()) return;
  if (mqtt.takeAnnounce()) { announceMqtt(); lastMqttState = ""; lastMqttStatus = ""; }
  MqttMessage message;
  if (mqtt.pop(message)) handleMqtt(message);
  static uint32_t checkedAt = 0;
  if (!mqtt.connected() || uint32_t(millis() - checkedAt) < 500) return;
  checkedAt = millis();
  if (integrations.discovery && announcedName != chimeApp.name) announceMqtt(); // Device name follows the chime.
  StaticJsonDocument<512> doc;
  chime::writeChime(currentChime(), doc.to<JsonObject>());
  String state; serializeJson(doc, state);
  if (state != lastMqttState && mqtt.publish(mqttBase + "/state", state, true)) lastMqttState = state;
  // Status: whenever a meaningful field changes, otherwise once a minute.
  doc.clear();
  doc["linked"] = rfLink.profileReady(); doc["paired"] = settings.paired; doc["radio_ok"] = radio.healthy;
  doc["rings"] = rings;
  uint32_t lastEpoch = 0;
  for (size_t i = 0; i < ringLog.size(); ++i)
    if (ringLog.newest(i).source == uint8_t(chime::RingSource::Doorbell)) { lastEpoch = ringLog.newest(i).epoch; break; }
  const String last = isoTime(lastEpoch);
  if (last.length()) doc["last_ring"] = last; else doc["last_ring"] = "None";
  doc["muted"] = chimeMuted();
  const String muteEnd = muteRemainingMs() && epochNow() ? isoTime(epochNow() + (muteRemainingMs() + 999) / 1000) : String();
  if (muteEnd.length()) doc["mute_until"] = muteEnd; else doc["mute_until"] = "None";
  doc["ip"] = WiFi.localIP().toString(); doc["time_synced"] = epochNow() != 0;
  String key; serializeJson(doc, key);
  if (key == lastMqttStatus && uint32_t(millis() - mqttStatusAt) < 60000) return;
  doc["wifi_rssi"] = WiFi.RSSI(); doc["uptime_s"] = millis() / 1000; doc["firmware"] = FIRMWARE_VERSION;
  String body; serializeJson(doc, body);
  if (mqtt.publish(mqttBase + "/status", body, true)) { lastMqttStatus = key; mqttStatusAt = millis(); }
}
