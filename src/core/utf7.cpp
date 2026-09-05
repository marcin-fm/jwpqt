// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/utf7.h"

#include <array>
#include <cstdint>
#include <limits>
#include <vector>

namespace jwpqt::core {
namespace {

constexpr std::string_view kBase64 =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int base64_value(unsigned char byte) noexcept {
  if (byte >= 'A' && byte <= 'Z')
    return byte - 'A';
  if (byte >= 'a' && byte <= 'z')
    return byte - 'a' + 26;
  if (byte >= '0' && byte <= '9')
    return byte - '0' + 52;
  if (byte == '+')
    return 62;
  if (byte == '/')
    return 63;
  return -1;
}

bool direct_character(unsigned char byte) noexcept {
  static constexpr std::array<bool, 128> kMustShift = {
      true,  true,  true,  true,  true,  true,  true,  true,
      true,  false, false, true,  true,  false, true,  true,
      true,  true,  true,  true,  true,  true,  true,  true,
      true,  true,  true,  true,  true,  true,  true,  true,
      false, true,  true,  true,  true,  true,  true,  false,
      false, false, true,  true,  false, false, false, false,
      false, false, false, false, false, false, false, false,
      false, false, false, true,  true,  true,  true,  false,
      true,  false, false, false, false, false, false, false,
      false, false, false, false, false, false, false, false,
      false, false, false, false, false, false, false, false,
      false, false, false, true,  true,  true,  true,  true,
      true,  false, false, false, false, false, false, false,
      false, false, false, false, false, false, false, false,
      false, false, false, false, false, false, false, false,
      false, false, false, true,  true,  true,  true,  true,
  };
  return byte < kMustShift.size() && !kMustShift[byte];
}

void append_scalar(std::u32string& output, std::uint16_t unit,
                   std::uint16_t& pending_high) {
  if (pending_high != 0) {
    if (unit < 0xdc00U || unit > 0xdfffU)
      throw Utf7Error("UTF-7 high surrogate is not followed by a low surrogate");
    output.push_back(0x10000U +
                     ((static_cast<char32_t>(pending_high) - 0xd800U) << 10U) +
                     (static_cast<char32_t>(unit) - 0xdc00U));
    pending_high = 0;
    return;
  }
  if (unit >= 0xd800U && unit <= 0xdbffU) {
    pending_high = unit;
    return;
  }
  if (unit >= 0xdc00U && unit <= 0xdfffU)
    throw Utf7Error("UTF-7 contains an unpaired low surrogate");
  output.push_back(unit);
}

std::vector<std::uint16_t> utf16_units(std::u32string_view text,
                                       std::size_t begin, std::size_t end) {
  std::vector<std::uint16_t> units;
  if (end - begin > std::numeric_limits<std::size_t>::max() / 2)
    throw Utf7Error("UTF-7 input is too large");
  units.reserve((end - begin) * 2);
  for (std::size_t index = begin; index < end; ++index) {
    const char32_t scalar = text[index];
    if (scalar > 0x10ffffU || (scalar >= 0xd800U && scalar <= 0xdfffU))
      throw Utf7Error("UTF-7 input contains an invalid Unicode scalar");
    if (scalar < 0x10000U) {
      units.push_back(static_cast<std::uint16_t>(scalar));
    } else {
      const char32_t value = scalar - 0x10000U;
      units.push_back(static_cast<std::uint16_t>(0xd800U + (value >> 10U)));
      units.push_back(static_cast<std::uint16_t>(0xdc00U + (value & 0x3ffU)));
    }
  }
  return units;
}

void append_shifted(std::string& output,
                    const std::vector<std::uint16_t>& units) {
  output.push_back('+');
  std::uint32_t buffer = 0;
  unsigned bits = 0;
  for (const std::uint16_t unit : units) {
    buffer = (buffer << 16U) | unit;
    bits += 16;
    while (bits >= 6) {
      bits -= 6;
      output.push_back(kBase64[(buffer >> bits) & 0x3fU]);
      buffer &= bits == 0 ? 0U : ((1U << bits) - 1U);
    }
  }
  if (bits != 0)
    output.push_back(kBase64[(buffer << (6U - bits)) & 0x3fU]);
  output.push_back('-');
}

}  // namespace

std::u32string decode_utf7(std::string_view bytes) {
  std::u32string output;
  output.reserve(bytes.size());
  for (std::size_t index = 0; index < bytes.size();) {
    const unsigned char byte = static_cast<unsigned char>(bytes[index]);
    if (byte >= 0x80U)
      throw Utf7Error("UTF-7 contains a non-ASCII byte");
    if (byte != '+') {
      output.push_back(byte);
      ++index;
      continue;
    }
    if (++index == bytes.size())
      throw Utf7Error("UTF-7 ends with an incomplete shift sequence");
    if (bytes[index] == '-' || bytes[index] == '+') {
      output.push_back(U'+');
      ++index;
      continue;
    }

    std::uint32_t buffer = 0;
    unsigned bits = 0;
    std::size_t sextets = 0;
    std::uint16_t pending_high = 0;
    while (index < bytes.size()) {
      const unsigned char shifted = static_cast<unsigned char>(bytes[index]);
      const int value = base64_value(shifted);
      if (value < 0)
        break;
      buffer = (buffer << 6U) | static_cast<unsigned>(value);
      bits += 6;
      ++sextets;
      ++index;
      if (bits >= 16) {
        bits -= 16;
        append_scalar(output,
                      static_cast<std::uint16_t>((buffer >> bits) & 0xffffU),
                      pending_high);
        buffer &= bits == 0 ? 0U : ((1U << bits) - 1U);
      }
    }
    if (sextets < 3 || (bits != 0 && bits != 2 && bits != 4) ||
        (bits != 0 && buffer != 0) || pending_high != 0)
      throw Utf7Error("UTF-7 shift sequence is malformed");
    if (index < bytes.size() && bytes[index] == '-')
      ++index;
  }
  return output;
}

std::string encode_utf7(std::u32string_view text) {
  std::string output;
  output.reserve(text.size());
  for (std::size_t index = 0; index < text.size();) {
    const char32_t scalar = text[index];
    if (scalar > 0x10ffffU || (scalar >= 0xd800U && scalar <= 0xdfffU))
      throw Utf7Error("UTF-7 input contains an invalid Unicode scalar");
    if (scalar == U'+') {
      output.append("+-");
      ++index;
      continue;
    }
    if (scalar < 0x80U && direct_character(static_cast<unsigned char>(scalar))) {
      output.push_back(static_cast<char>(scalar));
      ++index;
      continue;
    }
    const std::size_t begin = index;
    while (index < text.size()) {
      const char32_t value = text[index];
      if (value == U'+' ||
          (value < 0x80U && direct_character(static_cast<unsigned char>(value))))
        break;
      if (value > 0x10ffffU || (value >= 0xd800U && value <= 0xdfffU))
        throw Utf7Error("UTF-7 input contains an invalid Unicode scalar");
      ++index;
    }
    append_shifted(output, utf16_units(text, begin, index));
  }
  return output;
}

}  // namespace jwpqt::core
