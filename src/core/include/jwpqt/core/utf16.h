// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef JWPQT_CORE_UTF16_H
#define JWPQT_CORE_UTF16_H

#include <stdexcept>
#include <string>
#include <string_view>

namespace jwpqt::core {

enum class Utf16ByteOrder { kLittleEndian, kBigEndian };

class Utf16Error : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

struct Utf16File {
  std::u32string text;
  bool has_byte_order_mark = false;
};

// Raw strings have no file signature: initial U+FEFF/U+FFFE remain content.
std::u32string decode_utf16(std::string_view bytes, Utf16ByteOrder order);
std::string encode_utf16(std::u32string_view text, Utf16ByteOrder order);

// An initial BOM is consumed. Write a BOM to preserve a leading text U+FEFF.
Utf16File decode_utf16_file(std::string_view bytes, Utf16ByteOrder order);
std::string encode_utf16_file(const Utf16File& file, Utf16ByteOrder order);

}  // namespace jwpqt::core

#endif
