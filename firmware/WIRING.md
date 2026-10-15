# ESP32 + CC1101 wiring

![Radio wiring](wiring.svg)

Updated 2026-10-14. This diagram and the table below show the **default GPIO
map**. Settings → Advanced → Wiring can change assignments; saved changes apply immediately without a restart.

The diagram identifies signals, not header positions. GERUI module pin order
can vary: follow the silkscreen on your module. This target is an ESP32-WROOM-32
`esp32dev` board with a 26 MHz CC1101 module crystal. **CC1101 VCC and all radio
signals are 3.3 V only.** Power the ESP32 by USB-C; join all low-voltage grounds.

## Module and board pinouts

![CC1101 868 MHz module pinout](docs/images/cc1101-868mhz-pinout.jpg)

![ESP32-WROOM-32 38-pin board pinout](docs/images/esp32-38pin-pinout.png)

Both references show the component side. Use the printed GPIO labels on your
board, not the diagram's grey pin-number boxes: G4 / IO4 is GPIO4 and G16 / IO16
is GPIO16. Module revisions can differ; follow signal labels. Image sources
are recorded in [docs/README.md](docs/README.md).

## Default radio connections

| CC1101 label | ESP32 label | Reference wire colour | Function in this firmware |
|---|---|---|---|
| VCC | 3V3 | Orange | Optional decoupling: 100 nF ceramic + 10 µF across VCC/GND near module |
| GND | GND | Yellow | Common ground |
| SCK | GPIO18 | Blue | SPI clock |
| SO / MISO | GPIO19 | Purple | SPI data / CHIP_RDYn |
| SI / MOSI | GPIO23 | Green | SPI data |
| CSN / CS | GPIO5 | White | Chip select; optional 10 kΩ pull-up to 3V3 |
| GDO0 | GPIO4 | Grey | Serial clock in ESP32 receive mode; otherwise unused |
| GDO2 | GPIO16 | Added wire; colour unspecified | Serial data in ESP32 receive mode; otherwise unused |

The reference wiring uses GDO0 on G4 and GDO2 on G16. GPIO2 is reserved for the onboard status LED. With the default
receiver (CC1101 packet engine, 1.0.7+) the CC1101 receives through its FIFO
over SPI and neither GDO wire is needed; they may stay connected. The optional
**ESP32 bit sampling** receiver (Settings → Advanced → Radio → Receiver, 1.0.10+;
the only receiver up to 1.0.6) samples the recovered clock (GDO0) and data
(GDO2) and requires both.
With grey GDO0 on GPIO4, the radio no longer drives the GPIO2 boot strap during
USB flashing; the grey wire can stay connected for uploads.

Use short leads and an antenna suited to 868 MHz (915 MHz module and antenna
for the untested 915/920 MHz bands). GPIO5 is a boot strap; an
optional radio CS pull-up can keep it high at reset. GPIO16 is available on
ESP32-WROOM boards; this wiring is not intended for ESP32-WROVER/PSRAM boards.

## Audio mode for the built-in melodies

**Gong** is the default sound mode. Choose **Audio** under Sound (or Setup)
to play Ding dong, Ascending or Double beep through an external amplifier.
Within Audio, choose **PWM** or **DAC** as the Audio output method. Both use the
same **Audio output** GPIO, default GPIO25, with a shared melody and 0–100%
audio level scaled by the Reolink app volume (0–4). The selected method is
remembered while Gong or None is selected.

```text
ESP32 audio GPIO ---- filter / AC coupling ---- amplifier audio input
ESP32 GND ----------------------------------- amplifier signal ground
Separate rated supply ----------------------- amplifier power
Amplifier speaker output -------------------- speaker
```

PWM uses LEDC square-wave tones on any valid output GPIO. Use an amplifier
input compatible with a 0–3.3 V signal and the filtering/AC coupling specified
for it. The level changes PWM duty from zero to 50%; it is not a fixed DC output.

DAC uses the ESP32's [built-in 8-bit converter](https://docs.espressif.com/projects/esp-idf/en/v4.4.4/esp32/api-reference/peripherals/dac.html)
on **GPIO25 or GPIO26 only**.
The firmware streams 16 kHz sine-wave samples using internal I2S DMA; external
I2S amplifier modules are not supported. Audio is centred around midscale
(approximately 1.65 V while playing), so AC coupling is required unless your
amplifier already provides it. Use the amplifier's specified input network.
DAC voltage follows the ESP32 supply and is not a precision voltage reference.

Do not connect a speaker or inductive load directly to an ESP32 GPIO. Give the
amplifier a suitable supply; the CC1101 still receives **3V3**. If you choose
GPIO26 for audio, move or disable the relay assignment first: the two roles
cannot share a pin. PWM/DAC pins return low after playback or interruption.

## Gong mode (default)

GPIO26 is the default relay output, **active high** by default and switchable to
**active low** under Settings → Advanced → Wiring (GPIO). Audio mode defaults to no
extra relay pulse; its optional pulse is 50–2000 ms. **Sound → Gong**
uses the relay for one, double, triple, long or five-strike patterns. Strike
length is 50–2000 ms and gap 100–3000 ms (defaults 300/700 ms). Long ring is
four times the strike length, capped at 3000 ms; Insistent halves the strike/gap
with minimums 50/100 ms. Check the gong ratings before selecting these patterns.

