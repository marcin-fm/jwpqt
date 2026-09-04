// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/jis_unicode.h"

#include <array>
#include <cstddef>

// These mappings and their ordering are retained from Glenn Rosenthal's
// JWPxp jwp_jisc.cpp, jwp_ukan.dat, and jwp_umis.dat.

namespace jwpqt::core {
namespace {

constexpr std::size_t kCellsPerRow = 94;

constexpr std::array<char32_t, 6398> kKanjiUnicode = {{
#include "jwp_ukan.dat"
}};

constexpr std::array<char32_t, 220> kMiscUnicode = {{
#include "jwp_umis.dat"
}};

std::optional<char32_t> mapped_value(char32_t value) noexcept {
  if (value == 0) {
    return std::nullopt;
  }
  return value;
}

std::optional<char32_t> map_greek(JisCode code) noexcept {
  if (code >= 0x2621 && code <= 0x2631) {
    return static_cast<char32_t>(code - 0x2621 + 0x0391);
  }
  if (code >= 0x2632 && code <= 0x2638) {
    return static_cast<char32_t>(code - 0x2621 + 0x0392);
  }
  if (code >= 0x2641 && code <= 0x2651) {
    return static_cast<char32_t>(code - 0x2621 + 0x0391);
  }
  if (code >= 0x2652 && code <= 0x2658) {
    return static_cast<char32_t>(code - 0x2621 + 0x0392);
  }
  return std::nullopt;
}

std::optional<char32_t> map_cyrillic(JisCode code) noexcept {
  if (code >= 0x2721 && code <= 0x2726) {
    return static_cast<char32_t>(code - 0x2721 + 0x0410);
  }
  if (code == 0x2727) {
    return U'\u0401';
  }
  if (code >= 0x2728 && code <= 0x2741) {
    return static_cast<char32_t>(code - 0x2722 + 0x0410);
  }
  if (code >= 0x2751 && code <= 0x2756) {
    return static_cast<char32_t>(code - 0x2751 + 0x0430);
  }
  if (code == 0x2757) {
    return U'\u0451';
  }
  if (code >= 0x2758 && code <= 0x2771) {
    return static_cast<char32_t>(code - 0x2752 + 0x0430);
  }
  return std::nullopt;
}

std::optional<JisCode> find_kanji(char32_t code_point) noexcept {
  for (std::size_t index = 0; index < kKanjiUnicode.size(); ++index) {
    if (kKanjiUnicode[index] == code_point) {
      const std::size_t row = index / kCellsPerRow + 0x30U;
      const std::size_t cell = index % kCellsPerRow + 0x21U;
      return static_cast<JisCode>((row << 8U) | cell);
    }
  }
  return std::nullopt;
}

std::optional<JisCode> find_misc(char32_t code_point) noexcept {
  for (std::size_t index = 0; index < kMiscUnicode.size(); ++index) {
    if (kMiscUnicode[index] != code_point) {
      continue;
    }

    const std::size_t group = index / kCellsPerRow;
    const std::size_t cell = index % kCellsPerRow + 0x21U;
    if (group == 0) {
      return static_cast<JisCode>(0x2100U | cell);
    }
    if (group == 1) {
      return static_cast<JisCode>(0x2200U | cell);
    }
    return static_cast<JisCode>(0x2800U | cell);
  }
  return std::nullopt;
}

}  // namespace

std::optional<char32_t> jis_x0208_to_unicode(JisCode code) noexcept {
  if (!is_jis_x0208_pair(code)) {
    return std::nullopt;
  }

  if (code >= 0x2330 && code <= 0x237a) {
    return static_cast<char32_t>(code - 0x2330 + 0xff10);
  }
  if (code >= 0x2421 && code <= 0x2473) {
    return static_cast<char32_t>(code - 0x2421 + 0x3041);
  }
  if (code >= 0x2521 && code <= 0x2576) {
    return static_cast<char32_t>(code - 0x2521 + 0x30a1);
  }
  if (code >= 0x2621 && code <= 0x2658) {
    return map_greek(code);
  }
  if (code >= 0x2721 && code <= 0x2771) {
    return map_cyrillic(code);
  }
  if (code >= 0x3021 && code <= 0x7426) {
    const std::size_t row = static_cast<std::size_t>(code >> 8U);
    const std::size_t cell = static_cast<std::size_t>(code & 0xffU);
    const std::size_t index =
        (row - 0x30U) * kCellsPerRow + (cell - 0x21U);
    if (index < kKanjiUnicode.size()) {
      return mapped_value(kKanjiUnicode[index]);
    }
    return std::nullopt;
  }
  if (code >= 0x2121 && code <= 0x217e) {
    return mapped_value(kMiscUnicode[code - 0x2121]);
  }
  if (code >= 0x2221 && code <= 0x227e) {
    return mapped_value(kMiscUnicode[code - 0x2221 + kCellsPerRow]);
  }
  if (code >= 0x2821 && code <= 0x2840) {
    return mapped_value(kMiscUnicode[code - 0x2821 + 2 * kCellsPerRow]);
  }
  return std::nullopt;
}

std::optional<JisCode> unicode_to_jis_x0208(char32_t code_point) noexcept {
  if (code_point == 0) {
    return std::nullopt;
  }

  if (code_point >= U'\u3041' && code_point <= U'\u3093') {
    return static_cast<JisCode>(code_point - U'\u3041' + 0x2421);
  }
  if (code_point >= U'\u30a1' && code_point <= U'\u30f6') {
    return static_cast<JisCode>(code_point - U'\u30a1' + 0x2521);
  }

  if (code_point >= U'\u0391' && code_point <= U'\u03c9') {
    if (code_point <= U'\u03a1') {
      return static_cast<JisCode>(code_point - U'\u0391' + 0x2621);
    }
    if (code_point >= U'\u03a3' && code_point <= U'\u03a9') {
      return static_cast<JisCode>(code_point - U'\u0392' + 0x2621);
    }
    if (code_point >= U'\u03b1' && code_point <= U'\u03c1') {
      return static_cast<JisCode>(code_point - U'\u0391' + 0x2621);
    }
    if (code_point >= U'\u03c3') {
      return static_cast<JisCode>(code_point - U'\u0392' + 0x2621);
    }
  }

  if (code_point == U'\u0401') {
    return 0x2727;
  }
  if (code_point == U'\u0451') {
    return 0x2757;
  }
  if (code_point >= U'\u0410' && code_point <= U'\u044f') {
    if (code_point <= U'\u0415') {
      return static_cast<JisCode>(code_point - U'\u0410' + 0x2721);
    }
    if (code_point <= U'\u042f') {
      return static_cast<JisCode>(code_point - U'\u0416' + 0x2728);
    }
    if (code_point <= U'\u0435') {
      return static_cast<JisCode>(code_point - U'\u0430' + 0x2751);
    }
    return static_cast<JisCode>(code_point - U'\u0436' + 0x2758);
  }

  if (const std::optional<JisCode> kanji = find_kanji(code_point)) {
    return kanji;
  }
  if (const std::optional<JisCode> misc = find_misc(code_point)) {
    return misc;
  }
  if (code_point >= U'\uff10' && code_point <= U'\uff5a') {
    return static_cast<JisCode>(code_point - U'\uff10' + 0x2330);
  }
  return std::nullopt;
}

}  // namespace jwpqt::core
