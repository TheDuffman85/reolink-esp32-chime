#pragma once
#include "ChimeControl.h"

namespace chime {
struct SetupOutputPins { int8_t audio = 25, relay = 26; bool relayActiveLow = false; };
// Setup can edit only sound controls and output wiring. Apply both groups only
// after their JSON types/ranges validate; hardware pin checks follow on-board.
const char *applySetupSound(JsonObjectConst request, ChimeState &state, SetupOutputPins &pins);
}
