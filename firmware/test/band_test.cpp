// Host tests for the radio band limits and per-band transmit power.
#include "Band.h"
#include <cassert>
#include <iostream>

using namespace chime;

int main() {
  // The three vendor setups: 868 MHz (tested) and 915/920 MHz.
  assert(bandOf(868000000) == Band::Eu868 && testedBand(868000000));
  assert(bandOf(915000000) == Band::Ism915 && !testedBand(915000000));
  assert(bandOf(920000000) == Band::Ism915 && !testedBand(920000000));
  // Band edges are inclusive; the gap and outside values are rejected.
  assert(validFrequency(863000000) && validFrequency(870000000));
  assert(validFrequency(902000000) && validFrequency(928000000));
  assert(!validFrequency(862999999) && !validFrequency(870000001));
  assert(!validFrequency(901999999) && !validFrequency(928000001));
  assert(!validFrequency(433920000) && !validFrequency(0));
  // EU keeps the bench-tested power; 902-928 MHz transmits at about 0 dBm.
  assert(paTable(bandOf(868000000)) == 0xC0);
  assert(paTable(bandOf(915000000)) == 0x8E);
  std::cout << "Band tests passed\n";
}
