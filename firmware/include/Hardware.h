#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <atomic>
#include "Protocol.h"
#include "ChimeControl.h"

// GPIO assignment, editable in the web UI and stored in NVS ("pins").
// Defaults are the owner-confirmed harness: blue SCK, purple MISO, green MOSI,
// white CSN, grey GDO0 on GPIO4 and the added GDO2 data wire on GPIO16.
// FIFO reception (1.0.7+) no longer reads GDO0/GDO2; they stay configured
// as inputs so existing harnesses and saved maps keep working.
// -1 marks an optional output/input that is not connected.
// audio occupies the former tone-output slot in the persisted GPIO map.
// PWM accepts any output GPIO; DAC audio requires GPIO25 or GPIO26.
// relayActiveLow: the relay switches on when its input is LOW, as on common
// optocoupler relay modules; stored under its own NVS key so the "map" blob
// keeps the layout older firmware reads.
struct PinMap {
  int8_t sck = 18, miso = 19, mosi = 23, cs = 5, clock = 4, data = 16;
  int8_t audio = 25, relay = 26, led = 2, button = 0;
  bool relayActiveLow = false;
};
extern PinMap pins;
extern uint32_t pinMapRevision;
enum class PinIssue : uint8_t { Info, Warning, Error };
// Checks a pin map against ESP32-WROOM-32 constraints. Calls report() for each
// finding with a role name ("sck", "relay", ...). Returns false on any error.
bool checkPins(const PinMap &map, void (*report)(void *, const char *role, PinIssue, const char *text),
               void *context);
bool loadPins(PinMap &map);
bool savePins(const PinMap &map);
struct RadioSettings { uint32_t hz = 868000000; int8_t trim = 0;
                       chime::Polarity polarity = chime::Polarity::Auto; };
