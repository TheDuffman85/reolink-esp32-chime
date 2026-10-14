# Firmware developer guide

This PlatformIO project implements Reolink Chime V2 on ESP32-WROOM-32 + CC1101.
See the [project README](../README.md) for illustrated setup,
[WIRING.md](WIRING.md) for circuits, [PROTOCOL.md](../PROTOCOL.md) for RF bytes
and [API.md](../API.md) for HTTP/MQTT interfaces.

## Build and packaging

The target is `esp32dev`, 4 MB flash. Dependencies are pinned in `platformio.ini`:
espressif32 6.9.0 (Arduino core 2.0.17) and ArduinoJson 6.21.5. Run here:

```bash
pio run -e esp32dev -t build_initial
pio run --target upload --upload-port /dev/ttyUSB0
pio device monitor --port /dev/ttyUSB0 --baud 115200
python3 tools/package.py
```

USB upload installs bootloader, partitions and app. Packaging generates
`dist/firmware.bin` (OTA app only), `dist/firmware-initial.bin` (merged USB-install
image), `dist/manifest.json` (version, SHA-256 and size of both images, and
source hashes), and `dist/web/` (the matching static browser installer). The
`build_initial` target builds the app and merges the same
bootloader, partition table, OTA boot data and flash settings used by PlatformIO
USB upload. `.pio/` and `dist/` are Git-ignored.

`tools/pio_version.py` defines `FIRMWARE_VERSION` from `version.json`. Builds
never change the version. Edit the `version` field manually (`MAJOR.MINOR.PATCH`)
or run `python3 tools/version.py patch`, `minor` or `major` to explicitly bump
that part (minor/major bumps reset the lower parts to zero). Commit the version
with the corresponding source changes. The package tool checks the image's
embedded version before packaging it. The UI reads `REOLINK_CHIME_VERSION` from
OTA files and compares the running version after restart. V2 compatibility
version `00A2` is a separate RF value, not the app version.

The build compresses the two static pages in `include/WebUi.h` with gzip and
generates `WebUiGzip.h` and `.html.gz` assets under `.pio/build/esp32dev/generated/`.
Edit the original HTML; generated assets are not checked in. The ESP32 sends
the compressed bytes directly from flash with `Content-Encoding: gzip` and an
explicit byte length. Compression runs only on the build computer; the browser
decompresses the pages. API responses and logs remain uncompressed. Clients
that explicitly decline gzip receive HTTP 406 for HTML pages; use a browser
or `curl --compressed`. Existing authentication and no-store headers remain.

### Initial USB installation from a release

