// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <functional>
#include <stdexcept>
#include <vector>

#include "jwpqt/core/edict_dictionary.h"

namespace jwpqt::core {

enum class EdictSortMode { kReading, kLength, kEntry, kDefinition };

struct EdictSortOptions {
  EdictSortMode mode = EdictSortMode::kReading;
  bool reverse = false;
  bool headword_length = false;
  LegacyCodePage code_page = kDefaultLegacyCodePage;
};

struct EdictSortLimits {
  std::size_t records = 100'000;
  std::size_t text_cells = 32U * 1024U * 1024U;
  std::size_t comparisons = 10'000'000;
  std::size_t work = 100'000'000;
};

class EdictSortError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

// Returns original indices, retaining the first identical record's provenance.
// Source comparators are not strict weak orders; sorting uses bounded selection.
std::vector<std::size_t> sort_edict_records(
    const std::vector<std::reference_wrapper<const EdictRecord>>& records,
    const EdictSortOptions& options = {}, const EdictSortLimits& limits = {});

}  // namespace jwpqt::core
