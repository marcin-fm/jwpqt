// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "jwpqt/core/jwp_search.h"
#include <string_view>
#include <utility>
#include <vector>

namespace jwpqt::core {
// Scalar offsets in document order. A shared work budget spans workspace files.
std::vector<std::pair<std::size_t, std::size_t>> find_unicode_text(
    std::u32string_view source, std::u32string_view pattern,
    JwpSearchOptions options = {}, std::size_t maximum_matches = 100000,
    std::size_t* remaining_work = nullptr, bool overlapping = false);
}