The easiest option is the [browser USB installer](https://TheDuffman85.github.io/reolink-esp32-chime/).
Use desktop Chrome or Edge, connect the board with a USB data cable, click
**Connect to ESP32**, select the serial port, then choose Install. Close other
serial monitors first. It flashes the merged initial image at `0x0` and clears
saved settings and pairing. The installer also provides Logs & Console.
After installation, join `Reolink-Chime-xxxxxx` and open `http://192.168.4.1/`
to configure 2.4 GHz WiFi. WiFi provisioning inside the USB installer is not
implemented; configuration uses the existing hotspot flow.

For manual USB installation:

Download `firmware-initial.bin` from the GitHub release. Install
[esptool](https://docs.espressif.com/projects/esptool/en/latest/esp32/esptool/basic-commands.html)
and flash the merged image at **address `0x0`**:

```bash
python3 -m pip install esptool
python3 -m esptool --chip esp32 --port /dev/ttyUSB0 write-flash 0x0 firmware-initial.bin
```

Replace the port with your board's port (e.g. `COM3` on Windows). If automatic
upload fails, hold BOOT while connecting and release when writing starts.
This installs a blank ESP32-WROOM-32 with 4 MB flash without compiling the
project. Flashing the merged image on an existing installation clears stored
settings; use `firmware.bin` through Settings → Firmware for normal OTA updates.
Do not upload the merged image through the web updater.

## GitHub releases

From the repository root, push a tag exactly matching the committed version in
`firmware/version.json`, without a `v` prefix. For example, for version `1.0.24`:

```bash
git tag 1.0.24
git push origin 1.0.24
```

The [Release workflow](../.github/workflows/release.yml) runs on every pushed
tag and rejects a mismatch before building. It builds `esp32dev`, runs the host
tests and fuzz corpus, verifies the embedded version while packaging, then
creates a GitHub release with generated notes and attaches `firmware.bin`,
`firmware-initial.bin` and `manifest.json`. Use `firmware.bin` for OTA updates
and `firmware-initial.bin` for [initial USB installation](#initial-usb-installation-from-a-release).
A failed check, build, test or packaging step prevents release publication.

### GitHub Pages browser installer

Before the first deployment, open the repository's **Settings → Pages** and set
**Build and deployment → Source** to **GitHub Actions**. In **Settings →
Environments → github-pages**, allow deployments from `main`. The installer
workflow runs from the default branch, including after tagged releases; release
tags do not need deployment access.

After a successful Release workflow run, GitHub triggers the
[USB installer workflow](../.github/workflows/pages.yml) on the default branch.
It downloads the latest release's initial image and hash manifest, verifies their version, size and
SHA-256, then generates and deploys `dist/web/` using
`actions/configure-pages`, `actions/upload-pages-artifact` and
`actions/deploy-pages`. It needs `pages: write` and `id-token: write`; those
permissions are scoped to that job. A Pages failure leaves the published
release available for manual installation. Re-run the failed installer job after
fixing the repository settings.

The installer also deploys when its page, builder or workflow changes on
`main`, using the latest published firmware without compiling or creating a
new release. For the first deployment or a retry, open **Actions → USB
installer → Run workflow** on `main`. Leave `release_tag` empty to use the
latest release, or enter an existing release tag. Ordinary firmware source
changes do not update the hosted firmware until a release is published.

The source page, styles and loader live in `../web/`. Packaging generates a
separate ESP Web Tools manifest for chip family `ESP32` with the initial image
at numeric offset `0`. Image and manifest filenames contain the version and
image hash to prevent cached pages from selecting another build. They are
served alongside the page on Pages, so firmware fetches do not need CORS or the
GitHub Releases API. ESP Web Tools is pinned to `10.1.0` and loaded from unpkg;
the page offers manual installation instructions if that download fails.

Packaging uses `GITHUB_REPOSITORY` for repository links in Actions, defaulting
to `TheDuffman85/reolink-esp32-chime` locally. Forks should update the browser
installer links in both READMEs. Release notes link to the standard
`https://<owner>.github.io/<repository>/` address; update that link if using a
custom Pages domain.

To preview the packaged installer locally, build and package as above, then
run from `firmware/`:

```bash
python3 -m http.server 8000 --bind 127.0.0.1 --directory dist/web
```

Open `http://localhost:8000/` in Chrome or Edge. HTTPS is required when hosted;
localhost is also allowed. `file://` previews are not supported. Verifying an
actual install requires an attached ESP32-WROOM-32 board; use a spare board or
expect saved settings and pairing to be cleared.

## Source modules

| Source | Responsibility |
|---|---|
| `main.cpp`, `App.h` | Loop, WiFi/AP/mDNS, clock, button, LED, serial and text log |
| `LinkService.cpp` | RX/TX, AES, profile persistence, AFC and ring/app dispatch |
| `Hardware.cpp` | CC1101 SPI/FIFO/bit sampling, GPIO validation, relay/PWM/DAC playback |
| `Audio.h` | Portable DAC sine samples, phase increments and DAC pin constraints |
| `Protocol.cpp` | PN9, CRC, frame parsing and ring retry cache |
| `Link.cpp` | Portable pairing/join/rejoin/heartbeat state machine |
| `AppProtocol.cpp` | Portable V2 name/volume/LED/config/version handlers |
| `Identity.h` | Signed-positive node ID generation |
| `Band.h`, `Afc.cpp` | Band selection and frequency-trim tracker |
| `Settings.cpp`, `WifiConfig.cpp` | Validation, persistence and migration |
| `ChimeService.cpp`, `ChimeControl.cpp` | Playback, mute, live GPIO application and remote-safe controls |
| `SetupSound.cpp` | Restricted setup sound/wiring JSON validation |
| `RingLog.cpp` | Portable 50-entry history and NVS image |
| `Web.cpp`, `WebAuth.cpp`, `Auth.cpp` | HTTP/OTA, sessions, CSRF and throttle |
| `Mqtt.cpp`, `MqttService.cpp` | MQTT worker, state/events/commands and discovery |
| `WebUi.h` | Embedded HTML/CSS/JavaScript interface and sign-in page |

## Storage and timing

Settings schema 2 preserves WiFi credentials, RF parameters, identity and profile.
Schema-1 profiles migrate as passive. Separate NVS namespaces hold app config
(`chimeapp`), gong (`gongx`), GPIO (`pins`), permanent mute (`mute`), authentication
(`auth`), integrations (`integrations`), ring history (`ringlog`), receiver (`radio`),
automatic trim (`afc`), WiFi mode (`wifi`) and UI preferences (`webui`, with
Setup visibility in `setup` and initial completion in `init_done`).
Until initial setup finishes, the UI exposes only Setup, overriding saved browser
page preferences even after pairing or a restart. Finish saves sound/wiring and
any requested sign-in settings, then marks setup complete and unlocks the other
pages. Failed saves keep the review available for retry. Setup hides after Finish
and can be shown again under Settings → General without locking navigation.
Upgrades migrate an existing hidden wizard as complete and a visible wizard as
unfinished; older paired installations without a visibility preference stay complete.
Secrets are in ordinary NVS; settings GETs omit passwords/keys. Gong is the default for new
configurations. Public modes are 0 Audio, 1 Gong and 2 None; Audio has a separate
method (0 PWM, 1 DAC) in the `gongx/audioout` NVS key, remembered across mode
changes. Legacy type 0 migrates to Audio/PWM and type 3 to Audio/DAC. Ring history
retains its compact legacy backend IDs internally and exposes the new mode plus
Audio method in JSON. The Audio output GPIO retains the former tone-output slot
in the ten-byte pin map. Ring history writes after three seconds of
quiet reception/playback. Timed mute, packet history and text logs are RAM-only.
Flash writes/OTA pause reception. Check dropped/error counts under WiFi load.

Serving a UI page temporarily disables WiFi modem sleep to reduce HTTP latency.
Only the visible browser UI marks its requests with `X-WebUI-Active: 1` to keep
it awake; ordinary API calls and polling do not change the sleep policy. After
60 seconds without UI activity, the previous sleep mode returns. Hidden/closed
tabs stop normal polling and hidden tabs omit the marker. OTA retains its
existing sleep-disabled upload behavior; a failed/aborted update uses the same
idle timeout. This trades higher WiFi power consumption during web use for
faster responses, without changing the CPU clock or RF receive/playback timing.

`Sounds.h` defines ten synthesized melodies and ten relay patterns, selected per
doorbell ring by music ID 0–9. Received IDs do not write NVS or replace local
test/MQTT defaults. Existing sound indices and blob layouts are retained. The
[vendor sound audit](docs/vendor-sounds.md) explains the ten-entry table,
rejection of ID 10 and why original audio cannot be extracted from this controller.

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
The initial setup captures show all five steps with only Setup available and
completion with Home, Sound and Settings unlocked. Checks cover saved browser
pages, pairing, reloads, failed Finish saves and reopening Setup after completion.
See [image documentation](docs/README.md).

Pairing/join/rejoin, RF/CRC and press/Test reception have been validated on the
stated 868 MHz setup, along with app settings and OTA. Other models/bands,
audible output, custom GPIO maps, sustained-load reception and interrupted
OTA/NVS recovery need physical testing.
