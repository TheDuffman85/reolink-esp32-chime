#pragma once
#include <cstddef>
#include <cstdint>

namespace chime {
struct AppConfig {
  char name[33] = "Reolink Chime";
  uint8_t volume = 4, led = 1;
};
struct AppResult {
  bool handled = false;
  bool valid = false;
  bool changed = false;
  size_t replyLength = 0;
};
// Plaintext application requests, after CRC, profile and AES validation.
// Replies retain opaque request fields. Invalid requests never change config
// or produce a partial response; callers persist only when changed is true.
AppResult appRequest(AppConfig &config, const uint8_t *request, size_t length,
                     uint8_t *reply, size_t capacity, uint32_t version);
}
