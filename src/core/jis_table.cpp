// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/jis_table.h"

#include "jwpqt/core/jis_unicode.h"

namespace jwpqt::core {

bool JisTableEntry::operator==(const JisTableEntry& other) const noexcept {
  return jis == other.jis && euc.lead == other.euc.lead &&
         euc.trail == other.euc.trail &&
         shift_jis.lead == other.shift_jis.lead &&
         shift_jis.trail == other.shift_jis.trail && unicode == other.unicode;
}

std::optional<JisTableEntry> describe_jis_character(JisCode code) noexcept {
  const std::optional<char32_t> unicode = jis_x0208_to_unicode(code);
  if (!unicode.has_value()) return std::nullopt;
  try {
    return JisTableEntry{code, encode_euc_jp_pair(code),
                         encode_shift_jis_pair(code), *unicode};
  } catch (const JisEncodingError&) {
    return std::nullopt;
  }
}

std::optional<JisTableEntry> describe_unicode_character(
    char32_t code_point) noexcept {
  const std::optional<JisCode> jis = unicode_to_jis_x0208(code_point);
  return jis.has_value() ? describe_jis_character(*jis) : std::nullopt;
}

std::vector<JisTableEntry> jis_table_page(std::uint8_t page) {
  std::vector<JisTableEntry> entries;
  if (page < 0x21U || page > 0x74U) return entries;
  entries.reserve(94);
  for (std::uint16_t cell = 0x21U; cell <= 0x7eU; ++cell) {
    const JisCode code =
        static_cast<JisCode>((static_cast<std::uint16_t>(page) << 8U) | cell);
    if (const auto entry = describe_jis_character(code); entry.has_value())
      entries.push_back(*entry);
  }
  return entries;
}

}  // namespace jwpqt::core
