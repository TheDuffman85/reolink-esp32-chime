#include "Identity.h"
#include <cassert>
#include <iostream>

int main() {
  using namespace chime;
  static_assert(compatibleNodeId(1), "first positive ID");
  static_assert(compatibleNodeId(0x7FFFFFFF), "last signed-positive ID");
  static_assert(!compatibleNodeId(0), "zero is reserved");
  static_assert(!compatibleNodeId(0x80000000), "signed-negative boundary");
  static_assert(!compatibleNodeId(UINT32_MAX), "invalid sentinel");
  static_assert(!compatibleNodeId(0x8AC7CA4D), "owner's original pairing-only ID");
  static_assert(nodeIdCandidate(0x8AC7CA4D) == 0x0AC7CA4D, "clear only the sign bit");
  static_assert(nodeIdCandidate(UINT32_MAX) == 0x7FFFFFFF, "maximum candidate");
  static_assert(nodeIdCandidate(0x80000000) == 0, "caller must retry zero");
  const uint32_t samples[] = {0, 1, 0x7FFFFFFF, 0x80000000, 0x80000001, 0x8AC7CA4D, UINT32_MAX};
  for (uint32_t sample : samples) {
    const uint32_t candidate = nodeIdCandidate(sample);
    assert(candidate <= 0x7FFFFFFF);
    assert((candidate & 0x7FFFFFFF) == (sample & 0x7FFFFFFF));
    assert(compatibleNodeId(candidate) == (candidate != 0));
  }
  // Model entropy retry: both sign-only and all-zero draws are rejected.
  const uint32_t draws[] = {0x80000000, 0, 0x8AC7CA4D};
  uint32_t selected = 0;
  size_t consumed = 0;
  do { selected = nodeIdCandidate(draws[consumed++]); } while (!compatibleNodeId(selected));
  assert(consumed == 3 && selected == 0x0AC7CA4D);
  std::cout << "Identity tests passed: signed-positive range, original ID, sign mask, zero retries\n";
}
