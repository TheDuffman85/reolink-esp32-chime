#include "Hardware.h"
#include "Band.h"
#include "Audio.h"
#include <driver/i2s.h>
#include <soc/gpio_struct.h>
#include <cstddef>
#include <Preferences.h>

PinMap pins;
uint32_t pinMapRevision = 0;
namespace {
// Read by the Esp32-mode clock ISR; set from pins.data in Radio::begin().
volatile uint32_t dataMask = 0;
volatile bool dataHigh = false;
}

volatile uint32_t Radio::dropped = 0, Radio::falseSyncs = 0, Radio::invalidLengths = 0;
QueueHandle_t Radio::samples_ = nullptr;
uint8_t Radio::octet_ = 0, Radio::bits_ = 0;
uint32_t Radio::generation_ = 0;

bool Radio::select() {
  // Ample headroom over 25 kbit/s RF without demanding fast edges from
  // jumper-wired modules.
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  digitalWrite(pins.cs, LOW);
  const uint32_t start = micros();
  while (digitalRead(pins.miso)) {
    if (uint32_t(micros() - start) > 3000) {
      digitalWrite(pins.cs, HIGH); SPI.endTransaction();
      error = "CC1101 CHIP_RDYn timeout; check 3.3 V and SPI wiring";
      healthy = false; return false;
    }
  }
  return true;
}
bool Radio::write(uint8_t address, uint8_t value) {
  if (!select()) return false;
  SPI.transfer(address); SPI.transfer(value);
  digitalWrite(pins.cs, HIGH); SPI.endTransaction(); return true;
}
bool Radio::writeFifo(const uint8_t *bytes, size_t count) {
  if (!select()) return false;
  SPI.transfer(0x7F); // TX FIFO burst write
  for (size_t i = 0; i < count; ++i) SPI.transfer(bytes[i]);
  digitalWrite(pins.cs, HIGH); SPI.endTransaction(); return true;
}
bool Radio::readFifo(uint8_t *bytes, size_t count) {
  if (!select()) return false;
  SPI.transfer(0xFF); // RX FIFO burst read
  for (size_t i = 0; i < count; ++i) bytes[i] = SPI.transfer(0);
  digitalWrite(pins.cs, HIGH); SPI.endTransaction(); return true;
}
bool Radio::read(uint8_t address, uint8_t &value, bool status) {
  if (!select()) return false;
  SPI.transfer(address | (status ? 0xC0 : 0x80)); value = SPI.transfer(0);
  digitalWrite(pins.cs, HIGH); SPI.endTransaction(); return true;
}
bool Radio::strobe(uint8_t command) {
  if (!select()) return false;
  SPI.transfer(command); digitalWrite(pins.cs, HIGH); SPI.endTransaction(); return true;
}
bool Radio::readStable(uint8_t address, uint8_t &value) {
  // TI's SPI synchronization erratum applies to live status registers.
  // Require two consecutive identical reads, with a bounded retry count.
  uint8_t previous;
  if (!read(address, previous, true)) return false;
  for (uint8_t attempt = 0; attempt < 8; ++attempt) {
    if (!read(address, value, true)) return false;
    if (value == previous) return true;
    previous = value;
  }
  error = "CC1101 status register did not stabilize";
  return false;
}
bool Radio::waitForState(uint8_t expected, uint32_t timeoutMs) {
  const uint32_t start = millis();
  do {
    uint8_t value;
    if (!readStable(0x35, value)) return false;
    if ((value & 0x1F) == expected) return true;
    delay(1);
  } while (uint32_t(millis() - start) < timeoutMs);
  error = "CC1101 state transition timed out";
  return false;
}
bool Radio::enterRx() {
  frame_.size = 0; expected_ = 0;
  if (samples_) xQueueReset(samples_);
  bits_ = 0; octet_ = 0; ++generation_;
  if (!strobe(0x3A) || !strobe(0x34) || !waitForState(0x0D, 100)) return false; // SFRX, SRX
  healthy = true;
  receiving_ = true;
  if (mode_ == RxMode::Esp32) { attachInterrupt(pins.clock, sampleBit, RISING); attached_ = true; }
  return true;
}
void IRAM_ATTR Radio::sampleBit() {
  const uint32_t level = dataHigh ? GPIO.in1.val : GPIO.in;
  octet_ = (octet_ << 1) | ((level & dataMask) ? 1 : 0);
  if (++bits_ < 8) return;
  const Sample sample{octet_, generation_};
  BaseType_t woke = pdFALSE;
  if (xQueueSendFromISR(samples_, &sample, &woke) != pdTRUE) {
    ++dropped; ++generation_;
  }
  bits_ = 0; octet_ = 0;
  if (woke) portYIELD_FROM_ISR();
}
// Re-arms sync search after a frame, a false sync or a FIFO overflow.
// MCSM0 recalibrates on IDLE -> RX (about 0.8 ms).
bool Radio::restartRx() {
  frame_.size = 0; expected_ = 0;
  if (strobe(0x36) && waitForState(0x01, 100) && strobe(0x3A) && strobe(0x34) &&
      waitForState(0x0D, 100)) return true;
  healthy = false; receiving_ = false; // checkRadio() reinitialises.
  return false;
}
void Radio::rxTask(void *self) {
  Radio &radio = *static_cast<Radio *>(self);
  for (;;) {
    // 1 ms polling: the FIFO holds 64 bytes, about 20 ms at 25 kbit/s.
    vTaskDelay(1);
    radio.lock();
    if (radio.receiving_ && radio.healthy && radio.mode_ == RxMode::PacketEngine) radio.serviceRx();
    radio.unlock();
  }
}
void Radio::serviceRx() {
  uint8_t status;
  if (!readStable(0x3B, status)) { healthy = false; receiving_ = false; return; } // RXBYTES
  if (status & 0x80) { ++dropped; restartRx(); return; }
  // RX FIFO erratum: never read the last byte while the chip may be writing it.
  // Infinite length mode keeps bytes arriving, so the frame's last byte follows.
  size_t available = (status & 0x7F) > 1 ? (status & 0x7F) - 1 : 0;
  while (available) {
    const size_t target = frame_.size < 3 ? 3 : expected_;
    const size_t count = available < target - frame_.size ? available : target - frame_.size;
    if (!readFifo(frame_.bytes + frame_.size, count)) { healthy = false; receiving_ = false; return; }
    frame_.size += count; available -= count;
    if (frame_.size == 3 && !expected_) {
      // The hardware matched 0x930B; the rest of the 32-bit sync must follow.
      const uint8_t flip = inverted_ ? 0xFF : 0x00;
      if (uint8_t(frame_.bytes[0] ^ flip) != 0x51 || uint8_t(frame_.bytes[1] ^ flip) != 0xDE) {
        ++falseSyncs; restartRx(); return;
      }
      const uint8_t length = frame_.bytes[2] ^ flip ^ 0xFF; // First PN9 byte is 0xFF.
      if (length < 2 || length > chime::MaxMac) { ++invalidLengths; restartRx(); return; }
      expected_ = 2 + 1 + length + 2; // Sync tail, PHY length, MAC, CRC.
      // The carrier is present now: latch signal strength and frequency offset.
      uint8_t rssi, offset;
      frame_.rssi = readStable(0x34, rssi) ? int8_t(rssi) / 2.0f - 74.0f : -127;
      frame_.offset = readStable(0x32, offset) ? int8_t(offset) : 0;
      frame_.trim = trim_;
    }
    if (expected_ && frame_.size == expected_) {
      frame_.inverted = inverted_;
      if (xQueueSend(frames_, &frame_, 0) != pdTRUE) ++dropped;
      restartRx(); return;
    }
  }
}
void Radio::pause() {
  lock();
  if (attached_) { detachInterrupt(pins.clock); attached_ = false; }
  receiving_ = false;
  if (healthy) strobe(0x36); // SIDLE
  frame_.size = 0; expected_ = 0;
  if (samples_) xQueueReset(samples_);
  bits_ = 0; octet_ = 0; ++generation_;
  unlock();
}
void Radio::remapPins(const PinMap &map) {
  lock(); pause();
  const bool radioChanged=memcmp(&pins,&map,offsetof(PinMap,audio))!=0;
  if (radioChanged) {
    SPI.end();
    const int8_t oldPins[]={pins.sck,pins.miso,pins.mosi,pins.cs,pins.clock,pins.data};
    for (int8_t pin:oldPins) if (pin>=0) pinMode(pin,INPUT);
  }
  if (frames_) xQueueReset(frames_);
  pins=map;
  unlock();
}
bool Radio::begin(const RadioSettings &settings, bool wide, RxMode mode) {
  lock();
  const bool ok = beginLocked(settings, wide, mode);
  unlock();
  return ok;
}
bool Radio::beginLocked(const RadioSettings &settings, bool wide, RxMode mode) {
  pause();
  healthy = false; error = "";
  if (!frames_) frames_ = xQueueCreate(8, sizeof(RxFrame));
  if (mode == RxMode::Esp32 && !samples_) samples_ = xQueueCreate(1024, sizeof(Sample));
  if (!frames_ || (mode == RxMode::Esp32 && !samples_)) { error = "RX queue allocation failed"; return false; }
  if (!task_ && xTaskCreatePinnedToCore(rxTask, "cc1101-rx", 3072, this, 3, &task_, 1) != pdPASS) {
    task_ = nullptr; error = "RX task creation failed"; return false;
  }
  pinMode(pins.cs, OUTPUT); digitalWrite(pins.cs, HIGH);
  pinMode(pins.miso, INPUT); pinMode(pins.clock, INPUT); pinMode(pins.data, INPUT);
  dataHigh = pins.data >= 32; dataMask = 1UL << (pins.data & 31);
  SPI.begin(pins.sck, pins.miso, pins.mosi, pins.cs);
  // TI reset sequence; select() checks CHIP_RDYn before and after SRES.
  digitalWrite(pins.cs, LOW); delayMicroseconds(10);
  digitalWrite(pins.cs, HIGH); delayMicroseconds(50);
  if (!strobe(0x30)) return false;
  delay(1);
  if (!read(0x30, part, true) || !read(0x31, version, true)) return false;
  if (part != 0 || version == 0 || version == 0xFF) {
    error = "CC1101 identification failed"; return false;
  }
  // PacketEngine: FIFO packet mode. The CC1101's 32-bit sync repeats one 16-bit word, so
  // the hardware matches the first half (0x930B) and serviceRx() checks the
  // second (0x51DE). Infinite length; no native whitening, CRC, address
  // filtering, Manchester or FEC: chime::Decoder does those in software.
  // GDO0 = sync received / end of packet (diagnostic only), GDO2 unused.
  // Esp32: synchronous serial mode, GDO0 = clock (0x0B), GDO2 = data (0x0C),
  // no sync detection (MDMCFG2 SYNC_MODE 0); the ISR samples every bit.
  // MDMCFG4 0xD9: 81.25 kHz RX bandwidth (was 0xC9, 101.6 kHz) for less
  // noise; fits the ~55 kHz GFSK signal once automatic trim has centred it.
  // 0x99 (162.5 kHz, FOC range ±40 kHz) hears an uncorrected crystal while
  // automatic trim acquires the offset, at about 3 dB lower sensitivity.
  // Normal polarity unless Inverted is set: the bench never saw inverted RX.
  inverted_ = settings.polarity == chime::Polarity::Inverted;
  mode_ = mode; wide_ = wide && mode == RxMode::PacketEngine; trim_ = settings.trim;
  paTable_ = chime::paTable(chime::bandOf(settings.hz));
  const bool engine = mode == RxMode::PacketEngine;
  iocfg2_ = engine ? 0x2E : 0x0C; iocfg0_ = engine ? 0x06 : 0x0B;
  pktctrl0_ = engine ? 0x02 : 0x12; mdmcfg2_ = engine ? 0x12 : 0x10;
  const uint8_t flip = inverted_ ? 0xFF : 0x00;
  const uint8_t registers[][2] = {
    {0x00,iocfg2_},{0x01,0x2E},{0x02,iocfg0_},{0x03,0x47},
    {0x04,uint8_t(0x93 ^ flip)},{0x05,uint8_t(0x0B ^ flip)},
    {0x06,0xFF},{0x07,0x00},{0x08,pktctrl0_},{0x09,0},{0x0A,0},
    {0x0B,0x06},{0x10,uint8_t(wide_ ? 0x99 : 0xD9)},{0x11,0xF8},{0x12,mdmcfg2_},
    {0x13,0x22},{0x14,0xF8},{0x15,0x31},{0x16,0x07},
    {0x17,0x0C},{0x18,0x18},{0x19,0x16},{0x1A,0x6C},
    {0x1B,0x43},{0x1C,0x40},{0x1D,0x91},{0x21,0x56},
    {0x22,0x10},{0x23,0xE9},{0x24,0x2A},{0x25,0},{0x26,0x1F},
    {0x2C,0x81},{0x2D,0x35},{0x2E,0x09}
  };
  for (const auto &reg : registers) if (!write(reg[0], reg[1])) return false;
  const uint32_t word = (uint64_t(settings.hz) * 65536 + 13000000) / 26000000;
  if (!write(0x0C, uint8_t(settings.trim)) || !write(0x0D, word >> 16) ||
      !write(0x0E, word >> 8) || !write(0x0F, word)) return false;
  uint8_t verify;
  if (!read(0x08, verify) || verify != pktctrl0_) {
    error = "CC1101 register verification failed"; return false;
  }
  return enterRx(); // MCSM0 auto-calibrates IDLE -> RX.
}
bool Radio::transmit(const uint8_t *frame, size_t size) {
  lock();
  const bool sent = transmitLocked(frame, size, trim_);
  unlock();
  return sent;
}
bool Radio::transmit(const uint8_t *frame, size_t size, int8_t trim) {
  lock();
  const bool sent = transmitLocked(frame, size, trim);
  unlock();
  return sent;
}
bool Radio::transmitLocked(const uint8_t *frame, size_t size, int8_t trim) {
  if (!frame || !size || size > MaxTransmitSize) {
    error = "TX frame must contain 1 to 255 on-air bytes";
    return false;
  }
  if (!healthy || !receiving_) {
    error = "CC1101 is not receiving";
    return false;
  }
  error = "";
  pause();
  const auto send = [&]() {
    if (!healthy || !waitForState(0x01, 100) || !strobe(0x3B)) return false;
    // Normal FIFO, fixed length, no native whitening/CRC. SYNC_MODE zero:
    // TI specifies that this disables both preamble and sync transmission.
    // The complete software-generated waveform goes in FIFO.
    // PATABLE[0] per band (Band.h): about +11 dBm at 868 MHz, 0 dBm at
    // 902-928 MHz; FREND0.PA_POWER=0.
    // FSCTRL0 takes effect with the IDLE -> TX calibration (MCSM0 FS_AUTOCAL).
    if (!write(0x00, 0x2E) || !write(0x02, 0x2E) || !write(0x12, 0x10) ||
        !write(0x06, uint8_t(size)) || !write(0x08, 0x00) ||
        !write(0x17, 0x00) || !write(0x3E, paTable_) ||
        (trim != trim_ && !write(0x0C, uint8_t(trim)))) return false;
    size_t written = size < 64 ? size : 64;
    if (!writeFifo(frame, written)) return false;
    if (!strobe(0x35)) return false; // STX; TXOFF_MODE returns to IDLE.
    const uint32_t start = millis();
    do {
      uint8_t count, state;
      if (!readStable(0x3A, count) || !readStable(0x35, state)) return false;
      if ((count & 0x80) || (state & 0x1F) == 0x16) {
        error = "CC1101 TX FIFO underflow";
        return false;
      }
      if (written < size) {
        // Refill without sleeping: one byte leaves the FIFO every 320 us.
        const size_t room = 64 - (count & 0x7F), left = size - written;
        const size_t chunk = room < left ? room : left;
        if (chunk && !writeFifo(frame + written, chunk)) return false;
        written += chunk;
        continue;
      }
      // FIFO can become empty before the final byte finishes transmitting.
      // Require IDLE as well, rather than treating an empty FIFO as success.
      if ((count & 0x7F) == 0 && (state & 0x1F) == 0x01) return true;
      delay(1);
    } while (uint32_t(millis() - start) < 150);
    error = "CC1101 TX completion timed out";
    return false;
  };
  const bool sent = send();
  const String txError = error;
  healthy = false;
  // Restore all changed receive registers even after a failed SPI/TX step.
  // Flush only after IDLE, as required by the CC1101 state machine.
  bool restored = strobe(0x36) && waitForState(0x01, 100);
  if (restored) restored = strobe(0x3B);
  const uint8_t rxRegisters[][2] = {
    {0x00, iocfg2_}, {0x02, iocfg0_}, {0x06, 0xFF}, {0x08, pktctrl0_}, {0x12, mdmcfg2_}, {0x17, 0x0C},
    {0x0C, uint8_t(trim_)}
  };
  for (const auto &reg : rxRegisters) restored = write(reg[0], reg[1]) && restored;
  if (restored) restored = enterRx();
  if (!restored) {
    const String restoreError = error;
    strobe(0x36); // Best effort: leave a failed recovery out of TX.
    error = "CC1101 RX recovery failed: " + restoreError;
    return false;
  }
  error = sent ? String() : txError;
  return sent;
}
bool Radio::setTrim(int8_t trim) {
  lock();
  bool ok = healthy;
  if (ok) {
    pause();
    if (!waitForState(0x01, 100) || !write(0x0C, uint8_t(trim))) { healthy = false; ok = false; }
    else { trim_ = trim; ok = enterRx(); }
  }
  unlock();
  return ok;
}
bool Radio::configured() {
  lock();
  uint8_t iocfg0 = 0, pktctrl0 = 0, state = 0;
  // A failed begin/recovery can leave correct registers while RX polling is
  // disabled. Keep retrying until both the driver and the chip are ready.
  const bool ok = healthy && receiving_ && read(0x02, iocfg0) &&
    read(0x08, pktctrl0) && readStable(0x35, state);
  unlock();
  // 0x11 is an RX FIFO overflow that serviceRx() is about to clear.
  return ok && iocfg0 == iocfg0_ && pktctrl0 == pktctrl0_ &&
    ((state & 0x1F) == 0x0D || (mode_ == RxMode::PacketEngine && (state & 0x1F) == 0x11));
}
bool Radio::pop(RxFrame &frame) { return frames_ && xQueueReceive(frames_, &frame, 0) == pdTRUE; }
bool Radio::pop(Sample &sample) { return samples_ && xQueueReceive(samples_, &sample, 0) == pdTRUE; }
float Radio::rssi() {
  lock();
  uint8_t value;
  const bool ok = healthy && read(0x34, value, true);
  unlock();
  return ok ? int8_t(value) / 2.0f - 74.0f : -127; // Approximate; 868 MHz offset needs bench check.
}
uint8_t Radio::state() {
  lock();
  uint8_t value = 0xFF;
  if (healthy) read(0x35, value, true);
  unlock();
  return value & 0x1F;
}
String Radio::lastError() {
  lock();
  const String copy = error;
  unlock();
  return copy;
}

