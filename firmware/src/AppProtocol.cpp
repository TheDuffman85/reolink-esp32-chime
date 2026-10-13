#include "AppProtocol.h"
#include "Protocol.h"
#include <cstring>

namespace chime {
AppResult appRequest(AppConfig &config, const uint8_t *request, size_t length,
                     uint8_t *reply, size_t capacity, uint32_t version) {
  AppResult result;
  if (!request || !length) return result;
  size_t expected = 0, response = 0;
  switch (request[0]) {
    case 0x0B: case 0x0C: expected = 4; break;
    case 0x0E: expected = 35; break;
    case 0x0F: expected = response = 36; break;
    case 0x13: expected = response = 56; break;
    case 0x14: expected = response = 20; break;
    case 0x15: expected = 56; response = 8; break;
    default: return result;
  }
  result.handled = true;
  if (length != expected || (response && (!reply || capacity < response))) return result;
  AppConfig next = config;
  switch (request[0]) {
    case 0x0B:
      if (request[1] > 4) return result;
      next.volume = request[1]; break;
    case 0x0C:
      if (request[1] > 1) return result;
      next.led = request[1]; break;
    case 0x0E:
      std::memcpy(next.name, request + 1, 32); next.name[32] = 0; break;
    case 0x15: {
      // Both fields are little-endian u32, despite their small valid ranges.
      const uint32_t led = read32le(request + 12), volume = read32le(request + 16);
      if (led > 1 || volume > 4) return result;
      next.led = static_cast<uint8_t>(led); next.volume = static_cast<uint8_t>(volume);
      std::memcpy(next.name, request + 24, 32); next.name[32] = 0; break;
    }
    default: break;
  }
  if (response) std::memmove(reply, request, response);
  switch (request[0]) {
    case 0x0F:
      reply[1] = 0; std::memcpy(reply + 4, config.name, 32); break;
    case 0x13:
      // +4 contains u16 status=0 and u16 config length=48, not a flag mask.
      write32le(reply + 4, 0x00300000);
      write32le(reply + 12, config.led); write32le(reply + 16, config.volume);
      std::memcpy(reply + 24, config.name, 32); break;
    case 0x14:
      write16le(reply + 4, 0); write16le(reply + 6, 20);
      write32le(reply + 8, version); std::memcpy(reply + 12, "CC13x2R1", 8); break;
    case 0x15: write16le(reply + 4, 0); break;
    default: break;
  }
  result.changed = next.volume != config.volume || next.led != config.led ||
    std::memcmp(next.name, config.name, sizeof(config.name)) != 0;
  config = next;
  result.valid = true; result.replyLength = response;
  return result;
}
}
