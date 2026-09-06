// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdint>
#include <vector>

namespace jwpqt::core {

struct KanjiSpahnChoice {
  std::uint16_t sprite_index = 0;
  std::uint8_t radical_strokes = 0;
  std::uint8_t radical = 0;
};

std::vector<KanjiSpahnChoice> kanji_spahn_choices(
    std::uint8_t radical_strokes, bool include_variants);

}  // namespace jwpqt::core
