# HTTP and MQTT API

This reference documents the ESP32 firmware in this repository. HTTP is served
on port 80 at `http://reolink-chime.local/`, the station IP, or
`http://192.168.4.1/` on the hotspot. WiFi-off mode disables HTTP. These are
the **custom ESP32 chime APIs**, not the Reolink camera API.

## Authentication, CSRF and responses

When sign-in is enabled, use the `chime_session` cookie from `/api/login` or
HTTP Basic authentication with the configured username/password. Without valid
credentials protected routes return JSON 401; no Basic challenge is issued.
When sign-in is off, protected routes are accessible without credentials.

Read **GET `/api/status`** to obtain `csrf`. Send it as **`X-CSRF-Token` on
every POST except `/api/login` and `/api/logout`**, even when sign-in is off.
The CSRF token changes on reboot. The UI page is GET `/`; sign-in page is
GET `/login`. JSON bodies use `Content-Type: application/json`.

Updates generally accept partial objects. Some routes reject unknown fields;
others ignore them, so only send the fields documented for that endpoint.
HTTP status 200 means the operation was accepted; pairing, WiFi connection and
restart are asynchronous and must be checked through subsequent GETs.

| Status | Meaning |
|---|---|
| 200 | JSON object, or `{"message":"..."}` for a successful command |
| 400 | Malformed JSON, invalid fields, failed GPIO validation or rejected OTA |
| 401 | Authentication missing/invalid |
| 403 | Missing/wrong CSRF token |
| 404 | Unknown API route |
| 409 | Busy link, unavailable radio/relay or incompatible operation |
| 413 | `/api/settings` body exceeds 2048 bytes |
| 429 | Login throttle; `Retry-After` contains seconds |
| 500 | Persistent storage write failed |
| 503 | Storage/gong unavailable or log allocation failed |

Errors usually contain `{"error":"..."}`. GPIO validation can also include
`ok` and `issues`. Responses use `Cache-Control: no-store`. Passwords and AES
keys are write-only; GET endpoints expose presence flags where applicable.
HTTP and MQTT do not provide TLS in this implementation.

Example using Basic auth (curl prompts for the password; omit `--user` when
sign-in is off). Requires jq:

```bash
CHIME_URL=http://reolink-chime.local
csrf=$(curl --fail --silent --user admin "$CHIME_URL/api/status" | jq -r .csrf)
curl --fail --user admin -H "X-CSRF-Token: $csrf" \
  -H 'Content-Type: application/json' -d '{"volume":2,"led":true}' \
  "$CHIME_URL/api/chime"
```

For a cookie client, POST `{username,password,remember}` to `/api/login`, store
the returned cookie, then read `/api/status` with that cookie. Cookies are
HttpOnly and SameSite=Lax. Login with a synchronized clock creates a 12-hour
session or 30-day remembered session; expiry checks depend on available clock
time. Credential changes and sign-out epoch changes invalidate old tokens.

## Status and diagnostics

### GET `/api/status`

Returns a flat status object, plus the nested `mute` object:

| Fields | Description |
|---|---|
| `csrf`, `uptime_ms`, `version`, `firmware`, `free_heap` | Current token, uptime, semantic app version, build date/time and free bytes |
| `wifi_connected`, `wifi_configured`, `ssid`, `ip`, `wifi_rssi` | Station state, configured SSID, reachable IP and RSSI |
| `ap_active`, `ap_name`, `wifi_mode`, `hostname` | Hotspot state and active client/ap/off mode |
| `radio_ok`, `radio_error`, `radio_part`, `radio_version`, `marcstate` | Radio health and CC1101 status |
| `rf_frequency_hz`, `rf_trim`, `rssi_dbm` | Effective frequency, active trim and sampled RSSI |
| `auto_trim`, `trim_calibrated`, `rx_bandwidth_khz`, `rx_mode` | Automatic tuning and receiver mode |
| `tx_polarity`, `sweep_step`, `sweep_steps`, `learned_polarity` | Polarity/diagnostic tuning fields; inactive step is null |
| `pairing`, `profile`, `paired`, `joined` | Link state and profile readiness |
| `node_id`, `doorbell_id`, `identity_compatible`, `chime_name` | Hex identities, signed-ID compatibility and name |
| `transmitted`, `transmit_errors`, `packets`, `crc_ti`, `crc_bad`, `crc_ccitt` | RF counters since restart |
| `dropped`, `invalid_lengths`, `false_syncs`, `radio_resets` | Receiver error/reset counters |
| `rings`, `last_ring_ms`, `ring_seq`, `gong_active`, `gong_ready`, `storage_ok` | Ring counters, last uptime or null, output and storage state |
| `auth_enabled`, `auth_user` | Authentication state |
| `setup_visible` | Device-wide Setup tab visibility, persisted across restarts |
| `mute` | `{active, permanent, remaining_s}`; remaining seconds rounded up |
| `time_synced`, `epoch`, `mqtt` | Clock state, UTC seconds or null and MQTT connection state |

