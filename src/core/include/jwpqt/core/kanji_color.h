// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "jwpqt/core/jis_encoding.h"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace jwpqt::core {

enum class KanjiListColorMode {
  kOff,
  kMatch,
  kNoMatch,
};

struct RgbColor {
  std::uint8_t red;
  std::uint8_t green;
  std::uint8_t blue;
};

constexpr bool operator==(RgbColor left, RgbColor right) noexcept {
  return left.red == right.red && left.green == right.green &&
         left.blue == right.blue;
}

constexpr bool operator!=(RgbColor left, RgbColor right) noexcept {
  return !(left == right);
}

struct KanjiColorPolicy {
  KanjiListColorMode list_mode = KanjiListColorMode::kOff;
  RgbColor list_color{0, 0, 255};
  bool colorize_uncommon = false;
  RgbColor uncommon_color{88, 88, 88};
};

constexpr JisCode kKanjiColorListBase = 0x3021;
constexpr std::size_t kKanjiColorListLastIndex = 6450;

// Mirrors the historical list's arithmetic, including its treatment of raw
// non-canonical row/cell values. Equal returned indices identify the same bit.
std::optional<std::size_t> kanji_color_list_index(JisCode code) noexcept;

// Windows COLORREF stores red in the low byte and uses a nonzero high byte for
// special non-RGB values. The legacy uncommon color falls back in that case.
RgbColor decode_legacy_color_ref(std::uint32_t color_ref,
                                 RgbColor fallback) noexcept;

// Returns only an explicit foreground override. Selection and background
// rendering remain responsibilities of the native presentation layer.
std::optional<RgbColor> kanji_foreground_color(
    JisCode code, bool list_member,
    const KanjiColorPolicy& policy = {}) noexcept;

}  // namespace jwpqt::core
