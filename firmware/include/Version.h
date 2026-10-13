#pragma once
// Firmware release (semantic version). The build defines FIRMWARE_VERSION
// from version.json (tools/pio_version.py), bumping the patch number when the
// sources changed; see tools/version.py for minor/major bumps.
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "0.0.0-dev"
#endif
// Searched for by the web UI in a chosen .bin to show its version before
// installing; logged at start-up so the linker keeps the string.
#define FIRMWARE_MARKER "REOLINK_CHIME_VERSION=" FIRMWARE_VERSION