IDs are numeric values rendered as eight hexadecimal characters. `pairing`
can be `idle`, `discovering`, `pair acknowledgement`, `joining`,
`join acknowledgement`, `joined`, or an error state string. Treat error text
as diagnostic text rather than a stable enumeration.

### GET `/api/log?from=<byte-offset>`

Returns **text/plain**, not JSON. The 32 KB RAM log is indexed by byte offset.
`X-Log-Next` supplies the next offset to request; an old offset clamps to the
retained range and a future offset clamps to the current end. Reboot resets
the log/offsets. Log text can include decrypted application payloads and names;
inspect exports before sharing.

### GET `/api/packets`

Returns `{"packets":[...]}` with at most 32 rows, **oldest first**. Each row
contains `id`, `ms`, `rssi_dbm`, `inverted`, `offset_khz`, `crc`, `hex`, `result`
and `epoch`. Synchronized entries also contain `epoch_ms`, the millisecond
fraction of the UTC receive time. Without clock sync `epoch` is null and
`epoch_ms` is omitted. Frequency offset is null in ESP32 bit-sampling mode.
Type-1 pairing payloads are redacted. CRC strings are `TI-8005 candidate`,
`CCITT diagnostic only` or `bad`; only TI CRC is accepted for protocol actions.

## Radio, audio and manual profile settings

### GET / POST `/api/settings`

GET returns the saved values below plus `wifi_password_set`, `profile`,
`paired` and `node_id`. It never returns the WiFi password or AES key.

| POST field | Type / range | Behavior |
|---|---|---|
| `frequency_hz` | Integer 863000000–870000000 or 902000000–928000000 | Carrier; 868000000 default |
| `trim` | Integer −128..127 | Approximately 1586.914 Hz/step |
| `auto_trim` | Boolean | Automatic tuning can replace manual trim |
| `rx_mode` | Integer 0/1 | CC1101 packet engine / ESP32 bit sampling |
| `polarity` | Integer 0/1/2 | Auto / normal / inverted RX |
| `melody` | Integer 0–2 | Local PWM/DAC melody |
| `volume` | Integer 0–100 | **PWM/DAC audio level**, distinct from app volume |
| `relay` | Boolean | Extra relay pulse in PWM/DAC modes |
| `relay_ms` | Integer 50–2000 | Extra pulse length |
| `ssid` | String ≤32 bytes | Save station SSID; restart for this route's WiFi changes |
| `wifi_password` | Empty or string 8–63 bytes | Empty/missing preserves password |
| `open_wifi` | Boolean | true clears station password |
| `source`, `destination` | Strings of 8 hex digits | Doorbell and chime IDs respectively |
| `pair` | String of 4 hex digits | Session value |
| `key` | String of 32 hex digits | AES key; empty/missing preserves it |
| `clear_profile` | Boolean | true removes the saved doorbell profile/key |

Radio changes restart reception; relevant changes also reinitialize the link.
Changes during pairing/join return 409; `clear_profile:true` cancels the
handshake first. A nonempty new key requires full source/destination/pair
strings in the same request and makes the profile passive. Changing IDs/session
also removes paired status. Manual profiles receive only; they cannot join or
transmit. Numeric hex ID `11223344` has on-air bytes `44 33 22 11`.

Example: `{"frequency_hz":868000000,"auto_trim":true,"rx_mode":0}`.

## Chime, sound and mute

### GET / POST `/api/chime`

