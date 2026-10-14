# Firmware documentation images

Updated 2026-10-14. The illustrated [project README](../../README.md)
uses the hardware references and current UI captures listed below.

## Hardware pinout references

- `images/cc1101-868mhz-pinout.jpg`: CC1101 868 MHz module signal layout,
  from the [Roboter-Bausatz module reference](https://www.roboter-bausatz.de/p/cc1101-868mhz-rf-funkmodul-transceiver-fuer-arduino-iot).
- `images/esp32-38pin-pinout.png`: 38-pin ESP32 reference, extracted
  from the [diymore board reference](https://m.media-amazon.com/images/I/91gZHiaq7NL.pdf).

These show the component side. Follow signal/GPIO labels on the actual hardware,
not header positions or grey diagram pin numbers. [WIRING.md](../WIRING.md) and
[wiring.svg](../wiring.svg) show the default **GDO0 → GPIO4, GDO2 → GPIO16** map;
the default packet-engine receiver does not read them; the optional ESP32
receive mode needs both. Settings → Wiring may override GPIO assignments.
The references are stored locally and do not represent a photograph of the completed harness. They have been retained unchanged.

## Current embedded UI screenshots

The Sound and Setup captures show all ten numbered melodies and gong patterns.
Doorbell rings follow the Reolink ringtone slot; the local choice sets the
default for web/button tests and MQTT plays. The audio sounds are synthesized
alternatives, as explained in the [original firmware audit](vendor-sounds.md).

| File | View |
|---|---|
| `images/web-status.png` | Home: status, Mute/Play chime and ring log |
| `images/web-sound.png` | Sound: Audio mode with PWM output, ten numbered melodies, volume, extra relay pulse and LED |
| `images/web-sound-dac.png` | Sound: Audio mode with DAC output, the same ten melodies and level |
| `images/web-sound-relay.png` | Sound: Gong mode, ten numbered strike patterns and timing |
| `images/web-sound-none.png` | Sound: mode None (silent) |
| `images/web-muted.png` | Home while muted for 45 minutes |
| `images/web-status-light.png` | Home in the light theme |
| `images/web-setup-sound.png` | Setup: sound and output wiring |
| `images/ui-auth-off-desktop.png`, `images/ui-auth-off-mobile.png` | Setup step 4: optional sign-in off |
| `images/ui-auth-enabled-desktop.png`, `images/ui-auth-enabled-mobile.png` | Setup step 4: new username/password, masked dummy password |
| `images/ui-auth-existing-desktop.png`, `images/ui-auth-existing-mobile.png` | Setup step 4: keeping a saved password |
| `images/ui-auth-review-desktop.png`, `images/ui-auth-review-mobile.png` | Setup step 5: sign-in choice in final review |
| `images/web-setup-review.png` | Setup: last step, reviewing GPIO changes before Finish |
| `images/web-setup-complete.png` | Setup: live GPIOs applied, sound testing enabled |
| `images/web-setup-wifi.png` | Setup step 1: WiFi scan/connect |
| `images/web-setup-pairing.png` | Setup step 2: pairing instructions and Start pairing |
| `images/ui-setup-hidden-desktop.png`, `images/ui-setup-hidden-mobile.png` | Home with Setup hidden after completion |
| `images/ui-settings-setup-hidden-desktop.png`, `images/ui-settings-setup-hidden-mobile.png` | General: Show Setup tab off |
| `images/ui-settings-setup-shown-desktop.png`, `images/ui-settings-setup-shown-mobile.png` | General: Show Setup tab on and Open Setup |
| `images/ui-settings-wifi-desktop.png`, `images/ui-settings-wifi-mobile.png` | WiFi mode first, then client connection/scan controls in Settings |
| `images/ui-settings-wifi-ap-desktop.png`, `images/ui-settings-wifi-ap-mobile.png` | Access point mode: hotspot settings, client connection hidden |
| `images/ui-settings-wifi-off-desktop.png`, `images/ui-settings-wifi-off-mobile.png` | Off mode: recovery instructions, client connection hidden |
| `images/ui-settings-pair-desktop.png`, `images/ui-settings-pair-mobile.png` | Full pairing controls directly in Settings with Setup hidden |
| `images/web-settings.png` | Settings: section list and General |
| `images/web-settings-ha.png` | Settings: MQTT (Home Assistant) |
| `images/web-settings-security.png` | Settings: Sign-in and sessions |
| `images/web-settings-advanced.png` | Settings: Advanced (radio and GPIO wiring) |
| `images/web-settings-mobile.png` | Settings section list on a 390 px phone |
| `images/web-update-countdown.png` | Waiting spinner after a firmware update (reloads when the chime is back) |
| `images/web-dialog.png` | Confirmation modal (Forget WiFi); the generator checks that Esc cancels without a request |
| `images/web-maintenance.png` | Debug: diagnostics, RAM log, packets and tools |
| `images/web-mobile.png` | Home at 390 × 1000 phone viewport with bottom navigation |
| `images/web-login.png`, `images/web-login-mobile.png` | Sign-in page (desktop, 390 px phone) |

`web-maintenance.png` retains its old filename for links but now shows **Settings → Debug**;
OTA is in **Settings → Firmware**. All captures render the current embedded
`include/WebUi.h` HTML through [tools/screenshots.cjs](../tools/screenshots.cjs).
They use synthetic API responses, signed-positive demonstration IDs and generated
AES/CRC packet examples. They are **not ESP32, doorbell or RF captures**.
The banner **DOCUMENTATION DEMO · Synthetic data · No hardware connected** is
added by the generator; the embedded firmware itself remains unchanged. No actual
password, AES key or device token appears in the captures.

Desktop captures use 1100 px width; the mobile view uses 390 px. Desktop captures include whole pages so all controls are visible; the phone
standard capture keeps the fixed navigation at the bottom of its viewport.
The `ui-*.png` change captures include complete desktop and phone pages.
Full phone captures use the page height as the viewport so fixed navigation
lands at the bottom and does not cover any controls. Every tab is checked at **390, 768 and
1100 px** for horizontal page overflow; scrolling within the packet table is
intentional. Gong/Audio/None and both Audio methods are checked at each width. Preview
requests are checked for the selected method in Sound and Setup; saving and
reloading across mode changes preserves that method. Switching between Audio
and Gong also preserves the numbered sound slot. Both catalogs are checked for ten choices, with previews of the last
slot verified for PWM, DAC and Gong. The shared Audio GPIO
selector and DAC pin restrictions are also checked. Setup is checked for
validation failures, Back/Continue without saving, and live GPIO application
only on the final Finish click. The five-step wizard also checks optional
sign-in off/on, Skip preserving existing settings, password confirmation,
keeping saved passwords, and recovery from a failed authentication save.
Setup melody/pattern drafts survive status/settings refreshes. Audio previews
check the pending extra-relay choice and pulse length, and a DAC method remembered
in Gong selects a valid DAC GPIO when switching to Audio. One intentional `/api/auth` HTTP 500 response tests retry handling.
The checks also cover Setup hiding only after successful Finish, showing/hiding
it in General, preference persistence on reload, ignoring a stale saved Setup
tab, fresh-device onboarding, direct WiFi connection/pairing in Settings, and
recovering an unpaired device while Setup stays hidden.
All other JavaScript/console errors and unexpected
requests are rejected. All network requests are intercepted, including the UI's automatic
POST `/api/pins/check` validation; no device settings are saved and no device is
contacted. The source SHA-256, fixture provenance, viewports and results are in
[images/screenshot-manifest.json](images/screenshot-manifest.json).

## Reproduce the captures

Requires Node.js, Playwright and a compatible Chromium installation. From the
repository root:

```bash
node firmware/tools/screenshots.cjs
```

When Playwright is installed outside the project, set `NODE_PATH` to that
installation's `node_modules`. `SCREENSHOT_BROWSER_EXE` can select an existing
Chromium executable when it differs from Playwright's default browser.

```bash
NODE_PATH=/path/to/node_modules SCREENSHOT_BROWSER_EXE=/path/to/chromium node firmware/tools/screenshots.cjs
```

The generator replaces the `web-*.png` and `ui-*.png` files and screenshot
manifest. Its synthetic version labels follow `firmware/version.json`.
README images are copies of the verified captures named
`readme-<version>-<ui-hash>-<view>.png`, where `ui-hash` is the first twelve
characters of the embedded UI's SHA-256. The generator updates image links in
the project README and screenshot gallery. These URLs refresh when the UI
changes, even without a firmware version change, avoiding stale cached images.
The manifest records each README image's source capture, UI hash and firmware
version, plus the ten-slot and mode-switch checks.
If the browser cannot launch under a restricted execution sandbox, use an
approved local browser environment. Device firmware, OTA package and hardware
pinout references are unaffected by screenshot generation.
