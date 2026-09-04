// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/jis_text.h"

#include <cstddef>
#include <string>

#include "jwpqt/core/jis_encoding.h"
#include "jwpqt/core/jis_unicode.h"

namespace jwpqt::core {
namespace {

constexpr unsigned int kEscape = 0x1bU;

std::string_view encoding_name(JisTextEncoding encoding) {
  switch (encoding) {
    case JisTextEncoding::kNewJis:
      return "New JIS";
    case JisTextEncoding::kOldJis:
      return "Old JIS";
    case JisTextEncoding::kNecJis:
      return "NEC JIS";
  }
  throw JisTextError("Unknown JIS text encoding");
}

unsigned int byte_at(std::string_view bytes, std::size_t index) {
  return static_cast<unsigned int>(
      static_cast<unsigned char>(bytes[index]));
}

[[noreturn]] void fail_byte(JisTextEncoding encoding, std::size_t offset,
                            std::string_view reason) {
  throw JisTextError(std::string(encoding_name(encoding)) + " byte " +
                     std::to_string(offset) + ": " + std::string(reason));
}

[[noreturn]] void fail_character(JisTextEncoding encoding,
                                 std::size_t index) {
  throw JisTextError(std::string(encoding_name(encoding)) + " character " +
                     std::to_string(index) + ": not representable");
}

std::string_view enter_sequence(JisTextEncoding encoding) {
  switch (encoding) {
    case JisTextEncoding::kNewJis:
      return "\x1b$B";
    case JisTextEncoding::kOldJis:
      return "\x1b$@";
    case JisTextEncoding::kNecJis:
      return "\x1bK";
  }
  throw JisTextError("Unknown JIS text encoding");
}

std::string_view exit_sequence(JisTextEncoding encoding) {
  switch (encoding) {
    case JisTextEncoding::kNewJis:
    case JisTextEncoding::kOldJis:
      return "\x1b(J";
    case JisTextEncoding::kNecJis:
      return "\x1bH";
  }
  throw JisTextError("Unknown JIS text encoding");
}

std::size_t consume_escape(std::string_view bytes, std::size_t offset,
                           JisTextEncoding encoding, bool& in_two_byte) {
  if (offset + 1 >= bytes.size()) {
    fail_byte(encoding, offset, "truncated escape sequence");
  }
  const unsigned int middle = byte_at(bytes, offset + 1);
  if (middle == static_cast<unsigned int>('K')) {
    in_two_byte = true;
    return 2;
  }
  if (middle == static_cast<unsigned int>('H')) {
    in_two_byte = false;
    return 2;
  }

  if (middle != static_cast<unsigned int>('$') &&
      middle != static_cast<unsigned int>('(')) {
    fail_byte(encoding, offset, "unsupported escape sequence");
  }
  if (offset + 2 >= bytes.size()) {
    fail_byte(encoding, offset, "truncated escape sequence");
  }
  const unsigned int marker = byte_at(bytes, offset + 2);
  if (middle == static_cast<unsigned int>('$') &&
      (marker == static_cast<unsigned int>('B') ||
       marker == static_cast<unsigned int>('@'))) {
    in_two_byte = true;
  } else if (middle == static_cast<unsigned int>('(') &&
             (marker == static_cast<unsigned int>('B') ||
              marker == static_cast<unsigned int>('J'))) {
    in_two_byte = false;
  } else {
    fail_byte(encoding, offset, "unsupported escape sequence");
  }
  return 3;
}

}  // namespace

std::u32string decode_jis_text(std::string_view bytes,
                               JisTextEncoding encoding) {
  static_cast<void>(encoding_name(encoding));
  std::u32string output;
  output.reserve(bytes.size());
  bool in_two_byte = false;

  for (std::size_t i = 0; i < bytes.size();) {
    const unsigned int lead = byte_at(bytes, i);
    if (lead == kEscape) {
      i += consume_escape(bytes, i, encoding, in_two_byte);
      continue;
    }
    if (lead == static_cast<unsigned int>('\r') ||
        lead == static_cast<unsigned int>('\n')) {
      in_two_byte = false;
      output.push_back(static_cast<char32_t>(lead));
      ++i;
      continue;
    }
    if (!in_two_byte) {
      if (lead > 0x7fU) {
        fail_byte(encoding, i, "non-ASCII byte outside JIS mode");
      }
      output.push_back(static_cast<char32_t>(lead));
      ++i;
      continue;
    }

    if (i + 1 >= bytes.size()) {
      fail_byte(encoding, i, "truncated JIS X 0208 pair");
    }
    const unsigned int trail = byte_at(bytes, i + 1);
    const JisCode jis = static_cast<JisCode>((lead << 8U) | trail);
    if (!is_jis_x0208_pair(jis)) {
      fail_byte(encoding, i, "invalid JIS X 0208 pair");
    }
    const std::optional<char32_t> code_point = jis_x0208_to_unicode(jis);
    if (!code_point.has_value()) {
      fail_byte(encoding, i, "unmapped JIS X 0208 character");
    }
    output.push_back(*code_point);
    i += 2;
  }
  return output;
}

std::string encode_jis_text(std::u32string_view text,
                            JisTextEncoding encoding) {
  static_cast<void>(encoding_name(encoding));
  std::string output;
  output.reserve(text.size() * 2);
  bool in_two_byte = false;

  for (std::size_t i = 0; i < text.size(); ++i) {
    const char32_t code_point = text[i];
    if (code_point <= 0x7fU) {
      if (code_point == kEscape) {
        fail_character(encoding, i);
      }
      if (in_two_byte) {
        output.append(exit_sequence(encoding));
        in_two_byte = false;
      }
      output.push_back(static_cast<char>(code_point));
      continue;
    }

    const std::optional<JisCode> jis = unicode_to_jis_x0208(code_point);
    if (!jis.has_value()) {
      fail_character(encoding, i);
    }
    if (!in_two_byte) {
      output.append(enter_sequence(encoding));
      in_two_byte = true;
    }
    output.push_back(static_cast<char>(*jis >> 8U));
    output.push_back(static_cast<char>(*jis & 0xffU));
  }
  if (in_two_byte) {
    output.append(exit_sequence(encoding));
  }
  return output;
}

}  // namespace jwpqt::core
