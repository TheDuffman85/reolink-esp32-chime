#pragma once
#include <cstdint>

namespace chime {
// Doorbell Linux stores chime IDs as signed integers and tests id > 0 when
// selecting ring targets. Pairing accepts high-bit IDs but ringing does not.
constexpr bool compatibleNodeId(uint32_t id) {
  return id != 0 && id <= UINT32_C(0x7FFFFFFF);
}
// Zero remains a rejected candidate: the entropy caller must draw again.
constexpr uint32_t nodeIdCandidate(uint32_t entropy) {
  return entropy & UINT32_C(0x7FFFFFFF);
}
}
