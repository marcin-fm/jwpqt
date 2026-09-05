// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdint>
#include <vector>

namespace jwpqt::core {

inline constexpr std::uint8_t kMaximumBushuRadicalStrokes = 17;

struct KanjiBushuChoice {
  std::uint16_t sprite_index = 0;
  std::uint8_t bushu = 0;

  bool operator==(const KanjiBushuChoice& other) const noexcept {
    return sprite_index == other.sprite_index && bushu == other.bushu;
  }
};

std::vector<KanjiBushuChoice> kanji_bushu_choices(
    std::uint8_t radical_strokes, bool include_variants);

}  // namespace jwpqt::core
