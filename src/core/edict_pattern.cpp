// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/edict_pattern.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace jwpqt::core {
namespace {

constexpr std::size_t kMaximumQueryLength = 100;
constexpr std::uint16_t kJisSpace = 0x2121;
constexpr std::uint16_t kJisQuestion = 0x2129;
constexpr std::uint16_t kJisLeftBracket = 0x214e;
constexpr std::uint16_t kJisRightBracket = 0x214f;
constexpr std::uint16_t kJisNumberSign = 0x2174;
constexpr std::uint16_t kJisAsterisk = 0x2176;

bool is_space(std::uint16_t token) noexcept {
  return token == ' ' || token == '\t' || token == kJisSpace;
}

bool is_kana(std::uint16_t token) noexcept {
  const std::uint16_t row = static_cast<std::uint16_t>(token & 0x7f00U);
  return row == 0x2400U || row == 0x2500U;
}

bool is_kanji(std::uint16_t token) noexcept { return token >= 0x3000U; }

bool is_pattern(std::uint16_t token) noexcept {
  return token == '[' || token == ']' || token == '*' || token == '?' ||
         token == '#';
}

bool is_version_id(const JwpText& input) noexcept {
  return input.size() == 4 &&
         std::all_of(input.begin(), input.end(), [](std::uint16_t token) {
           return token == kJisQuestion;
         });
}

bool is_spaced_version_id(const JwpText& input) noexcept {
  return input.size() == 4 && input[0] == kJisSpace &&
         input[1] == kJisQuestion && input[2] == kJisQuestion &&
         input[3] == kJisQuestion;
}

std::uint16_t lowercase_ascii(std::uint16_t token) noexcept {
  if (token >= 'A' && token <= 'Z') {
    return static_cast<std::uint16_t>(token - 'A' + 'a');
  }
  return token;
}

}  // namespace

EdictSearchPlan prepare_edict_search_plan(
    const JwpText& input, const EdictPatternOptions& options) {
  const std::size_t bounded_size =
      std::min(input.size(), kMaximumQueryLength);
  JwpText bounded(input.begin(),
                  input.begin() + static_cast<std::ptrdiff_t>(bounded_size));
  EdictSearchPlan plan;
  plan.input_truncated = input.size() > bounded.size();

  const bool spaced_version_id = is_spaced_version_id(bounded);
  if (!spaced_version_id) {
    while (!bounded.empty() && is_space(bounded.front())) {
      bounded.erase(bounded.begin());
    }
    while (!bounded.empty() && is_space(bounded.back())) {
      bounded.pop_back();
    }
  }
  if (bounded.empty()) {
    throw EdictPatternError("EDICT search text is empty");
  }

  if (is_version_id(bounded) || is_spaced_version_id(bounded)) {
    plan.anchor = prepare_edict_query(bounded);
    return plan;
  }

  JwpText normalized;
  normalized.reserve(bounded.size());
  bool ascii_search = false;
  std::size_t pattern_count = 0;
  for (std::size_t index = 0; index < bounded.size(); ++index) {
    std::uint16_t token = bounded[index];
    if (token == kJisLeftBracket || token == '[') {
      token = '[';
      ++pattern_count;
    } else if (token == kJisRightBracket || token == ']') {
      token = ']';
      ++pattern_count;
      normalized.push_back(token);
      plan.input_truncated = plan.input_truncated || index + 1 < bounded.size();
      plan.adaptive_disabled = true;
      break;
    } else if (token == kJisAsterisk || token == '*') {
      token = '*';
      ++pattern_count;
    } else if (token == kJisQuestion || token == '?') {
      token = '?';
      ++pattern_count;
    } else if (token == kJisNumberSign || token == '#') {
      token = '#';
      ++pattern_count;
    } else if ((token & 0x7f00U) == 0) {
      token = lowercase_ascii(token);
      ascii_search = true;
    } else if (options.jascii_to_ascii &&
               (token & 0x7f00U) == 0x2300U) {
      if (token >= 0x2330U && token <= 0x2339U) {
        token = static_cast<std::uint16_t>('0' + (token - 0x2330U));
        ascii_search = true;
      } else if (token >= 0x2341U && token <= 0x235aU) {
        token = static_cast<std::uint16_t>('a' + (token - 0x2341U));
        ascii_search = true;
      } else if (token >= 0x2361U && token <= 0x237aU) {
        token = static_cast<std::uint16_t>('a' + (token - 0x2361U));
      } else {
        token = 0;
      }
      ascii_search = true;
    } else if ((token & 0x7f00U) == 0x2500U) {
      token = static_cast<std::uint16_t>(0x2400U | (token & 0x007fU));
    }
    normalized.push_back(token);
  }

  bool pattern = pattern_count != 0;
  if (ascii_search && pattern) {
    pattern = false;
  }
  plan.ascii_boundaries = ascii_search;
  plan.adaptive_disabled = plan.adaptive_disabled || ascii_search;

  if (ascii_search && normalized.size() >= 3 && normalized[0] == 't' &&
      normalized[1] == 'o' && normalized[2] == ' ') {
    plan.kind = EdictSearchPlanKind::kPattern;
    plan.prefix.assign(normalized.begin(), normalized.begin() + 3);
    plan.anchor =
        prepare_edict_query(JwpText(normalized.begin() + 3, normalized.end()));
    return plan;
  }

  if (pattern) {
    const auto first_kanji =
        std::find_if(normalized.begin(), normalized.end(), is_kanji);
    if (first_kanji == normalized.end()) {
      if (pattern_count == 1 && normalized.back() == '*') {
        if (normalized.size() == 1) {
          throw EdictPatternError("EDICT wildcard search has no anchor");
        }
        normalized.pop_back();
        plan.anchor = prepare_edict_query(normalized);
        plan.force_closed_boundaries =
            normalized.size() == 1 && is_kana(normalized.front());
        plan.force_open_end = !plan.force_closed_boundaries;
        plan.adaptive_disabled = true;
        return plan;
      }
      throw EdictPatternError(
          "Japanese EDICT wildcard search requires a kanji anchor");
    }

    const auto anchor_end =
        std::find_if(first_kanji + 1, normalized.end(), is_pattern);
    plan.kind = EdictSearchPlanKind::kPattern;
    plan.prefix.assign(normalized.begin(), first_kanji);
    plan.postfix.assign(anchor_end, normalized.end());
    plan.anchor = prepare_edict_query(JwpText(first_kanji, anchor_end));
    return plan;
  }

  plan.anchor = prepare_edict_query(normalized);
  plan.force_closed_boundaries =
      normalized.size() == 1 && is_kana(normalized.front());
  return plan;
}

}  // namespace jwpqt::core
