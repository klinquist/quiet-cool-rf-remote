#include "../components/quiet_cool/fan/quietcool_learning.h"

#include <cassert>
#include <cstdint>
#include <vector>

using namespace esphome::quiet_cool::learning;

static void append_byte(std::vector<uint8_t> &bits, uint8_t value) {
  for (int bit = 7; bit >= 0; bit--)
    bits.push_back((value >> bit) & 1U);
}

static std::vector<uint8_t> make_frame(const uint8_t *id, uint8_t first_command, uint8_t second_command) {
  std::vector<uint8_t> bits;
  for (uint8_t byte : PREFIX)
    append_byte(bits, byte);
  for (size_t i = 0; i < 7; i++)
    append_byte(bits, id[i]);
  append_byte(bits, first_command);
  append_byte(bits, second_command);
  return bits;
}

int main() {
  const uint8_t id[] = {0x2D, 0xD4, 0x06, 0xCB, 0x00, 0xF7, 0xF2};

  // Prefix and payload extraction work at every possible starting bit offset.
  for (size_t offset = 0; offset < 8; offset++) {
    std::vector<uint8_t> bits(offset, 0);
    const auto frame_bits = make_frame(id, 0x9F, 0x9F);
    bits.insert(bits.end(), frame_bits.begin(), frame_bits.end());
    Frame frame{};
    assert(find_frame(bits.data(), bits.size(), frame));
    for (size_t i = 0; i < 7; i++)
      assert(frame.remote_id[i] == id[i]);
    assert(frame.command == 0x9F);
    assert(frame.end_bit == offset + FRAME_BITS);
    assert(frame.prefix_errors == 0);
  }

  // Small RF errors in the repeated prefix are tolerated, but the payload
  // checks still have to pass before the frame can be used.
  auto noisy_prefix = make_frame(id, 0x90, 0x90);
  noisy_prefix[10] ^= 1;
  noisy_prefix[35] ^= 1;
  assert(find_frame(noisy_prefix.data(), noisy_prefix.size(), frame));
  assert(frame.command == 0x90);
  assert(frame.prefix_errors == 2);
  noisy_prefix[60] ^= 1;
  assert(!find_frame(noisy_prefix.data(), noisy_prefix.size(), frame));

  // The duplicated command bytes and known speed/duration nibbles are checked.
  const uint8_t commands[] = {0x90, 0x91, 0x92, 0x94, 0x98, 0x9C, 0x9F,
                              0xA0, 0xA1, 0xA2, 0xA4, 0xA8, 0xAC, 0xAF,
                              0xB0, 0xB1, 0xB2, 0xB4, 0xB8, 0xBC, 0xBF};
  for (uint8_t command : commands)
    assert(valid_command(command));
  assert(!valid_command(0x66));
  assert(!valid_command(0x93));
  assert(!valid_command(0xC0));

  Frame frame{};
  auto bad_duplicate = make_frame(id, 0x90, 0x91);
  assert(!find_frame(bad_duplicate.data(), bad_duplicate.size(), frame));
  auto setup_command = make_frame(id, 0x66, 0x66);
  assert(find_frame(setup_command.data(), setup_command.size(), frame));
  assert(frame.command == 0x66);

  // The three packets from one press cannot confirm a second press. A later
  // frame after the required quiet interval confirms the same remote ID.
  Confirmation confirmation{};
  assert(!confirmation.observe(id, 100, 700));
  assert(!confirmation.observe(id, 180, 700));
  assert(!confirmation.observe(id, 340, 700));
  assert(!confirmation.observe(id, 800, 700));
  assert(!confirmation.observe(id, 1000, 700));
  assert(confirmation.observe(id, 1800, 700));

  const uint8_t other_id[] = {1, 2, 3, 4, 5, 6, 7};
  confirmation.reset();
  assert(!confirmation.observe(id, 100, 700));
  assert(!confirmation.observe(other_id, 200, 700));
  assert(confirmation.press_count == 1);
  assert(confirmation.observe(other_id, 1000, 700));
}
