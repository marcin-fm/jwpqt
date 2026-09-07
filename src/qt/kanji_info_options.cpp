// SPDX-License-Identifier: GPL-2.0-or-later

#include "kanji_info_options.h"

#include <algorithm>

#include "jwpqt/core/jwp_configuration.h"

namespace jwpqt::qt {

void validate_kanji_info_options(const KanjiInfoOptions& options) {
  for (std::size_t i = 0; i < kKanjiInfoFieldCount; ++i)
    if (options.fields[i] > kKanjiInfoFieldCount)
      throw core::JwpConfigurationError("Unknown character information field");
}

void select_kanji_info_field(KanjiInfoOptions& options, std::size_t slot,
                             std::uint8_t field) {
  validate_kanji_info_options(options);
  if (slot >= kKanjiInfoFieldCount || field > kKanjiInfoFieldCount)
    throw core::JwpConfigurationError("Invalid character information field selection");
  options.fields[slot] = field;
  if (field == 0) return;
  const auto end = options.fields.begin() + kKanjiInfoFieldCount;
  for (std::uint8_t missing = 1; missing <= kKanjiInfoFieldCount; ++missing) {
    if (std::find(options.fields.begin(), end, missing) != end) continue;
    for (std::size_t i = 0; i < kKanjiInfoFieldCount; ++i) {
      if (i != slot && options.fields[i] == field) {
        options.fields[i] = missing;
        return;
      }
    }
    return;
  }
}

}  // namespace jwpqt::qt
