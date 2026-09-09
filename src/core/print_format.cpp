// SPDX-License-Identifier: GPL-2.0-or-later
#include "jwpqt/core/print_format.h"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace jwpqt::core {

JwpPrintFormatting::JwpPrintFormatting() {
  for (std::size_t i = 0; i < patterns.size(); ++i) patterns[i].resize(i < 2 ? 20 : 10);
}

void validate_print_formatting(const JwpPrintFormatting& formatting) {
  for (int position : formatting.position)
    if (position < 0 || position > 1000) throw std::invalid_argument("Print header position is outside 0..1000");
  for (std::size_t i = 0; i < formatting.patterns.size(); ++i) {
    const auto& pattern = formatting.patterns[i];
    if (pattern.size() != (i < 2 ? 20U : 10U) || std::find(pattern.begin(), pattern.end(), 0) == pattern.end())
      throw std::invalid_argument("Print pattern has invalid size or no terminator");
    for (auto code : pattern) {
      if (!code) break; // Unused legacy array cells are retained, not interpreted.
      if ((code < 32 && code != '\t') || (code >= 256 && !is_jis_x0208_pair(code)))
        throw std::invalid_argument("Print pattern contains an invalid JWP character");
    }
  }
}

JwpText expand_print_pattern(const JwpPrintFormatting& formatting, bool time_pattern,
                             int year, int month, int day, int hour, int minute) {
  validate_print_formatting(formatting);
  const int days[] = {31, 28 + (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)),
      31,30,31,30,31,31,30,31,30,31};
  if (year < 1 || year > 9999 || month < 1 || month > 12 || day < 1 ||
      day > days[month - 1] || hour < 0 || hour > 23 || minute < 0 || minute > 59)
    throw std::invalid_argument("Invalid print date or time");
  JwpText result;
  const auto number = [&](int value, bool pad = false) {
    const auto text = std::to_string(value);
    if (pad && value < 10) result.push_back('0');
    result.insert(result.end(), text.begin(), text.end());
  };
  const auto& pattern = formatting.patterns[time_pattern ? 1 : 0];
  for (std::size_t i = 0; i < pattern.size() && pattern[i]; ++i) {
    if (pattern[i] != '&' || i + 1 == pattern.size() || !pattern[i + 1]) {
      result.push_back(pattern[i]); continue;
    }
    const auto code = pattern[++i];
    switch (code) {
      case 'a': case 'A': {
        // Preserve the source's noon-AM/midnight-zero convention.
        const auto& suffix = formatting.patterns[hour > 12 ? 3 : 2];
        result.insert(result.end(), suffix.begin(), std::find(suffix.begin(), suffix.end(), 0));
        break;
      }
      case 'd': case 'D': number(day); break;
      case 'H': number(hour); break;
      case 'h': number(hour > 12 ? hour - 12 : hour); break;
      case 'm': case 'M': number(month); break;
      case 'n': case 'N': number(minute, true); break;
      case 'Y': number(year); break;
      case 'y': number(year % 100, true); break;
      case '&': result.push_back('&'); break;
      default: result.push_back('&'); result.push_back(code);
    }
  }
  return result;
}

}  // namespace jwpqt::core
