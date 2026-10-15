#include "LinkService.h"
#include <mbedtls/aes.h>
#include <esp_system.h>
#include <sys/time.h>
#include "App.h"
#include "AppProtocol.h"
#include "ChimeService.h"
#include "Identity.h"

Radio radio;
chime::Decoder decoder;
chime::Duplicates duplicates;
chime::Doorbells doorbells;
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
chime::TrimSet trimSet;
LogEntry packetLog[PacketLogSize];
size_t packetLogNext = 0, packetLogSize = 0;

namespace {
// V2 compatibility version: ASCII "00A2" as a little-endian word.
// This is separate from the ESP32 application's semantic firmware version.
constexpr uint32_t BootWord = 0x32413030;
// One pending application reply per doorbell, sent on its own session.
struct AppReply { uint32_t doorbell = 0; uint8_t bytes[chime::MaxMac - 16]{}; size_t length = 0; };
AppReply appReplies[chime::MaxDoorbells];
struct Seen { uint32_t id = 0, at = 0; float rssi = -127; };
Seen seen[chime::MaxDoorbells];
// The doorbell store as last saved, so unchanged stores are not rewritten.
DoorbellStore savedStore;
uint32_t sweepBase = 0;
uint8_t learnedCandidate = 0, learnedCount = 0;
bool afcPending = false; int8_t afcTarget = 0;
uint32_t afcFrameAt = 0, afcChangeAt = 0;
constexpr uint32_t AfcMinIntervalMs = 60000, AfcSilenceMs = 30UL * 60 * 1000;

void dropReply(uint32_t doorbell) {
  for (auto &reply : appReplies) if (reply.doorbell == doorbell) reply = AppReply{};
}
// Doorbell IDs the links talk to, including a pairing candidate.
size_t peers(uint32_t *ids) {
  size_t n = 0;
  for (size_t i = 0; i < doorbells.active(); ++i)
    if (const uint32_t peer = doorbells.link(i).peer()) ids[n++] = peer;
  return n;
}
// Drops estimates of forgotten doorbells and failed pairing candidates.
// Without any doorbell, frames collected under ID 0 stay.
void retainTrims() {
  uint32_t ids[chime::MaxDoorbells]; size_t n = peers(ids);
  if (!n) ids[n++] = 0;
  trimSet.retain(ids, n);
}

// Frequency estimates only from valid frames of a paired doorbell or the
// pairing candidate: the doorbell ID sits at MAC +8 in join responses and
// data frames, at +12 in a pairing response. Before any doorbell is known
// every valid frame counts, under ID 0.
bool afcSource(const chime::Packet &packet, uint32_t &id) {
  if (!chime::validPacket(packet)) return false;
  const uint8_t *mac = packet.bytes + 1;
  const size_t length = mac[0];
  const uint8_t type = mac[1] & 0x3F;
  id = 0;
  if (type == 1 && length == 32 && chime::read32le(mac + 4) == settings.nodeId) {
    id = chime::read32le(mac + 12);
    if (id && doorbells.pairing()) return true;
  } else if (type >= 2 && length >= 12) id = chime::read32le(mac + 8);
  uint32_t ids[chime::MaxDoorbells];
  const size_t n = peers(ids);
  for (size_t i = 0; i < n; ++i) if (ids[i] == id) return true;
  if (n) return false;
  id = 0;
  return true;
}
void trackFrequency(const chime::Packet &packet, const RxFrame &frame) {
  uint32_t id;
  if (!radioOptions.autoTrim || !frame.offsetValid || !afcSource(packet, id)) return;
  afcFrameAt = millis();
  if (id) trimSet.remove(0); // Unattributed frames no longer count.
  trimSet.add(id, frame.trim, frame.offset);
  int8_t target;
  if (trimSet.decide(settings.radio.trim, radioOptions.calibrated && !afcAcquiring, target) &&
      (!radioOptions.calibrated || afcAcquiring || uint32_t(millis() - afcChangeAt) >= AfcMinIntervalMs)) {
    afcTarget = target; afcPending = true;
  }
}
// Wide reception until the first calibration or after a long silence, while
// a new doorbell is being added (its offset is unknown), and when the
// doorbells' estimates are too far apart for the narrow filter.
bool wideReception() {
  return radioOptions.autoTrim && (!radioOptions.calibrated || afcAcquiring ||
    trimSet.spread() > chime::TrimSet::MaxSpread ||
    (doorbells.pairing() && doorbells.pairSlot() == doorbells.count()));
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

chime::LinkProfile settingsProfile() {
  chime::LinkProfile p;
  p.paired = settings.paired; p.heartbeatMode = settings.heartbeatMode;
  p.source = settings.source; p.destination = settings.destination;
  p.pair = settings.pair; memcpy(p.key, settings.key, sizeof(p.key));
  return p;
}
DoorbellStore buildStore() {
  DoorbellStore store;
  const size_t count = doorbells.count();
  store.count = uint8_t(count > 1 ? count - 1 : 0);
  for (size_t i = 0; i < count; ++i) {
    const auto &p = doorbells.link(i).profile();
    if (i) {
      StoredDoorbell &e = store.extra[i - 1];
      e.paired = p.paired; e.heartbeatMode = p.heartbeatMode; e.pair = p.pair;
      e.source = p.source; e.destination = p.destination; memcpy(e.key, p.key, sizeof(e.key));
    }
    strlcpy(store.names[i], doorbells.name(i), sizeof(store.names[i]));
    int8_t trim;
    if (trimSet.estimate(p.source, trim)) { store.trim[i] = trim; store.trimValid |= uint8_t(1u << i); }
  }
  return store;
}
// Writes slot 0 into Settings and the other slots into DoorbellStore.
bool saveDoorbells() {
  Settings next = settings;
  if (doorbells.count()) {
    const auto &p = doorbells.link(0).profile();
    next.profile = true; next.paired = p.paired; next.heartbeatMode = p.heartbeatMode;
    next.source = p.source; next.destination = p.destination;
    next.pair = p.pair; memcpy(next.key, p.key, sizeof(next.key));
  } else {
    next.profile = false; next.paired = false; next.heartbeatMode = 0;
    next.source = 0; next.destination = 0; next.pair = 0; memset(next.key, 0, sizeof(next.key));
  }
  const DoorbellStore store = buildStore();
  radio.pause();
  bool ok = !memcmp(&next, &settings, sizeof(next)) || saveSettings(next);
  if (ok && memcmp(&store, &savedStore, sizeof(store))) ok = saveDoorbellStore(store);
  restartRadio();
  if (!ok) { storageOk = false; return false; }
  settings = next; savedStore = store;
  return true;
}

String decodeRing(const chime::Packet &packet, float rssi) {
  chime::Ring ring;
  if (!chime::ringHeader(packet, ring)) return "log only";
  if (!doorbells.count()) return "ring header; no receive profile";
  int slot = -1;
  for (size_t i = 0; i < doorbells.count(); ++i) {
    const auto &p = doorbells.link(i).profile();
    if (p.source == ring.source && p.destination == ring.destination && p.pair == ring.pair) slot = int(i);
  }
  if (slot < 0) return "other device/session";
  const chime::Link &link = doorbells.link(slot);
  if (link.pairing()) return "ring ignored during pairing";
  if (link.profile().paired && !link.profileReady()) return "ring ignored until joined";
  uint8_t plain[4]; memcpy(plain, packet.bytes + 17, sizeof(plain));
  if (!crypt(nullptr, link.profile().key, plain, sizeof(plain))) return "AES error";
  if (!chime::ringPayload(plain, ring)) return "payload opcode differs (check key)";
  // Rejected before counting or playing, also while the ringtone is overridden.
  if (ring.parameter >= chime::DoorbellSounds) return String("unsupported music ID ") + ring.parameter;
  GongRequest request = gongRequest(chimeApp.volume, uint8_t(ring.parameter));
  if (duplicates.seen(ring, millis())) return "duplicate ring";
  lastRing = millis(); ++rings;
  const auto outcome = playChime(chime::RingSource::Doorbell, request, chimeApp.volume,
    0, ring.parameter, int8_t(constrain(lroundf(rssi), -127L, 0L)), false, ring.source);
  return String("ring parameter ") + ring.parameter + ", " + chime::RingOutcomeNames[uint8_t(outcome)] +
    (doorbells.count() > 1 ? ", " + doorbellName(slot) : String());
}
// Answers doorbell requests the way the vendor chime does: the request buffer
// is edited in place and sent back as an encrypted type-3 frame.
void handleAppRequest(uint32_t doorbell, const uint8_t *request, size_t length) {
  uint8_t reply[sizeof(AppReply::bytes)]{};
  const auto result = chime::appRequest(chimeApp, request, length,
    reply, sizeof(reply), BootWord);
  lastAppAt = millis(); lastAppOpcode = request[0]; ++appRequests;
  if (!result.handled) {
    if (request[0] != 9) logf("[app] opcode 0x%02X not handled\n", request[0]);
  } else if (!result.valid) {
    logf("[app] invalid request opcode 0x%02X (%u bytes)\n", request[0], unsigned(length));
  }
  if (result.changed) {
    // Shared by all doorbells: the latest change from any doorbell's app wins.
    logf("[app] config: name='%s' volume=%u led=%u\n", chimeApp.name, chimeApp.volume, chimeApp.led);
    saveChimeApp();
  }
  if (result.replyLength) {
    AppReply *slot = nullptr;
    for (auto &r : appReplies) if (r.length && r.doorbell == doorbell) slot = &r;
    for (auto &r : appReplies) if (!slot && !r.length) slot = &r;
    if (!slot) return;
    slot->doorbell = doorbell; memcpy(slot->bytes, reply, result.replyLength);
    slot->length = result.replyLength;
  }
}
void noteSeen(uint32_t id, float rssi) {
  Seen *slot = nullptr;
  for (auto &s : seen) if (s.id == id) slot = &s;
  // Otherwise replace the entry of a doorbell no longer paired, or the oldest.
  for (auto &s : seen) if (!slot && doorbells.find(s.id) < 0) slot = &s;
  if (!slot) { slot = &seen[0]; for (auto &s : seen) if (uint32_t(millis() - s.at) > uint32_t(millis() - slot->at)) slot = &s; }
  slot->id = id; slot->at = millis(); slot->rssi = rssi;
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
  if (packet.size >= 13 && chime::validPacket(packet) && doorbells.find(chime::read32le(packet.bytes + 9)) >= 0)
    noteSeen(chime::read32le(packet.bytes + 9), frame.rssi);
  size_t linkSlot = 0;
  const char *linkResult = doorbells.receive(packet, millis(), linkSlot);
  entry.result = linkResult ? String(linkResult) : decodeRing(packet, entry.rssi);
  logf("[rx] #%u %.1f dBm %+.1f kHz %s crc=%s len=%u type=%u: %s\n", packetCount, entry.rssi,
    frame.offset * 26000.0f / 16384.0f, packet.inverted ? "inv" : "norm", packet.crc==chime::Crc::Ti8005 ? "TI" :
    packet.crc==chime::Crc::CcittDiagnostic ? "CCITT" : "bad", unsigned(packet.size),
    packet.size > 2 ? unsigned(packet.bytes[2] & 0x3F) : 0u, entry.result.c_str());
  packetLogNext = (packetLogNext + 1) % PacketLogSize; if (packetLogSize < PacketLogSize) ++packetLogSize;
  // Application requests must belong to a joined doorbell and pass framing,
  // CRC and AES checks before they can mutate settings or elicit replies.
  for (size_t i = 0; i < doorbells.count(); ++i) {
    const chime::Link &link = doorbells.link(i);
    if (!link.profile().paired || !link.profileReady() ||
        (linkResult && linkSlot == i && !strcmp(linkResult, "stale data frame"))) continue;
    const auto &p = link.profile();
    uint8_t plain[chime::MaxMac];
    const size_t count = chime::dataPayload(packet, p.destination, p.source, p.pair, plain, sizeof(plain));
    if (count && crypt(nullptr, p.key, plain, count)) {
      logf("[app] seq=%u session=%04X%s plain(%u)=%s\n", chime::read32le(packet.bytes + 13), unsigned(p.pair),
        doorbells.count() > 1 ? (" " + doorbellName(i)).c_str() : "", unsigned(count), hexBytes(plain, count).c_str());
      handleAppRequest(p.source, plain, count);
      break;
    }
  }
}
}

bool doorbellSeen(uint32_t id, uint32_t &at, float &rssi) {
  for (const auto &s : seen) if (id && s.id == id) { at = s.at; rssi = s.rssi; return true; }
  return false;
}

void restartRadio() {
  resetDecoder();
  // The CC1101 matches one sync polarity; use a learned one when set to Auto.
  RadioSettings rf = settings.radio;
  if (rf.polarity == chime::Polarity::Auto && settings.learnedPolarity)
    rf.polarity = static_cast<chime::Polarity>(settings.learnedPolarity);
  // Until automatic trim has measured the crystal offset, listen wide
  // (packet engine only; Esp32 mode cannot measure and keeps the trim).
  radio.begin(rf, wideReception(), radioOptions.rxMode);
  sweepTrim = settings.radio.trim;
}

void resetAfc(bool estimates) {
  if (estimates) trimSet.reset(); else trimSet.restart();
  afcPending = false; afcAcquiring = false; afcFrameAt = millis();
}
bool clearDoorbellTrims() {
  if (!savedStore.trimValid) return true;
  DoorbellStore next = savedStore; next.trimValid = 0; memset(next.trim, 0, sizeof(next.trim));
  if (!saveDoorbellStore(next)) return false;
  savedStore = next;
  return true;
}

void serviceAfc() {
  if (!radioOptions.autoTrim || radioOptions.rxMode != RxMode::PacketEngine ||
      otaStarted || restartAt || doorbells.busy()) return;
  const uint32_t now = millis();
  retainTrims();
  int8_t midpoint;
  if (afcPending && uint32_t(now - lastPacketAt) > 250 && trimSet.midpoint(midpoint)) {
    // Applied while quiet so no frame or reply is cut off. Several doorbells
    // meet at the midpoint of their estimates (recomputed in case a pairing
    // candidate failed); each one's own trim is kept with the doorbells for
    // transmitting to it.
    afcPending = false; afcTarget = midpoint; trimSet.restart();
    const int8_t old = settings.radio.trim;
    Settings next = settings; next.radio.trim = afcTarget;
    RadioOptions nextOptions = radioOptions; nextOptions.calibrated = true;
    const DoorbellStore store = buildStore();
    radio.pause();
    bool saved = next.radio.trim == settings.radio.trim || saveSettings(next);
    if (saved && !radioOptions.calibrated) saved = saveRadioOptions(nextOptions);
    if (saved && memcmp(&store, &savedStore, sizeof(store))) {
      if (saveDoorbellStore(store)) savedStore = store; else storageOk = false;
    }
    if (saved) {
      settings = next; radioOptions = nextOptions; afcAcquiring = false; afcChangeAt = now;
      logf("[afc] trim %d -> %d (%+.1f kHz)%s\n", old, settings.radio.trim,
        settings.radio.trim * 26000.0f / 16384.0f,
        doorbells.count() > 1 ? (String(", midpoint of ") + doorbells.count() + " doorbells, spread " +
          trimSet.spread()).c_str() : "");
    } else {
      storageOk = false; logf("[afc] trim %d not saved: NVS write failed\n", afcTarget);
    }
    restartRadio();
    return;
  }
  // A long silence from the paired doorbells may mean the offset drifted out
  // of the narrow filter: listen wide again until the next estimate.
  if (radioOptions.calibrated && !afcAcquiring && doorbells.count() && uint32_t(now - afcFrameAt) > AfcSilenceMs) {
    afcAcquiring = true; trimSet.restart();
    logf("[afc] no doorbell frames for %u min; reacquiring with wide bandwidth\n",
      unsigned(AfcSilenceMs / 60000));
    radio.pause(); restartRadio();
    return;
  }
  // After adding a doorbell, or when the doorbells' spread changes sides of
  // the limit, switch the filter once reception is quiet.
  if (radio.healthy && radio.wide() != wideReception() && uint32_t(now - lastPacketAt) > 250) {
    logf("[afc] %s bandwidth%s\n", wideReception() ? "wide" : "normal",
      trimSet.spread() > chime::TrimSet::MaxSpread ? ": doorbell trims too far apart for the narrow filter" : "");
    radio.pause(); restartRadio();
  }
}

void initLink(bool rejoin) {
  DoorbellStore store; loadDoorbellStore(store);
  chime::LinkProfile profiles[chime::MaxDoorbells];
  size_t from[chime::MaxDoorbells], count = 0; // Index into store.names/trim.
  if (settings.profile && settings.source) { profiles[count] = settingsProfile(); from[count++] = 0; }
  // Additional doorbells: paired with this chime ID, each doorbell once.
  for (size_t i = 0; i < store.count && count < chime::MaxDoorbells; ++i) {
    const StoredDoorbell &e = store.extra[i];
    bool valid = e.paired && e.source && e.destination == settings.nodeId;
    for (size_t j = 0; j < count; ++j) if (profiles[j].source == e.source) valid = false;
    if (!valid) { logf("[link] stored doorbell %u ignored\n", unsigned(i + 2)); continue; }
    chime::LinkProfile &p = profiles[count];
    p = chime::LinkProfile{}; p.paired = true; p.heartbeatMode = e.heartbeatMode; p.pair = e.pair;
    p.source = e.source; p.destination = e.destination; memcpy(p.key, e.key, sizeof(p.key));
    from[count++] = i + 1;
  }
  doorbells.init(settings.nodeId, BootWord, crypt, nullptr, profiles, count);
  for (size_t i = 0; i < count; ++i) {
    store.names[from[i]][chime::DoorbellNameSize - 1] = 0;
    doorbells.setName(i, store.names[from[i]]);
    if (store.trimValid & (1u << from[i])) trimSet.seed(profiles[i].source, store.trim[from[i]]);
  }
  savedStore = store;
  for (auto &reply : appReplies) reply = AppReply{};
  retainTrims();
  if (rejoin) doorbells.startJoinAll(millis());
}
bool startPairing(size_t slot) {
  if (slot > doorbells.count() || slot >= chime::MaxDoorbells) return false;
  if (!storageOk || !radio.healthy || otaStarted || restartAt || doorbells.pairing()) return false;
  if (!chime::compatibleNodeId(settings.nodeId)) {
    // Linux alarm rules require a positive signed ID. Keep the old identity
    // through OTA/reboot; replace it only when pairing is explicitly started.
    // Every doorbell knew the old ID, so all of them are forgotten.
    Settings next = settings;
    do { next.nodeId = chime::nodeIdCandidate(esp_random()); } while (!next.nodeId);
    next.paired = false; next.profile = false; next.source = 0; next.destination = 0;
    next.pair = 0; next.heartbeatMode = 0; memset(next.key, 0, sizeof(next.key));
    doorbells.cancel(millis());
    radio.pause(); storageOk = saveSettings(next) && saveDoorbellStore(DoorbellStore{});
    restartRadio();
    if (!storageOk) return false;
    logf("[link] replacing incompatible chime ID %08X with %08X; remove old app entries\n",
      settings.nodeId, next.nodeId);
    settings = next; duplicates.reset(); initLink(false); slot = 0;
  }
  // A rejoin (after boot or a doorbell request) must not block new pairing;
  // an old doorbell that no longer knows the chime never answers it.
  if (doorbells.link(slot).busy()) logf("[link] rejoin cancelled for pairing\n");
  sweepBase = transmitted + transmitErrors; sweepCombo = UINT32_MAX; // Sweep restarts at step 1.
  const uint32_t old = slot < doorbells.count() ? doorbells.link(slot).profile().source : 0;
  if (!doorbells.startPair(slot, millis())) return false;
  if (old) dropReply(old);
  if (slot == doorbells.count()) logf("[link] pairing an additional doorbell (experimental)\n");
  // A new doorbell's offset is unknown: listen wide while adding it.
  if (radio.wide() != wideReception()) { radio.pause(); restartRadio(); }
  return true;
}
bool forgetDoorbell(size_t slot) {
  if (slot >= doorbells.count()) return false;
  if (doorbells.link(slot).busy()) doorbells.link(slot).cancel(millis());
  const uint32_t id = doorbells.link(slot).profile().source;
  doorbells.remove(slot);
  if (!saveDoorbells()) { initLink(true); return false; }
  doorbells.clearDirty(); trimSet.remove(id); dropReply(id); duplicates.forget(id);
  logf("[link] doorbell %08X forgotten\n", id);
  return true;
}
bool clearAdditionalDoorbells() {
  if (!savedStore.count && !savedStore.trimValid && !savedStore.names[0][0]) return true;
  if (!saveDoorbellStore(DoorbellStore{})) return false;
  savedStore = DoorbellStore{};
  return true;
}
bool renameDoorbell(size_t slot, const char *name) {
  if (slot >= doorbells.count()) return false;
  char old[chime::DoorbellNameSize]; strlcpy(old, doorbells.name(slot), sizeof(old));
  doorbells.setName(slot, name);
  if (saveDoorbells()) return true;
  doorbells.setName(slot, old);
  return false;
}
String doorbellName(size_t slot) {
  if (slot >= chime::MaxDoorbells) return String();
  return doorbells.name(slot)[0] ? String(doorbells.name(slot)) : "Doorbell " + String(unsigned(slot + 1));
}
String doorbellNameFor(uint32_t id) {
  const int slot = doorbells.find(id);
  return slot < 0 ? String() : doorbellName(size_t(slot));
}
bool doorbellsPaired() {
  for (size_t i = 0; i < doorbells.count(); ++i) if (doorbells.link(i).profile().paired) return true;
  return false;
}
bool doorbellsJoined() {
  bool any = false;
  for (size_t i = 0; i < doorbells.count(); ++i) {
    const chime::Link &link = doorbells.link(i);
    if (!link.profile().paired) continue;
    if (!link.profileReady()) return false;
    any = true;
  }
  return any;
}
const char *linkState() {
  for (size_t i = 0; i < doorbells.active(); ++i)
    if (doorbells.link(i).busy()) return doorbells.link(i).state();
  return doorbells.link(doorbells.pairSlot()).state();
}
int8_t transmitTrim(size_t slot) {
  if (doorbells.active() < 2 || !radioOptions.autoTrim || radioOptions.rxMode != RxMode::PacketEngine)
    return sweepTrim;
  int8_t trim;
  const uint32_t peer = doorbells.link(slot).peer();
  return peer && trimSet.known(peer, trim) ? trim : sweepTrim;
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
  for (auto &reply : appReplies) {
    if (!reply.length) continue;
    const int slot = doorbells.find(reply.doorbell);
    if (slot < 0) { reply = AppReply{}; continue; }
    if (doorbells.link(slot).sendData(reply.bytes, reply.length)) {
      logf("[app] reply opcode 0x%02X (%u bytes) queued\n", reply.bytes[0], unsigned(reply.length));
      reply = AppReply{};
    }
  }
  doorbells.tick(millis());
  size_t slot = 0;
  if (const auto *frame = doorbells.peek(millis(), slot)) {
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
    const bool discovering = strcmp(doorbells.link(slot).state(), "discovering") == 0;
    if (!txPolarityKnown && discovering) {
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
    const bool sent = radio.transmit(air, size, sweepCombo == UINT32_MAX ? transmitTrim(slot) : sweepTrim);
    if (sent) ++transmitted; else ++transmitErrors;
    decoder.reset(); doorbells.consume(slot, sent, millis());
  }
  if (doorbells.dirty()) {
    for (size_t i = 0; i < doorbells.count(); ++i)
      if (doorbells.link(i).profileDirty()) duplicates.forget(doorbells.link(i).profile().source);
    if (saveDoorbells()) {
      doorbells.clearDirty(); retainTrims();
    } else {
      initLink(false);
      logf("Pairing profile could not be saved; retry pairing/rejoin after fixing NVS\n");
    }
  }
}

void saveLearnedPolarity() {
  if (learnPending && uint32_t(millis()-lastPacketAt)>250 && !otaStarted && !doorbells.busy()) {
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
