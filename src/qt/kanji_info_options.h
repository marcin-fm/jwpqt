// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace jwpqt::qt {

inline constexpr std::size_t kKanjiInfoFieldCount = 26;

struct KanjiInfoOptions {
  std::array<std::uint8_t, 60> fields{
      1, 2, 3, 4, 5, 6, 14, 8, 9, 10, 11, 12, 13, 7, 15, 16, 17, 18, 19, 20,
      21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38,
      39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56,
      57, 58, 59, 60};
  bool compact = false;
  bool headings = true;

  bool operator==(const KanjiInfoOptions& other) const noexcept {
    return fields == other.fields && compact == other.compact && headings == other.headings;
  }
};

void validate_kanji_info_options(const KanjiInfoOptions& options);
void select_kanji_info_field(KanjiInfoOptions& options, std::size_t slot,
                             std::uint8_t field);

}  // namespace jwpqt::qt
