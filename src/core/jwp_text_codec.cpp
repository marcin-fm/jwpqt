// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/jwp_text_codec.h"

#include "jwpqt/core/jis_unicode.h"

#include <cstddef>
#include <iomanip>
#include <optional>
#include <sstream>

namespace jwpqt::core {
namespace {

[[noreturn]] void throw_decode_error(JisCode code, std::size_t index) {
  std::ostringstream message;
  message << "JWP text code 0x" << std::uppercase << std::hex
          << static_cast<unsigned int>(code) << " at index " << std::dec
          << index << " has no Unicode mapping";
  throw JwpTextCodecError(message.str());
}

[[noreturn]] void throw_encode_error(char32_t code_point,
                                     std::size_t index) {
  std::ostringstream message;
  message << "Unicode code point U+" << std::uppercase << std::hex
          << static_cast<std::uint32_t>(code_point) << " at index " << std::dec
          << index << " cannot be represented in JWP text";
  throw JwpTextCodecError(message.str());
}

bool prefers_extended_byte(char32_t code_point,
                           LegacyCodePage code_page) noexcept {
  if (code_page == LegacyCodePage::k1251) {
    return code_point == U'\u0401' || code_point == U'\u0451' ||
           (code_point >= U'\u0410' && code_point <= U'\u044f');
  }
  if (code_page == LegacyCodePage::k1253) {
    return code_point >= U'\u0391' && code_point <= U'\u03c9';
  }
  return false;
}

bool is_legacy_misconstrued_code_point(char32_t code_point) noexcept {
  return code_point == U'\u201a' || code_point == U'\u0192' ||
         code_point == U'\u201e';
}

}  // namespace

std::u32string decode_jwp_text(const JwpText& text,
                               LegacyCodePage code_page) {
  std::u32string output;
  output.reserve(text.size());
  for (std::size_t index = 0; index < text.size(); ++index) {
    const JisCode code = text[index];
    std::optional<char32_t> code_point;
    if (code <= 0xffU) {
      code_point = legacy_byte_to_unicode(static_cast<std::uint8_t>(code),
                                          code_page);
    } else {
      code_point = jis_x0208_to_unicode(code);
    }
    if (!code_point.has_value() || *code_point == U'\0') {
      throw_decode_error(code, index);
    }
    output.push_back(*code_point);
  }
  return output;
}

std::optional<JisCode> unicode_to_jwp_code(char32_t code_point,
                                         LegacyCodePage code_page) noexcept {
  if (code_point == U'\0' || is_legacy_misconstrued_code_point(code_point)) return std::nullopt;
  if (code_point <= U'\u007e') return static_cast<JisCode>(code_point);
  const auto extended = unicode_to_legacy_byte(code_point, code_page);
  if (prefers_extended_byte(code_point, code_page) && extended) return *extended;
  if (const auto jis = unicode_to_jis_x0208(code_point)) return jis;
  if (extended) return *extended;
  return std::nullopt;
}

JwpText encode_jwp_text(std::u32string_view text,
                        LegacyCodePage code_page) {
  JwpText output;
  output.reserve(text.size());
  for (std::size_t index = 0; index < text.size(); ++index) {
    const auto code = unicode_to_jwp_code(text[index], code_page);
    if (!code) throw_encode_error(text[index], index);
    output.push_back(*code);
  }
  return output;
}

}  // namespace jwpqt::core
