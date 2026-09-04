// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/legacy_text.h"

#include <cstddef>
#include <cstdint>
#include <string>

#include "jwpqt/core/jis_encoding.h"
#include "jwpqt/core/jis_unicode.h"

namespace jwpqt::core {
namespace {

unsigned int byte_at(std::string_view bytes, std::size_t index) {
  return static_cast<unsigned int>(
      static_cast<unsigned char>(bytes[index]));
}

[[noreturn]] void fail_byte(std::string_view encoding, std::size_t offset,
                            std::string_view reason) {
  throw LegacyTextError(std::string(encoding) + " byte " +
                        std::to_string(offset) + ": " + std::string(reason));
}

[[noreturn]] void fail_character(std::string_view encoding,
                                 std::size_t index) {
  throw LegacyTextError(std::string(encoding) + " character " +
                        std::to_string(index) + ": not representable");
}

char32_t decode_mapped_pair(EncodedPair pair, LegacyEncoding encoding,
                            std::size_t offset) {
  JisCode jis = 0;
  try {
    switch (encoding) {
      case LegacyEncoding::kEucJp:
        jis = decode_euc_jp_pair(pair);
        break;
      case LegacyEncoding::kShiftJis:
        jis = decode_shift_jis_pair(pair);
        break;
    }
  } catch (const JisEncodingError&) {
    fail_byte(encoding == LegacyEncoding::kEucJp ? "EUC-JP" : "Shift-JIS",
              offset, "invalid double-byte sequence");
  }

  const std::optional<char32_t> code_point = jis_x0208_to_unicode(jis);
  if (!code_point.has_value()) {
    fail_byte(encoding == LegacyEncoding::kEucJp ? "EUC-JP" : "Shift-JIS",
              offset, "unmapped JIS X 0208 character");
  }
  return *code_point;
}

std::u32string decode_euc_jp(std::string_view bytes) {
  std::u32string output;
  output.reserve(bytes.size());

  for (std::size_t i = 0; i < bytes.size();) {
    const unsigned int lead = byte_at(bytes, i);
    if (lead <= 0x7fU) {
      output.push_back(static_cast<char32_t>(lead));
      ++i;
      continue;
    }
    if (lead == 0x8eU) {
      fail_byte("EUC-JP", i, "JIS X 0201 halfwidth kana is not supported");
    }
    if (lead == 0x8fU) {
      fail_byte("EUC-JP", i, "JIS X 0212 is not supported");
    }
    if (lead < 0xa1U || lead > 0xfeU) {
      fail_byte("EUC-JP", i, "invalid lead byte");
    }
    if (i + 1 >= bytes.size()) {
      fail_byte("EUC-JP", i, "truncated double-byte sequence");
    }

    const unsigned int trail = byte_at(bytes, i + 1);
    if (trail < 0xa1U || trail > 0xfeU) {
      fail_byte("EUC-JP", i, "invalid trail byte");
    }
    output.push_back(decode_mapped_pair(
        {static_cast<std::uint8_t>(lead), static_cast<std::uint8_t>(trail)},
        LegacyEncoding::kEucJp, i));
    i += 2;
  }
  return output;
}

std::u32string decode_shift_jis(std::string_view bytes) {
  std::u32string output;
  output.reserve(bytes.size());

  for (std::size_t i = 0; i < bytes.size();) {
    const unsigned int lead = byte_at(bytes, i);
    if (lead <= 0x7fU) {
      output.push_back(static_cast<char32_t>(lead));
      ++i;
      continue;
    }
    if (lead >= 0xa1U && lead <= 0xdfU) {
      fail_byte("Shift-JIS", i,
                "JIS X 0201 halfwidth kana is not supported");
    }
    const bool valid_lead =
        (lead >= 0x81U && lead <= 0x9fU) ||
        (lead >= 0xe0U && lead <= 0xefU);
    if (!valid_lead) {
      fail_byte("Shift-JIS", i, "invalid or unsupported lead byte");
    }
    if (i + 1 >= bytes.size()) {
      fail_byte("Shift-JIS", i, "truncated double-byte sequence");
    }

    const unsigned int trail = byte_at(bytes, i + 1);
    const bool valid_trail =
        (trail >= 0x40U && trail <= 0x7eU) ||
        (trail >= 0x80U && trail <= 0xfcU);
    if (!valid_trail) {
      fail_byte("Shift-JIS", i, "invalid trail byte");
    }
    output.push_back(decode_mapped_pair(
        {static_cast<std::uint8_t>(lead), static_cast<std::uint8_t>(trail)},
        LegacyEncoding::kShiftJis, i));
    i += 2;
  }
  return output;
}

std::string encode_with(std::u32string_view text, LegacyEncoding encoding) {
  std::string output;
  output.reserve(text.size() * 2);

  for (std::size_t i = 0; i < text.size(); ++i) {
    const char32_t code_point = text[i];
    if (code_point <= 0x7fU) {
      output.push_back(static_cast<char>(code_point));
      continue;
    }

    const std::optional<JisCode> jis = unicode_to_jis_x0208(code_point);
    if (!jis.has_value()) {
      fail_character(encoding == LegacyEncoding::kEucJp ? "EUC-JP"
                                                         : "Shift-JIS",
                     i);
    }
    const EncodedPair pair = encoding == LegacyEncoding::kEucJp
                                 ? encode_euc_jp_pair(*jis)
                                 : encode_shift_jis_pair(*jis);
    output.push_back(static_cast<char>(pair.lead));
    output.push_back(static_cast<char>(pair.trail));
  }
  return output;
}

}  // namespace

std::u32string decode_legacy_text(std::string_view bytes,
                                  LegacyEncoding encoding) {
  switch (encoding) {
    case LegacyEncoding::kEucJp:
      return decode_euc_jp(bytes);
    case LegacyEncoding::kShiftJis:
      return decode_shift_jis(bytes);
  }
  throw LegacyTextError("Unknown legacy encoding");
}

std::string encode_legacy_text(std::u32string_view text,
                               LegacyEncoding encoding) {
  switch (encoding) {
    case LegacyEncoding::kEucJp:
    case LegacyEncoding::kShiftJis:
      return encode_with(text, encoding);
  }
  throw LegacyTextError("Unknown legacy encoding");
}

}  // namespace jwpqt::core
