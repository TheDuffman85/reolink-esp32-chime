#include <Arduino.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <stdarg.h>
#include <time.h>
#include "App.h"
#include "ChimeService.h"
#include "LinkService.h"
#include "MqttService.h"
#include "Version.h"
#include "Web.h"
#include "WebAuth.h"

char logRing[LogCapacity];
uint32_t logTotal = 0;
void logf(const char *format, ...) {
  char line[320];
  const int prefix = snprintf(line, sizeof(line), "%10.3f ", millis() / 1000.0);
  va_list args; va_start(args, format);
  vsnprintf(line + prefix, sizeof(line) - prefix, format, args);
  va_end(args);
  Serial.print(line);
  for (const char *c = line; *c; ++c) logRing[logTotal++ % LogCapacity] = *c;
}

Settings settings;
chime::WifiConfig wifiConfig;
chime::WifiMode wifiMode = chime::WifiMode::Client;
bool storageOk = false, otaStarted = false, apActive = false;
uint32_t restartAt = 0, stationStarted = 0;
String apName, deviceId;

namespace {
DNSServer dns;
char ntpServer[sizeof(IntegrationSettings::ntp)]{}; // SNTP keeps this pointer.

// WPA2 key of the hotspot: the access-point password, else the sign-in
// password while sign-in is on; nullptr opens it.
const char *hotspotPassword() {
  if (wifiMode == chime::WifiMode::AccessPoint && wifiConfig.apPassword[0]) return wifiConfig.apPassword;
  return auth.enabled ? auth.password : nullptr;
}
void startMdns() { MDNS.begin("reolink-chime"); MDNS.addService("http","tcp",80); }

void serialDiagnostics() {
  static uint32_t lastAt = 0, lastRaw = 0, lastTx = 0, lastTxErr = 0;
  static float peak = -127; static String lastState;
  const float now = radio.rssi(); if (now > peak) peak = now;
  static uint32_t spikeAt = 0;
  if (now > -85 && uint32_t(millis() - spikeAt) > 200) {
    spikeAt = millis(); logf("[rssi] spike %.1f dBm\n", now);
  }
  const String state = rfLink.state();
  if (state != lastState) { logf("[link] %s\n", state.c_str()); lastState = state; }
  if (transmitted != lastTx || transmitErrors != lastTxErr) {
    logf("[tx] ok=%u err=%u %s trim=%d (%+.1f kHz) %s\n", transmitted, transmitErrors,
      lastTxInverted ? "inverted" : "normal", sweepTrim, sweepTrim * 26000.0f / 16384.0f, radio.lastError().c_str());
    lastTx = transmitted; lastTxErr = transmitErrors;
  }
  while (Serial.available()) {
    const char c = Serial.read();
    if (c == 'p') logf("%s\n", startPairing() ? "[cmd] pairing started" : "[cmd] pairing refused");
    else if (c == 'c') { rfLink.cancel(millis()); logf("[cmd] cancelled\n"); }
    else if (c == 'g') playChime(chime::RingSource::Serial, gongRequest(4), 4);
    else if (c == 'a' && signInOff()) logf("[cmd] web sign-in turned off\n"); // Physical access.
    else if (c == 'w') recoverWifi(false);
  }
  if (uint32_t(millis() - lastAt) < 5000) return;
  lastAt = millis();
  logf("[diag] radio=%s part=%u ver=%u marc=0x%02X rssi=%.1f peak=%.1f raw=%u/5s "
    "pkts=%u ti=%u ccitt=%u bad=%u badlen=%u falsesync=%u drop=%u link=%s paired=%d tx=%u/%u wifi=%d %s\n",
    radio.healthy ? "ok" : "FAIL", radio.part, radio.version, radio.state(), now, peak,
    rawBytes - lastRaw, packetCount, crcTi, crcCcitt, crcBad, unsigned(Radio::invalidLengths + decoder.invalidLengths),
    unsigned(Radio::falseSyncs), unsigned(Radio::dropped), state.c_str(), settings.paired, transmitted, transmitErrors,
    WiFi.status() == WL_CONNECTED, radio.lastError().c_str());
  lastRaw = rawBytes; peak = -127;
}
}

