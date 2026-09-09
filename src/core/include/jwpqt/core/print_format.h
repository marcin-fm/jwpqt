// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>
#include "jwpqt/core/jwp_document.h"

namespace jwpqt::core {

struct JwpPrintFormatting {
  std::array<JwpText, 4> patterns{{{'&','y','/','&','M','/','&','D'},
      {'&','h',':','&','N',' ','&','A'}, {'A','M'}, {'P','M'}}};
  std::array<int, 4> position{{0, 0, 100, 100}};
  JwpPrintFormatting();
};

void validate_print_formatting(const JwpPrintFormatting& formatting);
JwpText expand_print_pattern(const JwpPrintFormatting& formatting, bool time_pattern,
                             int year, int month, int day, int hour, int minute);

}  // namespace jwpqt::core
