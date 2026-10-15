#include "LinkService.h"
#include <mbedtls/aes.h>
#include <esp_system.h>
#include <sys/time.h>
#include "Afc.h"
#include "App.h"
#include "AppProtocol.h"
#include "ChimeService.h"
#include "Identity.h"

Radio radio;
chime::Decoder decoder;
chime::Duplicates duplicates;
chime::Link rfLink;
uint32_t transmitted = 0, transmitErrors = 0, radioResets = 0, rawBytes = 0;
uint32_t lastPacketAt = 0, packetCount = 0, crcTi = 0, crcBad = 0, crcCcitt = 0, rings = 0, lastRing = 0;
uint32_t lastAppAt = 0, appRequests = 0; uint8_t lastAppOpcode = 0;
int8_t sweepTrim = 0;
uint32_t sweepCombo = UINT32_MAX;
// Bench test 2026-10-13: the doorbell lists the ESP with normal TX polarity at
// trims 0 and +13, never inverted. Start locked to normal; no sweep.
bool txPolarityKnown = true, txInverted = false, lastTxInverted = false;
bool learnPending = false;
RadioOptions radioOptions;
bool afcAcquiring = false;
LogEntry packetLog[PacketLogSize];
size_t packetLogNext = 0, packetLogSize = 0;

