// SPDX-License-Identifier: GPL-2.0-or-later

#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "jwpqt/core/utf8.h"

namespace {

void require(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

void expect_invalid(std::string_view bytes) {
  try {
    static_cast<void>(jwpqt::core::decode_utf8(bytes));
  } catch (const jwpqt::core::Utf8Error&) {
    return;
  }
  throw std::runtime_error("Malformed UTF-8 was accepted");
}

void test_round_trip() {
  const std::u32string text =
      U"\ufeffJWPqt: \u65e5\u672c\u8a9e, \u304b\u306a, \U0001f5fe\n";
  require(jwpqt::core::decode_utf8(jwpqt::core::encode_utf8(text)) == text,
          "Unicode text did not round-trip");
}

void test_boundaries() {
  const std::u32string text = {0x00, 0x7f, 0x80, 0x7ff, 0x800,
                               0xffff, 0x10000, 0x10ffff};
  require(jwpqt::core::decode_utf8(jwpqt::core::encode_utf8(text)) == text,
          "UTF-8 boundary values did not round-trip");
}

void test_bom() {
  require(jwpqt::core::decode_utf8("\xef\xbb\xbftext") == U"\ufefftext",
          "General UTF-8 decoder discarded U+FEFF");

  const jwpqt::core::Utf8File file =
      jwpqt::core::decode_utf8_file("\xef\xbb\xbftext");
  require(file.has_byte_order_mark, "File BOM was not detected");
  require(file.text == U"text", "File BOM leaked into document text");
  require(jwpqt::core::encode_utf8_file(file) == "\xef\xbb\xbftext",
          "File BOM was not preserved");
}

void test_malformed_input() {
  expect_invalid("\x80");
  expect_invalid("\xc0\x80");
  expect_invalid("\xe2\x82");
  expect_invalid("\xe2\x28\xa1");
  expect_invalid("\xed\xa0\x80");
  expect_invalid("\xf4\x90\x80\x80");
  expect_invalid("\xf5\x80\x80\x80");
}

void test_invalid_code_points() {
  try {
    static_cast<void>(jwpqt::core::encode_utf8(
        std::u32string{static_cast<char32_t>(0xd800)}));
    throw std::runtime_error("UTF-16 surrogate was encoded");
  } catch (const jwpqt::core::Utf8Error&) {
  }

  try {
    static_cast<void>(jwpqt::core::encode_utf8(
        std::u32string{static_cast<char32_t>(0x110000)}));
    throw std::runtime_error("Out-of-range code point was encoded");
  } catch (const jwpqt::core::Utf8Error&) {
  }
}

}  // namespace

int main() {
  try {
    test_round_trip();
    test_boundaries();
    test_bom();
    test_malformed_input();
    test_invalid_code_points();
    std::cout << "All UTF-8 tests passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
