// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/line_relaxation.h"

#include <limits>
#include <stdexcept>

namespace jwpqt::core {
namespace {

bool is_space(JisCode code) noexcept {
  return code == static_cast<JisCode>(' ') ||
         code == static_cast<JisCode>('\t') || code == 0x2121;
}

std::int64_t checked_add(std::int64_t left, std::int64_t right) {
  if (right > std::numeric_limits<std::int64_t>::max() - left) {
    throw std::length_error("Line-relaxation position overflow");
  }
  return left + right;
}

}  // namespace

bool is_relaxable_margin_character(
    JisCode code, const LineRelaxationOptions& options) noexcept {
  const std::uint8_t row = static_cast<std::uint8_t>(code >> 8U);
  const std::uint8_t cell = static_cast<std::uint8_t>(code & 0xffU);
  if (options.small_kana && (row == 0x24 || row == 0x25)) {
    switch (cell) {
      case 0x21:
      case 0x23:
      case 0x25:
      case 0x27:
      case 0x29:
      case 0x43:
      case 0x63:
      case 0x65:
      case 0x67:
        return true;
      default:
        break;
    }
  }
  if (options.punctuation && row == 0x21) {
    switch (cell) {
      case 0x22:
      case 0x23:
      case 0x4b:
      case 0x57:
      case 0x59:
        return true;
      default:
        break;
    }
  }
  return false;
}

std::vector<std::size_t> plan_line_relaxation(
    const JwpText& text, const std::vector<std::int64_t>& advances,
    const LineRelaxationOptions& options) {
  if (text.size() != advances.size()) {
    throw std::invalid_argument("Line-relaxation advance count mismatch");
  }
  if (text.size() > options.max_characters) {
    throw std::length_error("Line-relaxation character limit exceeded");
  }
  if (options.first_line_width <= 0 ||
      options.continuation_line_width <= 0 || options.jis_advance <= 0) {
    throw std::invalid_argument("Invalid line-relaxation metrics");
  }
  for (std::size_t index = 0; index < advances.size(); ++index) {
    if (text[index] != static_cast<JisCode>('\t') && advances[index] <= 0) {
      throw std::invalid_argument("Invalid line-relaxation character advance");
    }
  }

  std::vector<std::size_t> result;
  std::int64_t x = 0;
  std::int64_t split_x = 0;
  std::int64_t width = options.first_line_width;
  bool in_ascii_word = false;
  bool relaxation_available = true;
  for (std::size_t index = 0; index < text.size(); ++index) {
    const JisCode code = text[index];
    std::int64_t advance = advances[index];
    if (code == static_cast<JisCode>('\t')) {
      advance = options.jis_advance - (x % options.jis_advance);
    }
    std::int64_t next_x = checked_add(x, advance);
    bool relaxed = false;
    if (next_x > width && !is_space(code) && relaxation_available &&
        is_relaxable_margin_character(code, options)) {
      result.push_back(index);
      relaxation_available = false;
      relaxed = true;
    }
    if (next_x > width && !is_space(code) && !relaxed && split_x != 0) {
      relaxation_available = true;
      width = options.continuation_line_width;
      next_x -= split_x;
    }

    if (code > 0xff || is_space(code)) {
      in_ascii_word = false;
      split_x = next_x;
    } else if (!in_ascii_word) {
      in_ascii_word = true;
      split_x = x;
    }
    x = next_x;
  }
  return result;
}

}  // namespace jwpqt::core
