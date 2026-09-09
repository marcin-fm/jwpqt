// SPDX-License-Identifier: GPL-2.0-or-later
#include "jwpqt/core/print_format.h"
#include <cmath>
#include <cstring>
#include <limits>

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace jwpqt::core {

std::vector<std::uint8_t> encode_page_defaults(const JwpPageDefaults& page) {
  static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
  std::vector<std::uint8_t> bytes(20);
  for (std::size_t i = 0; i < 4; ++i) {
    const float value = page.margins[i];
    if (!std::isfinite(value) || value < 0 || value > 10)
      throw std::invalid_argument("Default page margin is outside 0..10 inches");
    std::uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    for (std::size_t j = 0; j < 4; ++j) bytes[i * 4 + j] = static_cast<std::uint8_t>(bits >> (8 * j));
  }
  bytes[16] = page.vertical; bytes[17] = page.landscape;
  bytes[18] = page.padding[0]; bytes[19] = page.padding[1];
  return bytes;
}

JwpPageDefaults decode_page_defaults(const std::vector<std::uint8_t>& bytes) {
  if (bytes.size() != 20 || bytes[16] > 1 || bytes[17] > 1)
    throw std::invalid_argument("Invalid default page layout record");
  JwpPageDefaults page;
  for (std::size_t i = 0; i < 4; ++i) {
    std::uint32_t bits = 0;
    for (std::size_t j = 0; j < 4; ++j) bits |= static_cast<std::uint32_t>(bytes[i * 4 + j]) << (8 * j);
    std::memcpy(&page.margins[i], &bits, sizeof(bits));
  }
  page.vertical = bytes[16] != 0; page.landscape = bytes[17] != 0;
  page.padding = {bytes[18], bytes[19]};
  (void)encode_page_defaults(page);
  return page;
}

void JwpPageDefaults::apply(JwpDocument& document) const {
  (void)encode_page_defaults(*this);
  document.margins = margins;
  document.vertical = vertical;
  document.landscape = landscape;
}

std::vector<int> print_grid_positions(const JwpText& text, const std::vector<int>& advances,
                                    int cell, bool justify, bool paragraph_end) {
  if (text.size() > 65535 || advances.size() != text.size() || cell < 1 || cell > 65536)
    throw std::invalid_argument("Invalid print grid dimensions");
  for (std::size_t i = 0; i < text.size(); ++i)
    if (advances[i] < 0 || advances[i] > 65536 ||
        (text[i] >= 256 && !is_jis_x0208_pair(text[i])))
      throw std::invalid_argument("Invalid print grid character or advance");
  std::vector<int> positions(text.size() + 1);
  std::int64_t x = 0;
  const auto checked = [](std::int64_t value) {
    if (value > 0x3fffffff) throw std::invalid_argument("Print grid advance exceeds its limit");
    return static_cast<int>(value);
  };
  for (std::size_t i = 0; i < text.size();) {
    positions[i] = checked(x);
    if (text[i] >= 256) { x += cell; ++i; }
    else if (text[i] == '\t') { x = (x / cell + 1) * cell; ++i; }
    else {
      auto end = i;
      std::int64_t natural = x, spaces = 2;
      while (end < text.size() && text[end] < 256 && text[end] != '\t') {
        natural += advances[end];
        if (text[end] == ' ') ++spaces;
        ++end;
      }
      std::int64_t extra = (natural / cell + 1) * cell - natural;
      if (!justify || ((end != text.size() || paragraph_end) &&
                       (end == text.size() || text[end] != '\t'))) extra = 0;
      if (end == text.size() && spaces != 2) --spaces;
      const auto padding = [&](std::int64_t n) { return extra * (n + 1) / spaces - extra * n / spaces; };
      x += padding(0);
      std::int64_t n = 1;
      while (i < end) {
        positions[i] = checked(x);
        x += advances[i];
        if (text[i] == ' ') x += padding(n++);
        ++i;
      }
    }
    checked(x);
  }
  positions.back() = checked(x);
  return positions;
}

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
