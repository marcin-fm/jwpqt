// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/utf8.h"

#include <cstdint>
#include <iomanip>
#include <sstream>

namespace jwpqt::core {
namespace {

[[noreturn]] void invalid_utf8(std::size_t offset) {
  throw Utf8Error("Invalid UTF-8 at byte " + std::to_string(offset));
}

[[noreturn]] void invalid_code_point(char32_t code_point,
                                     std::size_t offset) {
  std::ostringstream message;
  message << "Invalid Unicode code point U+" << std::uppercase << std::hex
          << static_cast<std::uint32_t>(code_point) << " at character "
          << std::dec << offset;
  throw Utf8Error(message.str());
}

bool is_continuation(unsigned char byte) {
  return (byte & 0xc0U) == 0x80U;
}

}  // namespace

std::u32string decode_utf8(std::string_view bytes) {
  std::u32string result;
  result.reserve(bytes.size());

  std::size_t offset = 0;

  while (offset < bytes.size()) {
    const auto start = offset;
    const auto lead = static_cast<unsigned char>(bytes[offset++]);
    if (lead < 0x80U) {
      result.push_back(static_cast<char32_t>(lead));
      continue;
    }

    std::size_t continuation_count = 0;
    char32_t code_point = 0;
    char32_t minimum = 0;
    if (lead >= 0xc2U && lead <= 0xdfU) {
      continuation_count = 1;
      code_point = static_cast<char32_t>(lead & 0x1fU);
      minimum = 0x80;
    } else if (lead >= 0xe0U && lead <= 0xefU) {
      continuation_count = 2;
      code_point = static_cast<char32_t>(lead & 0x0fU);
      minimum = 0x800;
    } else if (lead >= 0xf0U && lead <= 0xf4U) {
      continuation_count = 3;
      code_point = static_cast<char32_t>(lead & 0x07U);
      minimum = 0x10000;
    } else {
      invalid_utf8(start);
    }

    if (bytes.size() - offset < continuation_count) {
      invalid_utf8(start);
    }

    for (std::size_t index = 0; index < continuation_count; ++index) {
      const auto byte = static_cast<unsigned char>(bytes[offset++]);
      if (!is_continuation(byte)) {
        invalid_utf8(offset - 1);
      }
      code_point = (code_point << 6U) | static_cast<char32_t>(byte & 0x3fU);
    }

    if (code_point < minimum || code_point > 0x10ffffU ||
        (code_point >= 0xd800U && code_point <= 0xdfffU)) {
      invalid_utf8(start);
    }
    result.push_back(code_point);
  }

  return result;
}

std::string encode_utf8(std::u32string_view text) {
  std::string result;
  result.reserve(text.size());

  for (std::size_t offset = 0; offset < text.size(); ++offset) {
    const char32_t code_point = text[offset];
    if (code_point <= 0x7fU) {
      result.push_back(static_cast<char>(code_point));
    } else if (code_point <= 0x7ffU) {
      result.push_back(static_cast<char>(0xc0U | (code_point >> 6U)));
      result.push_back(static_cast<char>(0x80U | (code_point & 0x3fU)));
    } else if (code_point >= 0xd800U && code_point <= 0xdfffU) {
      invalid_code_point(code_point, offset);
    } else if (code_point <= 0xffffU) {
      result.push_back(static_cast<char>(0xe0U | (code_point >> 12U)));
      result.push_back(
          static_cast<char>(0x80U | ((code_point >> 6U) & 0x3fU)));
      result.push_back(static_cast<char>(0x80U | (code_point & 0x3fU)));
    } else if (code_point <= 0x10ffffU) {
      result.push_back(static_cast<char>(0xf0U | (code_point >> 18U)));
      result.push_back(
          static_cast<char>(0x80U | ((code_point >> 12U) & 0x3fU)));
      result.push_back(
          static_cast<char>(0x80U | ((code_point >> 6U) & 0x3fU)));
      result.push_back(static_cast<char>(0x80U | (code_point & 0x3fU)));
    } else {
      invalid_code_point(code_point, offset);
    }
  }

  return result;
}

Utf8File decode_utf8_file(std::string_view bytes) {
  constexpr std::string_view byte_order_mark{"\xef\xbb\xbf", 3};
  const bool has_byte_order_mark = bytes.substr(0, 3) == byte_order_mark;
  if (has_byte_order_mark) {
    bytes.remove_prefix(byte_order_mark.size());
  }
  return Utf8File{decode_utf8(bytes), has_byte_order_mark};
}

std::string encode_utf8_file(const Utf8File& file) {
  std::string bytes = encode_utf8(file.text);
  if (file.has_byte_order_mark) {
    bytes.insert(0, "\xef\xbb\xbf", 3);
  }
  return bytes;
}

}  // namespace jwpqt::core