GET returns `name`, `volume` (0–4), `led` (0/1), `joined`, `doorbell_id`,
`node_id`, `session` (four hex digits), `requests`, `last_opcode`,
`last_request_ms` (null until a request) and `uptime_ms`.
POST accepts only `name` (1–32 bytes, no control characters), `volume` (0–4)
and `led` (boolean or supported boolean value). Name/volume/LED persist and
the doorbell reads them on its next configuration request. This is **app volume**,
not the audio percentage in `/api/settings`.

### GET / POST `/api/gong`

New configurations default to Gong. The UI order is **Gong, Audio, None**.
Audio uses a separate `audio_output` setting (PWM or DAC), remembered even
while Gong or None is selected. Legacy saved type 0 migrates to Audio/PWM and
type 3 to Audio/DAC; saved Gong/None choices, GPIO assignments and levels remain.
The GPIO JSON key is `audio`, and MQTT state, commands and ring events use
`audio_level`. Home Assistant discovery replaces the old level entity with
Audio level and exposes Sound mode plus a separate Audio output selector.

| Field | Values |
|---|---|
| `type` | 0 Audio, 1 Gong (default), 2 None/silent |
| `audio_output` | 0 PWM (default), 1 DAC |
| `pattern` | 0 Ding-dong, 1 Double, 2 Triple, 3 Long ring, 4 Insistent |
| `pulse_ms` | 50–2000 |
| `gap_ms` | 100–3000 |

POST accepts only these integer fields. Selecting relay type without a relay
GPIO returns 409. Audio requires an audio GPIO; DAC output requires GPIO25 or
GPIO26. Invalid output wiring returns 409. Independent name, LED, volume and
mute changes remain available if an optional output pin is disconnected. GET also returns `relay_available`,
`pwm_available` and `dac_available` for the active GPIO map.

### POST `/api/gong/test`

Empty body or `{}` tests saved output at full app volume without saving
settings. Local tests bypass owner mute. A nonempty JSON object supplies
temporary overrides: `type` 0–2, `audio_output` 0–1, `melody` 0–2, `volume` **0–100 audio level**,
`pattern` 0–4, `pulse_ms` 50–2000, `gap_ms` 100–3000, `extra_relay` (boolean)
and `extra_relay_ms` 50–2000. Audio previews use the selected extra relay
settings; an enabled extra relay needs a Relay GPIO. Out-of-range preview
fields are ignored; this route does not strictly reject malformed/unknown
preview fields. Missing/unsupported output GPIO returns 409; busy/unavailable output returns
503. None mode returns success and logs a silent event.

### GET / POST `/api/mute`

GET returns `{active, permanent, remaining_s}`. POST accepts
`{"mute":true}` for persistent owner mute, `{"mute":false}` to unmute,
or `{"mute_minutes":45}` for a timed mute. Minutes range is 0–1440; zero
ends timed mute. Send one form per request. Timed mute is RAM-only and ends
on restart; permanent mute survives. This setting is separate from app volume 0.

### GET `/api/rings` and POST `/api/rings/clear`

GET returns `uptime_ms`, `epoch` (UTC seconds/null), `last_id`,
`rings_since_start` and `rings` (up to 50 entries, **newest first**).
Entries contain `id`, `source`, `outcome`, numeric `type` and `sound`,
`ms`, `epoch`, `previous_boot`, `volume`, `audio_level`, `preview`.
Audio entries also contain numeric `audio_output` (0 PWM, 1 DAC), including
legacy entries restored from flash. Doorbell entries add `tone` (received ringtone parameter) and `rssi_dbm`.

Sources are `doorbell`, `web`, `button`, `mqtt`, `serial`; outcomes are
`played`, `muted`, `silent`, `busy`, `chime_muted`. History is saved after a
quiet period and survives restart; uptime values from previous boots are
identified by `previous_boot`. Clear deletes history while event IDs keep
increasing; the RF ring counter since startup is separate.

## GPIO configuration

### GET `/api/pins`

Returns `pins` (active), `saved` (persisted) and `defaults`, each containing:

```json
{"sck":18,"miso":19,"mosi":23,"cs":5,"clock":4,"data":16,"audio":25,"relay":26,"led":2,"button":0,"relay_active_low":false}
```

### POST `/api/pins/check`, `/api/pins`, `/api/pins/reset`

Check and save accept a partial flat GPIO object, merged with the **active**
map. Check returns `{"ok":true|false,"issues":[{"role":"...","level":"error|warning|info","text":"..."}]}`
without persistence. Valid save adds a message and applies the map live without restarting.
Invalid save returns 400 with issues. Reset takes no body and saves defaults,
applying the default map live.

