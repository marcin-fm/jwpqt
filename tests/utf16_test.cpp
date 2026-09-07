// SPDX-License-Identifier: GPL-2.0-or-later
#include "jwpqt/core/utf16.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
using namespace jwpqt::core;

void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

template <typename Function>
void rejects(Function function) {
  try {
    function();
  } catch (const Utf16Error&) {
    return;
  }
  throw std::runtime_error("Malformed UTF-16 accepted");
}

void test_utf16() {
  const Utf16File sample{U"A\u65e5\u672c\U0001f600", false};
  const std::string little("\x41\0\xe5\x65\x2c\x67\x3d\xd8\0\xde", 10);
  const std::string big("\0\x41\x65\xe5\x67\x2c\xd8\x3d\xde\0", 10);
  require(encode_utf16_file(sample, Utf16ByteOrder::kLittleEndian) == little &&
              encode_utf16_file(sample, Utf16ByteOrder::kBigEndian) == big,
          "Wrong UTF-16 byte order");
  std::u32string all;
  for (char32_t value = 0; value <= 0x10ffff; ++value)
    if (value < 0xd800 || value > 0xdfff) all.push_back(value);
  for (const auto order : {Utf16ByteOrder::kLittleEndian,
                           Utf16ByteOrder::kBigEndian}) {
    for (const auto& text : {all, std::u32string(U"\ufeff\ufffeX"),
                             std::u32string(U"\ufffe\ufeffX")})
      require(decode_utf16(encode_utf16(text, order), order) == text,
              "Raw UTF-16 string interpreted a character as a file signature");
    for (const bool bom : {false, true}) {
      for (const auto& text : {std::u32string{}, sample.text,
                               std::u32string{U'X', U'\ufeff', U'\0', U'\r', U'\n'},
                               all}) {
        const auto decoded = decode_utf16_file(
            encode_utf16_file({text, bom}, order), order);
        require(decoded.text == text && decoded.has_byte_order_mark == bom,
                "UTF-16 scalar/BOM round trip failed");
      }
    }
    const auto other = order == Utf16ByteOrder::kLittleEndian
                           ? Utf16ByteOrder::kBigEndian
                           : Utf16ByteOrder::kLittleEndian;
    rejects([&] { decode_utf16_file(encode_utf16_file({{}, true}, other), order); });
    rejects([&] { decode_utf16_file("x", order); });
    const auto raw_units = [&](std::initializer_list<unsigned> values) {
      std::string bytes;
      for (auto value : values) {
        const char low = static_cast<char>(value & 0xff);
        const char high = static_cast<char>(value >> 8);
        bytes += order == Utf16ByteOrder::kLittleEndian ? low : high;
        bytes += order == Utf16ByteOrder::kLittleEndian ? high : low;
      }
      return bytes;
    };
    for (const auto& malformed : {raw_units({0xd800}), raw_units({0xdc00}),
                                  raw_units({0xd800, 0x0041}),
                                  raw_units({0xdc00, 0xd800}),
                                  raw_units({0xd800, 0xdbff})}) {
      rejects([&] { decode_utf16_file(malformed, order); });
      rejects([&] { decode_utf16(malformed, order); });
    }
    for (const char32_t value : {char32_t{0xd800}, char32_t{0xdfff},
                                 char32_t{0x110000}, char32_t{0xffffffff}})
      rejects([&] { encode_utf16_file({std::u32string(1, value), false}, order); });
    require(decode_utf16_file(encode_utf16_file({U"\ufeffX", true}, order), order)
                    .text == U"\ufeffX",
            "BOM did not preserve initial text FEFF");
  }
  rejects([] { decode_utf16_file({}, static_cast<Utf16ByteOrder>(99)); });
  rejects([] { encode_utf16_file({}, static_cast<Utf16ByteOrder>(99)); });
  rejects([] { decode_utf16({}, static_cast<Utf16ByteOrder>(99)); });
  rejects([] { encode_utf16({}, static_cast<Utf16ByteOrder>(99)); });
  rejects([] { decode_utf16("x", Utf16ByteOrder::kLittleEndian); });
  rejects([] { encode_utf16(std::u32string(1, char32_t{0x110000}),
                            Utf16ByteOrder::kLittleEndian); });
}
}  // namespace

int main() {
  try {
    test_utf16();
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
