// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/kanji_bushu_selector.h"

#include <array>
#include <stdexcept>

namespace jwpqt::core {
namespace {

struct BushuGroup {
  std::uint8_t variant_count;
  std::uint8_t reduced_count;
  std::uint8_t variant_start;
  std::uint8_t reduced_start;
};

constexpr std::array<BushuGroup, 18> kGroups{{
    {241, 211, 0, 0},   {6, 6, 0, 0},       {28, 23, 6, 6},
    {40, 30, 34, 29},   {43, 34, 74, 59},   {25, 23, 117, 93},
    {29, 29, 142, 116}, {19, 18, 171, 145}, {10, 9, 190, 163},
    {11, 11, 200, 172}, {9, 8, 211, 183},   {7, 6, 220, 191},
    {4, 4, 227, 197},   {4, 4, 231, 201},   {2, 2, 235, 205},
    {1, 1, 237, 207},   {2, 2, 238, 208},   {1, 1, 240, 209},
}};

// Sprite index is zero-based; the leading zero preserves the recovered
// source's one-based radical_value = &bushus[1] lookup.
constexpr std::array<std::uint8_t, 242> kBushuBySprite{{
    0,
    1, 2, 3, 4, 5, 6,
    7, 8, 9, 9, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 18, 19, 20, 21,
    22, 24, 25, 26, 27, 28, 29, 162, 163, 170,
    30, 31, 32, 33, 34, 36, 37, 38, 39, 40, 41, 42, 42, 43, 44, 45, 46,
    47, 47, 48, 49, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 58, 58, 59,
    60, 61, 64, 85, 94, 140,
    61, 255, 62, 63, 64, 65, 66, 66, 67, 68, 69, 70, 71, 71, 72, 73, 74,
    75, 76, 77, 78, 79, 80, 80, 81, 82, 83, 84, 85, 86, 86, 87, 88, 89,
    90, 91, 92, 93, 94, 96, 113, 125, 130,
    95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109,
    110, 111, 112, 113, 114, 115, 116, 117, 122, 145,
    118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131,
    132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145,
    146,
    147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160,
    161, 163, 164, 165, 166,
    167, 168, 169, 170, 171, 172, 173, 174, 175, 210,
    176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186,
    187, 188, 189, 190, 191, 192, 193, 194, 212,
    195, 196, 197, 198, 199, 200, 213,
    201, 202, 203, 204,
    205, 206, 207, 208,
    209, 210,
    211,
    212, 213,
    214,
}};

constexpr std::array<std::uint8_t, 211> kReducedSprites{{
    0, 1, 2, 3, 4, 5,
    6, 7, 8, 11, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 24, 25,
    26, 27, 28, 29, 30, 31,
    34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 47, 48, 49, 50, 51,
    53, 54, 56, 57, 58, 59, 60, 61, 62, 63, 65, 67, 68,
    74, 76, 77, 78, 79, 80, 82, 83, 84, 85, 86, 88, 89, 90, 91, 92, 93,
    94, 95, 97, 98, 99, 100, 101, 102, 103, 105, 106, 107, 108, 109, 110,
    111, 112,
    117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130,
    131, 132, 133, 134, 135, 136, 137, 138, 139,
    142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155,
    156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169,
    170,
    171, 172, 173, 174, 175, 176, 178, 179, 180, 181, 182, 183, 184, 185,
    186, 187, 188, 189,
    190, 191, 192, 193, 194, 195, 196, 197, 198,
    200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210,
    211, 212, 213, 214, 215, 216, 217, 218,
    220, 221, 222, 223, 224, 225,
    227, 228, 229, 230,
    231, 232, 233, 234,
    235, 236,
    237,
    238, 239,
    240,
}};

}  // namespace

std::vector<KanjiBushuChoice> kanji_bushu_choices(
    std::uint8_t radical_strokes, bool include_variants) {
  if (radical_strokes > kMaximumBushuRadicalStrokes)
    throw std::invalid_argument("Bushu radical stroke count is out of range");
  const BushuGroup group = kGroups[radical_strokes];
  const std::size_t count = include_variants ? group.variant_count
                                             : group.reduced_count;
  std::vector<KanjiBushuChoice> choices;
  choices.reserve(count);
  for (std::size_t offset = 0; offset < count; ++offset) {
    const std::uint16_t sprite = static_cast<std::uint16_t>(
        include_variants
            ? static_cast<std::size_t>(group.variant_start) + offset
            : kReducedSprites[static_cast<std::size_t>(group.reduced_start) +
                              offset]);
    choices.push_back({sprite, kBushuBySprite[sprite + 1]});
  }
  return choices;
}

}  // namespace jwpqt::core