Pin integers must be −1..39, then pass hardware validation. Only audio, relay,
LED and button allow −1. Duplicate assignments, nonexistent pins, GPIO6–11,
UART GPIO1/3 and output roles on input-only GPIO34–39 are rejected. Button
GPIO34–39 needs an external pull-up. Strap and PSRAM pins can produce warnings.
With Audio/DAC selected, a connected `audio` pin other than GPIO25/26 is an
error; −1 is allowed with a warning that Audio is disconnected. Audio and Relay
cannot share a GPIO. The wiring UI uses one shared Audio output assignment and
shows the selected Audio method.
`relay_active_low` must be boolean. See [WIRING.md](firmware/WIRING.md).

## Setup visibility

`GET /api/status` includes `setup_visible`.

### POST `/api/setup`

Accepts exactly `{"visible":true}` or `{"visible":false}` and returns the saved
`{visible}` value. Authentication and CSRF checks apply. The preference persists
on the chime across restarts and applies to every browser; no reboot is needed.
New devices show Setup until Finish. Existing paired installations default to
hidden when upgrading. Settings → General can show or hide the tab explicitly.
The wizard hides only after sound/wiring and any requested authentication saves
succeed. A failed save keeps the final review available for retry. WiFi and
pairing remain independently available in Settings while Setup is hidden.

## Setup sound and output wiring

### POST `/api/setup/sound/check` and `/api/setup/sound`

Both accept the same object:

```json
{"chime":{"name":"Hallway","type":0,"audio_output":1,"melody":0,"pattern":0,"extra_relay":false},"pins":{"audio":26,"relay":-1,"relay_active_low":false}}
```

`chime` accepts only name, type, audio_output, melody, pattern and extra_relay,
with the ranges above. `pins` accepts only audio, relay and relay_active_low,
merged into the saved full GPIO map. Other fields are rejected. Hardware
validation checks duplicates and all reserved pins, including unchanged roles.
The selected output must have usable GPIOs; an enabled extra relay pulse
requires a Relay GPIO.

Check returns `{ok, issues, pins_changed, restart_required:false}` without
saving. The wizard uses it while preparing Step 3, then offers optional
authentication in Step 4 and final review in Step 5. Sound, wiring and sign-in
changes wait until **Finish** calls the setup save route. If authentication was configured, Finish
then calls `/api/auth`; Skip omits that call. After those saves succeed,
Finish calls `/api/setup` with `{"visible":false}`. Credential validation catches
invalid usernames, short passwords and mismatched confirmation before review.
A failed sign-in save keeps the review open for retry and reports that sound
and wiring were already saved. The current browser receives the normal new
session cookie when sign-in is enabled or credentials change.
Save validates both groups before writing, applies new GPIOs live with playback
inhibited, and returns the same fields plus a message. No ESP32 reboot is
scheduled. Radio remapping releases SPI and reinitializes reception, while
output/LED/button roles are reset to their configured idle/input states.
Update/restart or an active radio handshake can return 409; storage failures
return 500. Authentication and CSRF requirements apply to both routes.

## WiFi

### GET `/api/wifi/scan[?refresh=1]`

Starts/polls an asynchronous 2.4 GHz scan. While pending returns
`{"scanning":true}`. Complete results contain `scanning:false` and up to
24 distinct nonempty `networks`, each `{ssid,rssi,secure}`. Any `refresh`
query parameter starts a fresh scan. Poll without it to obtain results.

### POST `/api/wifi/connect`

Body: `{"ssid":"Home WiFi","password":"..."}`. SSID is 1–32 bytes;
password is empty for an open network or 8–63 bytes. **Empty/missing password
means open**, unlike `/api/settings`. Saves credentials, selects client mode
and starts connecting immediately while keeping/opening the setup AP. Read
`/api/status` to check actual connection success.

### GET / POST `/api/wifi`

GET returns `mode` (saved), `active_mode`, `ap_ssid`, `ap_password_set`,
`default_ap_ssid` and `button` (whether a recovery button is configured).
POST accepts only `mode` (`client`, `ap`, `off` or numeric 0–2), `ap_ssid`
(0–32 printable ASCII characters), `ap_password` (empty or 8–63 printable
characters) and `clear_ap_password` (boolean). Blank/missing password keeps
the saved custom password; clearing it restores sign-in-password/open-AP
fallback. All mode/hotspot changes apply after restart. Physical button/serial
recovery can turn WiFi back on when off.

