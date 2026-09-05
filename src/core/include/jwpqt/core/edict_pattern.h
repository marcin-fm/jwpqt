// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdexcept>

#include "jwpqt/core/edict_search.h"

namespace jwpqt::core {

class EdictPatternError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

enum class EdictSearchPlanKind {
  kDirect,
  kPattern,
};

enum class EdictContingentMode {
  kOpen,
  kLimited,
};

struct EdictPatternOptions {
  bool jascii_to_ascii = false;
};

struct EdictSearchPlan {
  EdictQuery anchor;
  JwpText prefix;
  JwpText postfix;
  EdictSearchPlanKind kind = EdictSearchPlanKind::kDirect;
  bool force_open_end = false;
  bool force_closed_boundaries = false;
  bool ascii_boundaries = false;
  bool adaptive_disabled = false;
  bool input_truncated = false;
};

EdictSearchPlan prepare_edict_search_plan(
    const JwpText& input,
    const EdictPatternOptions& options = EdictPatternOptions{});
EdictSearchPlan prepare_edict_contingent_plan(
    const EdictQuery& query, EdictContingentMode mode);

}  // namespace jwpqt::core
