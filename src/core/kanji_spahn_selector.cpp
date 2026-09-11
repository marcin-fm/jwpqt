// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/kanji_spahn_selector.h"

#include <algorithm>
#include <iterator>
#include <stdexcept>

namespace jwpqt::core {
namespace {

// Recovered hs_data, hs_radlist and hs_radvalue from jwpxp-1.67:jwp_lkup.cpp.
constexpr std::uint16_t kStarts[]{0, 0, 0, 30, 57, 79, 89, 96, 102, 112, 113, 114, 116};
constexpr std::uint8_t kBases[]{0, 0, 0, 19, 37, 50, 59, 65, 70, 75, 76, 77};
constexpr std::uint8_t kCanonical[]{
    0, 3, 4, 6, 7, 8, 11, 12, 13, 14, 15, 17, 19, 20, 23, 24, 27, 28, 29,
    30, 34, 36, 38, 40, 41, 42, 44, 45, 46, 47, 48, 49, 52, 53, 54, 55, 56,
    57, 58, 59, 60, 62, 64, 66, 67, 68, 71, 72, 75, 77,
    79, 80, 81, 82, 83, 85, 86, 87, 88,
    89, 90, 91, 92, 93, 94,
    96, 97, 98, 99, 101,
    102, 103, 106, 107, 110,
    112, 113, 114, 115};
constexpr std::uint8_t kValues[]{
    1, 1, 1, 2, 3, 3, 4, 5, 6, 6, 6, 7, 8, 9, 10, 11, 11, 12, 12, 13,
    14, 14, 14, 15, 16, 16, 16, 17, 18, 19,
    20, 20, 20, 20, 21, 21, 22, 22, 23, 23, 24, 25, 26, 26, 27, 28, 29,
    30, 31, 32, 32, 32, 33, 34, 35, 36, 37,
    38, 39, 40, 41, 41, 42, 42, 43, 43, 44, 45, 46, 46, 46, 47, 48, 48,
    48, 49, 49, 50, 50,
    51, 52, 53, 54, 55, 55, 56, 57, 58, 59,
    60, 61, 62, 63, 64, 65, 65,
    66, 67, 68, 69, 69, 70,
    71, 72, 72, 72, 73, 74, 74, 74, 75, 75,
    76, 77, 78, 79};
static_assert(std::size(kValues) == 116 && std::size(kCanonical) == 79);

}  // namespace

std::vector<KanjiSpahnChoice> kanji_spahn_choices(
    std::uint8_t radical_strokes, bool include_variants) {
  if (radical_strokes > 11)
    throw std::invalid_argument("Spahn radical stroke count is out of range");
  std::vector<KanjiSpahnChoice> result;
  for (std::uint8_t strokes = 2; strokes <= 11; ++strokes) {
    if (radical_strokes != 0 && radical_strokes != strokes) continue;
    for (std::uint16_t sprite = kStarts[strokes]; sprite < kStarts[strokes + 1]; ++sprite) {
      if (!include_variants &&
          !std::binary_search(std::begin(kCanonical), std::end(kCanonical), sprite))
        continue;
      const unsigned ordinal = static_cast<unsigned>(kValues[sprite] - kBases[strokes]);
      result.push_back({sprite, strokes,
                        static_cast<std::uint8_t>(ordinal < 12 ? ordinal - 1 : ordinal)});
    }
  }
  return result;
}

}  // namespace jwpqt::core