## Authentication settings

| Route | Request / response |
|---|---|
| POST `/api/login` | `{username,password,remember}`; no CSRF; sets session cookie |
| POST `/api/logout` | No body/CSRF/auth required; clears this browser cookie |
| POST `/api/logout/all` | Auth + CSRF; invalidates old sessions and reissues current browser cookie if enabled |
| GET `/api/auth` | `{enabled,username,password_set}` |
| POST `/api/auth` | Partial `{enabled,username,password}`; blank password preserves it |
| POST `/api/password` | `{password}`; compatibility route for changing only password |

Username is 1–32 printable ASCII characters without a colon. Password is
8–63 printable ASCII characters. Enabling sign-in requires a saved password.
After three failed logins per client address, retries wait one second,
doubling up to five minutes. Login returns 429/`Retry-After` during throttle.
Credential changes renew the current browser cookie and invalidate old
credentials' cookies. The setup hotspot uses the sign-in password after
restart while sign-in is enabled. BOOT held 15 seconds or serial `a` disables
sign-in with physical access.

## Pairing, maintenance and OTA

| POST route | Behavior |
|---|---|
| `/api/pair/start` | Open five-minute pairing; 409 busy/radio unavailable; 503 unavailable persistent storage |
| `/api/pair/cancel` | Cancel pair/join without deleting saved credentials |
| `/api/join/start` | Rejoin saved paired profile; 409 if passive/unpaired, busy or radio unavailable |
| `/api/reboot` | Reply then schedule restart after about 500 ms |
| `/api/update` | Multipart app-only firmware upload, then restart |

Commands except update need no body. All require authentication when enabled
and the CSRF header. Poll `joined`, `pairing`, `storage_ok` and radio state.
Canceling/rejoining is distinct from removing a profile with `/api/settings`.