uint32_t epochNow() { const time_t now = time(nullptr); return now > 1700000000 ? uint32_t(now) : 0; }
String isoTime(uint32_t epoch) {
  if (!epoch) return String();
  const time_t t = epoch; struct tm utc; gmtime_r(&t, &utc);
  char text[24]; strftime(text, sizeof(text), "%Y-%m-%dT%H:%M:%SZ", &utc); return text;
}
void startClock() {
  if (sntp_enabled()) sntp_stop();
  strlcpy(ntpServer, integrations.ntp, sizeof(ntpServer));
  if (ntpServer[0]) configTime(0, 0, ntpServer);
}

String defaultApName() { String id = deviceId; id.toUpperCase(); return "Reolink-Chime-" + id; }
void startAp() {
  if (apActive) return;
  // Access-point mode runs the hotspot alone; the setup hotspot runs beside the station.
  const bool own = wifiMode == chime::WifiMode::AccessPoint;
  WiFi.mode(own ? WIFI_AP : WIFI_AP_STA);
  apName = own && wifiConfig.apSsid[0] ? String(wifiConfig.apSsid) : defaultApName();
  const char *password = hotspotPassword();
  apActive = WiFi.softAP(apName.c_str(), password);
  if (apActive) dns.start(53, "*", WiFi.softAPIP());
  logf("%s: %s, %s, %s\n", own ? "Access point" : "Setup AP", apName.c_str(), apActive ? "192.168.4.1" : "failed",
    !password ? "open" : password == auth.password ? "WPA2 (sign-in password)" : "WPA2");
}
void recoverWifi(bool forget) {
  const bool wasClient = wifiMode == chime::WifiMode::Client;
  radio.pause();
  bool saved = true;
  if (forget) { settings.ssid[0]=0; settings.wifiPassword[0]=0; saved = storageOk = saveSettings(settings); }
  if (wifiConfig.mode != chime::WifiMode::Client) {
    chime::WifiConfig next = wifiConfig; next.mode = chime::WifiMode::Client;
    if (saveWifiConfig(next)) wifiConfig = next; else saved = false;
  }
  restartRadio();
  wifiMode = chime::WifiMode::Client;
  if (!wasClient && apActive) { dns.stop(); WiFi.softAPdisconnect(false); apActive = false; } // Reopen as setup hotspot.
  if (forget) WiFi.disconnect(false, true);
  startAp();
  if (!wasClient && settings.ssid[0]) { WiFi.setAutoReconnect(true); WiFi.begin(settings.ssid, settings.wifiPassword); }
  stationStarted = millis();
  registerWeb(); // Not started while WiFi was off.
  logf("WiFi: client mode, setup hotspot on%s%s\n", forget ? ", saved network cleared" : "",
    saved ? "" : " (NVS write failed; not kept after a restart)");
}

void setup() {
  Serial.begin(115200);
  const bool customPins = loadPins(pins);
  if (pins.led >= 0) { pinMode(pins.led,OUTPUT); digitalWrite(pins.led,LOW); }
  if (pins.button >= 0) pinMode(pins.button,INPUT_PULLUP);
  loadGongExtra();
  gongReady=gong.begin();
  bool existingInstallation=false;
  { Preferences prefs; if (prefs.begin("chime",true)) { existingInstallation=prefs.getBytesLength("settings")>0; prefs.end(); } }
  storageOk=loadSettings(settings); loadRadioOptions(radioOptions); loadChimeApp(); beginAuth(existingInstallation);
  loadIntegrations(integrations); loadRingLog(); loadMute(); mqtt.begin();
  initLink(false);
  deviceId=hex32(uint32_t(ESP.getEfuseMac())).substring(2); apName=String("Reolink-Chime-")+deviceId; deviceId.toLowerCase();
  logf("Firmware %s (build " __DATE__ " " __TIME__ ")\n", FIRMWARE_MARKER);
  if (auth.enabled) logf("Web sign-in on for user '%s'. Hold the button 15 s (or send 'a' here) to turn it off.\n", auth.user);
  else logf("Web sign-in off; the setup hotspot is open until a password is set in Settings > Security.\n");
  logf("Chime node ID: %s (app shows it as signed decimal %d)\n", hex32(settings.nodeId).c_str(), int32_t(settings.nodeId));
  if (!storageOk) logf("WARNING: settings persistence unavailable\n");
  logf("GPIO: %s map, radio SCK %d MISO %d MOSI %d CS %d GDO0 %d GDO2 %d, audio %d relay %d (active %s) LED %d button %d\n",
    customPins ? "custom" : "default", pins.sck, pins.miso, pins.mosi, pins.cs, pins.clock, pins.data,
    pins.audio, pins.relay, pins.relayActiveLow ? "low" : "high", pins.led, pins.button);
  loadWifiConfig(wifiConfig); wifiMode = wifiConfig.mode;
  WiFi.persistent(false); WiFi.setHostname("reolink-chime");
  if (wifiMode == chime::WifiMode::Off) {
    WiFi.mode(WIFI_OFF);
    logf("WiFi is off. Hold the button 8 s (or send 'w' here) to turn it on with the setup hotspot.\n");
  } else if (wifiMode == chime::WifiMode::AccessPoint) {
    startAp(); startMdns();
  } else {
    WiFi.mode(WIFI_STA); WiFi.setAutoReconnect(true);
    if (strlen(settings.ssid)) {
      WiFi.begin(settings.ssid,settings.wifiPassword); stationStarted=millis();
    } else startAp();
  }
  restartRadio();
  if (wifiMode != chime::WifiMode::Off) registerWeb(); // The web server needs the network stack.
  // Web preference migration can pause RF for NVS writes; join afterwards.
  if (settings.paired && radio.healthy) rfLink.startJoin(millis());
}

