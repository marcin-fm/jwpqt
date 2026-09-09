// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_CORE_LINE_RELAXATION_H
#define JWPQT_CORE_LINE_RELAXATION_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include "jwpqt/core/jwp_document.h"

namespace jwpqt::core {

struct LineRelaxationOptions {
  std::int64_t first_line_width = 1;
  std::int64_t continuation_line_width = 1;
  std::int64_t jis_advance = 1;
  bool punctuation = true;
  bool small_kana = true;
  std::size_t max_characters = 33'554'432;
};

bool is_relaxable_margin_character(JisCode code,
                                   const LineRelaxationOptions& options) noexcept;

// Returns raw-token indices whose normal advance may hang beyond the right
// margin. Advances use caller-defined fixed-point units; tab entries are
// ignored and advance to the next JIS cell.
std::vector<std::size_t> plan_line_relaxation(
    const JwpText& text, const std::vector<std::int64_t>& advances,
    const LineRelaxationOptions& options);

}  // namespace jwpqt::core

#endif
