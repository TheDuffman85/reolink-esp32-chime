#pragma once
#include <Arduino.h>
#include "Hardware.h"
#include "Link.h"
#include "Settings.h"

// CC1101 reception/transmission and the doorbell link: software decoding,
// pairing/join through chime::Link, doorbell rings and app requests.
extern Radio radio;
extern chime::Decoder decoder;
extern chime::Duplicates duplicates;
extern chime::Link rfLink;

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

// Automatic trim (see Afc.h). afcAcquiring: wide RX after a long silence.
extern RadioOptions radioOptions;
extern bool afcAcquiring;

void restartRadio();
// Clears automatic trim samples, e.g. after manual radio settings changed.
void resetAfc();
// Applies a measured trim while the link is quiet; reacquires after silence.
void serviceAfc();
void initLink(bool rejoin);
bool startPairing();
// Decodes the frames queued by the CC1101 RX task; bounded per call.
void receiveRadio();
// Queues app replies, advances the link and transmits; saves new profiles.
void serviceLink();
// Persists an Auto RX polarity once reception is quiet.
void saveLearnedPolarity();
// Once a second: reinitialises a CC1101 that lost its configuration.
void checkRadio();
