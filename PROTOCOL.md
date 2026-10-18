# Reolink Chime V2 protocol

This document describes the bidirectional sub-GHz protocol implemented by
`reolink-esp32-chime` for **Reolink Chime hardware V2**. Pairing, join, reboot
rejoin, physical presses and main chime Test reception have been validated with
the 868 MHz WiFi doorbell setup described in [README.md](README.md). Other
hardware revisions and 915/920 MHz operation remain untested.

The terms **AP** and **node** below mean the RF doorbell and chime respectively;
RF AP is unrelated to the ESP32's WiFi access point. All multi-byte integers
are little-endian unless explicitly stated. Offsets in MAC tables exclude
the separate PHY length byte. Payload offsets start after the MAC header.

## Radio physical layer

| Parameter | V2 link |
|---|---|
| Modulation | Binary GFSK / 2-GFSK |
| Symbol rate | 25,000 baud; one bit per symbol, 25 kbit/s |
| Frequency deviation | 15 kHz |
| Carrier presets | 868.000, 915.000 and 920.000 MHz |
| Preamble | Four bytes `55 55 55 55` |
| Sync word | `93 0B 51 DE` (`0x930B51DE`) |
| Bit order | MSB first within each byte |
| Polarity | Positive frequency deviation represents logical 1 |
| Whitening | PN9, reset to `0x1FF` each packet |
| Error correction | No FEC |
| Length | Separate PHY length byte followed by a MAC with its own length byte |
| Integrity check | CRC-16 polynomial `0x8005`, details below |
| V2 receive bandwidth | Approximately 68.3 kHz at 868 MHz; 72.0 kHz at 915 MHz |

The carrier is a selected configuration. No automatic frequency-hopping scheme
is implemented or established here. The ESP32 accepts 863–870 MHz or 902–928 MHz
as configuration ranges, with 868 MHz the default. These are software bounds,
not a list of verified doorbell channels or regional transmission approvals.
Use a matching module and antenna for the doorbell's carrier.

