# Firmware developer guide

This PlatformIO project implements Reolink Chime V2 on ESP32-WROOM-32 + CC1101.
See the [project README](../README.md) for illustrated setup,
[WIRING.md](WIRING.md) for circuits, [PROTOCOL.md](../PROTOCOL.md) for RF bytes
and [API.md](../API.md) for HTTP/MQTT interfaces.

## Build and packaging

The target is `esp32dev`, 4 MB flash. Dependencies are pinned in `platformio.ini`:
espressif32 6.9.0 (Arduino core 2.0.17) and ArduinoJson 6.21.5. Run here:

```bash
pio run
pio run --target upload --upload-port /dev/ttyUSB0
pio device monitor --port /dev/ttyUSB0 --baud 115200
python3 tools/package.py
```

USB upload installs bootloader, partitions and app. Packaging generates
`dist/firmware.bin` (OTA app only) and `dist/manifest.json` (version, SHA-256,
image size and source hashes). `.pio/` and `dist/` are Git-ignored.

`tools/pio_version.py` defines `FIRMWARE_VERSION` from `version.json`. A SHA-256
change in `platformio.ini`, `include/*.h` or `src/*.cpp` bumps the patch version;
unchanged inputs keep it. `python3 tools/version.py minor` or `major` bumps that
part and records the current source hash. The package tool checks the image's
embedded version before packaging it. The UI reads `REOLINK_CHIME_VERSION` from
OTA files and compares the running version after restart. V2 compatibility
version `00A2` is a separate RF value, not the app version.

## Source modules

| Source | Responsibility |
|---|---|
| `main.cpp`, `App.h` | Loop, WiFi/AP/mDNS, clock, button, LED, serial and text log |
| `LinkService.cpp` | RX/TX, AES, profile persistence, AFC and ring/app dispatch |
| `Hardware.cpp` | CC1101 SPI/FIFO/bit sampling, GPIO validation and playback |
| `Protocol.cpp` | PN9, CRC, frame parsing and ring retry cache |
| `Link.cpp` | Portable pairing/join/rejoin/heartbeat state machine |
| `AppProtocol.cpp` | Portable V2 name/volume/LED/config/version handlers |
| `Identity.h` | Signed-positive node ID generation |
| `Band.h`, `Afc.cpp` | Band selection and frequency-trim tracker |
| `Settings.cpp`, `WifiConfig.cpp` | Validation, persistence and migration |
| `ChimeService.cpp`, `ChimeControl.cpp` | Playback, mute and remote-safe controls |
| `RingLog.cpp` | Portable 50-entry history and NVS image |
| `Web.cpp`, `WebAuth.cpp`, `Auth.cpp` | HTTP/OTA, sessions, CSRF and throttle |
| `Mqtt.cpp`, `MqttService.cpp` | MQTT worker, state/events/commands and discovery |
| `WebUi.h` | Embedded HTML/CSS/JavaScript interface and sign-in page |

## Storage and timing

Settings schema 2 preserves WiFi credentials, RF parameters, identity and profile.
Schema-1 profiles migrate as passive. Separate NVS namespaces hold app config
(`chimeapp`), gong (`gongx`), GPIO (`pins`), permanent mute (`mute`), authentication
(`auth`), integrations (`integrations`), ring history (`ringlog`), receiver (`radio`),
automatic trim (`afc`) and WiFi mode (`wifi`). Secrets are in ordinary NVS;
settings GETs omit passwords/keys. Ring history writes after three seconds of
quiet reception/playback. Timed mute, packet history and text logs are RAM-only.
Flash writes/OTA pause reception. Check dropped/error counts under WiFi load.

## Host tests and fuzzing

Requires g++, OpenSSL development headers (e.g. `libssl-dev`) and ArduinoJson
installed in `.pio/libdeps/esp32dev`:

```bash
pio pkg install
bash tools/test.sh
```

The script uses AddressSanitizer/UndefinedBehaviorSanitizer. Protocol tests cover
PN9/CRC, both polarities/all alignments, malformed-length recovery, duplicates,
rollover and a million noise bits. Link tests use real AES for pairing/join,
repeated responses, cancellation, timeout and TX failures. App tests cover exact
lengths, full-u32 bounds, reply capacity and LED/name/version round trips.
Additional tests cover identity, AFC, bands, sessions, throttle, persistent
history, WiFi modes and restricted MQTT updates. The checked-in generated fuzz
corpus is replayed without needing clang.

Coverage-guided fuzzing requires clang/libFuzzer (or set `CLANGXX`):

```bash
bash tools/fuzz.sh             # 60 seconds per target
bash tools/fuzz.sh 600 link    # ten minutes on link
```

| Target | Checks |
|---|---|
| protocol | Raw-bit/MAC bounds, round trips, polarity and duplicate handling |
| link | Command/time/TX/RX stream, state invariants and valid outgoing frames |
| app_protocol | Arbitrary config/request/capacity, no invalid mutation or overflow |

The link fuzzer uses an identity cipher to reach payload branches; link tests
cover real AES. Corpus inputs are in `fuzz/corpus/<target>`; crash artifacts
are written to `.pio/fuzz/<target>/`.

## Screenshots and hardware validation

From the repository root with Playwright and Chromium installed:

```bash
node firmware/tools/screenshots.cjs
```

`NODE_PATH` selects an external Node module installation;
`SCREENSHOT_BROWSER_EXE` selects Chromium. The generator renders the embedded
HTML, intercepts requests, uses synthetic fixtures, checks JavaScript errors,
dialogs and overflow at 390/768/1100 px, and writes PNGs plus a UI hash manifest.
See [image documentation](docs/README.md).

Pairing/join/rejoin, RF/CRC and press/Test reception have been validated on the
stated 868 MHz setup, along with app settings and OTA. Other models/bands,
audible output, custom GPIO maps, sustained-load reception and interrupted
OTA/NVS recovery need physical testing.
