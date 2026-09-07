// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "jwpqt/core/legacy_code_page.h"
#include "jwpqt/core/query_history.h"

namespace jwpqt::core {

inline constexpr std::uint32_t kQueryHistoryMagic = 0x3148514a;  // JQH1
inline constexpr std::uint32_t kLegacyQueryHistoryMagic = 0x1510be00;
inline constexpr std::size_t kMaximumQueryHistoryFileBytes = 1024 * 1024;

struct QueryHistories {
  explicit QueryHistories(std::size_t storage_cells = 300)
      : dictionary(storage_cells), search(storage_cells), replace(storage_cells) {}

  QueryHistory dictionary;
  QueryHistory search;
  QueryHistory replace;
};

// Each list has its own scalar/count budget; all three share one stored capacity.
QueryHistories parse_query_history_file(std::string_view bytes);
std::string encode_query_history_file(const QueryHistories& histories);

// The source capacity/page are not present in JWPxp.his. Only the three history
// blocks are imported; any trailing recent/workspace paths remain uninterpreted.
QueryHistories parse_legacy_query_history_file(
    std::string_view bytes, std::size_t storage_cells,
    LegacyCodePage code_page);

}  // namespace jwpqt::core
