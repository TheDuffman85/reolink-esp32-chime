#pragma once
#include <cstdint>

namespace chime {
constexpr uint32_t AudioSampleRate = 16000;
// Fixed-point phase keeps pitch independent of task scheduling. DAC samples
// are unsigned 8-bit audio centred on midscale, placed in I2S's high byte.
inline uint32_t audioPhaseStep(uint16_t hz) {
  return uint32_t((uint64_t(hz) << 32) / AudioSampleRate);
}
inline uint16_t dacSample(uint32_t phase, uint8_t level) {
  static const int8_t sine[32] = {0,25,49,71,90,106,117,125,127,125,117,106,90,71,49,25,
                               0,-25,-49,-71,-90,-106,-117,-125,-127,-125,-117,-106,-90,-71,-49,-25};
  return uint16_t(128 + int(sine[phase >> 27]) * level / 100) << 8;
}
inline bool dacPin(int pin) { return pin == 25 || pin == 26; }
}
