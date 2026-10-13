# Firmware documentation images

Updated 2026-10-13. The illustrated [project README](../../README.md)
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

| File | View |
|---|---|
| `images/web-status.png` | Home: status, Mute/Play chime and ring log |
| `images/web-sound.png` | Sound: buzzer melodies, volume, extra relay pulse and LED |
| `images/web-sound-relay.png` | Sound: existing gong, strike patterns and timing |
| `images/web-sound-none.png` | Sound: gong type None (silent) |
| `images/web-muted.png` | Home while muted for 45 minutes |
| `images/web-status-light.png` | Home in the light theme |
| `images/web-setup-wifi.png` | Setup step 1: WiFi scan/connect |
| `images/web-setup-pairing.png` | Setup step 2: pairing instructions and Start pairing |
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
capture keeps the fixed navigation at the bottom of its viewport. Every tab is checked at **390, 768 and
1100 px** for horizontal page overflow; scrolling within the packet table is
intentional. The generator rejects JavaScript/console errors and unexpected
requests. All network requests are intercepted, including the UI's automatic
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

The generator replaces the `web-*.png` files and screenshot manifest.
If the browser cannot launch under a restricted execution sandbox, use an
approved local browser environment. Device firmware, OTA package and hardware
pinout references are unaffected by screenshot generation.
