#pragma once
#include <cstddef>
#include <cstdint>

namespace chime {
// HMAC-SHA256; supplied by mbedTLS on the ESP32 and OpenSSL in host tests.
using HmacFn = bool (*)(const uint8_t *key, size_t keyLength,
                        const uint8_t *data, size_t length, uint8_t out[32]);

// Stateless sign-in cookies: "<expiry>.<nonce>.<mac>" as 8, 16 and 32 hex
// digits. The MAC is the first 16 bytes of HMAC-SHA256(key, "<expiry>.<nonce>").
// The firmware derives the key from the admin password, so changing the
// password (or the stored sign-out epoch) invalidates every session at once.
// Expiry is UTC seconds; 0 means no server-side expiry (clock unknown).
constexpr size_t TokenLength = 58;
bool makeToken(const uint8_t key[32], uint32_t expiry, const uint8_t nonce[8],
               HmacFn hmac, char out[TokenLength + 1]);
// `now` is UTC seconds, or 0 when the clock is unknown (expiry not checked).
bool checkToken(const uint8_t key[32], const char *token, size_t length,
                uint32_t now, HmacFn hmac);
// Compares without data-dependent early exit. Lengths are not secret.
bool sameSecret(const char *a, const char *b);

// Slows password guessing per client address. Three failures are free; each
// further failure doubles the lock-out from 1 s up to 5 minutes. A success
// resets the address. Times are millis() values; rollover is handled.
class LoginThrottle {
 public:
  bool allowed(uint32_t address, uint32_t now, uint32_t &waitMs) const;
  void failure(uint32_t address, uint32_t now);
  void success(uint32_t address);
 private:
  struct Slot { uint32_t address = 0, lockedUntil = 0, lastAt = 0; uint16_t failures = 0; bool used = false; };
  Slot *find(uint32_t address);
  const Slot *find(uint32_t address) const;
  Slot slots_[8];
};
}
