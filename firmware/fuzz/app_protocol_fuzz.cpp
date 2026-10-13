// Decrypted doorbell application requests against arbitrary stored config.
#include "AppProtocol.h"
#include "Protocol.h"
#include "FuzzInput.h"
#include <vector>

using namespace chime;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  FuzzInput in(data, size);
  AppConfig config;
  // NVS loads at most 32 name bytes; the terminator is never overwritten.
  in.bytes(reinterpret_cast<uint8_t *>(config.name), sizeof(config.name) - 1);
  config.volume = in.u8(); config.led = in.u8();
  const uint32_t version = in.u32();
  // Exact-size heap buffers let AddressSanitizer catch any overrun.
  std::vector<uint8_t> reply(in.u8() % (MaxMac - 16 + 1));
  std::vector<uint8_t> request(in.remaining());
  in.bytes(request.data(), request.size());

  const AppConfig before = config;
  const AppResult result = appRequest(config, request.data(), request.size(),
    reply.empty() ? nullptr : reply.data(), reply.size(), version);
  const bool same = !std::memcmp(&before, &config, sizeof(config));
  FUZZ_CHECK(config.name[sizeof(config.name) - 1] == 0);
  FUZZ_CHECK(result.replyLength <= reply.size());
  FUZZ_CHECK(result.changed == !same);
  if (!result.valid) FUZZ_CHECK(same && !result.replyLength);
  if (result.valid) FUZZ_CHECK(result.handled);
  // Values a request writes are always in range; untouched ones may not be.
  if (config.volume != before.volume) FUZZ_CHECK(config.volume <= 4);
  if (config.led != before.led) FUZZ_CHECK(config.led <= 1);
  return 0;
}
