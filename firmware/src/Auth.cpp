#include "Auth.h"
#include <cstring>

namespace chime {
namespace {
const char Digits[] = "0123456789abcdef";
void hex(const uint8_t *bytes, size_t count, char *out) {
  for (size_t i = 0; i < count; ++i) { out[2 * i] = Digits[bytes[i] >> 4]; out[2 * i + 1] = Digits[bytes[i] & 15]; }
}
bool mac(const uint8_t key[32], const char *signedPart, HmacFn hmac, char out[32]) {
  uint8_t digest[32];
  if (!hmac || !hmac(key, 32, reinterpret_cast<const uint8_t *>(signedPart), 25, digest)) return false;
  hex(digest, 16, out);
  return true;
}
bool hexDigits(const char *text, size_t count) {
  for (size_t i = 0; i < count; ++i)
    if (!((text[i] >= '0' && text[i] <= '9') || (text[i] >= 'a' && text[i] <= 'f'))) return false;
  return true;
}
}

bool makeToken(const uint8_t key[32], uint32_t expiry, const uint8_t nonce[8],
               HmacFn hmac, char out[TokenLength + 1]) {
  const uint8_t be[4] = {uint8_t(expiry >> 24), uint8_t(expiry >> 16), uint8_t(expiry >> 8), uint8_t(expiry)};
  hex(be, 4, out); out[8] = '.';
  hex(nonce, 8, out + 9); out[25] = '.';
  if (!mac(key, out, hmac, out + 26)) return false;
  out[TokenLength] = 0;
  return true;
}

bool checkToken(const uint8_t key[32], const char *token, size_t length,
                uint32_t now, HmacFn hmac) {
  if (!token || length != TokenLength || token[8] != '.' || token[25] != '.' ||
      !hexDigits(token, 8) || !hexDigits(token + 9, 16) || !hexDigits(token + 26, 32)) return false;
  char expected[32];
  if (!mac(key, token, hmac, expected)) return false;
  uint8_t difference = 0;
  for (size_t i = 0; i < 32; ++i) difference |= uint8_t(expected[i] ^ token[26 + i]);
  if (difference) return false;
  uint32_t expiry = 0;
  for (size_t i = 0; i < 8; ++i)
    expiry = (expiry << 4) | uint32_t(token[i] <= '9' ? token[i] - '0' : token[i] - 'a' + 10);
  return !expiry || !now || now < expiry;
}

bool sameSecret(const char *a, const char *b) {
  const size_t la = strlen(a), lb = strlen(b);
  uint8_t difference = la != lb;
  for (size_t i = 0; i < la; ++i) difference |= uint8_t(a[i] ^ b[i < lb ? i : 0]);
  return !difference && lb;
}

LoginThrottle::Slot *LoginThrottle::find(uint32_t address) {
  for (auto &slot : slots_) if (slot.used && slot.address == address) return &slot;
  return nullptr;
}
const LoginThrottle::Slot *LoginThrottle::find(uint32_t address) const {
  for (const auto &slot : slots_) if (slot.used && slot.address == address) return &slot;
  return nullptr;
}
bool LoginThrottle::allowed(uint32_t address, uint32_t now, uint32_t &waitMs) const {
  waitMs = 0;
  const Slot *slot = find(address);
  if (!slot || slot->failures <= 3) return true;
  const int32_t left = int32_t(slot->lockedUntil - now);
  if (left <= 0) return true;
  waitMs = uint32_t(left);
  return false;
}
void LoginThrottle::failure(uint32_t address, uint32_t now) {
  Slot *slot = find(address);
  if (!slot) {
    // Reuse a free slot, or the one with the oldest activity.
    slot = &slots_[0];
    for (auto &candidate : slots_) {
      if (!candidate.used) { slot = &candidate; break; }
      if (int32_t(candidate.lastAt - slot->lastAt) < 0) slot = &candidate;
    }
    *slot = Slot{}; slot->used = true; slot->address = address;
  }
  if (slot->failures < 1000) ++slot->failures;
  slot->lastAt = now;
  if (slot->failures > 3) {
    const uint32_t shift = slot->failures - 4 < 9 ? slot->failures - 4 : 9;
    const uint32_t lock = 1000u << shift;
    slot->lockedUntil = now + (lock < 300000 ? lock : 300000);
  }
}
void LoginThrottle::success(uint32_t address) {
  if (Slot *slot = find(address)) *slot = Slot{};
}
}