namespace {
// V2 compatibility version: ASCII "00A2" as a little-endian word.
// This is separate from the ESP32 application's semantic firmware version.
constexpr uint32_t BootWord = 0x32413030;
uint8_t appReply[chime::MaxMac - 16]; size_t appReplyLength = 0;
uint32_t sweepBase = 0;
uint8_t learnedCandidate = 0, learnedCount = 0;
chime::TrimTracker trimTracker;
bool afcPending = false; int8_t afcTarget = 0;
uint32_t afcFrameAt = 0, afcChangeAt = 0;
constexpr uint32_t AfcMinIntervalMs = 60000, AfcSilenceMs = 30UL * 60 * 1000;

// Frequency estimates only from valid frames of the paired doorbell (any
// valid frame before a profile exists). The source ID sits at MAC +8 in
// join responses and data frames alike.
bool afcFrame(const chime::Packet &packet) {
  if (!chime::validPacket(packet)) return false;
  if (!settings.profile) return true;
  return packet.size >= 13 && chime::read32le(packet.bytes + 9) == settings.source;
}
void trackFrequency(const chime::Packet &packet, const RxFrame &frame) {
  if (!radioOptions.autoTrim || !frame.offsetValid || !afcFrame(packet)) return;
  afcFrameAt = millis();
  trimTracker.add(frame.trim, frame.offset);
  int8_t target;
  if (trimTracker.decide(settings.radio.trim, radioOptions.calibrated && !afcAcquiring, target) &&
      (!radioOptions.calibrated || afcAcquiring || uint32_t(millis() - afcChangeAt) >= AfcMinIntervalMs)) {
    afcTarget = target; afcPending = true;
  }
}

void resetDecoder() {
  chime::Polarity polarity = settings.radio.polarity;
  if (polarity == chime::Polarity::Auto && settings.learnedPolarity)
    polarity = static_cast<chime::Polarity>(settings.learnedPolarity);
  decoder.setPolarity(polarity);
  learnedCount = 0; learnedCandidate = 0;
}

bool crypt(void *, const uint8_t key[16], uint8_t *bytes, size_t count) {
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  uint8_t counter[16]{}, stream[16]{};
  size_t offset = 0;
  const int setup = mbedtls_aes_setkey_enc(&aes, key, 128);
  const int result = setup == 0 ? mbedtls_aes_crypt_ctr(&aes, count, &offset,
    counter, stream, bytes, bytes) : setup;
  mbedtls_aes_free(&aes);
  return result == 0;
}

String decodeRing(const chime::Packet &packet, float rssi) {
  chime::Ring ring;
  if (!chime::ringHeader(packet, ring)) return "log only";
  if (rfLink.pairing()) return "ring ignored during pairing";
  if (!settings.profile) return "ring header; no receive profile";
  if (settings.paired && !rfLink.profileReady()) return "ring ignored until joined";
  if (ring.pair != settings.pair || ring.source != settings.source ||
      ring.destination != settings.destination) return "other device/session";
  uint8_t plain[4]; memcpy(plain, packet.bytes + 17, sizeof(plain));
  if (!crypt(nullptr, settings.key, plain, sizeof(plain))) return "AES error";
  if (!chime::ringPayload(plain, ring)) return "payload opcode differs (check key)";
  // Rejected before counting or playing, also while the ringtone is overridden.
  if (ring.parameter >= chime::DoorbellSounds) return String("unsupported music ID ") + ring.parameter;
  GongRequest request = gongRequest(chimeApp.volume, uint8_t(ring.parameter));
  if (duplicates.seen(ring, millis())) return "duplicate ring";
  lastRing = millis(); ++rings;
  const auto outcome = playChime(chime::RingSource::Doorbell, request, chimeApp.volume,
    0, ring.parameter, int8_t(constrain(lroundf(rssi), -127L, 0L)));
  return String("ring parameter ") + ring.parameter + ", " + chime::RingOutcomeNames[uint8_t(outcome)];
}
// Answers doorbell requests the way the vendor chime does: the request buffer
// is edited in place and sent back as an encrypted type-3 frame.
void handleAppRequest(const uint8_t *request, size_t length) {
  uint8_t reply[sizeof(appReply)]{};
  const auto result = chime::appRequest(chimeApp, request, length,
    reply, sizeof(reply), BootWord);
  lastAppAt = millis(); lastAppOpcode = request[0]; ++appRequests;
  if (!result.handled) {
    if (request[0] != 9) logf("[app] opcode 0x%02X not handled\n", request[0]);
  } else if (!result.valid) {
    logf("[app] invalid request opcode 0x%02X (%u bytes)\n", request[0], unsigned(length));
  }
  if (result.changed) {
    logf("[app] config: name='%s' volume=%u led=%u\n", chimeApp.name, chimeApp.volume, chimeApp.led);
    saveChimeApp();
  }
  if (result.replyLength) {
    memcpy(appReply, reply, result.replyLength); appReplyLength = result.replyLength;
  }
}
void onPacket(const chime::Packet &packet, const RxFrame &frame) {
  lastPacketAt = millis(); ++packetCount;
  if (packet.crc == chime::Crc::Ti8005) {
    ++crcTi; txInverted = packet.inverted;
    if (!txPolarityKnown) {
      txPolarityKnown = true; settings.radio.trim = sweepTrim; // Saved with the pairing profile.
      logf("[lock] polarity=%s trim=%d (%.1f kHz)\n", packet.inverted ? "inverted" : "normal",
        sweepTrim, sweepTrim * 26000.0f / 16384.0f);
    }
    if (settings.radio.polarity == chime::Polarity::Auto && !settings.learnedPolarity) {
      const uint8_t candidate = packet.inverted ? 2 : 1;
      if (learnedCandidate != candidate) { learnedCandidate = candidate; learnedCount = 0; }
      if (++learnedCount >= 3) {
        settings.learnedPolarity = candidate; learnPending = true;
        decoder.setPolarity(static_cast<chime::Polarity>(candidate));
      }
    }
  } else {
    learnedCount = 0;
    if (packet.crc == chime::Crc::CcittDiagnostic) ++crcCcitt; else ++crcBad;
  }
  auto &entry = packetLog[packetLogNext];
  entry.id = packetCount; entry.ms = millis(); entry.rssi = frame.rssi; entry.offset = frame.offset;
  entry.offsetValid = frame.offsetValid;
  struct timeval now; gettimeofday(&now, nullptr);
  entry.epoch = epochNow() ? uint32_t(now.tv_sec) : 0; entry.epochMs = uint16_t(now.tv_usec / 1000);
  entry.inverted = packet.inverted; entry.crc = packet.crc;
  entry.hex = packet.size > 2 && (packet.bytes[2] & 0x3F) == 1 ?
    "[pairing frame redacted]" : hexBytes(packet.bytes, packet.size);
  trackFrequency(packet, frame);
  const char *linkResult = rfLink.receive(packet, millis());
  entry.result = linkResult ? String(linkResult) : decodeRing(packet, entry.rssi);
  logf("[rx] #%u %.1f dBm %+.1f kHz %s crc=%s len=%u type=%u: %s\n", packetCount, entry.rssi,
    frame.offset * 26000.0f / 16384.0f, packet.inverted ? "inv" : "norm", packet.crc==chime::Crc::Ti8005 ? "TI" :
    packet.crc==chime::Crc::CcittDiagnostic ? "CCITT" : "bad", unsigned(packet.size),
    packet.size > 2 ? unsigned(packet.bytes[2] & 0x3F) : 0u, entry.result.c_str());
  packetLogNext = (packetLogNext + 1) % PacketLogSize; if (packetLogSize < PacketLogSize) ++packetLogSize;
  // Application requests must belong to the active profile and pass framing,
  // CRC and AES checks before they can mutate settings or elicit replies.
  if (settings.paired && rfLink.profileReady() &&
      (!linkResult || strcmp(linkResult, "stale data frame") != 0)) {
    uint8_t plain[chime::MaxMac];
    const size_t count = chime::dataPayload(packet, settings.destination, settings.source,
      settings.pair, plain, sizeof(plain));
    if (count && crypt(nullptr, settings.key, plain, count)) {
      logf("[app] seq=%u session=%04X plain(%u)=%s\n", chime::read32le(packet.bytes + 13),
        unsigned(settings.pair), unsigned(count), hexBytes(plain, count).c_str());
      handleAppRequest(plain, count);
    }
  }
}
}

