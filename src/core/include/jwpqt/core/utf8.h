// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_CORE_UTF8_H
#define JWPQT_CORE_UTF8_H

#include <stdexcept>
#include <string>
#include <string_view>

namespace jwpqt::core {

class Utf8Error : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

struct Utf8File {
  std::u32string text;
  bool has_byte_order_mark = false;
};

std::u32string decode_utf8(std::string_view bytes);
std::string encode_utf8(std::u32string_view text);

Utf8File decode_utf8_file(std::string_view bytes);
std::string encode_utf8_file(const Utf8File& file);

}  // namespace jwpqt::core

#endif