void Gong::configurePins() {
  // Set the idle level before enabling the output so the relay does not blip.
  if (pins.relay >= 0) { digitalWrite(pins.relay, pins.relayActiveLow ? HIGH : LOW); pinMode(pins.relay, OUTPUT); }
  if (pins.audio >= 0) { digitalWrite(pins.audio, LOW); pinMode(pins.audio, OUTPUT); }
}
bool Gong::begin() {
  configurePins();
  requests_ = xQueueCreate(1, sizeof(GongRequest));
  if (!requests_) return false;
  if (xTaskCreate(task, "gong", 3072, this, 2, nullptr) != pdPASS) {
    vQueueDelete(requests_); requests_ = nullptr; return false;
  }
  return true;
}
bool Gong::play(const GongRequest &request) {
  if ((request.type == GongType::Relay && pins.relay < 0) ||
      ((request.type == GongType::Pwm || request.type == GongType::Dac) && pins.audio < 0) ||
      (request.type == GongType::Dac && !chime::dacPin(pins.audio)) ||
      request.tone.melody >= chime::Melodies || request.pattern >= chime::Patterns ||
      request.tone.volume > 100) return false;
  return requests_ && !inhibited_ && xQueueSend(requests_, &request, 0) == pdTRUE;
}
void Gong::inhibit(bool value) {
  inhibited_ = value;
  if (value && requests_) {
    xQueueReset(requests_);
    // Give the worker a full tick to observe inhibition before flash writes.
    delay(10);
    while (active_) delay(1);
  }
}
void Gong::relay(bool on) { if (pins.relay >= 0) digitalWrite(pins.relay, on != pins.relayActiveLow ? HIGH : LOW); }
bool Gong::wait(uint32_t ms) {
  // Sleeps in small steps so inhibition (OTA, flash writes) stops output quickly.
  const uint32_t start = millis();
  while (uint32_t(millis() - start) < ms) {
    if (inhibited_) return false;
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  return !inhibited_;
}
void Gong::melody(const GongRequest &request) {
  const GongSettings &settings = request.tone;
  // Keep the optional relay pulse when audio is muted, without enabling the
  // DAC and producing a DC transition at the amplifier input.
  if (!settings.volume) {
    if (settings.relay) { relay(true); wait(settings.relayMs); relay(false); }
    return;
  }
  const bool dac = request.type == GongType::Dac;
  if (dac) {
    i2s_config_t config{};
    config.mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN);
    config.sample_rate = chime::AudioSampleRate;
    config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    config.communication_format = I2S_COMM_FORMAT_STAND_MSB;
    config.dma_buf_count = 2; config.dma_buf_len = 80;
    // Clear DMA underruns so old audio is not replayed.
    config.tx_desc_auto_clear = true;
    if (i2s_driver_install(I2S_NUM_0, &config, 0, nullptr) != ESP_OK) {
      Serial.println("DAC audio driver unavailable"); return;
    }
    // Enable only the selected DAC. GPIO26 may still drive the relay.
    if (i2s_set_dac_mode(pins.audio == 25 ? I2S_DAC_CHANNEL_RIGHT_EN : I2S_DAC_CHANNEL_LEFT_EN) != ESP_OK) {
      i2s_driver_uninstall(I2S_NUM_0); return;
    }
  } else {
    ledcSetup(0, 1000, 8); ledcAttachPin(pins.audio, 0); ledcWrite(0, 0);
  }
  const chime::Note *notes = chime::melodyScore(settings.melody);
  const auto hasNote = [&](int i) { return i < chime::MaxNotes && notes[i].ms != 0; };
  const uint32_t start = millis();
  uint32_t noteStart = start, phase = 0;
  int note = 0;
  bool relayOn = settings.relay;
  relay(relayOn);
  const auto tone = [&]() {
    if (dac) return;
    if (hasNote(note) && notes[note].hz && settings.volume) {
      ledcWriteTone(0, notes[note].hz);
      ledcWrite(0, 128 * settings.volume / 100);
    } else ledcWrite(0, 0);
  };
  tone();
  while (!inhibited_) {
    const uint32_t now = millis();
    if (relayOn && uint32_t(now - start) >= settings.relayMs) { relay(false); relayOn = false; }
    if (hasNote(note) && uint32_t(now - noteStart) >= notes[note].ms) {
      ++note; noteStart = now; tone();
    }
    if (!hasNote(note) && !relayOn) break;
    if (dac) {
      // Five milliseconds of stereo samples; only the selected DAC is enabled.
      // Blocking DMA writes pace playback without a high-frequency software ISR.
      uint16_t samples[80 * 2];
      const uint16_t hz = hasNote(note) ? notes[note].hz : 0;
      const uint32_t step = chime::audioPhaseStep(hz);
      for (size_t i = 0; i < 80; ++i) {
        samples[2*i] = samples[2*i+1] = chime::dacSample(phase, hz ? settings.volume : 0);
        phase += step;
      }
      size_t written = 0;
      if (i2s_write(I2S_NUM_0, samples, sizeof(samples), &written, pdMS_TO_TICKS(20)) != ESP_OK ||
          written != sizeof(samples)) break;
    } else vTaskDelay(pdMS_TO_TICKS(5));
  }
  if (dac) {
    i2s_stop(I2S_NUM_0);
    i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE);
    i2s_driver_uninstall(I2S_NUM_0);
  } else { ledcWrite(0, 0); ledcDetachPin(pins.audio); }
  // Restore a quiet output after playback or inhibition.
  digitalWrite(pins.audio, LOW); pinMode(pins.audio, OUTPUT);
}
void Gong::pattern(const GongRequest &request) {
  // Each strike energises the gong coil for pulseMs. On a classic two-tone
  // gong, energising strikes "ding" and releasing lets the plunger hit "dong".
  const auto score = chime::strikeScore(request.pattern, request.pulseMs, request.gapMs);
  for (uint8_t i = 0; i < score.count; ++i) {
    relay(true); const bool ok = wait(score.onMs); relay(false);
    if (!ok || (i + 1 < score.count && !wait(score.gaps[i]))) return;
  }
}
void Gong::task(void *self) {
  auto &gong = *static_cast<Gong *>(self);
  // Timed output runs independently of HTTP, radio parsing, and flash writes.
  while (true) {
    GongRequest request;
    if (xQueueReceive(gong.requests_, &request, portMAX_DELAY) != pdTRUE) continue;
    if (gong.inhibited_) continue;
    gong.active_ = true;
    if (request.type == GongType::Relay) gong.pattern(request);
    else if (request.type == GongType::Pwm || request.type == GongType::Dac) gong.melody(request);
    gong.relay(false); gong.active_ = false;
  }
}

