// SPDX-License-Identifier: GPL-2.0-or-later
#include "jwpqt/core/utf16.h"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace jwpqt::core {
namespace {

bool valid_order(Utf16ByteOrder order) {
  return order == Utf16ByteOrder::kLittleEndian ||
         order == Utf16ByteOrder::kBigEndian;
}

std::uint16_t read_unit(const char* bytes, Utf16ByteOrder order) {
  const auto first = static_cast<std::uint8_t>(bytes[0]);
  const auto second = static_cast<std::uint8_t>(bytes[1]);
  return order == Utf16ByteOrder::kLittleEndian
             ? static_cast<std::uint16_t>(first | (second << 8))
             : static_cast<std::uint16_t>((first << 8) | second);
}

void append_unit(std::string& bytes, std::uint16_t unit, Utf16ByteOrder order) {
  bytes.push_back(static_cast<char>(
      order == Utf16ByteOrder::kLittleEndian ? unit & 0xff : unit >> 8));
  bytes.push_back(static_cast<char>(
      order == Utf16ByteOrder::kLittleEndian ? unit >> 8 : unit & 0xff));
}

Utf16File decode_utf16_data(std::string_view bytes, Utf16ByteOrder order,
                            bool file_signature) {
  if (!valid_order(order)) throw Utf16Error("Invalid UTF-16 byte order");
  if (bytes.size() % 2 != 0) {
    throw Utf16Error("UTF-16 input has odd length at byte offset " +
                     std::to_string(bytes.size() - 1));
  }
  std::size_t offset = 0;
  Utf16File file;
  if (file_signature && bytes.size() >= 2) {
    const auto first = read_unit(bytes.data(), order);
    if (first == 0xfffe)
      throw Utf16Error("Opposite UTF-16 byte-order mark at byte offset 0");
    if (first == 0xfeff) {
      file.has_byte_order_mark = true;
      offset = 2;
    }
  }
  file.text.reserve((bytes.size() - offset) / 2);
  while (offset < bytes.size()) {
    const auto unit_offset = offset;
    const auto first = read_unit(bytes.data() + offset, order);
    offset += 2;
    if (first >= 0xdc00 && first <= 0xdfff) {
      throw Utf16Error("Isolated low surrogate at byte offset " +
                       std::to_string(unit_offset));
    }
    if (first >= 0xd800 && first <= 0xdbff) {
      if (offset == bytes.size()) {
        throw Utf16Error("Truncated surrogate pair at byte offset " +
                         std::to_string(unit_offset));
      }
      const auto second = read_unit(bytes.data() + offset, order);
      if (second < 0xdc00 || second > 0xdfff) {
        throw Utf16Error("Invalid surrogate pair at byte offset " +
                         std::to_string(unit_offset));
      }
      offset += 2;
      file.text.push_back(0x10000u +
                          ((static_cast<char32_t>(first) - 0xd800u) << 10) +
                          (static_cast<char32_t>(second) - 0xdc00u));
    } else {
      file.text.push_back(first);
    }
  }
  return file;
}

std::string encode_utf16_data(std::u32string_view text, Utf16ByteOrder order,
                              bool byte_order_mark) {
  if (!valid_order(order)) throw Utf16Error("Invalid UTF-16 byte order");
  std::size_t units = byte_order_mark ? 1 : 0;
  for (std::size_t index = 0; index < text.size(); ++index) {
    const auto value = text[index];
    if (value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) {
      throw Utf16Error("Invalid Unicode scalar at code-point offset " +
                       std::to_string(index));
    }
    const std::size_t count = value > 0xffff ? 2 : 1;
    if (units > std::numeric_limits<std::size_t>::max() / 2 - count)
      throw Utf16Error("UTF-16 output size overflow");
    units += count;
  }
  std::string bytes;
  bytes.reserve(units * 2);
  if (byte_order_mark) append_unit(bytes, 0xfeff, order);
  for (const auto value : text) {
    if (value <= 0xffff) {
      append_unit(bytes, static_cast<std::uint16_t>(value), order);
    } else {
      append_unit(bytes, static_cast<std::uint16_t>(
                             0xd800u + ((value - 0x10000u) >> 10)), order);
      append_unit(bytes, static_cast<std::uint16_t>(
                             0xdc00u + ((value - 0x10000u) & 0x3ffu)), order);
    }
  }
  return bytes;
}

}  // namespace

std::u32string decode_utf16(std::string_view bytes, Utf16ByteOrder order) {
  return decode_utf16_data(bytes, order, false).text;
}

std::string encode_utf16(std::u32string_view text, Utf16ByteOrder order) {
  return encode_utf16_data(text, order, false);
}

Utf16File decode_utf16_file(std::string_view bytes, Utf16ByteOrder order) {
  return decode_utf16_data(bytes, order, true);
}

std::string encode_utf16_file(const Utf16File& file, Utf16ByteOrder order) {
  return encode_utf16_data(file.text, order, file.has_byte_order_mark);
}

}  // namespace jwpqt::core
