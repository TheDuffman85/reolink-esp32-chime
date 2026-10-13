#pragma once
#include <cstdint>

namespace chime {
// Supported configuration ranges. Carrier presets are 868, 915 and 920 MHz;
// only 868 MHz has been tested with a doorbell. A module and antenna matched
// to the configured band are required.
enum class Band : uint8_t { Invalid, Eu868, Ism915 };
constexpr uint32_t Eu868Min = 863000000, Eu868Max = 870000000;
constexpr uint32_t Ism915Min = 902000000, Ism915Max = 928000000;
constexpr Band bandOf(uint32_t hz) {
  return hz >= Eu868Min && hz <= Eu868Max ? Band::Eu868 :
         hz >= Ism915Min && hz <= Ism915Max ? Band::Ism915 : Band::Invalid;
}
constexpr bool validFrequency(uint32_t hz) { return bandOf(hz) != Band::Invalid; }
// Only the EU band was confirmed against a doorbell.
constexpr bool testedBand(uint32_t hz) { return bandOf(hz) == Band::Eu868; }
// CC1101 PATABLE[0] (datasheet table 39): 0xC0 for the 868 MHz range,
// reduced setting 0x8E for 902-928 MHz. Actual RF power depends on hardware.
constexpr uint8_t paTable(Band band) { return band == Band::Ism915 ? 0x8E : 0xC0; }
}
