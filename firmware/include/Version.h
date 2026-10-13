#pragma once
// Firmware release (semantic version). The build defines FIRMWARE_VERSION
// from the manually maintained version.json (tools/pio_version.py).
// See tools/version.py for explicit patch/minor/major bumps.
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "0.0.0-dev"
#endif
// Searched for by the web UI in a chosen .bin to show its version before
// installing; logged at start-up so the linker keeps the string.
#define FIRMWARE_MARKER "REOLINK_CHIME_VERSION=" FIRMWARE_VERSION
