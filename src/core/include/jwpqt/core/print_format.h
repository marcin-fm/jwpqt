// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>
#include <vector>
#include "jwpqt/core/jwp_document.h"

namespace jwpqt::core {

struct JwpPrintFormatting {
  std::array<JwpText, 4> patterns{{{'&','y','/','&','M','/','&','D'},
      {'&','h',':','&','N',' ','&','A'}, {'A','M'}, {'P','M'}}};
  std::array<int, 4> position{{0, 0, 100, 100}};
  bool justify_ascii = true;
  JwpPrintFormatting();
};

void validate_print_formatting(const JwpPrintFormatting& formatting);
// Positions and final advance in caller-supplied fixed-point units.
std::vector<int> print_grid_positions(const JwpText& text, const std::vector<int>& advances,
                                    int cell, bool justify, bool paragraph_end);
JwpText expand_print_pattern(const JwpPrintFormatting& formatting, bool time_pattern,
                             int year, int month, int day, int hour, int minute);

}  // namespace jwpqt::core
