#include "AppProtocol.h"
#include "Protocol.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

using namespace chime;
using Bytes = std::vector<uint8_t>;
Bytes unhex(const char *hex) {
  Bytes result;
  while (*hex) {
    unsigned value = 0;
    for (int i = 0; i < 2; ++i) {
      const char c = *hex++;
      value = value * 16 + (c >= 'A' ? c - 'A' + 10 : c - '0');
    }
    result.push_back(static_cast<uint8_t>(value));
  }
  return result;
}
int main() {
  AppConfig config;
  uint8_t reply[80]; std::memset(reply, 0xCC, sizeof(reply));
  // V2 config fixture: LED on, volume 2, name at offset 24.
  const Bytes fixture = unhex("1500A400000000000000000001000000020000000000000052656F6C696E6B204553503332204368696D6500000000000000000000000000");
  assert(fixture.size() == 56);
  auto result = appRequest(config, fixture.data(), fixture.size(), reply, sizeof(reply), 0x32413030);
  assert(result.handled && result.valid && result.changed && result.replyLength == 8);
  assert(config.volume == 2 && config.led == 1 && std::strcmp(config.name, "Reolink ESP32 Chime") == 0);
  assert(std::memcmp(reply, fixture.data(), 8) == 0 && reply[8] == 0xCC);
  result = appRequest(config, fixture.data(), fixture.size(), reply, sizeof(reply), 0x32413030);
  assert(result.valid && !result.changed); // RF retries do not force a flash write/radio restart.

  Bytes get(56, 0xA5); get[0] = 0x13;
  result = appRequest(config, get.data(), get.size(), reply, sizeof(reply), 0x32413030);
  assert(result.valid && !result.changed && result.replyLength == 56);
  assert(read32le(reply + 4) == 0x00300000 && read32le(reply + 12) == 1 && read32le(reply + 16) == 2);
  assert(read32le(reply + 8) == 0xA5A5A5A5 && read32le(reply + 20) == 0xA5A5A5A5);
  assert(std::memcmp(reply + 24, fixture.data() + 24, 32) == 0);

  // LED transitions through both opcodes must round-trip through get config.
  Bytes set = fixture; write32le(set.data() + 12, 0);
  assert(appRequest(config, set.data(), set.size(), reply, sizeof(reply), 0).changed);
  appRequest(config, get.data(), get.size(), reply, sizeof(reply), 0);
  assert(read32le(reply + 12) == 0);
  const uint8_t ledOn[] = {0x0C, 1, 0xAB, 0xCD};
  result = appRequest(config, ledOn, sizeof(ledOn), nullptr, 0, 0);
  assert(result.valid && result.changed && !result.replyLength && config.led == 1);

  // Reject upper-byte field corruption; never truncate a u32 to its low byte.
  for (size_t field : {size_t(12), size_t(16)}) {
    set = fixture; write32le(set.data() + field, 0x10000001);
    const AppConfig before = config;
    std::memset(reply, 0xCC, sizeof(reply));
    result = appRequest(config, set.data(), set.size(), reply, sizeof(reply), 0);
    assert(result.handled && !result.valid && !result.changed && !result.replyLength);
    assert(config.volume == before.volume && config.led == before.led && std::memcmp(config.name, before.name, 33) == 0);
    for (uint8_t byte : reply) assert(byte == 0xCC);
  }
  const uint8_t badVolume[] = {0x0B, 5, 0, 0}, badLed[] = {0x0C, 2, 0, 0};
  assert(!appRequest(config, badVolume, 4, nullptr, 0, 0).valid);
  assert(!appRequest(config, badLed, 4, nullptr, 0, 0).valid);

  // Every supported opcode rejects short/long payloads without state or output mutation.
  for (const auto &item : std::vector<std::pair<uint8_t, size_t>>{{0x0B,4},{0x0C,4},{0x0E,35},{0x0F,36},{0x13,56},{0x14,20},{0x15,56}}) {
    Bytes request(item.second + 1, 0); request[0] = item.first;
    for (size_t size = 1; size <= request.size(); ++size) {
      if (size == item.second) continue;
      result = appRequest(config, request.data(), size, reply, sizeof(reply), 0);
      assert(result.handled && !result.valid && !result.changed && !result.replyLength);
    }
  }
  assert(!appRequest(config, nullptr, 0, reply, sizeof(reply), 0).handled);
  const uint8_t unknown[] = {0x77};
  assert(!appRequest(config, unknown, 1, reply, sizeof(reply), 0).handled);
  result = appRequest(config, fixture.data(), 56, reply, 7, 0);
  assert(!result.valid && !result.changed);

  Bytes version(20, 0x7A); version[0] = 0x14;
  // In-place buffer operation is supported, as in the vendor implementation.
  result = appRequest(config, version.data(), version.size(), version.data(), version.size(), 0x32413030);
  const Bytes expectedVersion = unhex("147A7A7A00001400303041324343313378325231");
  assert(result.valid && version == expectedVersion);
  Bytes name(35, 'X'); name[0] = 0x0E;
  assert(appRequest(config, name.data(), name.size(), nullptr, 0, 0).valid);
  assert(config.name[31] == 'X' && config.name[32] == 0);
  Bytes nameQuery(36, 0xBB); nameQuery[0] = 0x0F;
  result = appRequest(config, nameQuery.data(), nameQuery.size(), reply, sizeof(reply), 0);
  assert(result.valid && result.replyLength == 36 && reply[1] == 0 && reply[2] == 0xBB && reply[3] == 0xBB);
  for (size_t i = 4; i < 36; ++i) assert(reply[i] == 'X');
  std::cout << "App protocol tests passed: fixture config, LED round trips, lengths, u32 ranges, reply bounds, version/name\n";
}
