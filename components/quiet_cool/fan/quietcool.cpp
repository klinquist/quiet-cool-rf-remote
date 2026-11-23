#include <stdint.h>
typedef uint8_t byte;
#include "quietcool.h"
#include "esphome/core/log.h"
#include "ELECHOUSE_CC1101_SRC_DRV.h"
#include <algorithm>
#include <cstring>
#include "driver/gpio.h"

namespace esphome {
namespace quiet_cool {

static const char *TAG = "quietcool";

// The CC1101 adds the 0x15AA sync word in packet mode. The remaining seven
// 0xAA bytes are part of the payload before the remote ID.
const uint8_t SYNC[] = {0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa};
#define SYNC_LEN (sizeof(SYNC))

#define CMD_CODE_LEN 2

QuietCool *QuietCool::active_receiver_ = nullptr;

// Commands are structured like this:
// LOW: 0x90
// MED: 0xA0    
// HIGH:0xB0
//
//  1 hour: 0x01
//  2 hour: 0x02
//  4 hour: 0x04
//  8 hour: 0x08
// 12 hour: 0x0C
//      on: 0x0F
//     off: 0x00
//
// the special command that's sent before anything else is 0x66.

// On air the CC1101 sends its 0x15AA hardware sync word, followed by seven
// 0xAA bytes, the 7-byte ID, two duplicate command bytes, and zero padding.

// Helper: Convert bytes to a bit string (MSB first)
static void bytesToBitString(const uint8_t* data, size_t len, char* bitstr, size_t maxlen) {
    size_t idx = 0;
    for (size_t i = 0; i < len && idx + 8 < maxlen; i++) {
        for (int b = 7; b >= 0; b--) {
            bitstr[idx++] = ((data[i] >> b) & 1) ? '1' : '0';
        }
    }
    bitstr[idx] = '\0';
}

// Helper: Log the bits in a byte array (MSB first)
void QuietCool::logBits(const uint8_t* data, size_t len) {
    char bitstr[8 * 32 + 1]; // up to 32 bytes (256 bits)
    bytesToBitString(data, len, bitstr, sizeof(bitstr));
    
    // Print bytes in hex format
    ESP_LOGD(TAG, "Bits sent: %s", bitstr);

    bitstr[0] = 0;
    for (size_t i = 0; i < len; i++) {
	char bb[3];
	snprintf(bb, 3, "%02X", data[i]);
        strcat(bitstr, bb);
    }
    ESP_LOGD(TAG, "Bytes sent: %s", bitstr);
}

void QuietCool::sendRawData(const uint8_t* data, size_t len) {
    if (len == 0) {
        ESP_LOGE(TAG, "No data to send");
        return;
    }
    ESP_LOGD(TAG, "Sending %zu bytes (%zu bits)", len, len * 8);
    logBits(data, len);
    // SendData() always writes a length byte to TXFIFO. That byte is only
    // appropriate for variable-length mode; with the fixed 20-byte packet
    // used here it shifts the QuietCool payload and the fan rejects it.
    // Write the payload directly, as in the known-working PR #12 TX path.
    ELECHOUSE_cc1101.SpiStrobe(CC1101_SIDLE);
    ELECHOUSE_cc1101.SpiStrobe(CC1101_SFTX);
    ELECHOUSE_cc1101.SpiWriteBurstReg(CC1101_TXFIFO, (byte*)data, (byte)len);
    ELECHOUSE_cc1101.SpiStrobe(CC1101_STX);

    // GDO0=0x06 rises when the hardware sync is sent and falls at packet end.
    // Bound both waits so a radio/configuration problem cannot hang ESPHome.
    const uint32_t start_ms = millis();
    while (!digitalRead(gdo0_pin) && static_cast<uint32_t>(millis() - start_ms) < 250) {
        delay(1);
    }
    if (!digitalRead(gdo0_pin)) {
        ESP_LOGE(TAG, "TX timed out waiting for sync on GDO0");
        ELECHOUSE_cc1101.SpiStrobe(CC1101_SIDLE);
        ELECHOUSE_cc1101.SpiStrobe(CC1101_SFTX);
        return;
    }

    const uint32_t packet_start_ms = millis();
    while (digitalRead(gdo0_pin) && static_cast<uint32_t>(millis() - packet_start_ms) < 250) {
        delay(1);
    }
    if (digitalRead(gdo0_pin))
        ESP_LOGE(TAG, "TX timed out waiting for packet end on GDO0");

    ELECHOUSE_cc1101.SpiStrobe(CC1101_SIDLE);
    ELECHOUSE_cc1101.SpiStrobe(CC1101_SFTX);
    delay(10);
}

void QuietCool::configureTransmit() {
    // Match the known-working TX setup: hardware sends sync word 0x15AA,
    // followed by the 20-byte payload (7x AA, remote ID, command, padding).
    ELECHOUSE_cc1101.setSidle();
    ELECHOUSE_cc1101.setPktFormat(0);
    ELECHOUSE_cc1101.setSyncWord(0x15, 0xAA);
    ELECHOUSE_cc1101.setSyncMode(1);
    ELECHOUSE_cc1101.setLengthConfig(0);
    ELECHOUSE_cc1101.setPacketLength(20);
    ELECHOUSE_cc1101.setAppendStatus(false);
    // GDO0 signals sync transmission and end-of-packet in packet mode.
    ELECHOUSE_cc1101.SpiWriteReg(CC1101_IOCFG0, 0x06);
}

void QuietCool::configureReceive() {
    // Use the first two bytes of the configured remote ID as the hardware sync
    // word. The FIFO then contains the other five ID bytes and two command
    // bytes, allowing the CC1101 to establish both bit and byte alignment.
    ELECHOUSE_cc1101.setSidle();
    ELECHOUSE_cc1101.setSyncWord(remote_id[0], remote_id[1]);
    ELECHOUSE_cc1101.setSyncMode(2);  // 16/16 sync word, no carrier-sense gate
    ELECHOUSE_cc1101.setWhiteData(false);
    ELECHOUSE_cc1101.setManchester(false);
    ELECHOUSE_cc1101.setPktFormat(0);
    ELECHOUSE_cc1101.setCrc(false);
    ELECHOUSE_cc1101.setLengthConfig(0);
    ELECHOUSE_cc1101.setPacketLength(RX_PAYLOAD_LEN);
    ELECHOUSE_cc1101.setPQT(0);
    ELECHOUSE_cc1101.setAdrChk(0);
    ELECHOUSE_cc1101.setAppendStatus(true);
    // Restore the packet-complete signal after synchronous learning mode.
    ELECHOUSE_cc1101.SpiWriteReg(CC1101_IOCFG0, 0x06);
}

void QuietCool::startListening() {
    if (!initialized_ || !remote_id_valid_)
        return;

    detachInterrupt(digitalPinToInterrupt(gdo2_pin));
    listening_ = false;
    noInterrupts();
    packet_ready_ = false;
    interrupts();

    configureReceive();
    ELECHOUSE_cc1101.SpiStrobe(CC1101_SFRX);
    ELECHOUSE_cc1101.SetRx();
    attachInterrupt(digitalPinToInterrupt(gdo0_pin), QuietCool::handleGdo0Interrupt, FALLING);
    listening_ = true;
    mode_ = RadioMode::NORMAL_RX;
    ESP_LOGD(TAG, "Listening for remote ID %02X:%02X:%02X:%02X:%02X:%02X:%02X",
             remote_id[0], remote_id[1], remote_id[2], remote_id[3],
             remote_id[4], remote_id[5], remote_id[6]);
}

void QuietCool::restartReceiver() {
    listening_ = false;
    ELECHOUSE_cc1101.setSidle();
    ELECHOUSE_cc1101.SpiStrobe(CC1101_SFRX);
    ELECHOUSE_cc1101.SetRx();
    listening_ = true;
}

void QuietCool::configureLearning() {
    ELECHOUSE_cc1101.setSidle();
    // TI CC1101 datasheet: PKT_FORMAT=01 selects synchronous serial mode;
    // SYNC_MODE=100 disables hardware sync but gates RX on carrier sense, so
    // idle noise is not consumed as a packet before software finds the prefix.
    // GDO0=0x0C is synchronous data out; GDO2=0x0B is the serial clock.
    // RX data is set up on the falling clock edge, so the MCU samples on rise.
    ELECHOUSE_cc1101.setSyncMode(4);
    ELECHOUSE_cc1101.setManchester(false);
    ELECHOUSE_cc1101.setWhiteData(false);
    ELECHOUSE_cc1101.setCrc(false);
    ELECHOUSE_cc1101.setPktFormat(1);
    // Software scans for the QuietCool prefix without hardware sync, so a
    // fixed 20-byte packet would consume noise at RX entry and then stop RX.
    // Infinite length keeps the synchronous serial receiver active until
    // stopLearning() explicitly changes the radio state.
    ELECHOUSE_cc1101.setLengthConfig(2);
    ELECHOUSE_cc1101.setPQT(0);
    ELECHOUSE_cc1101.setAdrChk(0);
    ELECHOUSE_cc1101.setAppendStatus(false);
    ELECHOUSE_cc1101.SpiWriteReg(CC1101_IOCFG0, 0x0C);
    ELECHOUSE_cc1101.SpiWriteReg(CC1101_IOCFG2, 0x0B);
    pinMode(gdo0_pin, INPUT);
    pinMode(gdo2_pin, INPUT);
}

void IRAM_ATTR QuietCool::handleLearningClockInterrupt() {
    QuietCool *receiver = active_receiver_;
    if (receiver == nullptr || receiver->mode_ != RadioMode::LEARNING)
        return;
    const uint16_t head = receiver->capture_head_;
    receiver->captured_bits_[head] = static_cast<uint8_t>(gpio_get_level(static_cast<gpio_num_t>(receiver->gdo0_pin)));
    receiver->capture_head_ = static_cast<uint16_t>((head + 1) % LEARNING_CAPTURE_BITS);
    if (receiver->capture_count_ < LEARNING_CAPTURE_BITS)
        receiver->capture_count_++;
    receiver->capture_total_++;
}

bool QuietCool::startLearning() {
    if (!initialized_)
        return false;
    detachInterrupt(digitalPinToInterrupt(gdo0_pin));
    mode_ = RadioMode::LEARNING;
    listening_ = false;
    noInterrupts();
    capture_head_ = 0;
    capture_count_ = 0;
    capture_total_ = 0;
    last_learning_frame_end_ = 0;
    interrupts();
    configureLearning();
    ELECHOUSE_cc1101.SpiStrobe(CC1101_SFRX);
    ELECHOUSE_cc1101.SetRx();
    attachInterrupt(digitalPinToInterrupt(gdo2_pin), QuietCool::handleLearningClockInterrupt, RISING);
    return true;
}

void QuietCool::stopLearning() {
    if (mode_ != RadioMode::LEARNING)
        return;
    detachInterrupt(digitalPinToInterrupt(gdo2_pin));
    ELECHOUSE_cc1101.setSidle();
    ELECHOUSE_cc1101.SpiStrobe(CC1101_SFRX);
    if (remote_id_valid_) {
        startListening();
    } else {
        mode_ = RadioMode::UNCONFIGURED;
    }
}

void QuietCool::cancelLearning() { this->stopLearning(); }

void QuietCool::setRemoteId(const uint8_t *id) {
    memcpy(remote_id, id, REMOTE_ID_LEN);
    remote_id_valid_ = true;
}

bool QuietCool::captureLearningFrame(learning::Frame &frame) {
    uint8_t snapshot[LEARNING_CAPTURE_BITS];
    uint16_t count;
    uint16_t head;
    uint32_t total;
    noInterrupts();
    count = capture_count_;
    head = capture_head_;
    total = capture_total_;
    const uint16_t first = static_cast<uint16_t>((head + LEARNING_CAPTURE_BITS - count) % LEARNING_CAPTURE_BITS);
    for (uint16_t i = 0; i < count; i++)
        snapshot[i] = captured_bits_[(first + i) % LEARNING_CAPTURE_BITS];
    interrupts();
    if (!learning::find_frame(snapshot, count, frame))
        return false;
    const uint32_t absolute_end = total - count + static_cast<uint32_t>(frame.end_bit);
    if (absolute_end <= last_learning_frame_end_)
        return false;
    last_learning_frame_end_ = absolute_end;
    return true;
}

void QuietCool::getLearningCaptureStats(uint32_t &total_edges, uint16_t &buffered_bits) {
    noInterrupts();
    total_edges = capture_total_;
    buffered_bits = capture_count_;
    interrupts();
}

void QuietCool::logLearningCapture() {
    uint8_t snapshot[LEARNING_CAPTURE_BITS];
    uint16_t count;
    uint16_t head;
    noInterrupts();
    count = capture_count_;
    head = capture_head_;
    const uint16_t first = static_cast<uint16_t>((head + LEARNING_CAPTURE_BITS - count) % LEARNING_CAPTURE_BITS);
    for (uint16_t i = 0; i < count; i++)
        snapshot[i] = captured_bits_[(first + i) % LEARNING_CAPTURE_BITS];
    interrupts();

    ESP_LOGI(TAG, "Raw learning capture: %u bits, chronological, oldest first", static_cast<unsigned>(count));
    char line[65];
    for (uint16_t offset = 0; offset < count; offset += 64) {
        const uint16_t line_bits = static_cast<uint16_t>(std::min<uint16_t>(64, count - offset));
        for (uint16_t i = 0; i < line_bits; i++)
            line[i] = snapshot[offset + i] ? '1' : '0';
        line[line_bits] = '\0';
        ESP_LOGI(TAG, "  bits %u-%u: %s", static_cast<unsigned>(offset),
                 static_cast<unsigned>(offset + line_bits - 1), line);
    }
}

void IRAM_ATTR QuietCool::handleGdo0Interrupt() {
    QuietCool *receiver = active_receiver_;
    if (receiver != nullptr && receiver->listening_)
        receiver->packet_ready_ = true;
}

int8_t QuietCool::decodeRssi(uint8_t raw_rssi) {
    const int16_t signed_rssi = raw_rssi >= 128 ? raw_rssi - 256 : raw_rssi;
    return static_cast<int8_t>(signed_rssi / 2 - 74);
}

bool QuietCool::decodePayload(const uint8_t *payload, QuietCoolCommand &command) const {
    if (memcmp(payload, remote_id + 2, REMOTE_ID_LEN - 2) != 0) {
        ESP_LOGV(TAG, "Ignoring frame with non-matching remote ID suffix");
        return false;
    }

    const uint8_t command_code = payload[REMOTE_ID_LEN - 2];
    const uint8_t repeated_code = payload[REMOTE_ID_LEN - 1];
    if (command_code != repeated_code) {
        ESP_LOGW(TAG, "Ignoring frame with mismatched command bytes: %02X != %02X",
                 command_code, repeated_code);
        return false;
    }

    const uint8_t speed_code = command_code & 0xF0;
    switch (speed_code) {
        case QUIETCOOL_SPEED_LOW:
        case QUIETCOOL_SPEED_MEDIUM:
        case QUIETCOOL_SPEED_HIGH:
            command.speed = static_cast<QuietCoolSpeed>(speed_code);
            break;
        default:
            // 0x66 is a setup/pre-command and does not represent fan state.
            ESP_LOGV(TAG, "Ignoring non-state command 0x%02X", command_code);
            return false;
    }

    const uint8_t duration_code = command_code & 0x0F;
    switch (duration_code) {
        case QUIETCOOL_DURATION_1H:
        case QUIETCOOL_DURATION_2H:
        case QUIETCOOL_DURATION_4H:
        case QUIETCOOL_DURATION_8H:
        case QUIETCOOL_DURATION_12H:
        case QUIETCOOL_DURATION_ON:
        case QUIETCOOL_DURATION_OFF:
            command.duration = static_cast<QuietCoolDuration>(duration_code);
            break;
        default:
            ESP_LOGW(TAG, "Ignoring command with invalid duration: 0x%02X", command_code);
            return false;
    }

    command.code = command_code;
    return true;
}

bool QuietCool::receive(QuietCoolCommand &command) {
    if (!initialized_ || !packet_ready_)
        return false;

    noInterrupts();
    packet_ready_ = false;
    interrupts();

    const uint8_t rx_bytes_status = ELECHOUSE_cc1101.SpiReadStatus(CC1101_RXBYTES);
    if ((rx_bytes_status & 0x80) != 0) {
        ESP_LOGW(TAG, "CC1101 RX FIFO overflow; restarting receiver");
        restartReceiver();
        return false;
    }

    const uint8_t available = rx_bytes_status & 0x7F;
    constexpr size_t expected = RX_PAYLOAD_LEN + RX_STATUS_LEN;
    if (available < expected) {
        ESP_LOGW(TAG, "Incomplete RX frame: got %u bytes, expected %u",
                 available, static_cast<unsigned>(expected));
        restartReceiver();
        return false;
    }

    uint8_t frame[expected];
    ELECHOUSE_cc1101.SpiReadBurstReg(CC1101_RXFIFO, frame, expected);
    restartReceiver();

    command.rssi_dbm = decodeRssi(frame[RX_PAYLOAD_LEN]);
    command.lqi = frame[RX_PAYLOAD_LEN + 1] & 0x7F;
    if (!decodePayload(frame, command))
        return false;

    const uint32_t now = millis();
    const bool duplicate = command.code == last_rx_code_ &&
                           static_cast<uint32_t>(now - last_rx_at_) < RX_DEDUP_WINDOW_MS;
    last_rx_code_ = command.code;
    last_rx_at_ = now;
    if (duplicate) {
        ESP_LOGV(TAG, "Ignoring repeated remote frame 0x%02X", command.code);
        return false;
    }

    ESP_LOGI(TAG, "Received remote command 0x%02X (RSSI %d dBm, LQI %u)",
             command.code, command.rssi_dbm, command.lqi);
    return true;
}

void QuietCool::sendPacket(const uint8_t cmd_code) {
    const uint8_t padding_len = 4;
    uint8_t full_cmd[SYNC_LEN + 7 + CMD_CODE_LEN+padding_len];
    memset(full_cmd, 0, sizeof(full_cmd));
    memcpy(full_cmd, SYNC, SYNC_LEN);
    memcpy(full_cmd + SYNC_LEN, remote_id, 7);
    memcpy(full_cmd + SYNC_LEN + 7, &cmd_code, 1);
    memcpy(full_cmd + SYNC_LEN + 8, &cmd_code, 1);
    size_t total_len = SYNC_LEN + 7 + CMD_CODE_LEN+padding_len;
    ESP_LOGI(TAG, "Transmitting QuietCool command 0x%02X in three repeats", cmd_code);
    for (int i = 0; i < 3; i++) {
        sendRawData(full_cmd, total_len);
        delay(18);
    }
    ESP_LOGI(TAG, "Transmission sequence complete; restoring receive mode");
}

const uint8_t QuietCool::getCommand(QuietCoolSpeed speed, QuietCoolDuration duration) {
    ESP_LOGD(TAG, "getCommand got: speed=0x%02x, duration=0x%02x", speed, duration);
    const uint8_t off = 0x80;
    if (duration == QUIETCOOL_DURATION_OFF) {
        ESP_LOGD(TAG, "OFF command: 0x80");
        return off;
    }
    switch (speed) {
    case QUIETCOOL_SPEED_HIGH:
    case QUIETCOOL_SPEED_MEDIUM:
    case QUIETCOOL_SPEED_LOW:
    break;
    default:
	ESP_LOGD(TAG, "unknown speed: 0x%02x", speed);
	return off;
    };

    switch (duration) {
    case QUIETCOOL_DURATION_1H  :
    case QUIETCOOL_DURATION_2H  :
    case QUIETCOOL_DURATION_4H  :
    case QUIETCOOL_DURATION_8H  :
    case QUIETCOOL_DURATION_12H :
    case QUIETCOOL_DURATION_ON  :
    case QUIETCOOL_DURATION_OFF :
    break;
    default:
	ESP_LOGD(TAG, "unknown duration: 0x%02x", duration);
	return off;
    }
    uint8_t result = static_cast<uint8_t>(speed) | static_cast<uint8_t>(duration);
    ESP_LOGD(TAG, "Sending speed=0x%02x, duration=0x%02x: 0x%02x", speed, duration, result);
    return result;
}

