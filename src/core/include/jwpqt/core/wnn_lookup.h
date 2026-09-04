// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <stdexcept>
#include <vector>

#include "jwpqt/core/wnn_dictionary.h"

namespace jwpqt::core {

constexpr std::size_t kWnnDefaultMaximumLookupCells = 16'384;

class WnnLookupError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

struct WnnCandidate {
  JwpText text;
  std::size_t legacy_cell_offset = 0;
  bool original_kana = false;

  bool operator==(const WnnCandidate& other) const noexcept;
};

struct WnnLookupResult {
  std::vector<WnnCandidate> candidates;
  bool can_extend = false;
};

// Reproduces the recovered exact, ichidan/i-adjective, and conjugated WNN
// lookup passes. user_records may be unsorted and are scanned in their given
// order after the system records during each pass.
WnnLookupResult lookup_wnn_candidates(
    const WnnDictionary& system_dictionary, const JwpText& input,
    const std::vector<WnnRecord>* user_records = nullptr,
    std::size_t maximum_output_cells = kWnnDefaultMaximumLookupCells);

}  // namespace jwpqt::core
