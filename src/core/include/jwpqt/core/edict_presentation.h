// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef JWPQT_CORE_EDICT_PRESENTATION_H
#define JWPQT_CORE_EDICT_PRESENTATION_H

#include "jwpqt/core/edict_engine.h"

namespace jwpqt::core {

struct EdictPresentationOptions {
  bool priority_first = true;
  bool priority_separator = true;
  bool advanced_separator = true;
};

enum class EdictPresentationKind { kEntry, kPriorityEnd, kContingent, kAdaptive };

struct EdictPresentationItem {
  EdictPresentationKind kind = EdictPresentationKind::kEntry;
  std::size_t index = 0;
};

// Empty sections describe a single direct pass. Entry indices always refer to
// the original report; labels never acquire a record or insertion identity.
std::vector<EdictPresentationItem> prepare_edict_presentation(
    const std::vector<bool>& priority,
    const std::vector<EdictSearchSection>& sections,
    const EdictPresentationOptions& options = {});

}  // namespace jwpqt::core
#endif
