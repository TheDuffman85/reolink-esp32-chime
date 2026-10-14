# Original Reolink chime tone catalog audit

The supplied chime firmware contains **10 defined selectable tone slots**, with
RF music IDs **0–9**. It sends a serial sound command for each slot. The audited
controller firmware does not supply note/duration data or PCM samples for these
tones; reproducing their exact sound requires additional audio assets or a
recording of the original chime. Replacement PWM melodies and relay patterns
are local equivalents, whose audible similarity has not been established.

## Artifact and evidence method

Audited artifact: `analysis/dd_full.elf` from the original-firmware investigation
in `Reolink_Video_Doorbell`. This is a generated ELF wrapper for the chime
CC1312R firmware, not a vendor-distributed ELF. SHA-256:
`603d72d08d0ea265e30187af483426193e833f459008a4ea44ac7af919adb87e`.
Its `.text` section has virtual base `0`, file offset `0x80`, length `0xef84`.
All addresses below are virtual addresses in that section. Findings were
regenerated directly from bytes with Capstone Thumb disassembly and the retained
`analysis/tools/thumbdis.py`. This is static instruction evidence; it does not
observe an original chime executing or identify the external sound IC model.

## Defined catalog and the RF handler's extra accepted value

**E1 — RF handler, `0x571c–0x5768`:** `msg_music_process` loads the little-endian
16-bit music ID at payload offset `+2` (`ldrh r5, [r4, #2]`, `0x573c`). Its
`cmp r5, #0xb` / `blo` at `0x573e–0x5740` admits values 0–10. It queues command
5 plus that ID at `0x575a–0x5768`. This bound by itself does not establish an
11-entry user catalog.

**E2 — Playback worker, `0x7be6–0x7c18`:** the worker loads the queued ID from
`+0x14`. At `0x7c10–0x7c12`, it loads table address `0xecae`, then uses
`ldrb r0, [r0, r4]`. At `0x7c18` it branches to sound-command wrapper `0xd0ae`.
The 10 table bytes, virtual `0xecae–0xecb7` (file `0xed2e–0xed37`), are:

| RF music ID | Sound command byte |
| --- | --- |
| 0 | `0x07` |
| 1 | `0x02` |
| 2 | `0x03` |
| 3 | `0x24` |
| 4 | `0x25` |
| 5 | `0x29` |
| 6 | `0x2a` |
| 7 | `0x2b` |
| 8 | `0x2c` |
| 9 | `0x2d` |

**E3 — Table boundary, `0x42e8–0x4302`:** code at `0x42e8` loads `0xecb8` as a
separate two-entry pointer array. At `0x42fe`, it indexes that array with
`ldr.w r8, [r5, sl, lsl #2]`; subsequent instructions dereference the selected
pointer. Bytes at `0xecb8` are `bc 39 00 20 c0 39 00 20`, the little-endian
RAM pointers `0x200039bc` and `0x200039c0`. Thus ID 10 reads `0xbc`, the low byte
of an unrelated pointer, rather than a defined eleventh tone entry. It should
not be promoted to an extra melody or claimed to be a system sound.

**E4 — Physical selection button, `0x16b8–0x16fa`:** `check_key` updates the
selected tone stored at `0x200058a0`. It wraps forward from 9 to 0 and backward
from 0 to 9 (`0x16bc–0x16da`), then indexes exactly the same `0xecae` byte table
at `0x16f0–0x16f6`. This independently establishes a 10-slot selectable catalog.

## Audio generation boundary

**E5 — Sound command driver, `0xd0ae` and `0x8a92–0x8af2`:** wrapper `0xd0ae`
branches to `0x8a92`. The latter drives GPIO number 23 through `0xca3c`, whose
`0xca40–0xca42` writes the value to GPIO byte alias base `0x40022000`. It sends
a high/low preamble with delay arguments 8000 and 1000, then loops eight times.
Each iteration inspects command bit 0 (`0x8ac6`), chooses high/low delay
arguments 1500/500 or 500/1500, and right-shifts the command (`0x8aee`). This is
an eight-bit LSB-first serial control transfer, not synthesis of each tone's
individual notes or streaming of waveform samples.

The finite command transfer and separate volume-command path (`0xc520–0xc536`)
strongly indicate an external sound device holding the audio. Static controller
bytes establish the control boundary; the sound device model and its ROM
contents remain unknown. No original tone names, frequencies, note sequences,
or waveform fidelity can be assigned from these command IDs alone.

## Reconstruction decision

Expose 10 local PWM melodies and 10 local relay patterns, mapped one-to-one to
RF IDs 0–9. Describe them as equivalents rather than the original sampled
library. Reject unsupported IDs, including 10, safely; the vendor's permissive
handler bound must not become an out-of-range lookup in the replacement.
