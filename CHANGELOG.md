# Changelog

Changes to the ESP32 firmware, listed per version from [`firmware/version.json`](firmware/version.json).

## 1.0.35 - 2026-10-15

### Changed

- **Settings → Doorbell**: a doorbell is forgotten with the **Forget** button on its own card, also when only one doorbell is paired. The separate "Forget doorbell" / "Forget all doorbells" card is gone; `clear_profile` in `POST /api/settings` still removes every doorbell.

## 1.0.34 - 2026-10-15

### Added

- Experimental range extender, off by default (**Settings → Doorbell → Range extender**). It repeats, unchanged and still encrypted, the frames between a doorbell paired with this chime and another chime, so a chime out of the doorbell's range can still ring. Pairing is repeated only during a five-minute window opened with **Relay pairing**; pairing near the doorbell is preferred. Untested with real chimes.
- API: `POST /api/extender` switches it and opens the pairing window; `/api/status` reports its counters and the other chimes heard. Packet history marks relayed frames.

### Changed

- While the range extender is on, frames addressed to other chimes no longer feed the doorbell's frequency estimate or signal report.

## 1.0.33 - 2026-10-15

### Added

- Experimental support for up to four doorbells ringing one chime. Setup still pairs one doorbell; further doorbells are paired from **Settings → Doorbell**. Each doorbell keeps its own key, session, heartbeat and rejoin, and the links take turns on the radio.
- Automatic frequency trim keeps one estimate per doorbell, receives at their midpoint and transmits with the addressed doorbell's own trim.
- Ring log entries, MQTT events and Home Assistant attributes name the ringing doorbell.
- API: `pair/start` accepts `add` and `doorbell_id`, `join/start` accepts `doorbell_id`, new `/api/doorbells` (labels) and `/api/doorbells/forget` routes, and `/api/status` lists the paired doorbells.

### Changed

- A doorbell can only be paired once; a pairing response from a doorbell that is already held by another slot fails that attempt.
- Name, volume and LED stay shared between doorbells; the latest change wins.
- Further doorbells, their labels and measured trims are stored in the new `doorbells` NVS namespace. The ring log uses image version 2; version 1 logs still load.

## 1.0.32 - 2026-10-15

### Added

- **Override ringtone** setting (off by default). Every trigger requests a sound slot: the doorbell its music ID, MQTT and API plays an optional `sound`, everything else slot 0. With the override on, the saved melody or pattern always plays instead.
- Home Assistant **Override ringtone** switch (23 discovery entities); the melody and pattern selects are unavailable while the override is off.
- The web UI warns before leaving a page with unsaved changes.
- WiFi disconnect reasons are logged.

### Changed

- The Sound tab lists the ten slots as previews and makes them selectable while overriding. Setup no longer picks a sound.
- The Home **Play chime** button is removed; doorbell rings open the ring log.
- The doorbell is shown in blue, the permanent mute option is named, and the header badges stay hidden until the first status arrives.

### Fixed

- The station retries the saved network every 30 seconds, because the WiFi core gives up after an authentication failure.
- The ring log scrollbar no longer overlaps its content.

## 1.0.31 - 2026-10-14

### Changed

- Setup runs only during initial setup and hides the navigation while it runs. The **Show Setup tab** switch, **Open Setup** button and the reopened wizard's Advanced wiring link are removed. After **Finish**, lost pairing is recovered under **Settings → Doorbell**.
- Step actions (Connect, Start pairing, Test sound) move into the step content; the footer only navigates and offers **Skip for now** until a step is done. Every step ends in **Back** / **Next**.
- The sign-in step offers **No sign-in** or **Require sign-in** choice cards. A sign-in change is only staged when it differs from the saved settings.
- **Finish** saves, closes Setup and opens Home, offering a restart while the setup hotspot is still on.
- On phones, melodies and gong patterns use a dropdown with a preview button, step actions get their own full-width row, and dropdowns show a more prominent arrow.
- The first-pairing proximity hint is highlighted and reworded, and the idle pairing status is hidden instead of showing "Ready.".

## 1.0.30 - 2026-10-14

Not published as a GitHub release.

### Changed

- WiFi modem sleep is disabled for 60 seconds while the visible web UI is in use, for faster responses. Ordinary API calls and polling do not change the sleep policy.

## 1.0.29 - 2026-10-14

Not published as a GitHub release.

### Changed

- The web pages are gzip-compressed at build time and served with `Content-Encoding: gzip`. Clients that explicitly decline gzip receive HTTP 406 for HTML pages; use a browser or `curl --compressed`. JSON API responses and logs remain uncompressed.

## 1.0.28 - 2026-10-14

Not published as a GitHub release.

### Changed

- Setup, Debug and the README advise placing the chime 1–2 metres from the doorbell for first pairing, so automatic frequency trim can calibrate.

### Fixed

- The radio keeps retrying after a failed start or recovery that left correct registers but disabled reception.
- The CC1101 SPI clock is lowered from 4 MHz to 1 MHz for more reliable jumper-wired modules.

## 1.0.26 - 2026-10-14

### Added

- Ten PWM/DAC melodies and ten gong patterns matching Reolink's ten ringtone slots. Doorbell rings play the slot selected for that event in Reolink; unknown music IDs, including 10, are ignored and shown in packet diagnostics.
- MIT license.

### Changed

- Until initial setup finishes, Setup is the only available page, including after pairing, a browser reload or a restart. **Finish** unlocks Home, Sound and Settings. `/api/status` reports `setup_required`.

## 1.0.25 - 2026-10-14

### Added

- PWM and 8-bit DAC (16 kHz sine) audio output through an external amplifier, replacing the passive buzzer mode. DAC output requires GPIO25 or GPIO26.
- Five-step Setup wizard: WiFi, pairing, sound and output wiring, optional sign-in and a final review. **Finish** saves everything without restarting.
- Setup tab visibility setting, persisted on the chime.
- Browser USB installer on GitHub Pages, deployed from the published release.

### Changed

- Gong is the default sound mode; the mode order is Gong, Audio, None. Saved buzzer settings migrate to Audio/PWM.
- MQTT and API rename `buzzer_level` to `audio_level` and the `buzzer` GPIO to `audio`. Home Assistant gains Sound mode and Audio output selectors (22 discovery entities).
- GPIO changes apply live without restarting.
- In **Settings → Network & time**, the WiFi mode comes first and the client connection controls appear only in Client mode.

## 1.0.24 - 2026-10-14

First release.

- Reolink Chime hardware V2 emulation with an ESP32 and CC1101: normal Reolink pairing with an individual AES key, saved credentials and automatic rejoin.
- Passive buzzer melodies, relay gong patterns and silent mode; permanent and timed mute, optional extra relay pulse and configurable LED.
- Mobile-friendly web UI with light/dark/system theme and optional sign-in.
- 2.4 GHz WiFi onboarding, standalone access point and WiFi-off operation.
- MQTT commands, events, retained state and 21 Home Assistant discovery entities.
- Last 50 ring events persisted locally, RF packet history and downloadable diagnostics.
- Configurable GPIOs, automatic frequency trim, two RF receivers and web OTA.
- Releases contain the OTA image, a merged image for initial USB installation and a manifest with image and source hashes.
