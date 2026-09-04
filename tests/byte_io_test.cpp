#include "jwpqt/core/byte_io.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

void require(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

std::uint32_t float_bits(float value) {
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

template <typename Function>
void require_binary_error(Function&& function, std::string_view text) {
  try {
    function();
  } catch (const jwpqt::core::BinaryError& error) {
    require(std::string_view(error.what()).find(text) != std::string_view::npos,
            "Binary error did not report the expected detail");
    return;
  }
  throw std::runtime_error("Expected BinaryError");
}

template <typename Function>
void require_transactional_error(jwpqt::core::ByteReader& reader,
                                 Function&& function) {
  const std::size_t position = reader.position();
  require_binary_error(std::forward<Function>(function), "Unexpected end");
  require(reader.position() == position,
          "Failed fixed-width read changed the reader position");
}

}  // namespace

int main() {
  try {
    jwpqt::core::ByteWriter writer;
    writer.write_u8(0xab);
    writer.write_u16_le(0x1234);
    writer.write_i16_le(std::numeric_limits<std::int16_t>::min());
    writer.write_u32_le(0x89abcdefU);
    writer.write_i32_le(std::numeric_limits<std::int32_t>::min());
    writer.write_f32_le(12.5F);
    writer.write_bytes("end");

    const std::array<std::uint8_t, 20> expected_bytes = {
        0xab, 0x34, 0x12, 0x00, 0x80, 0xef, 0xcd, 0xab, 0x89, 0x00,
        0x00, 0x00, 0x80, 0x00, 0x00, 0x48, 0x41, 0x65, 0x6e, 0x64};
    const std::string expected(
        reinterpret_cast<const char*>(expected_bytes.data()),
        expected_bytes.size());
    require(writer.bytes() == expected, "Writer emitted incorrect bytes");

    jwpqt::core::ByteReader reader(writer.bytes());
    require(reader.position() == 0 && reader.remaining() == expected.size(),
            "Reader initial state mismatch");
    require(reader.read_u8() == 0xab, "u8 round trip failed");
    require(reader.read_u16_le() == 0x1234, "u16 round trip failed");
    require(reader.read_i16_le() == std::numeric_limits<std::int16_t>::min(),
            "i16 round trip failed");
    require(reader.read_u32_le() == 0x89abcdefU, "u32 round trip failed");
    require(reader.read_i32_le() == std::numeric_limits<std::int32_t>::min(),
            "i32 round trip failed");
    require(reader.read_f32_le() == 12.5F,
            "float round trip failed");
    require(reader.read_bytes(3) == "end", "byte span round trip failed");
    require(reader.empty() && reader.position() == expected.size(),
            "Reader final state mismatch");

    require_binary_error([&reader] { reader.read_u8(); }, "byte 20");
    require_binary_error(
        [] {
          jwpqt::core::ByteReader short_reader("x");
          static_cast<void>(short_reader.read_u32_le());
        },
        "byte 0");
    require_binary_error(
        [] {
          jwpqt::core::ByteReader short_reader("abc");
          static_cast<void>(short_reader.read_bytes(4));
        },
        "need 4 bytes");

    jwpqt::core::ByteReader short_reader("\x01");
    require_transactional_error(short_reader,
                                [&short_reader] { short_reader.read_u16_le(); });
    require_transactional_error(short_reader,
                                [&short_reader] { short_reader.read_i16_le(); });
    require_transactional_error(short_reader,
                                [&short_reader] { short_reader.read_u32_le(); });
    require_transactional_error(short_reader,
                                [&short_reader] { short_reader.read_i32_le(); });
    require_transactional_error(short_reader,
                                [&short_reader] { short_reader.read_f32_le(); });
    require_transactional_error(
        short_reader, [&short_reader] { short_reader.read_bytes(2); });

    jwpqt::core::ByteWriter boundary_writer;
    boundary_writer.write_u8(0x00);
    boundary_writer.write_u8(0x80);
    boundary_writer.write_u8(0xff);
    boundary_writer.write_u16_le(std::numeric_limits<std::uint16_t>::max());
    boundary_writer.write_i16_le(std::numeric_limits<std::int16_t>::max());
    boundary_writer.write_u32_le(std::numeric_limits<std::uint32_t>::max());
    boundary_writer.write_i32_le(std::numeric_limits<std::int32_t>::max());
    const float negative_zero = -0.0F;
    const float infinity = std::numeric_limits<float>::infinity();
    const float quiet_nan = std::numeric_limits<float>::quiet_NaN();
    boundary_writer.write_f32_le(negative_zero);
    boundary_writer.write_f32_le(infinity);
    boundary_writer.write_f32_le(quiet_nan);

    jwpqt::core::ByteReader boundary_reader(boundary_writer.bytes());
    require(boundary_reader.read_u8() == 0x00, "u8 zero failed");
    require(boundary_reader.read_u8() == 0x80, "u8 high bit failed");
    require(boundary_reader.read_u8() == 0xff, "u8 maximum failed");
    require(boundary_reader.read_u16_le() ==
                std::numeric_limits<std::uint16_t>::max(),
            "u16 maximum failed");
    require(boundary_reader.read_i16_le() ==
                std::numeric_limits<std::int16_t>::max(),
            "i16 maximum failed");
    require(boundary_reader.read_u32_le() ==
                std::numeric_limits<std::uint32_t>::max(),
            "u32 maximum failed");
    require(boundary_reader.read_i32_le() ==
                std::numeric_limits<std::int32_t>::max(),
            "i32 maximum failed");
    require(float_bits(boundary_reader.read_f32_le()) ==
                float_bits(negative_zero),
            "negative zero bits were not preserved");
    require(float_bits(boundary_reader.read_f32_le()) == float_bits(infinity),
            "float infinity bits were not preserved");
    require(float_bits(boundary_reader.read_f32_le()) == float_bits(quiet_nan),
            "float NaN bits were not preserved");
    require(boundary_reader.empty(), "Boundary reader has trailing bytes");

    jwpqt::core::ByteWriter moved_writer;
    moved_writer.write_bytes("payload");
    require(moved_writer.take_bytes() == "payload", "take_bytes failed");

    std::cout << "All byte I/O tests passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