// ---- GPIO assignment ------------------------------------------------------
namespace {
struct Role { const char *name; int8_t PinMap::*pin; bool output, optional, pullup; };
const Role Roles[] = {
  {"sck", &PinMap::sck, true, false, false},   {"miso", &PinMap::miso, false, false, false},
  {"mosi", &PinMap::mosi, true, false, false}, {"cs", &PinMap::cs, true, false, false},
  {"clock", &PinMap::clock, false, false, false}, {"data", &PinMap::data, false, false, false},
  {"audio", &PinMap::audio, true, true, false}, {"relay", &PinMap::relay, true, true, false},
  {"led", &PinMap::led, true, true, false},    {"button", &PinMap::button, false, true, true},
};
bool exists(int pin) {
  return (pin >= 0 && pin <= 5) || (pin >= 12 && pin <= 19) || (pin >= 21 && pin <= 23) ||
    (pin >= 25 && pin <= 27) || (pin >= 32 && pin <= 39) || (pin >= 6 && pin <= 11);
}
}
bool checkPins(const PinMap &map, void (*report)(void *, const char *, PinIssue, const char *),
               void *context) {
  bool ok = true;
  const auto say = [&](const char *role, PinIssue level, const char *text) {
    if (level == PinIssue::Error) ok = false;
    if (report) report(context, role, level, text);
  };
  for (const auto &role : Roles) {
    const int pin = map.*(role.pin);
    if (pin < 0) {
      if (!role.optional) say(role.name, PinIssue::Error, "Required: the radio does not work without this connection.");
      continue;
    }
    if (!exists(pin)) { say(role.name, PinIssue::Error, "This GPIO does not exist on the ESP32."); continue; }
    if (pin >= 6 && pin <= 11) { say(role.name, PinIssue::Error, "GPIO6-11 connect to the module's internal flash; using them crashes the ESP32."); continue; }
    if (pin == 1 || pin == 3) { say(role.name, PinIssue::Error, "GPIO1/3 are the USB serial port (flashing and log console)."); continue; }
    if (pin >= 34 && role.output) { say(role.name, PinIssue::Error, "GPIO34-39 are input-only and cannot drive this output."); continue; }
    if (pin >= 34 && role.pullup) say(role.name, PinIssue::Warning, "GPIO34-39 have no internal pull-up; add an external 10 kOhm pull-up to 3.3 V.");
    for (const auto &other : Roles) {
      if (&other == &role) break;
      if (map.*(other.pin) == pin) { say(role.name, PinIssue::Error, "Already used by another function."); break; }
    }
    switch (pin) {
      case 0: if (strcmp(role.name, "button")) say(role.name, PinIssue::Warning, "GPIO0 selects the boot mode and is wired to the BOOT button; it must be high at reset."); break;
      case 2: if (!role.output) say(role.name, PinIssue::Warning, "GPIO2 is a boot strap pin; a radio output driving it high can block USB flashing.");
              else if (strcmp(role.name, "led")) say(role.name, PinIssue::Info, "GPIO2 also drives the on-board LED on most boards.");
              break;
      case 5: if (strcmp(role.name, "cs")) say(role.name, PinIssue::Warning, "GPIO5 is a boot strap pin that must be high at reset.");
              else say(role.name, PinIssue::Info, "Boot strap pin: an optional 10 kOhm pull-up to 3.3 V can keep CS high at reset.");
              break;
      case 12: say(role.name, PinIssue::Warning, "GPIO12 sets the flash voltage at reset; if it is pulled high the module will not boot."); break;
      case 15: say(role.name, PinIssue::Info, "GPIO15 is a boot strap pin (boot messages); usually harmless."); break;
      case 16: case 17: say(role.name, PinIssue::Info, "On WROVER modules GPIO16/17 belong to PSRAM; fine on WROOM-32."); break;
      default: break;
    }
    if (!strcmp(role.name, "relay") && map.relayActiveLow &&
        (pin == 0 || pin == 2 || pin == 5 || pin == 12 || pin == 15))
      say(role.name, PinIssue::Warning, "Active-low relay modules pull their input toward 5 V; avoid boot strap pins.");
  }
  return ok;
}
// The "map" blob holds the ten GPIO numbers only (layout up to 1.0.19).
constexpr size_t PinBlobSize = offsetof(PinMap, relayActiveLow);
static_assert(PinBlobSize == 10, "PinMap GPIO layout is persisted");
bool loadPins(PinMap &map) {
  Preferences prefs; PinMap stored;
  if (!prefs.begin("pins", true)) { map = PinMap{}; return false; }
  const bool found = prefs.getBytesLength("map") == PinBlobSize &&
    prefs.getBytes("map", &stored, PinBlobSize) == PinBlobSize;
  stored.relayActiveLow = prefs.getBool("relaylow", false);
  prefs.end();
  // Invalid stored maps fall back to the defaults so the chime always boots.
  map = found && checkPins(stored, nullptr, nullptr) ? stored : PinMap{};
  return found;
}
bool savePins(const PinMap &map) {
  if (!checkPins(map, nullptr, nullptr)) return false;
  Preferences prefs;
  if (!prefs.begin("pins", false)) return false;
  const bool ok = prefs.putBytes("map", &map, PinBlobSize) == PinBlobSize &&
    prefs.putBool("relaylow", map.relayActiveLow);
  prefs.end(); return ok;
}
