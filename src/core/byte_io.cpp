#include "jwpqt/core/byte_io.h"

#include <cstring>
#include <limits>
#include <sstream>
#include <utility>

namespace jwpqt::core {
namespace {

static_assert(sizeof(float) == sizeof(std::uint32_t));
static_assert(std::numeric_limits<float>::is_iec559);

std::int16_t signed_u16(std::uint16_t value) {
  if (value <= static_cast<std::uint16_t>(
                   std::numeric_limits<std::int16_t>::max())) {
    return static_cast<std::int16_t>(value);
  }
  return static_cast<std::int16_t>(
      static_cast<std::int32_t>(value) - 0x10000);
}

std::int32_t signed_u32(std::uint32_t value) {
  if (value <= static_cast<std::uint32_t>(
                   std::numeric_limits<std::int32_t>::max())) {
    return static_cast<std::int32_t>(value);
  }
  return static_cast<std::int32_t>(
      static_cast<std::int64_t>(value) - 0x100000000LL);
}

}  // namespace

ByteReader::ByteReader(std::string_view bytes) noexcept : bytes_(bytes) {}

std::size_t ByteReader::position() const noexcept { return position_; }

std::size_t ByteReader::remaining() const noexcept {
  return bytes_.size() - position_;
}

bool ByteReader::empty() const noexcept { return remaining() == 0; }

void ByteReader::require(std::size_t size) const {
  if (size <= remaining()) {
    return;
  }
  std::ostringstream message;
  message << "Unexpected end at byte " << position_ << ": need " << size
          << " bytes, have " << remaining();
  throw BinaryError(message.str());
}

std::uint8_t ByteReader::read_u8() {
  require(1);
  return static_cast<std::uint8_t>(
      static_cast<unsigned char>(bytes_[position_++]));
}

std::uint16_t ByteReader::read_u16_le() {
  require(2);
  const std::uint16_t low = static_cast<std::uint8_t>(
      static_cast<unsigned char>(bytes_[position_]));
  const std::uint16_t high = static_cast<std::uint8_t>(
      static_cast<unsigned char>(bytes_[position_ + 1]));
  position_ += 2;
  return static_cast<std::uint16_t>(low | (high << 8U));
}

std::int16_t ByteReader::read_i16_le() { return signed_u16(read_u16_le()); }

std::uint32_t ByteReader::read_u32_le() {
  require(4);
  const std::uint32_t low = read_u16_le();
  const std::uint32_t high = read_u16_le();
  return low | (high << 16U);
}

std::int32_t ByteReader::read_i32_le() { return signed_u32(read_u32_le()); }

float ByteReader::read_f32_le() {
  const std::uint32_t bits = read_u32_le();
  float value = 0.0F;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

std::string_view ByteReader::read_bytes(std::size_t size) {
  require(size);
  const std::string_view result = bytes_.substr(position_, size);
  position_ += size;
  return result;
}

void ByteWriter::write_u8(std::uint8_t value) {
  char byte = 0;
  std::memcpy(&byte, &value, sizeof(byte));
  bytes_.push_back(byte);
}

void ByteWriter::write_u16_le(std::uint16_t value) {
  write_u8(static_cast<std::uint8_t>(value & 0xffU));
  write_u8(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
}

void ByteWriter::write_i16_le(std::int16_t value) {
  const std::uint16_t bits =
      value >= 0
          ? static_cast<std::uint16_t>(value)
          : static_cast<std::uint16_t>(0x10000L + static_cast<long>(value));
  write_u16_le(bits);
}

void ByteWriter::write_u32_le(std::uint32_t value) {
  write_u16_le(static_cast<std::uint16_t>(value & 0xffffU));
  write_u16_le(static_cast<std::uint16_t>((value >> 16U) & 0xffffU));
}

void ByteWriter::write_i32_le(std::int32_t value) {
  const std::uint32_t bits =
      value >= 0
          ? static_cast<std::uint32_t>(value)
          : static_cast<std::uint32_t>(0x100000000LL +
                                       static_cast<std::int64_t>(value));
  write_u32_le(bits);
}

void ByteWriter::write_f32_le(float value) {
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(value));
  write_u32_le(bits);
}

void ByteWriter::write_bytes(std::string_view bytes) { bytes_.append(bytes); }

const std::string& ByteWriter::bytes() const noexcept { return bytes_; }

std::string ByteWriter::take_bytes() { return std::move(bytes_); }

}  // namespace jwpqt::core