void loop() {
  static bool buttonWasDown=false, resetHandled=false;
  static uint32_t buttonAt=0, ledAt=0;
  receiveRadio();
  server.handleClient(); if (apActive) dns.processNextRequest();
  serviceLink();
  static bool wasConnected = false;
  const bool connected = WiFi.status()==WL_CONNECTED;
  if (connected && !wasConnected) {
    startMdns();
    startClock();
    if (integrations.mqtt && !mqtt.running()) startMqtt(); // The client reconnects by itself afterwards.
    logf("WiFi connected: %s, IP %s, http://reolink-chime.local/\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
  } else if (!connected && wasConnected) logf("WiFi disconnected\n");
  wasConnected = connected;
  if (connected) stationStarted=millis();
  else if (wifiMode == chime::WifiMode::Client && !apActive && uint32_t(millis()-stationStarted)>20000) startAp();
  serviceMute();
  serviceMqtt();
  serviceRingLog();
  saveLearnedPolarity();
  serviceAfc();
  static uint32_t buttonPinRevision=0;
  static bool authResetHandled=false;
  const bool down=pins.button>=0 && digitalRead(pins.button)==LOW;
  if (buttonPinRevision!=pinMapRevision) {
    buttonPinRevision=pinMapRevision; buttonWasDown=down; buttonAt=millis();
    resetHandled=true; authResetHandled=true;
  }
  if (down && !buttonWasDown) { buttonAt=millis(); resetHandled=false; }
  if (down && !resetHandled && uint32_t(millis()-buttonAt)>=8000) {
    // Client mode forgets the network; access point/off only return to client mode.
    recoverWifi(wifiMode == chime::WifiMode::Client); resetHandled=true;
  }
  if (!down) authResetHandled=false;
  else if (!authResetHandled && auth.enabled && uint32_t(millis()-buttonAt)>=15000) {
    // Physical access recovers a forgotten password: sign-in off, credentials kept.
    if (signInOff()) logf("Web sign-in turned off by the button\n");
    authResetHandled=true;
    if (apActive && !hotspotPassword()) WiFi.softAP(apName.c_str(), nullptr); // Reopen the hotspot without a password.
  }
  if (!down && buttonWasDown && !resetHandled) {
    const uint32_t held=millis()-buttonAt;
    if (held>=2000) logf("%s\n", startPairing() ?
      "Pairing started for 5 minutes" : "Pairing busy, radio or storage unavailable");
    else if (held>=30) playChime(chime::RingSource::Button, gongRequest(4), 4);
  }
  buttonWasDown=down;
  serialDiagnostics();
  checkRadio();
  if (uint32_t(millis()-ledAt)>500) {
    ledAt=millis();
    // The Reolink app's LED switch turns the blinking status LED off.
    if (pins.led >= 0) digitalWrite(pins.led,gong.active() || (chimeApp.led && radio.healthy && !digitalRead(pins.led)));
  }
  if (restartAt && int32_t(millis()-restartAt)>=0) ESP.restart();
  delay(1);
}
