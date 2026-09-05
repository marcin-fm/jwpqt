// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <optional>

#include "jwpqt/core/edict_pattern.h"
#include "jwpqt/core/legacy_code_page.h"

namespace jwpqt::core {

struct EdictPatternMatchOptions {
  std::size_t work_steps = 64U * 1024U * 1024U;
  LegacyCodePage utf8_code_page = kDefaultLegacyCodePage;
};

struct EdictPatternMatch {
  std::size_t byte_offset = 0;
  std::size_t byte_length = 0;
  std::size_t record_index = 0;

  bool operator==(const EdictPatternMatch& other) const noexcept;
};

struct EdictPatternMatchReport {
  std::optional<EdictPatternMatch> match;
  std::size_t work_steps = 0;
};

EdictPatternMatchReport match_edict_pattern_report(
    const EdictDictionary& dictionary, const EdictIndexMatch& anchor,
    const EdictSearchPlan& plan,
    const EdictPatternMatchOptions& options = EdictPatternMatchOptions{});

std::optional<EdictPatternMatch> match_edict_pattern(
    const EdictDictionary& dictionary, const EdictIndexMatch& anchor,
    const EdictSearchPlan& plan,
    const EdictPatternMatchOptions& options = EdictPatternMatchOptions{});

}  // namespace jwpqt::core