// PacketEngine (default): the CC1101 detects the sync word and buffers whole
// bytes in its FIFO; a high-priority task drains it over SPI, so timing does
// not depend on the ESP32. Esp32: synchronous serial mode as up to 1.0.6, the
// ESP32 samples GDO2 data on each GDO0 clock edge in an interrupt (needs both
// wires; single bit slips were observed on the owner's harness).
enum class RxMode : uint8_t { PacketEngine = 0, Esp32 = 1 };
// One frame from the CC1101 RX FIFO, still whitened: the bytes after the
// 16-bit hardware sync 0x930B (software-checked 0x51DE, PHY length, MAC, CRC).
// Esp32 mode reports decoded frames with this metadata only (no bytes).
struct RxFrame {
  uint8_t bytes[chime::MaxMac + 5]{};
  uint8_t size = 0;
  bool inverted = false;
  int8_t offset = 0;  // FREQEST, about 1.587 kHz per step (same unit as trim)
  bool offsetValid = true; // False in Esp32 mode: no sync event to latch it
  int8_t trim = 0;    // FSCTRL0 trim in effect while the frame was received
  float rssi = -127;  // Latched while the frame is being received
};
struct Sample { uint8_t byte; uint32_t generation; };
class Radio {
 public:
  // wide: 162.5 kHz RX bandwidth (frequency acquisition before automatic
  // trim has calibrated) instead of the normal 81.25 kHz.
  bool begin(const RadioSettings &settings, bool wide = false, RxMode mode = RxMode::PacketEngine);
  bool wide() const { return wide_; }
  RxMode mode() const { return mode_; }
  void pause();
  // Pause under the radio lock, release SPI if needed, then replace the map.
  void remapPins(const PinMap &map);
  // Complete on-air bytes, including the software preamble, sync and CRC.
  // Frames above the 64-byte FIFO are streamed. Always resumes RX.
  bool transmit(const uint8_t *frame, size_t size);
  static constexpr size_t MaxTransmitSize = 255;
  // Changes FSCTRL0 (about 1.587 kHz per step) and recalibrates into RX.
  bool setTrim(int8_t trim);
  // False if the chip lost its RX configuration (e.g. a supply brown-out reset).
  bool configured();
  bool pop(RxFrame &frame);    // PacketEngine frames
  bool pop(Sample &sample);    // Esp32 raw bytes; generation marks discontinuities
  float rssi();
  uint8_t state();
  String lastError();
  bool healthy = false;
  String error; // Written under the radio lock; read it through lastError().
  uint8_t part = 0, version = 0;
  // FIFO overflows or a full frame queue; 16-bit sync matches without the
  // 0x51DE tail; impossible PHY lengths.
  static volatile uint32_t dropped, falseSyncs, invalidLengths;
 private:
  bool select();
  bool write(uint8_t address, uint8_t value);
  bool writeFifo(const uint8_t *bytes, size_t count);
  bool readFifo(uint8_t *bytes, size_t count);
  bool read(uint8_t address, uint8_t &value, bool status = false);
  bool readStable(uint8_t address, uint8_t &value);
  bool waitForState(uint8_t expected, uint32_t timeoutMs);
  bool beginLocked(const RadioSettings &settings, bool wide, RxMode mode);
  bool transmitLocked(const uint8_t *frame, size_t size);
  bool enterRx();
  bool restartRx();
  void serviceRx();
  bool strobe(uint8_t command);
  void lock() { xSemaphoreTakeRecursive(lock_, portMAX_DELAY); }
  void unlock() { xSemaphoreGiveRecursive(lock_); }
  static void rxTask(void *self);
  SemaphoreHandle_t lock_ = xSemaphoreCreateRecursiveMutex();
  QueueHandle_t frames_ = nullptr;
  TaskHandle_t task_ = nullptr;
  volatile bool receiving_ = false;
  bool inverted_ = false, wide_ = false, attached_ = false;
  RxMode mode_ = RxMode::PacketEngine;
  // Mode-dependent RX values of IOCFG2, IOCFG0, PKTCTRL0 and MDMCFG2.
  uint8_t iocfg2_ = 0x2E, iocfg0_ = 0x06, pktctrl0_ = 0x02, mdmcfg2_ = 0x12;
  static void IRAM_ATTR sampleBit();
  static QueueHandle_t samples_;
  static uint8_t octet_, bits_;
  static uint32_t generation_;
  int8_t trim_ = 0;
  uint8_t paTable_ = 0xC0;
  size_t expected_ = 0;
  RxFrame frame_;
};

// Persisted inside Settings; layout must stay unchanged.
struct GongSettings { uint8_t melody = 0, volume = 35; bool relay = false;
                      uint16_t relayMs = 300; };
// PWM/DAC: melodies for an external amplifier (optional parallel relay pulse).
// Relay: an existing (e.g. 8 V AC) gong driven by the relay with strike patterns.
// None: no output; rings are only logged and reported (MQTT/Home Assistant).
// Hardware backends retain legacy ring-log IDs. Public modes use GongKind.
enum class GongType : uint8_t { Pwm = 0, Relay = 1, None = 2, Dac = 3 };
inline GongType outputType(uint8_t mode, uint8_t audioOutput) {
  return mode == uint8_t(chime::GongKind::Audio) ?
    (audioOutput == uint8_t(chime::AudioOutput::Dac) ? GongType::Dac : GongType::Pwm) : static_cast<GongType>(mode);
}
constexpr uint8_t RelayPatterns = chime::Patterns;
struct GongRequest {
  GongSettings tone;
  GongType type = GongType::Relay;
  uint8_t pattern = 0;          // Same 0..9 slots as the doorbell music ID.
  uint16_t pulseMs = 300, gapMs = 700;
};
class Gong {
 public:
  bool begin();
  void configurePins();
  bool play(const GongRequest &request);
  void inhibit(bool value);
  bool active() const { return active_; }
 private:
  static void task(void *self);
  void melody(const GongRequest &request);
  void pattern(const GongRequest &request);
  void relay(bool on);
  bool wait(uint32_t ms);
  QueueHandle_t requests_ = nullptr;
  std::atomic<bool> inhibited_{false}, active_{false};
};
