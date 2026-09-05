// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>

#include "jwpqt/core/edict_dictionary.h"

namespace jwpqt::core {

struct EdictNameFilterOptions {
  bool reject_personal_names = false;
  bool reject_place_names = false;
};

std::optional<EdictRecord> filter_edict_name_types(
    const EdictRecord& record,
    const EdictNameFilterOptions& options = EdictNameFilterOptions{});

}  // namespace jwpqt::core