void restartRadio() {
  resetDecoder();
  // The CC1101 matches one sync polarity; use a learned one when set to Auto.
  RadioSettings rf = settings.radio;
  if (rf.polarity == chime::Polarity::Auto && settings.learnedPolarity)
    rf.polarity = static_cast<chime::Polarity>(settings.learnedPolarity);
  // Until automatic trim has measured the crystal offset, listen wide
  // (packet engine only; Esp32 mode cannot measure and keeps the trim).
  radio.begin(rf, radioOptions.autoTrim && (!radioOptions.calibrated || afcAcquiring), radioOptions.rxMode);
  sweepTrim = settings.radio.trim;
}

void resetAfc() { trimTracker.reset(); afcPending = false; afcAcquiring = false; afcFrameAt = millis(); }

void serviceAfc() {
  if (!radioOptions.autoTrim || radioOptions.rxMode != RxMode::PacketEngine ||
      otaStarted || restartAt || rfLink.busy()) return;
  const uint32_t now = millis();
  if (afcPending && uint32_t(now - lastPacketAt) > 250) {
    // Applied while quiet so no frame or reply is cut off; TX uses it too.
    afcPending = false; trimTracker.reset();
    const int8_t old = settings.radio.trim;
    Settings next = settings; next.radio.trim = afcTarget;
    RadioOptions nextOptions = radioOptions; nextOptions.calibrated = true;
    radio.pause();
    bool saved = next.radio.trim == settings.radio.trim || saveSettings(next);
    if (saved && !radioOptions.calibrated) saved = saveRadioOptions(nextOptions);
    if (saved) {
      settings = next; radioOptions = nextOptions; afcAcquiring = false; afcChangeAt = now;
      logf("[afc] trim %d -> %d (%+.1f kHz)\n", old, settings.radio.trim,
        settings.radio.trim * 26000.0f / 16384.0f);
    } else {
      storageOk = false; logf("[afc] trim %d not saved: NVS write failed\n", afcTarget);
    }
    restartRadio();
    return;
  }
  // A long silence from the paired doorbell may mean the offset drifted out
  // of the narrow filter: listen wide again until the next estimate.
  if (radioOptions.calibrated && !afcAcquiring && settings.profile && uint32_t(now - afcFrameAt) > AfcSilenceMs) {
    afcAcquiring = true; trimTracker.reset();
    logf("[afc] no doorbell frames for %u min; reacquiring with wide bandwidth\n",
      unsigned(AfcSilenceMs / 60000));
    radio.pause(); restartRadio();
  }
}