    QuietCool::QuietCool(uint8_t csn, uint8_t gdo0, uint8_t gdo2, uint8_t sck, uint8_t miso, uint8_t mosi, const uint8_t* remote_id_in, float center_freq, float deviation_khz) : 
    csn_pin(csn),
    gdo0_pin(gdo0),
    gdo2_pin(gdo2),
    sck_pin(sck),
    miso_pin(miso),
    mosi_pin(mosi),
    center_freq_mhz(center_freq),
    deviation_khz(deviation_khz)
{
    for (int i = 0; i < 7; ++i) remote_id[i] = remote_id_in[i];
}

// --- Initialize CC1101 and verify communication ---
bool QuietCool::initCC1101() {
    ESP_LOGD(TAG, "sck:%d, miso:%d, mosi:%d, csn:%d, gdo0:%d\n", sck_pin, miso_pin, mosi_pin, csn_pin, gdo0_pin);
    ELECHOUSE_cc1101.setSpiPin(sck_pin, miso_pin, mosi_pin, csn_pin);

    int tries = 10;
    bool detected = false;
    while (tries--) {
        uint8_t version = readChipVersion();
        ESP_LOGI(TAG, "CC1101 VERSION READ: 0x%02X", version);
        if (version == 0x14 || version == 0x04) {
            ESP_LOGI(TAG, "CC1101 detected!");
            detected = true;
            break;
        }
    }

    if (!detected) {
        ESP_LOGE(TAG, "CC1101 not detected!");
        return false;
    }

    if (!ELECHOUSE_cc1101.getCC1101()) {
        ESP_LOGE(TAG, "CC1101 connection error");
        return false;
    }

    // ✅ Must set CC mode BEFORE Init so registers are configured correctly
    ELECHOUSE_cc1101.setCCMode(1);
    ELECHOUSE_cc1101.Init();

    ESP_LOGE(TAG, "Setting GDO0 pin to %d", gdo0_pin);
    ELECHOUSE_cc1101.setGDO0(gdo0_pin);

    // Basic configuration
    ESP_LOGE(TAG, "Setting center frequency to %f MHz", center_freq_mhz);
    ELECHOUSE_cc1101.setMHZ(center_freq_mhz);
    // Match the 10 dBm power used by the physical remote and working TX path.
    ELECHOUSE_cc1101.setPA(10);

    // Packet-related configuration
    ELECHOUSE_cc1101.setModulation(0);       // FSK
    ESP_LOGI(TAG, "Setting deviaion to %f kHz.  That's a total spread of %f kHz", deviation_khz, 2*deviation_khz);
    ELECHOUSE_cc1101.setDeviation(deviation_khz);
    ELECHOUSE_cc1101.setDRate(2.398);
    // Wide enough for this 2-FSK signal and typical module frequency error,
    // while substantially improving receive sensitivity over the reset maximum.
    ELECHOUSE_cc1101.setRxBW(100.0);
    ELECHOUSE_cc1101.setSyncMode(0);
    ELECHOUSE_cc1101.setWhiteData(false);
    ELECHOUSE_cc1101.setManchester(false);
    ELECHOUSE_cc1101.setPktFormat(0);
    ELECHOUSE_cc1101.setCrc(0);
    ELECHOUSE_cc1101.setLengthConfig(0);
    ELECHOUSE_cc1101.setPacketLength(20);
    ELECHOUSE_cc1101.setPRE(0);

    delay(500);
    return true;
}
uint8_t QuietCool::readChipVersion() {
    return ELECHOUSE_cc1101.SpiReadReg(0xF1);
}

bool QuietCool::begin(bool has_remote_id) {
    ESP_LOGI(TAG, "Starting CC1101 setup");
    if (!initCC1101()) {
        ESP_LOGE(TAG, "CC1101 not detected");
        return false;
    }
    initialized_ = true;
    active_receiver_ = this;
    remote_id_valid_ = has_remote_id;
    if (remote_id_valid_)
        startListening();
    else {
        ELECHOUSE_cc1101.setSidle();
        mode_ = RadioMode::UNCONFIGURED;
    }
    ESP_LOGI(TAG, "CC1101 ready");
    return true;
}

void QuietCool::send(QuietCoolSpeed speed, QuietCoolDuration duration) {
    this->sendCommand(this->getCommand(speed, duration));
}

void QuietCool::sendCommand(uint8_t command_code) {
    if (!initialized_) {
        ESP_LOGE(TAG, "Cannot transmit command 0x%02X: CC1101 initialization failed; check startup detection logs and CSn wiring",
                 command_code);
        return;
    }
    if (!remote_id_valid_) {
        ESP_LOGW(TAG, "Cannot transmit command 0x%02X: no remote ID is configured or learned",
                 command_code);
        return;
    }
    if (mode_ == RadioMode::UNCONFIGURED) {
        ESP_LOGW(TAG, "Cannot transmit command 0x%02X: radio is unconfigured",
                 command_code);
        return;
    }

    if (mode_ == RadioMode::LEARNING)
        stopLearning();

    ESP_LOGI(TAG, "Sending raw QuietCool command 0x%02X", command_code);
    listening_ = false;
    mode_ = RadioMode::TRANSMITTING;
    detachInterrupt(digitalPinToInterrupt(gdo0_pin));
    noInterrupts();
    packet_ready_ = false;
    interrupts();
    configureTransmit();
    sendPacket(command_code);
    startListening();
}

}  // namespace quiet_cool
}  // namespace esphome 
