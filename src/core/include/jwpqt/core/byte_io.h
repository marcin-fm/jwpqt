#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace jwpqt::core {

class BinaryError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

class ByteReader {
 public:
  // The caller must keep the referenced bytes alive for the reader's lifetime.
  explicit ByteReader(std::string_view bytes) noexcept;

  std::size_t position() const noexcept;
  std::size_t remaining() const noexcept;
  bool empty() const noexcept;

  std::uint8_t read_u8();
  std::uint16_t read_u16_le();
  std::int16_t read_i16_le();
  std::uint32_t read_u32_le();
  std::int32_t read_i32_le();
  float read_f32_le();
  // The returned view remains valid only while the input bytes remain alive.
  std::string_view read_bytes(std::size_t size);

 private:
  void require(std::size_t size) const;

  std::string_view bytes_;
  std::size_t position_ = 0;
};

class ByteWriter {
 public:
  void write_u8(std::uint8_t value);
  void write_u16_le(std::uint16_t value);
  void write_i16_le(std::int16_t value);
  void write_u32_le(std::uint32_t value);
  void write_i32_le(std::int32_t value);
  void write_f32_le(float value);
  void write_bytes(std::string_view bytes);

  const std::string& bytes() const noexcept;
  std::string take_bytes();

 private:
  std::string bytes_;
};

}  // namespace jwpqt::core