void initLink(bool rejoin) {
  chime::LinkProfile profile;
  profile.paired = settings.paired; profile.heartbeatMode = settings.heartbeatMode;
  profile.source = settings.source; profile.destination = settings.destination;
  profile.pair = settings.pair; memcpy(profile.key, settings.key, sizeof(profile.key));
  rfLink.init(settings.nodeId, BootWord, crypt, nullptr, profile);
  if (rejoin && settings.paired) rfLink.startJoin(millis());
}
bool startPairing() {
  // A rejoin (after boot or a doorbell request) must not block new pairing;
  // an old doorbell that no longer knows the chime never answers it.
  if (rfLink.busy() && !rfLink.pairing() && !otaStarted && !restartAt) {
    rfLink.cancel(millis()); logf("[link] rejoin cancelled for pairing\n");
  }
  if (!storageOk || !radio.healthy || otaStarted || restartAt || rfLink.busy()) return false;
  if (!chime::compatibleNodeId(settings.nodeId)) {
    // Linux alarm rules require a positive signed ID. Keep the old identity
    // through OTA/reboot; replace it only when pairing is explicitly started.
    Settings next = settings;
    do { next.nodeId = chime::nodeIdCandidate(esp_random()); } while (!next.nodeId);
    next.paired = false; next.profile = false; next.source = 0; next.destination = 0;
    next.pair = 0; next.heartbeatMode = 0; memset(next.key, 0, sizeof(next.key));
    radio.pause(); storageOk = saveSettings(next);
    restartRadio();
    if (!storageOk) return false;
    logf("[link] replacing incompatible chime ID %08X with %08X; remove old app entry\n",
      settings.nodeId, next.nodeId);
    settings = next; appReplyLength = 0; duplicates.reset(); initLink(false);
  }
  sweepBase = transmitted + transmitErrors; sweepCombo = UINT32_MAX; // Sweep restarts at step 1.
  const bool started = rfLink.startPair(millis());
  if (started) appReplyLength = 0;
  return started;
}

void receiveRadio() {
  // Esp32 mode: bytes sampled by the clock ISR, searched bit by bit for sync.
  // RSSI is read when the frame is decoded; no frequency offset is available.
  static uint32_t generation=UINT32_MAX, lastDrop=0;
  if (Radio::dropped != lastDrop) { lastDrop=Radio::dropped; decoder.reset(); }
  Sample sample;
  for (int n=0;n<256 && radio.pop(sample);++n) {
    ++rawBytes;
    if (sample.generation!=generation) { generation=sample.generation; decoder.reset(); }
    for (int bit=7;bit>=0;--bit) {
      chime::Packet packet;
      if (decoder.feedBit((sample.byte>>bit)&1,packet)) {
        RxFrame meta; meta.rssi=radio.rssi(); meta.offsetValid=false; meta.trim=sweepTrim;
        meta.inverted=packet.inverted;
        onPacket(packet,meta);
      }
    }
  }
  // Packet engine: complete frames from the FIFO task.
  RxFrame frame;
  // Bound each pass so a burst cannot starve the web server or button handling.
  for (int n=0;n<4 && radio.pop(frame);++n) {
    rawBytes+=frame.size;
    // Each frame starts after the hardware-matched first sync half; restore
    // it so the software decoder sees the complete 32-bit sync.
    const uint8_t flip=frame.inverted?0xFF:0x00;
    const uint8_t head[]={uint8_t(0x93^flip),uint8_t(0x0B^flip)};
    decoder.reset();
    chime::Packet packet;
    for (uint8_t byte : head)
      for (int bit=7;bit>=0;--bit) decoder.feedBit((byte>>bit)&1,packet);
    for (size_t i=0;i<frame.size;++i)
      for (int bit=7;bit>=0;--bit)
        if (decoder.feedBit((frame.bytes[i]>>bit)&1,packet)) onPacket(packet,frame);
  }
}