The original V2 radio is a TI CC1312R-class device. This implementation uses
the ESP32 for protocol processing/AES and a CC1101 as the RF transceiver.
The CC1101 supports configurable GFSK and SPI packet FIFOs; see the
[TI CC1101 datasheet](https://www.ti.com/lit/ds/symlink/cc1101.pdf).

### CC1101 implementation

The CC1101's native 32-bit sync mode repeats its 16-bit sync word; it cannot
directly match this protocol's four distinct sync bytes. The default receiver
matches `930B` in hardware and verifies `51DE` in software, then reads the
whitened length/body/CRC through the FIFO. It uses infinite packet length and
disables native whitening/CRC so the software performs the V2 checks.

With a 26 MHz crystal, the receiver uses 81.25 kHz bandwidth after calibration
and 162.5 kHz during automatic-trim acquisition. Valid packets from the configured
doorbell feed a five-sample median; three samples permit the first calibration.
Subsequent adjustments require at least two trim steps and 60 seconds. Automatic
trim is clamped to ±40 steps; manual trim permits −128..127 at approximately
`26,000,000 / 16,384 = 1,586.914 Hz` per step. Thirty minutes without doorbell
frames returns to wide acquisition.

The optional ESP32 synchronous-bit receiver searches full sync in either
polarity and needs GDO0 clock/GDO2 data. It cannot measure frequency offset,
so automatic trim pauses. TX uses normal polarity. The firmware selects
CC1101 PATABLE `0xC0` at 868 MHz and `0x8E` in the 902–928 MHz range; these are
module register settings, not measured radiated power. TX streams frames
longer than the 64-byte FIFO and restores reception afterwards.

## On-air packet envelope

Before whitening, a packet is:

```text
55 55 55 55 | 93 0B 51 DE | L | MAC[0..L-1] | CRC_hi CRC_lo
                             \____________________________/
                               PN9 whitening covers this
```

`L` counts the complete MAC, including its own length byte. `MAC[0]` must equal
`L`. Preamble and sync are neither encrypted nor whitened. Software packet
objects contain de-whitened `L + MAC + CRC`, without preamble/sync.

The replacement bounds MAC length to **2–96 bytes**. Header-specific parsers
apply tighter lengths. This implementation bound is not a claim about every
packet supported by original devices.

### CRC

CRC covers the **PHY length byte followed by all unwhitened MAC bytes**.
Initialize to `0xFFFF`, process bytes MSB first with polynomial `0x8005`, do not
reflect, apply xor-out zero, and append high byte first. Compute the CRC before
whitening. The two CRC bytes are whitened with the same continuing PN9 stream.

```python
def crc16(data):
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ (0x8005 if crc & 0x8000 else 0)) & 0xFFFF
    return crc
```

CRC-CCITT/`0x1021` appears only as a diagnostic candidate in this firmware;
it is never accepted for pairing, application data or ring playback.

### PN9 whitening

Reset the 9-bit state to `0x1FF` after sync. For each byte, XOR with the low
eight bits of the state, then advance the LFSR eight times:

```python
def pn9_byte(state):
    mask = state & 0xFF
    for _ in range(8):
        feedback = ((state >> 5) ^ state) & 1
        state = (state >> 1) | (feedback << 8)
    return mask, state
```

Use consecutive masks for length, MAC and CRC. Whitening/de-whitening are the
same XOR operation. Do not reset at the MAC or CRC boundary. AES applies only
to the application payload; whitening also covers the clear MAC headers.

## MAC headers and identities

Every MAC begins with `length:u8` and `type:u8`. Interpret frame type as
`type & 0x3F`; the implementation emits zero in the upper two bits.

| Type | Header length | Fields after length/type | Payload |
|---|---:|---|---|
| 0 | 8 | Reserved u16 at +2, chime ID u32 at +4 | Discovery has none |
| 1 | 8 | Reserved u16 at +2, addressed/originating chime ID u32 at +4 | Plaintext pairing |
| 2 | 12 | Reserved u16 at +2, chime ID u32 at +4, doorbell ID u32 at +8 | AES-encrypted join exchange |
| 3 | 16 | Session u16 at +2, chime ID u32 at +4, doorbell ID u32 at +8, sequence u32 at +12 | AES-encrypted data |
| 4 | At least 16 accepted here | Addressed chime/doorbell IDs at +4/+8 | Disconnect/rejoin indication |

The ID byte positions are the same in both directions: +4 is the chime and
+8 is the doorbell. For doorbell-originated types 2/3 these mean destination
then source; for chime-originated frames they mean source then destination.
Type 1 carries the doorbell ID inside its payload.

A stable nonzero 32-bit node ID identifies the custom chime. Generate IDs in
**1..0x7FFFFFFF** because the tested doorbell's physical-ring selection rejects
IDs with the sign bit set. The node ID persists across reboot/OTA. A session
is a separate 16-bit value assigned on join and can change after reboot/rejoin.

## Pairing: discovery and key exchange

Pairing requires the doorbell's normal add-chime flow and an explicitly opened
pairing window on the ESP32. It is not passive key discovery.

```text
ESP32 chime                         Doorbell
   |---- type 0 discovery ------------->|
   |<--- type 1 key/ID response --------|
   |---- type 1 key echo ACK ---------->|
   |---- type 2 encrypted join -------->|
   |<--- type 2 session response -------|
   |---- type 2 final ACK ------------->|
   |     save profile; joined           |
```

Discovery is an eight-byte MAC:

```text
08 00 00 00 [chime_id:u32le]
```

The doorbell answers with a **32-byte type-1 MAC**, including a 24-byte plaintext
payload. The header identifies the target chime.

| Payload offset | Length | Pair response |
|---|---:|---|
| +0 | 1 | Opcode `00` |
| +1 | 1 | Heartbeat mode |
| +2 | 2 | Opaque fields, preserve unless changed below |
| +4 | 4 | Doorbell ID, u32le |
| +8 | 16 | Fresh individual AES-128 key |

The chime checks length, its addressed ID, opcode and a valid distinct doorbell
ID. It copies the payload, changes bytes +0/+1/+2 to **`01 00 00`**, preserves
byte +3, ID and key, then sends the 24-byte payload in a type-1 ACK. The doorbell
checks the echoed ID/key before installing the pairing key.

The custom discovery interval is two seconds; the pairing window is five
minutes. Repeated matching key responses may be acknowledged again without
replacing a joined profile. An unrelated reply is ignored. Candidate credentials
remain temporary until the final encrypted join ACK transmits successfully.
Cancel/timeout retains the previous saved profile, though a doorbell that has
already accepted a new key may require pairing again.

**The pairing key is sent in plaintext over RF.** There is no universal key in
the firmware and no key agreement or authenticated enrollment layer here.

## Encryption

Types 2 and 3 encrypt only the bytes after their respective headers using
**AES-128 CTR**, a 16-byte pairing key and an **all-zero 16-byte initial counter**.
Each payload starts a fresh CTR operation. There is no transmitted IV, padding,
authentication tag or encrypted MAC header. Ciphertext length equals plaintext
length. Encryption and decryption both XOR the AES counter keystream.

```python
# Example with PyCryptodome; key and payload are bytes.
from Crypto.Cipher import AES
ciphertext = AES.new(key, AES.MODE_CTR, nonce=b'', initial_value=0).encrypt(payload)
```

The first counter block is 16 zero bytes; standard CTR increments the counter
for later blocks. `LinkService.cpp` uses mbedTLS AES-CTR and host link tests
exercise real AES. The RF sequence/session fields are **not** an IV and are
not mixed into the counter.

CRC detects transmission errors; it is not a cryptographic authenticator.
Reusing the zero counter with a fixed key reuses the keystream. Payload
shape/address/session checks and retry suppression do not provide modern
authenticated encryption or durable replay protection. This describes V2
compatibility behavior, not a recommended design for a new protocol.

## Join and rejoin

A saved paired node can join without repeating plaintext pairing. Type-2
headers carry chime/doorbell IDs with zero reserved bytes. The firmware
compatibility version is **`0x32413030`**, whose little-endian bytes are ASCII
**`00A2`**. This is separate from this project's semantic firmware version
and is not a boot nonce.

| Direction | Plaintext length | Plaintext bytes |
|---|---:|---|
| Chime → doorbell | 8 | `02 00 00 00 [compatibility_version:u32le]` |
| Doorbell → chime | 12 | `03 06 [session:u16le] 00 00 00 00 [opaque:4]` |
| Chime → doorbell | 12 | `04 06 [session:u16le] 00 00 00 00 [compatibility_version:u32le]` |

Request MAC length is 20 bytes; response and ACK MAC lengths are 24. The node
checks addressing, length and decrypted response prefix, installs the session
from +2 and changes opcode 3 to 4 while replacing the final word with its
compatibility version. Successful ACK TX commits the profile and resets TX/RX
counters. The protocol has no established additional doorbell confirmation
after that ACK. Matching duplicate responses receive another ACK without
resetting an already established session.

The doorbell sets the chime online only when this ACK arrives (`0x7d0c`,
`net_respone_deal`, calls `device_join_net`); a lost ACK leaves the chime
joined on its side and offline on the doorbell's. The doorbell sends its
response in quick repeats (copies about 23 ms apart were observed) and
retries it about every 0.4 s until the ACK arrives, so an ACK sent right after
the first copy can be lost, and a chime that cannot decode the retries never
answers them. The ESP32 therefore sends the ACK twice more, 300 ms apart.

The custom implementation retries join requests every two seconds, at most
ten successfully transmitted requests, with a 22-second deadline outside the
pairing window. Radio TX failures retry after 200 ms and terminate after three
failures. These are implementation limits. Reboot and disconnect trigger rejoin
with saved IDs/key. Heartbeat mode zero sends encrypted type-3 plaintext **`0D`**
about every 25 seconds. Nonzero mode disables that schedule; broadcast interval
negotiation is not implemented.

Addressed type-4 disconnect indications and encrypted type-3 opcode **`16`**
(decimal 22) initiate rejoin while retaining the pairing key. The replacement
uses addressed type-4 frames as a rejoin trigger; it does not implement a
general type-4 payload decoder.

### Offline detection on the doorbell

The doorbell's radio keeps a state per chime in its pairing table; `2` means
online, and the doorbell's Linux side rings and reports a chime only in that
state. Each valid data frame from a chime that is online refreshes its
timestamp (`0x9670`, called from the data receiver at `0x0a7a`). A check in the
main loop (`0x998c`) sets every online chime whose last frame is 180 seconds
or more old (`0x112a881` ticks of 10 µs) to `1`, offline. Frames from an
offline chime no longer refresh anything: only a new join (`0x82b4`,
`device_join_net`) sets the state back to `2`. The doorbell sends the chime
nothing when this happens, so a chime that keeps sending heartbeats believes
it is still joined while the doorbell no longer rings it. Addresses refer to
the doorbell radio image (`db_full.elf`, SHA-256 `db8b06ff…2811`).

The ESP32 therefore joins again by itself when a joined doorbell has not
been heard for five minutes, and retries a failed join (no answer, or three
failed transmissions) after one minute. A failed pairing is not retried.

## Ring data

A ring is an exact **20-byte type-3 MAC**. Its decoded packet including PHY
length/CRC is 23 bytes; including preamble/sync the transmitted envelope is
31 bytes, about 9.92 ms at 25 kbit/s before radio/scheduler overhead.

| MAC offset | Bytes | Field |
|---|---:|---|
| +0 | 1 | `14` (decimal 20) |
| +1 | 1 | `03` |
| +2 | 2 | Session ID, u16le |
| +4 | 4 | Destination chime ID, u32le |
| +8 | 4 | Source doorbell ID, u32le |
| +12 | 4 | Sequence, u32le |
| +16 | 4 | AES-CTR ciphertext |

Decrypted ring payload:

```text
09 ?? [ring_parameter:u16le]
```

Opcode `09` means ring. Byte +1 is unspecified and must not be required to
equal zero. Bytes +2/+3 contain a music/ringtone parameter; they are neither
padding nor an ACK field. The ESP32 accepts IDs 0–9 and selects the corresponding
local melody or relay pattern for that ring, retaining the configured output
mode, level, timings and app volume. With the owner's ringtone override on, it
plays the saved melody/pattern instead. It records both the ID and the played
sound. Received rings never change saved settings.

The vendor playback table has ten entries. Although its opcode-9 handler
admits ID 10, that lookup reads a byte of the following pointer array; the ESP32
rejects it and other unsupported IDs before counting or playing a ring. Original
audio is controlled through serial commands to a separate sound device, so
the ESP32 uses synthesized alternatives. See [static firmware evidence](firmware/docs/vendor-sounds.md).

The doorbell queues ten transmission rounds retaining the same sequence on
repeats, with a requested approximately 10 ms delay between rounds. Actual RF
counts/timing depend on the radio worker. There is **no ring RF ACK**.

Acceptance requires TI CRC, correct length/type, exact configured chime ID,
doorbell ID and session, usable profile/decryption, and opcode 9. A volatile
32-entry cache suppresses repeated `(doorbell_id, session, sequence)` tuples
for five minutes. This prevents repeated playback; it is not replay protection.
Mute, app volume zero, silent mode and busy output still generate log outcomes
for accepted nonduplicate rings.

## Application configuration data

Application requests/replies travel as encrypted type-3 frames under the active
session. Opaque request fields are echoed where replies preserve them. All
lengths below are **plaintext payload lengths**, excluding the MAC header.

| Opcode | Request bytes | Reply bytes | Behavior |
|---|---:|---:|---|
| `0B` | 4 | None | Set app volume from byte +1, 0–4 |
| `0C` | 4 | None | Set LED from byte +1, 0/1 |
| `0E` | 35 | None | Set 32-byte name from +1 |
| `0F` | 36 | 36 | Read name; reply +1 = 0, name at +4 |
| `13` | 56 | 56 | Read full config |
| `14` | 20 | 20 | Read compatibility version and board identity |
| `15` | 56 | 8 | Write full config, success status at +4 |

Full config contains LED as **u32le at +12**, volume as **u32le at +16** and
a 32-byte name at +24. Read-config response +4 is status `u16(0)` and +6
is config length `u16(48)`. Both full u32 values are validated, even though
their ranges are small. Read-version response +4 is `u16(0)`, +6 is `u16(20)`,
+8 is `0x32413030`, and +12 is the eight ASCII bytes **`CC13x2R1`**.

Names are stored with an extra NUL terminator locally. Invalid request lengths,
out-of-range settings and insufficient reply capacity cause no partial update.
Unknown opcodes are not handled. Replies retain opaque request bytes outside
the explicitly written fields. The replacement does not implement every frame
type or vendor feature; broadcast timing negotiation and vendor RF firmware
distribution are outside its supported feature set.

## Several doorbells per chime (unverified)

The frame format does not tie a chime ID to one doorbell. Every type-2, -3 and
-4 frame names both the chime (+4) and the doorbell (+8); each doorbell issues
its own AES key during pairing and its own session during join. One chime ID
can therefore hold independent associations with several doorbells, each with
its own key, session, sequence counters, heartbeat and rejoin, identified by
the (chime ID, doorbell ID, session) triple.

- A further doorbell is added with the normal discovery, key and join
  exchange. Discovery (type 0) names no doorbell, so whichever doorbell is in
  its add-chime flow answers. A doorbell installs a new key only after the
  chime echoes it, so an existing association keeps its key as long as a
  second pair response from the same doorbell is not acknowledged.
- Sequence numbers and sessions are per association and may coincide between
  doorbells, so ring retry detection must include the doorbell ID.
- Each crystal has its own offset. Frames from several doorbells arrive at
  slightly different carriers, so the receive bandwidth must cover their
  spread, and a transmission is best tuned to the addressed doorbell's offset.
- Doorbells do not exchange chime lists over this RF link. Whether a doorbell
  objects to its chime also being associated with another doorbell is unknown;
  none of this has been verified with real devices.

## Relaying (unverified)

The link has no relay concept: no hop count, no transmitter address separate
from the chime/doorbell IDs, and no time-to-live. It nevertheless permits a
transparent relay on the same carrier:

- The PHY length, MAC header and CRC are not encrypted, and the AES-CTR
  keystream depends only on the key. A third device can de-whiten a frame,
  check its CRC and transmit the same bytes again without the key; the
  receiver cannot tell a relayed frame from a direct one.
- Types 2-4 use one header layout in both directions, and their payloads are
  encrypted with the association's key. Without the key, the direction is only
  partly visible: a 20-byte type-2 MAC is a chime's join request, but a 24-byte
  one may be the doorbell's response or the chime's acknowledgement, and type-3
  lengths overlap between rings, heartbeats and settings exchanges. A relay for
  one association therefore has to forward both directions.
- Pairing can be relayed too, with two limits. Type 0 discovery names only
  the chime, so a relay cannot attribute it to a doorbell and can only bound
  it in time, for example to a window opened while the doorbell is in its
  add-chime flow. The two 32-byte type-1 frames name the doorbell at MAC +12
  (payload +4) and differ in the payload opcode: `00` for the doorbell's key
  response, `01` for the chime's echo. Both carry the AES key in plaintext, so
  relaying them extends the area in which the key can be received; pairing
  within the doorbell's direct range avoids that.
- Without a hop count, a relay must suppress frames it has already forwarded
  to avoid loops between relays or with its own retransmissions.

### Timing

Radios on one carrier are half duplex, so a relay must receive a frame
completely before sending it. At 25 kbit/s a frame of `n` MAC bytes occupies
`(n + 11) × 0.32 ms` with preamble, sync, PHY length and CRC: 9.92 ms for a
ring (20 bytes), 11.2 ms for a 24-byte join frame and 34.2 ms for the 96-byte
maximum. Each relayed hop adds at least that time plus the radio turnaround.

A ring is queued as ten rounds with the same sequence about 10 ms apart, so a
relay transmitting during those rounds overlaps the remaining ones; waiting for
them to end delays the ring by roughly 200 ms. Rings have no acknowledgement
and repeats are recognised by their sequence, so a delayed or duplicated ring
is harmless. Type-2 frames carry no sequence and use a zero CTR counter: a
retried join request is byte-identical to the previous one, as is a repeated
response for the same session, so a relayed copy cannot be told from a retry.
Discovery frames are byte-identical every time. A pairing relayed in both
directions adds one hop to each of discovery, key response, echo and the
three join frames.

### Duplicates and collisions

A device within range of both the sender and the relay receives each frame
twice. Rings are deduplicated by sequence. A repeated join response with the
same session is acknowledged again (see [Join and rejoin](#join-and-rejoin));
how an original doorbell
handles a repeated join request or acknowledgement, and whether it accepts its
own frames heard back from a relay, is unknown. A relayed copy can also
coincide with the receiver's reply to the direct copy. Relaying at least doubles
the airtime of the relayed association, which counts towards the duty-cycle
limit of the band (1% in 868.0-868.6 MHz under ETSI EN 300 220).

None of this has been verified with original devices.

## Implementation map

| Source | Protocol role |
|---|---|
| [Protocol.cpp](firmware/src/Protocol.cpp) | PN9, CRC, envelope, ring parsing and duplicate cache |
| [Link.cpp](firmware/src/Link.cpp) | Pairing, join, heartbeat, retry and addressing state machine |
| [Doorbells.cpp](firmware/src/Doorbells.cpp) | One link per doorbell: frame routing, transmit turns, slots |
| [Relay.cpp](firmware/src/Relay.cpp) | Range extender: frame selection, repeat suppression, quiet wait, rate limit |
| [AppProtocol.cpp](firmware/src/AppProtocol.cpp) | Name/volume/LED/config/version payloads |
| [LinkService.cpp](firmware/src/LinkService.cpp) | AES, persistent profiles and ring/app dispatch |
| [Hardware.cpp](firmware/src/Hardware.cpp) | CC1101 registers, SPI reception and TX |

Protocol and link host tests cover framing, polarity/alignment, malformed
lengths, CRC gates, AES handshakes, duplicate replies, timeouts and retry paths.
They complement the tested 868 MHz configuration; they do not validate other
hardware models, carriers or audible output.