Use a relay module whose control input accepts 3.3 V logic. Connect GPIO26 to
the module's control input and power the module as its manufacturer specifies.
Never drive a relay coil directly from GPIO26.

Connect the relay's **COM and normally-open dry contacts** across the existing
low-voltage gong's pushbutton terminals. Keep the gong circuit on the contact
side of the relay; it does not connect to ESP32 GPIO or 3V3. This diagram covers
low-voltage circuits only.

Common optocoupler modules with a `JD-VCC` jumper (e.g. AZ-Delivery 1-/2-relay
modules) are **active low**: set the relay to **Active low** in the wiring
settings and save; the level applies immediately. **Remove the `JD-VCC` jumper**: `JD-VCC` → 5 V/VIN and its
`GND` → GND (relay coils); 4-pin header `VCC` → **3V3**, `GND` → GND, `IN1` →
GPIO26. The input LED then sees 0 V when the GPIO is HIGH and about 2 V (a few
mA) when it is LOW. With the jumper fitted and `VCC` on 5 V, a 3.3 V HIGH leaves
about 1.7 V across the input LED; on the tested AZ-Delivery module that kept the
relay switched on with either level. The firmware sets the idle level before
enabling the output and returns to it after every pulse; during reset the GPIO
floats and the relay stays off, so no extra pull resistor is needed. Avoid strap
pins (GPIO0, 2, 5, 12, 15) for an active-low module. The relay coil draws about 70 mA from 5 V. Gong pushbutton
terminals go to the relay's **COM** and **NO** contacts; low voltage only.

## Configurable GPIO map

Under **Settings → Advanced → Wiring (GPIO)**, choose assignments and save.
Changes apply live: playback pauses, old pins are released and the radio is
reinitialized when needed. Power off before moving wires.
GET `/api/pins` reports the active and saved maps. **Use defaults** edits the UI form; save it to persist.
Invalid stored maps fall back to the default map above.

Setup also exposes output wiring for Gong or Audio. The sound step prepares
the choices; the final **Finish** button saves and applies them. GPIO changes
do not reboot the ESP32. Preview buttons stay disabled for changed wiring
until Finish applies it; the completion screen then offers **Test sound**.
If an unused output occupies the selected GPIO, Setup shows that its old
assignment will be disconnected. Radio, LED and button assignments remain
available through **Advanced wiring**.

The **Relay** GPIO drives Gong mode and the optional extra pulse in Audio mode.
The shared **Audio output** GPIO drives PWM or DAC, selected under Sound.
When Audio uses DAC, its GPIO selector offers GPIO25/26 and **Not connected**;
saving any other connected Audio GPIO is rejected. GPIO26 cannot serve both
Audio and Relay, so move or disable Relay before assigning it to DAC audio.
PWM permits any valid output GPIO. The wiring hint shows the saved Audio method.

All six radio signal roles require valid GPIO assignments; the default packet
receiver does not require the two GDO wires. Audio, relay, LED and button can be
**Not connected** (API value −1). Validation rejects duplicates, unavailable GPIOs,
flash GPIO6–11, UART GPIO1/3 and output roles assigned to input-only GPIO34–39.
A button on GPIO34–39 needs an external 10 kΩ pull-up to 3V3. The UI explains
strap-pin reset constraints and GPIO16/17 PSRAM use on WROVER. Keep the default
WROOM default wiring unless you need a different assignment. Custom maps have
not been validated on the tested hardware.

## Local controls

| Control | Behavior |
|---|---|
| BOOT / GPIO0, release after 30 ms–2 s | Test the saved sound settings (slot 1 unless the ringtone is overridden) |
| BOOT, release after 2–8 s | Start a five-minute pairing window |
| BOOT, hold at least 8 s | Clear WiFi credentials and enable setup AP; keep profile and sign-in |
| BOOT, hold at least 15 s | Also turn web sign-in off and reopen the setup AP without a password |
| GPIO2 LED | Blink when radio is healthy and app LED setting is enabled; steady while gong is active |
| Serial `p`, `c`, `g`, `a` at 115200 baud | Pair, cancel handshake, test local gong, turn sign-in off |

These controls use the configured button/LED GPIOs; the table assumes defaults.
Local tests use full app volume; incoming relay-gong rings obey app Mute.
Only press BOOT **after startup** for these controls; holding it during reset
puts the ESP32 into its ROM flashing mode. Board LED polarity/presence may vary.

CC1101 raw-mode and GDO signal assignments follow the
[TI CC1101 datasheet](https://www.ti.com/lit/ds/symlink/cc1101.pdf),
Table 41 and §27.2. The radio settings and protocol assumptions are described in
[`../PROTOCOL.md`](../PROTOCOL.md). Owner-hardware CC1101 pairing, rejoin and
press reception are established; audible PWM/DAC/physical relay behavior remains
unverified because no output was connected. See [compatibility and validation](../README.md#compatibility-and-features).
