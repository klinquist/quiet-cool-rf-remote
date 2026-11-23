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
| `learn_button`    | No       | entity       | enabled   | Creates the `QuietCool Learn Remote` Home Assistant button.                 |
| `learning_status` | No       | entity       | enabled   | Creates the learning status text sensor.                                     |

**Note:** You must also define the SPI bus pins in your YAML:

```yaml
spi:
  clk_pin: 18
  mosi_pin: 23
  miso_pin: 19
```

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
bytes, including supported timer durations. Up to two bit errors are tolerated
in the long prefix; duplicated command bytes remain exact. Recognized 0x66
setup frames are logged and ignored. After confirmation, packet-mode RX/TX is
restored.
The register choices follow TI's [CC1101 datasheet, synchronous serial
operation and GDO configuration](https://www.ti.com/lit/ds/symlink/cc1101.pdf)
and the bundled ELECHOUSE driver.

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
- Fan state remains command tracking, not feedback from the fan. A lost
  physical-remote transmission or a fan power interruption can leave Home
  Assistant state out of sync.

## Physical Remote State Tracking

The component listens for the configured `remote_id` whenever it is not
transmitting. A valid physical-remote command updates the ESPHome/Home
Assistant fan power and speed state. The three repeated RF frames sent for a
single button press are deduplicated automatically.

Timed commands (1, 2, 4, 8, or 12 hours) also start a local countdown so the
published state changes to off when the requested duration elapses. An `ON`
or `OFF` command cancels that countdown.

This is command tracking rather than feedback from the fan itself: the fan
does not transmit an acknowledgement or current status. A missed RF command,
a command from a different remote ID, or a fan power interruption can
therefore leave the published state out of sync until the next matching
command is received.

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

# TODO
* Implement the timers in the esphome UI

## License
MIT License

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.
