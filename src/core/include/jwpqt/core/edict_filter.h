// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

#include "jwpqt/core/edict_dictionary.h"

namespace jwpqt::core {

// Ordered exactly as the legacy dict_keys entries at mask bits 4..24.
inline constexpr std::array<std::u32string_view, 21> kEdictCategoryTags = {
    U"vulg", U"X", U"col", U"m-sl", U"sl", U"MA", U"id",
    U"arch", U"obs", U"obsc", U"ok", U"abbr", U"fam", U"pol",
    U"hum", U"hon", U"fem", U"male", U"pref", U"suf", U"oK"};
inline constexpr std::uint32_t kEdictCategoryMask = 0x01fffff0U;

struct EdictNameFilterOptions {
  bool reject_personal_names = false;
  bool reject_place_names = false;
  std::uint32_t category_exclusions = 0;
};

std::optional<EdictRecord> filter_edict_name_types(
    const EdictRecord& record,
    const EdictNameFilterOptions& options = EdictNameFilterOptions{});

}  // namespace jwpqt::core
