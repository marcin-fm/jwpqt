// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/kanji_color.h"

namespace jwpqt::core {

std::optional<std::size_t> kanji_color_list_index(JisCode code) noexcept {
  if (code < kKanjiColorListBase) {
    return std::nullopt;
  }

  const std::uint16_t offset =
      static_cast<std::uint16_t>(code - kKanjiColorListBase);
  const std::size_t index =
      static_cast<std::size_t>(offset >> 8U) * 94U +
      static_cast<std::size_t>(offset & 0xffU);
  if (index > kKanjiColorListLastIndex) {
    return std::nullopt;
  }
  return index;
}

RgbColor decode_legacy_color_ref(std::uint32_t color_ref,
                                 RgbColor fallback) noexcept {
  if ((color_ref & 0xff000000U) != 0U) {
    return fallback;
  }
  return {static_cast<std::uint8_t>(color_ref & 0xffU),
          static_cast<std::uint8_t>((color_ref >> 8U) & 0xffU),
          static_cast<std::uint8_t>((color_ref >> 16U) & 0xffU)};
}

std::optional<RgbColor> kanji_foreground_color(
    JisCode code, bool list_member, const KanjiColorPolicy& policy) noexcept {
  constexpr JisCode kKanjiStart = 0x3000;
  constexpr JisCode kUncommonKanjiStart = 0x5000;
  if (code < kKanjiStart) {
    return std::nullopt;
  }

  const bool member =
      list_member && kanji_color_list_index(code).has_value();
  if (policy.list_mode != KanjiListColorMode::kOff) {
    const bool color_with_list =
        policy.list_mode == KanjiListColorMode::kMatch ? member : !member;
    if (color_with_list) {
      return policy.list_color;
    }
  }
  if (policy.colorize_uncommon && code >= kUncommonKanjiStart) {
    return policy.uncommon_color;
  }
  return std::nullopt;
}

}  // namespace jwpqt::core
