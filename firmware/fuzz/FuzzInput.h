#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Checks stay active with NDEBUG; libFuzzer records the abort as a crash.
#define FUZZ_CHECK(condition) do { if (!(condition)) { \
  std::fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
  std::abort(); } } while (0)

// Consumes fuzzer bytes; an exhausted input reads as zeros.
class FuzzInput {
 public:
  FuzzInput(const uint8_t *data, size_t size) : data_(data), size_(size) {}
  bool empty() const { return !size_; }
  size_t remaining() const { return size_; }
  uint8_t u8() {
    if (!size_) return 0;
    --size_; return *data_++;
  }
  uint16_t u16() { const uint16_t low = u8(); return low | uint16_t(u8() << 8); }
  uint32_t u32() { const uint32_t low = u16(); return low | (uint32_t(u16()) << 16); }
  // Copies up to count bytes and zero-fills the rest of out.
  size_t bytes(uint8_t *out, size_t count) {
    const size_t taken = count < size_ ? count : size_;
    if (taken) std::memcpy(out, data_, taken);
    if (count > taken) std::memset(out + taken, 0, count - taken);
    data_ += taken; size_ -= taken;
    return taken;
  }
 private:
  const uint8_t *data_;
  size_t size_;
};
