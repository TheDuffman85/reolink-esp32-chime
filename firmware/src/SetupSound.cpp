#include "SetupSound.h"
#include <cstring>

namespace chime {
const char *applySetupSound(JsonObjectConst request, ChimeState &state, SetupOutputPins &pins) {
  if (request.isNull() || !request["chime"].is<JsonObjectConst>() || !request["pins"].is<JsonObjectConst>())
    return "Expected chime and pins objects";
  for (JsonPairConst pair : request)
    if (strcmp(pair.key().c_str(), "chime") && strcmp(pair.key().c_str(), "pins")) return "Unknown setup field";
  const JsonObjectConst controls = request["chime"];
  for (JsonPairConst pair : controls) {
    const char *key = pair.key().c_str();
    if (strcmp(key, "name") && strcmp(key, "type") && strcmp(key, "audio_output") &&
        strcmp(key, "melody") && strcmp(key, "pattern") && strcmp(key, "extra_relay"))
      return "Unknown setup sound field";
  }
  ChimeState nextState = state;
  if (const char *error = applyChime(controls, nextState)) return error;
  SetupOutputPins nextPins = pins;
  for (JsonPairConst pair : request["pins"].as<JsonObjectConst>()) {
    const char *key = pair.key().c_str();
    if (!strcmp(key, "relay_active_low")) {
      if (!pair.value().is<bool>()) return "relay_active_low must be true or false";
      nextPins.relayActiveLow = pair.value().as<bool>();
    } else if (!strcmp(key, "audio") || !strcmp(key, "relay")) {
      if (!pair.value().is<int>() || pair.value().as<int>() < -1 || pair.value().as<int>() > 39)
        return "GPIO numbers are -1 to 39";
      if (!strcmp(key, "audio")) nextPins.audio = pair.value().as<int>();
      else nextPins.relay = pair.value().as<int>();
    } else return "Unknown setup wiring field";
  }
  state = nextState; pins = nextPins;
  return nullptr;
}
}
