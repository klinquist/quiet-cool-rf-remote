# Quiet Cool RF Remote with ESPHOME
Operate your remote controlled QuietCool Fan from ESPHome or Arduino.


This project implements an RF remote control system for Quiet Cool fans using an ESP32 microcontroller and CC1101 RF module.
The system can control various fan speeds and modes through RF signals.

## WARNING WARNING WARNING
**This should have been obvious to me before.  Alas, it wasn't.  Each RF remote has a unique ID built-in, and this code literally
only works with my remote for that reason.  Currently, getting this working will take some effort decoding your individual remote. 
Lame, I know.  Whatcha gonna do?
**


![QuietCool Remote](images/quietcool_fan.png)

# TL;DR -- get it running on ESPHome
Get a generateic CC1101 board.  Like this [CC1101 Board](https://www.amazon.com/dp/B0D3W9GVRQ?ref=ppx_yo2ov_dt_b_fed_asin_title)

![CC1101](images/41x16EagV+L._SL1000_.jpg)


Make these connections from your ESP32 to your CC1101 chip/board.

NOTE: the pinouts on these boards seem to be not reliable.  I had to trace them manually to figure out which header pin went to which CC1101 pin.  

| Module Pin | CC1101 Pin Name | CC1101 Pin #| ESP32 Pin |
|------------|-----------------|-------------|-----------|
| 1          | GND             | 16,19,PAD   | GND       |
| 2          | VCC             | -           | 3.3V      |
| 3          | GDO0            | 6           | GPIO13    |
| 4          | CSn             | 7           | GPIO15    |
| 5          | SCK             | 1           | GPIO18    |
| 6          | MOSI            | 20          | GPIO23    |
| 7          | MISO            | 2           | GPIO19    |
| 8          | GDO2            | 3           | GPIO12    |


## TL;DR -- get it running on ESPHome from ESPHome Builder on HomeAssistant

* Go to ESPHome Builder
* Click "+ NEW DEVICE" at the bottom of the screen
* Click `CONTINUE`
* name your fan
* click the ESP32 type you have.
* click `SKIP`
* on your new entry, click `EDIT`
* add the following to the bottom of the file:
```yaml
spi:
  clk_pin: 18
  mosi_pin: 23
  miso_pin: 19

external_components:
  - source:
      type: git
      url: https://github.com/ccrome/quiet-cool-rf-remote.git
      ref: main
    components: [ quiet_cool ]

fan:
  - platform: quiet_cool
    name: QuietCool fan
    cs_pin: 15
    gdo0_pin: 13
    gdo2_pin: 12
    remote_id: [0x2D, 0xD4, 0x06, 0xCB, 0x00, 0xF7, 0xF2]
# optional variables
#    center_freq_mhz: 433.897
#    deviation_khz: 10
```
* click `INSTALL` and install it in the normal ESPHome ways...

## TL;DR -- get it running on ESPHome, building locally

Create a `secrets.yaml` file
```yaml
wifi_ssid: <ssid>
wifi_password: <wifi password>
api_enctryption_key: <api key>
ota_password: <ota password>
```


compile, upload, and run
```
esphome compile quietcool-fan-example.yaml && \
esphome upload --device /dev/ttyUSB0 quietcool-fan-example.yaml  && \
esphome logs --device /dev/ttyUSB0 quietcool-fan-example.yaml
```

If everything is working, you should see
```
[16:48:24][D][quietcool:168]: gdo0_pin = 13, gdo2_pin = 12
[16:48:24][I][quietcool:174]: Starting CC1101 setup
[16:48:24][D][quietcool:041]: sck:18, miso:19, mosi:23, csn:15
[16:48:24][I][quietcool:047]: CC1101 VERSION READ: 0x00
[16:48:24][I][quietcool:047]: CC1101 VERSION READ: 0x14
[16:48:24][I][quietcool:049]: CC1101 detected!
[16:48:24][I][quietcool:179]: CC1101 ready
[16:48:24][D][empty_fan.fan:022]: QuietCool initialized
```

# Configuration

## Configuration

`remote_id` is optional. Without it, fan transmission stays disabled until a
physical remote ID has been learned. Learning is available as a native Home
Assistant button and does not require firmware changes after the first OTA.

### Example Configuration

```yaml
fan:
  - platform: quiet_cool
    id: quietcool_fan
    name: QuietCool fan
    cs_pin: 15
    gdo0_pin: 13
    gdo2_pin: 12
    # remote_id: [0x2D, 0xD4, 0x06, 0xCB, 0x00, 0xF7, 0xF2]  # optional
    # Optional:
    # speed_count: 2
    # center_freq_mhz: 433.897
    # deviation_khz: 10
    # Frequency controls are created automatically under HA Configuration.
    # To disable them, set frequency_controls: {}.
    # Optional listener control; omitted by default:
    # remote_listener: {}
    # 0x66 polling controls are created automatically under HA Configuration.
    # To disable polling by default or change its interval:
    # status_polling:
    #   enabled: false
    #   interval_seconds: 30
    # Optional timer selector and remaining-time sensor. Add a Gauge card
    # in Home Assistant using the remaining-time sensor for a dial display.
    # timer:
    #   control:
    #     name: "QuietCool Timer"
    #   remaining:
    #     name: "QuietCool Time Remaining"
    # To customize control names, or explicitly choose controls:
    # frequency_controls:
    #   center_freq_mhz:
    #     name: "QuietCool Center Frequency"
    #   deviation_khz:
    #     name: "QuietCool Frequency Deviation"
    # Raw RF test buttons are omitted unless configured. Each sends its
    # command byte three times using the learned/configured remote ID.
    # test_buttons:
    #   - name: "Send QuietCool 0x66"
    #     command: 0x66
    #   - name: "Send QuietCool 0x00"
    #     command: 0x00
```

---

### QuietCool YAML Configuration Options

| Option            | Required | Type         | Default    | Description                                                                 |
|-------------------|----------|--------------|-----------|-----------------------------------------------------------------------------|
| `name`            | Yes      | string       |           | The name of the fan in Home Assistant.                                      |
| `platform`        | Yes      | string       |           | Must be `quiet_cool`.                                                       |
| `cs_pin`          | Yes      | int          |           | SPI chip select pin for CC1101.                                             |
| `gdo0_pin`        | Yes      | int          |           | CC1101 GDO0 data/RX packet-complete pin.                                     |
| `gdo2_pin`        | Yes      | int          |           | CC1101 GDO2 clock pin used during learning.                                  |
| `remote_id`       | No       | list[hex]    |           | Optional 7-byte remote ID. A valid ID learned in flash takes priority.       |
| `speed_count`     | No       | int (2 or 3) | 3         | Set to `2` for LOW/HIGH fans, or `3` for LOW/MEDIUM/HIGH fans.               |
| `center_freq_mhz` | No       | float        | 433.897    | Center frequency in MHz for RF transmission.                                |
| `deviation_khz`   | No       | float        | 10         | Frequency deviation (spread) in kHz for FSK modulation.                     |
| `frequency_controls` | No    | map          | automatic  | Creates center-frequency and deviation controls under Configuration, even when YAML values are set. An explicit map selects controls and names; an empty map disables them. |
| `remote_listener` | No       | entity       | omitted   | Optional Configuration switch to enable/ignore unsolicited physical-remote frames. This switch is created only when this parameter is present. When disabled, replies are still accepted for 2 seconds after sending command `0x66`. |
| `status_polling` | No       | map       | enabled, 30 seconds | Creates a Configuration switch to enable polling fan status with `0x66`, plus a poll-interval control. Polling starts 30 seconds after boot or enable; the adjustable interval range is 10 to 3600 seconds. Set `enabled: false` to disable polling by default. |
| `timer` | No       | map       | disabled   | Enables the timer duration select and remaining-time sensor. The select offers no timer, 1, 2, 4, 8, or 12 hours. Received timer commands are decoded even when this option is omitted. |
| `learn_button`    | No       | entity       | enabled   | Creates the `QuietCool Learn Remote` button under Configuration.            |
| `learning_status` | No       | entity       | enabled   | Creates the learning status text sensor under Diagnostics.                   |
| `rx_packet_sensor` | No      | entity       | enabled   | Creates the `QuietCool RF Activity` diagnostic sensor. Each matching remote frame reports its command byte, including unknown commands. |
| `test_buttons`    | No       | list         | omitted   | Optional raw RF command buttons under Configuration; each item has a Home Assistant `name` and one-byte hex `command`. |

**Note:** You must also define the SPI bus pins in your YAML:

```yaml
spi:
  clk_pin: 18
  mosi_pin: 23
  miso_pin: 19
```

Raw RF test buttons transmit the configured byte twice in each packet and
repeat the packet three times. They use the active remote ID; transmitting a
test command does not directly change the fan entity's state. If command `0x66`
elicits a decodable status reply, that reply can update the fan state. This can
help check whether the CC1101 transmitter reaches the fan independently of the
normal speed command mapping.
If the `remote_listener` parameter is present, its Configuration switch
defaults to on. Turn it off to prevent unsolicited transmissions from the
handheld remote from changing the Home Assistant fan state. The radio still
receives during a short response window after a test button or polling sends
`0x66`, so matching status replies can update the state. Without this YAML
parameter, physical-remote listening stays enabled and no switch is created.

The `status_polling` block creates a Configuration switch for periodic `0x66`
queries and an interval control in seconds. Polling is enabled by default at
30-second intervals; the first query is sent 30 seconds after boot or after
turning polling on. Set `enabled: false` in YAML or turn off the HA switch to
disable polling. Polling begins once a remote ID is configured or learned and
pauses during remote learning.

## Automatic Remote Learning

Connect CC1101 **GDO0 to the configured `gdo0_pin`** and **GDO2 to the
configured `gdo2_pin`**. Both signals are required. GDO0 carries synchronous
data during learning; GDO2 supplies the bit clock. The configured SPI bus and
CS wiring remain required for CC1101 control.

In Home Assistant, press **QuietCool Learn Remote**, then press and release the
same normal speed button on the physical QuietCool remote twice. Allow the
remote's full delayed flashing/transmission cycle to finish after each press;
leave about a second between completed cycles. The status sensor reports
`Learning`, then `Confirming`, and finally `Learned`. Learning stops after 60
seconds and reports `Timeout` if it does not receive two separated, valid
frames for the same ID. Repeated packets from one continuous press are grouped
and cannot confirm the ID by themselves.
The Home Assistant learning status sensor also includes the active or
candidate remote ID and current center frequency and deviation, for example
`Confirming | ID 2D:D4:06:CB:00:B9:66 | 433.883 MHz | dev 10.0 kHz`.
Startup and learning-start ESPHome logs report the configured or learned ID
and active tuning as well.

Discovery logs report GDO2 clock edges, buffered bits, valid decoded frames,
remote IDs, and command bytes. If no clock edges arrive, check that the radio
is receiving the remote transmission and that GDO2 is connected to the YAML
pin. If edges arrive but no valid frames decode, check frequency and signal
quality. Per-frame details are logged at DEBUG level. A timeout also prints
the captured raw bits, oldest first, so the received stream can be compared
against the expected prefix and frame layout.

The CC1101 enters 2-FSK synchronous serial RX with carrier-sense gating and
infinite packet length at the configured frequency (433.897 MHz by default).
The component captures bits into a fixed-size ring
buffer from the GDO2 clock interrupt, then scans outside the ISR for the fixed
QuietCool prefix and validates the ID and duplicated LOW/MEDIUM/HIGH command
bytes, including supported timer durations. The leading marker tolerates up
to five bit errors and the following eight-byte sync run tolerates up to two;
duplicated command bytes remain exact. Recognized 0x66 setup frames are logged
and ignored. After confirmation, packet-mode RX/TX is restored.
The register choices follow TI's [CC1101 datasheet, synchronous serial
operation and GDO configuration](https://www.ti.com/lit/ds/symlink/cc1101.pdf)
and the bundled ELECHOUSE driver.

The Home Assistant center-frequency and deviation number entities appear
under Configuration by default and immediately retune the CC1101. Center
frequency can be set from 433.000 to 434.500 MHz in 1 kHz increments, and
deviation from 1 to 100 kHz in 0.5 kHz increments. Runtime changes are stored
in ESPHome preferences and restored on later boots. YAML values are used when
there is no saved value; `frequency_controls: {}` disables the controls.

When the receiver hears a duplicated command frame for the configured remote
ID that does not map to a supported fan speed/duration, the ESPHome activity
logs show its raw command byte and remote ID. These frames do not change the
Home Assistant fan state. Frames with a different remote ID suffix are ignored.
The `QuietCool RF Activity` diagnostic sensor also publishes matching command
frames, including setup and unknown commands, to make them visible in Home
Assistant's entity history.

The learned ID is stored through ESPHome's ESP32 preferences in NVS with a
format version and checksum. Startup priority is a valid learned ID, then an
ID in YAML, then unconfigured. OTA updates preserve NVS. Learning replaces an
existing ID only after two valid, separated confirmations and a successful
preference write. To replace an ID, start learning again; to force the YAML ID
to be used, clear the device's ESPHome preferences/NVS before rebooting.

### Troubleshooting and limitations

- `Not Configured`: check CC1101 wiring, press Learn Remote, then send two
  separate physical remote presses.
- `Error`: check CC1101 detection and SPI/CS wiring, then reboot and retry.
- If learning times out, inspect the discovery edge/frame counts in the logs.
  Verify GDO0/GDO2 pins, configured frequency, remote battery, and that each
  delayed button transmission completes before the 60-second learning window.
- A fan command during learning cancels learning and is sent if an ID is
  already available. Without an ID it is safely ignored.
- Learning uses the CC1101 synchronous serial mode and has not been verified
  on hardware by this change. It requires an ESP32 Arduino target, a CC1101,
  and a compatible physical QuietCool remote.
- Physical-remote commands update Home Assistant state. The fan can also send
  status frames after a `0x66` query; see the command reference below. A lost
  command or status reply can leave Home Assistant state out of sync.

## Known QuietCool Command Bytes

Each command byte is sent twice in the RF payload. For speed commands, the
upper nibble selects speed and the lower nibble selects the duration:

| Duration nibble | LOW (`0x9x`) | MEDIUM (`0xAx`) | HIGH (`0xBx`) | Meaning |
|---|---:|---:|---:|---|
| `0x0` | `0x90` | `0xA0` | `0xB0` | PREP/off; `0xB0` is the observed fan OFF status reply (HIGH selected, duration zero) |
| `0x1` | `0x91` | `0xA1` | `0xB1` | On for 1 hour |
| `0x2` | `0x92` | `0xA2` | `0xB2` | On for 2 hours |
| `0x4` | `0x94` | `0xA4` | `0xB4` | On for 4 hours |
| `0x8` | `0x98` | `0xA8` | `0xB8` | On for 8 hours |
| `0xC` | `0x9C` | `0xAC` | `0xBC` | On for 12 hours |
| `0xF` | `0x9F` | `0xAF` | `0xBF` | On continuously, no timer |

| Special byte | Meaning | Current behavior |
|---|---|---|
| `0x66` | Setup/WAKE frame; it does not itself encode fan state. Sending it can prompt a fan status reply. | Ignored as a state command; a two-second receive window follows a locally sent `0x66`. |
| `0x80` | Special OFF command sent by the remote/controller, as identified in PR #12. | This branch currently reports it as unknown on receive. It is distinct from the observed `0xB0` fan OFF status reply. |

The speed and duration meanings follow the component decoder and PR #12. In
the captured test, the controller sent `0x66` while the fan was off and then
received `0xB0`; that makes `0xB0` the observed OFF status reply. `0x80` is a
separate special OFF command, not the reply seen in that test.
The component does not reliably distinguish whether a matching frame came
from the handheld remote or the fan, so the `0x66` response window is useful
for controlled status checks.

## Physical Remote State Tracking

The component listens for the configured `remote_id` whenever it is not
transmitting. A valid physical-remote command updates the ESPHome/Home
Assistant fan power and speed state. The three repeated RF frames sent for a
single button press are deduplicated automatically.

Timed commands (1, 2, 4, 8, or 12 hours) also start a local countdown so the
published state changes to off when the requested duration elapses. An `ON`
or `OFF` command cancels that countdown.

The fan may also transmit a status frame after receiving a `0x66` WAKE query.
For controlled checks, turn off the `Listen to Physical Remote` switch and
send `0x66`; matching frames are accepted for two seconds so the fan's reply
can update Home Assistant without unrelated handheld-remote traffic updating
the state. A missed command or reply can still leave the published state out
of sync.

The optional `timer` block adds a timer selector under Configuration and a
numeric estimated-time-remaining sensor in hours. Selecting a duration while
the fan is running immediately sends the current speed with that timer,
restarting the fan's timer. The selector also follows timer commands received
from the physical remote or fan. The remaining sensor updates when a timer
starts and once per minute; it reaches `0` when the timer expires or no
countdown is active. This is a local estimate: the command byte contains the
requested duration, and the captures so far do not establish that the fan
reports elapsed time. Add the sensor to a Home Assistant Gauge card (for
example, minimum `0`, maximum `12`) to display a dial. With a two-speed fan,
`0xB1`, `0xB2`, `0xB4`, `0xB8`, and `0xBC` are the HIGH-speed timer frames;
LOW-speed timer frames use the same duration nibble with `0x9` as the upper
nibble.

For a two-speed fan, set `speed_count: 2`. ESPHome/Home Assistant will then
expose two speeds, map speed 1 to the LOW RF command, and map speed 2 to HIGH.
The default value of `3` retains the LOW/MEDIUM/HIGH behavior.

## Recent Improvements

### Transmission System Overhaul
- **Consolidated bit conversion logic**: Unified logging and transmission to ensure consistency
- **Improved reliability**: Fixed potential timing and synchronization issues
- **Better debugging**: Enhanced logging with bit-level visibility
- **Configurable remote ID**: Support for custom remote identifiers via YAML configuration

### Technical Changes
- Replaced string-based command system with byte arrays for better performance
- Consolidated `logBits` and `sendBitsFromBytes` functions to use shared logic
- Added configurable `remote_id` parameter throughout the stack
- Improved error handling and validation

# Run it on arduino
The arduino code doesn't do much by itself.  But it gets you going:

```
cd arduino
platformio run --target upload
```

## Hardware Requirements

- ESP32 DOIT DevKit V1
- CC1101 RF Module
- Wiring connections as follows:
  ```
  CC1101 Pin -> ESP32 Pin
  GND       -> GND
  VCC       -> 3.3V
  GDO0      -> GPIO13
  CSn       -> GPIO15
  SCK       -> GPIO18
  MOSI      -> GPIO23
  MISO      -> GPIO19
  GDO2      -> GPIO12
  ```

## Case
Here's a case for the parts that I used.

The source is here [in OnShape](https://cad.onshape.com/documents/23ba2be84b2f4dcf9bfd73fc/v/3fa4bea9b4bb4353b99b35e1/e/0985333c7a3d43ad0383c966)

### Step File

* The whole design: [QuietCoolCase.step](3d/step/QuietCoolCase.step)
* Or just the Top: [QuietCoolCaseTop.step](3d/step/QuietCoolCaseTop.step)
* and just the Bottom: [QuietCoolCaseBottom.step](3d/step/QuietCoolCaseBottom.step)

## Software Requirements

- PlatformIO


## Features

- Controls Quiet Cool fan system via RF signals
- Supports multiple fan speeds and timer durations
  - Speed
    - L
    - M
    - H
  - Duration
    - 1
    - 2
    - 4
    - 8
    - 12
    - off
    - on
- **Configurable remote ID** for compatibility with different remotes

# Reverse Engineering
I used an RTL-SDR.COM SDR like this:
![RTL-SDR v3](images/rtl-sdrv3-500.jpg)

And for software, I used Universal Radio Haacker.  Here's a recording of all the signals as they progress from H 1 2 4 8 12 ON OFF -> to medium -> low.  It was recorded with these settings

![Settings](images/record-settings.png)

[Download the raw recording](recordings/RTL-SDR-20250510_080358-433_92MHz-1MSps-1MHz.complex16s.gz)

![Here's the signal demodulated](images/urh-demodulated.png)

![Spectrum Analysis](images/spectrum.png)

## Modulation
This remote uses FSK modulation at 433.92MHz.  The CC1101 RF module ended up transmitting high, so I reduced the frequency in the code to 433.897 to make it work.  If things don't work, check the actual frequency.


## Configuration

The system operates at 433.897 MHz and uses FSK modulation. The CC1101 module is configured for:
- Direct mode transmission
- No packet handling
- Raw data mode
- No CRC or preamble

## Building and Uploading

```pio run --target upload```

## License
MIT License

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.