void serviceLink() {
  if (otaStarted || restartAt) return;
  if (appReplyLength && rfLink.sendData(appReply, appReplyLength)) {
    logf("[app] reply opcode 0x%02X (%u bytes) queued\n", appReply[0], unsigned(appReplyLength));
    appReplyLength = 0;
  }
  rfLink.tick(millis());
  if (const auto *frame = rfLink.peek(millis())) {
    // TX polarity is unproven. Use the polarity of the last valid received
    // frame; until one is seen, alternate so each sign gets every other try.
    // Pairing discovery also sweeps the synthesizer offset, because cheap
    // CC1101 crystals can be tens of kHz off: each offset is tried with both
    // polarities. RX stays on the same offset to hear the doorbell's reply.
    // Each offset/polarity combination dwells for 20 discoveries (~40 s) so
    // the doorbell's app list keeps a found chime long enough to select it,
    // and so a sighting in the app can be matched to one combination.
    static const int8_t sweep[] = {0,13,-13,26,-26,38,-38,51,-51,64,-64};
    bool invert = txInverted;
    if (!txPolarityKnown && strcmp(rfLink.state(), "discovering") == 0) {
      const uint32_t next = (transmitted + transmitErrors - sweepBase) / 20 % (2 * sizeof(sweep));
      const int8_t trim = int8_t(constrain(settings.radio.trim + sweep[next / 2], -128, 127));
      invert = next % 2;
      if (trim != sweepTrim) { radio.setTrim(trim); sweepTrim = trim; }
      if (next != sweepCombo) {
        sweepCombo = next;
        logf("[sweep] combo %u/%u: %s, trim=%d (%+.1f kHz)\n", unsigned(next + 1),
          unsigned(2 * sizeof(sweep)), invert ? "inverted" : "normal", trim, trim * 26000.0f / 16384.0f);
      }
    } else if (!txPolarityKnown) {
      sweepCombo = UINT32_MAX; invert = false;
      if (sweepTrim != settings.radio.trim) { radio.setTrim(settings.radio.trim); sweepTrim = settings.radio.trim; }
    }
    uint8_t air[Radio::MaxTransmitSize]; const size_t size = min(frame->size, sizeof(air));
    for (size_t i = 0; i < size; ++i) air[i] = invert ? uint8_t(~frame->bytes[i]) : frame->bytes[i];
    lastTxInverted = invert;
    const bool sent = radio.transmit(air, size);
    if (sent) ++transmitted; else ++transmitErrors;
    decoder.reset(); rfLink.consume(sent, millis());
  }
  if (rfLink.profileDirty()) {
    Settings next = settings;
    const auto &profile = rfLink.profile();
    next.profile = true; next.paired = profile.paired; next.heartbeatMode = profile.heartbeatMode;
    next.source = profile.source; next.destination = profile.destination;
    next.pair = profile.pair; memcpy(next.key, profile.key, sizeof(next.key));
    radio.pause(); storageOk = saveSettings(next);
    if (storageOk) {
      settings = next; rfLink.clearProfileDirty(); duplicates.reset();
    } else {
      initLink(false);
      logf("Pairing profile could not be saved; retry pairing/rejoin after fixing NVS\n");
    }
    restartRadio();
  }
}

void saveLearnedPolarity() {
  if (learnPending && uint32_t(millis()-lastPacketAt)>250 && !otaStarted && !rfLink.busy()) {
    radio.pause(); storageOk=saveSettings(settings); learnPending=false; restartRadio();
  }
}

// A 3.3 V dip (WiFi current peaks, weak supply, no decoupling capacitor)
// can reset the CC1101 to defaults: IDLE, GDO0 = 135 kHz clock. Re-init it.
void checkRadio() {
  static uint32_t radioCheckAt = 0;
  if (uint32_t(millis() - radioCheckAt) >= 1000 && !otaStarted) {
    radioCheckAt = millis();
    if (!radio.configured()) {
      ++radioResets;
      logf("[radio] lost configuration (reset #%u, error '%s'); reinitialising\n", radioResets, radio.lastError().c_str());
      restartRadio();
      logf("[radio] reinit %s %s\n", radio.healthy ? "ok" : "FAILED", radio.lastError().c_str());
    }
  }
}
