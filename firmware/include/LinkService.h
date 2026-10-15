#pragma once
#include <Arduino.h>
#include "Afc.h"
#include "Doorbells.h"
#include "Hardware.h"
#include "Relay.h"
#include "Settings.h"

// CC1101 reception/transmission and the doorbell links: software decoding,
// pairing/join through chime::Doorbells, doorbell rings and app requests.
// Slot 0 is the first doorbell (Settings); further slots are experimental
// additional doorbells (DoorbellStore). They share the chime settings, so the
// last name/volume/LED change from any doorbell's app wins.
extern Radio radio;
extern chime::Decoder decoder;
extern chime::Duplicates duplicates;
extern chime::Doorbells doorbells;
// Range extender (experimental, radioOptions.extender): frames between a paired
// doorbell and another chime, sent again when the link has nothing to send.
extern chime::Relay extender;

// Counters for the status API and serial diagnostics.
extern uint32_t transmitted, transmitErrors, radioResets, rawBytes;
extern uint32_t lastPacketAt, packetCount, crcTi, crcBad, crcCcitt, rings, lastRing;
extern uint32_t lastAppAt, appRequests;
extern uint8_t lastAppOpcode;

// Pairing sweep and polarity state; see serviceLink().
extern int8_t sweepTrim;
extern uint32_t sweepCombo; // Current pairing sweep step, if sweeping.
extern bool txPolarityKnown, txInverted, lastTxInverted, learnPending;

// Recent packets for GET /api/packets.
// epoch/epochMs: UTC receive time (0 while the clock is not set).
// offset: CC1101 FREQEST, about 1.587 kHz per step (the trim unit).
struct LogEntry { uint32_t id = 0, ms = 0, epoch = 0; uint16_t epochMs = 0; float rssi = -127;
                  int8_t offset = 0; bool offsetValid = false;
                  bool inverted = false; chime::Crc crc = chime::Crc::Bad;
                  String hex, result; };
constexpr size_t PacketLogSize = 32;
extern LogEntry packetLog[PacketLogSize];
extern size_t packetLogNext, packetLogSize;

// Time and RSSI of a doorbell's last valid frame, for the status API.
bool doorbellSeen(uint32_t id, uint32_t &at, float &rssi);

// Automatic trim (see Afc.h). afcAcquiring: wide RX after a long silence.
// trimSet: one estimate per doorbell; reception uses their midpoint.
extern RadioOptions radioOptions;
extern bool afcAcquiring;
extern chime::TrimSet trimSet;

void restartRadio();
// Clears automatic trim samples, e.g. after manual radio settings changed.
// estimates: also forget each doorbell's measured trim (call clearDoorbellTrims()
// with the radio paused to drop the stored ones as well).
void resetAfc(bool estimates = false);
bool clearDoorbellTrims();
// Applies a measured trim while the link is quiet; reacquires after silence.
void serviceAfc();
// Loads the doorbells from Settings and DoorbellStore; rejoin starts joins.
void initLink(bool rejoin);
// Pairs the doorbell in slot (replacing it), or adds one with slot == count().
bool startPairing(size_t slot = 0);
// Removes one or (from Settings) all doorbells; false if saving failed.
bool forgetDoorbell(size_t slot);
bool clearAdditionalDoorbells();
bool renameDoorbell(size_t slot, const char *name);
// The label, otherwise "Doorbell N"; empty for an unknown ID.
String doorbellName(size_t slot);
String doorbellNameFor(uint32_t id);
// Any doorbell holds a pairing key / every one of those has a session.
bool doorbellsPaired();
bool doorbellsJoined();
// A busy link's state (pairing/joining), otherwise the latest pairing slot's.
const char *linkState();
// Transmit trim for a slot: its own measurement when several doorbells are
// involved and automatic trim runs, otherwise the receive trim.
int8_t transmitTrim(size_t slot);
// Decodes the frames queued by the CC1101 RX task; bounded per call.
void receiveRadio();
// Queues app replies, advances the links and transmits; saves new profiles.
void serviceLink();
// Switches the range extender and saves the choice; false if NVS failed.
bool setExtender(bool on);
// Persists an Auto RX polarity once reception is quiet.
void saveLearnedPolarity();
// Once a second: reinitialises a CC1101 that lost its configuration.
void checkRadio();
