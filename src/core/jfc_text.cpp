// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/jfc_text.h"

#include <cstddef>
#include <cstdint>
#include <optional>

#include "jwpqt/core/edict_dictionary.h"
#include "jwpqt/core/jis_unicode.h"
#include "jwpqt/core/legacy_code_page.h"
#include "jwpqt/core/utf8.h"

namespace jwpqt::core {
namespace {

[[noreturn]] void fail(std::size_t offset, std::string_view reason) {
  throw JfcTextError("JFC byte " + std::to_string(offset) + ": " +
                     std::string(reason));
}

}  // namespace

std::u32string decode_jfc_text(std::string_view bytes) {
  try {
    return decode_utf8_file(bytes).text;
  } catch (const Utf8Error&) {
    // Retry the entire file, never individual invalid UTF-8 sequences.
  }

  std::u32string result;
  result.reserve(bytes.size());
  for (std::size_t offset = 0; offset < bytes.size();) {
    const auto lead = static_cast<std::uint8_t>(bytes[offset]);
    if (lead <= 0x7fU) {
      result.push_back(static_cast<char32_t>(lead));
      ++offset;
      continue;
    }
    const std::size_t length = lead == 0x8fU ? 3 : 2;
    if (bytes.size() - offset < length) {
      fail(offset, "truncated old-EUC sequence");
    }
    const auto second = static_cast<std::uint8_t>(bytes[offset + 1]);
    std::optional<char32_t> code_point;
    if (lead == 0x8eU) {
      // JWP encodes code-page bytes, not standard EUC halfwidth katakana.
      code_point = legacy_byte_to_unicode(
          static_cast<std::uint8_t>(second | 0x80U), LegacyCodePage::k1252);
    } else if (lead == 0x8fU) {
      const auto third = static_cast<std::uint8_t>(bytes[offset + 2]);
      const auto mapped = edict_euc_0212_byte(second, third);
      if (!mapped.has_value()) {
        fail(offset, "JIS X 0212 sequence is outside the recovered JWP subset");
      }
      code_point = legacy_byte_to_unicode(*mapped, LegacyCodePage::k1252);
    } else {
      if (lead < 0xa1U || lead > 0xfeU || second < 0xa1U || second > 0xfeU) {
        fail(offset, "invalid old-EUC double-byte sequence");
      }
      const auto jis = static_cast<JisCode>(
          ((lead & 0x7fU) << 8U) | (second & 0x7fU));
      code_point = jis_x0208_to_unicode(jis);
    }
    if (!code_point.has_value()) {
      fail(offset, "unmapped old-EUC character");
    }
    result.push_back(*code_point);
    offset += length;
  }
  return result;
}

std::string encode_jfc_text(std::u32string_view text) {
  try {
    return encode_utf8(text);
  } catch (const Utf8Error& error) {
    throw JfcTextError(error.what());
  }
}

}  // namespace jwpqt::core