OTA uses `multipart/form-data` with a `firmware` file whose name ends in `.bin`.
Build/package instructions are in [README.md](README.md#firmware-updates).

```bash
curl --fail --user admin -H "X-CSRF-Token: $csrf" \
  -F 'firmware=@firmware/dist/firmware.bin' "$CHIME_URL/api/update"
```

Do not set a JSON Content-Type on multipart uploads. RX and playback pause
during writing. A rejected/aborted upload resumes operation/rejoin; successful
upload replies then restarts. Arduino Update validates the app image/partition;
there is no custom signature verification. USB installation/recovery is
required for a blank board or interrupted update.

## MQTT API

### GET / POST `/api/integrations`

Configure MQTT and NTP over authenticated HTTP with CSRF for POST.

| POST field | Constraints |
|---|---|
| `enabled`, `discovery`, `clear_password` | Booleans |
| `host` | ≤64 bytes; broker hostname/address required when enabled |
| `port` | Integer 1–65535, default 1883 |
| `username`, `password` | ≤64 bytes; empty/missing password preserves saved secret |
| `topic` | ≤64 bytes; empty selects `reolink_chime/<device-id>` |
| `prefix` | ≤32 bytes, nonempty, default `homeassistant` |
| `ntp` | ≤64 bytes, default `pool.ntp.org` |

Topic/prefix reject wildcards, spaces/control bytes, leading/trailing `/` and
empty levels (`//`). Host/NTP use the hostname-character validation in
`Mqtt.cpp`. `clear_password:true` removes the saved broker password. GET adds
`password_set`, `default_topic`, `state`, `dropped`, `time_synced` and `epoch`;
it never returns the password. Changes persist and reconnect MQTT when on WiFi;
NTP changes restart clock synchronization.

MQTT uses plain TCP, keepalive 30 seconds and reconnect interval 10 seconds.
It runs in client WiFi mode. A 32-message publisher queue keeps broker work
separate from RF reception; events are dropped while disconnected rather
than stored for later delivery.

### Topics

Let `<base>` be the configured `topic` or returned `default_topic`.

| Topic suffix | Direction | Retained | Payload |
|---|---|---|---|
| `/availability` | Publish | Yes | `online` / `offline`, with offline last will |
| `/state` | Publish | Yes | Current settings JSON, on change |
| `/status` | Publish | Yes | Diagnostics JSON, on change or every 60 s |
| `/event` | Publish | No | Each logged ring/test event |
| `/doorbell` | Publish | No | `ON` on each accepted nonduplicate doorbell event |
| `/result` | Publish | No | `{"ok":true}` or `{"ok":false,"error":"..."}` |
| `/set` | Subscribe | — | Partial JSON settings |
| `/set/<key>` | Subscribe | — | Single bare value or JSON scalar |
| `/play` | Subscribe | — | Play command, optional JSON overrides |
| `/get` | Subscribe | — | Payload ignored; republish state/status |

Example retained state:

```json
{"type":"Gong","audio_output":"PWM","melody":"Ding dong","pattern":"Ding-dong","audio_level":35,"volume":4,"led":true,"extra_relay":false,"extra_relay_ms":300,"pulse_ms":300,"gap_ms":700,"name":"Hallway","mute":false,"mute_minutes":0}
```

Status fields: `linked`, `paired`, `radio_ok`, `rings`, `last_ring` (UTC ISO
string or literal `None`), `muted`, `mute_until` (ISO string or `None`), `ip`,
`wifi_rssi`, `uptime_s`, `time_synced`, `firmware`.

Event fields: `event_type` (`ring` for doorbell, `test` otherwise), `id`,
`source`, `outcome`, labeled `type`, labeled `sound` (omitted for None),
`time` (UTC ISO/null), `volume`, `audio_level`, `preview`; doorbell events
add `tone` and `rssi_dbm`. MQTT uses names for type/sound, while HTTP ring
history uses integer indices and includes uptime/previous-boot fields. Audio
events include `audio_output` (PWM or DAC); it is omitted for Gong/None events.

### Commands and validation

| `/set` field | Accepted values |
|---|---|
| `type` | 0–2 or Audio / Gong / None |
| `audio_output` | 0–1 or PWM / DAC |
| `melody` | 0–2 or Ding dong / Ascending / Double beep |
| `pattern` | 0–4 or Ding-dong / Double / Triple / Long ring / Insistent |
| `volume` | 0–4 app volume |
| `audio_level` | 0–100 PWM/DAC audio percentage |
| `led`, `extra_relay`, `mute` | Boolean, 0/1, ON/OFF or true/false text |
| `extra_relay_ms`, `pulse_ms` | 50–2000 |
| `gap_ms` | 100–3000 |
| `name` | 1–32 bytes without control characters |
| `mute_minutes` | 0–1440 |

Enums accept indices or names, ignoring letter case and punctuation/spacing.
Unknown keys or invalid values reject the complete settings update. MQTT
cannot modify credentials, radio/GPIOs, WiFi, pairing, OTA or reboot. Use
non-retained commands for one-shot play requests.

`/play` with `{}` plays current output at app volume and obeys owner mute.
Optional `type`, `audio_output`, `melody`, `pattern` and `volume` (0–4) override only this
play. `force:true` bypasses owner mute but does not configure a missing output
or override app volume zero. Success/error is published to `/result`;
an outcome is also logged/published. The handler treats empty or malformed
play JSON as a default play, so validate payloads in clients.

```text
<base>/set          {"volume":2,"melody":"Ascending"}
<base>/set/led      OFF
<base>/set/mute_minutes  45
<base>/play         {"force":true,"volume":4}
<base>/get          {}
```

### Home Assistant discovery

Discovery publishes 22 retained configs under
`<prefix>/<component>/reolink_chime_<device-id>/<object>/config` and republishes
when `<prefix>/status` is `online`. Entities cover doorbell events, a five-second
Ringing indicator, Play button, permanent/timed mute, mute-until time, sound mode/
audio output/melody/pattern, volume/audio level, LED/extra relay, timings/name and diagnostics.
Disabling discovery publishes empty retained configs to remove entities.
NTP supplies UTC event/sensor timestamps; RF reception does not depend on MQTT
or clock synchronization.

The implementation is in [Web.cpp](firmware/src/Web.cpp),
[WebAuth.cpp](firmware/src/WebAuth.cpp),
[MqttService.cpp](firmware/src/MqttService.cpp) and
[ChimeControl.cpp](firmware/src/ChimeControl.cpp).
