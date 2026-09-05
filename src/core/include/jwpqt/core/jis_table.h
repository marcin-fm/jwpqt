// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_CORE_JIS_TABLE_H
#define JWPQT_CORE_JIS_TABLE_H

#include <cstdint>
#include <optional>
#include <vector>

#include "jwpqt/core/jis_encoding.h"

namespace jwpqt::core {

struct JisTableEntry {
  JisCode jis = 0;
  EncodedPair euc{};
  EncodedPair shift_jis{};
  char32_t unicode = 0;

  bool operator==(const JisTableEntry& other) const noexcept;
};

std::optional<JisTableEntry> describe_jis_character(JisCode code) noexcept;
std::optional<JisTableEntry> describe_unicode_character(
    char32_t code_point) noexcept;
std::vector<JisTableEntry> jis_table_page(std::uint8_t page);

}  // namespace jwpqt::core

#endif
