#pragma once

#include <stddef.h>
#include <stdint.h>

namespace esphome {
namespace quiet_cool {
namespace learning {

constexpr size_t PREFIX_LENGTH = 9;
constexpr size_t FRAME_LENGTH = 18;
constexpr size_t FRAME_BITS = FRAME_LENGTH * 8;
constexpr uint8_t MAX_PREFIX_BIT_ERRORS = 2;
constexpr uint8_t PREFIX[PREFIX_LENGTH] = {0x15, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};

struct Frame {
  uint8_t remote_id[7];
  uint8_t command;
  size_t end_bit;
  uint8_t prefix_errors;
};

struct Confirmation {
  uint8_t remote_id[7]{};
  uint8_t press_count{0};
  uint32_t last_frame_at{0};

  void reset() { this->press_count = 0; }

  // Frames closer than min_gap_ms belong to the same held/auto-repeated
  // physical press. Repeated packets cannot advance confirmation.
  bool observe(const uint8_t *id, uint32_t now, uint32_t min_gap_ms) {
    bool same = true;
    for (size_t i = 0; i < 7; i++)
      same = same && this->remote_id[i] == id[i];
    if (this->press_count == 0 || !same) {
      for (size_t i = 0; i < 7; i++)
        this->remote_id[i] = id[i];
      this->press_count = 1;
      this->last_frame_at = now;
      return false;
    }
    if (static_cast<uint32_t>(now - this->last_frame_at) >= min_gap_ms) {
      this->press_count++;
      this->last_frame_at = now;
      return this->press_count >= 2;
    }
    this->last_frame_at = now;
    return false;
  }
};

inline bool valid_command(uint8_t command) {
  if (command == 0x66)
    return false;
  const uint8_t speed = command & 0xF0;
  const uint8_t duration = command & 0x0F;
  const bool known_speed = speed == 0x90 || speed == 0xA0 || speed == 0xB0;
  const bool known_duration = duration == 0x00 || duration == 0x01 || duration == 0x02 ||
                              duration == 0x04 || duration == 0x08 || duration == 0x0C ||
                              duration == 0x0F;
  return known_speed && known_duration;
}

inline uint8_t read_byte(const uint8_t *bits, size_t start) {
  uint8_t value = 0;
  for (size_t bit = 0; bit < 8; bit++)
    value = static_cast<uint8_t>((value << 1) | (bits[start + bit] & 1U));
  return value;
}

// Bits are chronological, MSB first within the on-air bytes. Search every bit
// offset and allow a couple of errors in the long fixed prefix to tolerate RF
// corruption. The duplicated command and supported-command checks validate
// candidates after the tolerant prefix match.
inline bool find_frame(const uint8_t *bits, size_t bit_count, Frame &frame) {
  if (bit_count < FRAME_BITS)
    return false;
  bool found = false;
  for (size_t start = 0; start + FRAME_BITS <= bit_count; start++) {
    uint8_t prefix_errors = 0;
    for (size_t byte = 0; byte < PREFIX_LENGTH && prefix_errors <= MAX_PREFIX_BIT_ERRORS; byte++) {
      uint8_t differing_bits = static_cast<uint8_t>(read_byte(bits, start + byte * 8) ^ PREFIX[byte]);
      while (differing_bits != 0) {
        prefix_errors = static_cast<uint8_t>(prefix_errors + (differing_bits & 1U));
        differing_bits >>= 1;
      }
    }
    if (prefix_errors > MAX_PREFIX_BIT_ERRORS)
      continue;
    const size_t payload = start + PREFIX_LENGTH * 8;
    const uint8_t command = read_byte(bits, payload + 7 * 8);
    if (command != read_byte(bits, payload + 8 * 8) ||
        (command != 0x66 && !valid_command(command)))
      continue;
    for (size_t i = 0; i < 7; i++)
      frame.remote_id[i] = read_byte(bits, payload + i * 8);
    frame.command = command;
    frame.end_bit = start + FRAME_BITS;
    frame.prefix_errors = prefix_errors;
    found = true;  // Return the newest complete frame in the snapshot.
  }
  return found;
}

}  // namespace learning
}  // namespace quiet_cool
}  // namespace esphome
