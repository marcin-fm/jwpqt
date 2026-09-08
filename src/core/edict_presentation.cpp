// SPDX-License-Identifier: GPL-2.0-or-later
#include "jwpqt/core/edict_presentation.h"

#include <list>

namespace jwpqt::core {

std::vector<EdictPresentationItem> prepare_edict_presentation(
    const std::vector<bool>& priority,
    const std::vector<EdictSearchSection>& sections,
    const EdictPresentationOptions& options) {
  if (priority.size() > 100000 || sections.size() > 4096)
    throw EdictSearchError("EDICT presentation exceeds its size limit");
  const std::vector<EdictSearchSection> fallback{{EdictSearchStage::kDirect, 0, true}};
  const auto& phases = sections.empty() ? fallback : sections;
  std::size_t previous = 0;
  int previous_rank = -1;
  for (std::size_t i = 0; i < phases.size(); ++i) {
    const auto& phase = phases[i];
    int rank;
    switch (phase.stage) {
      case EdictSearchStage::kDirect:
      case EdictSearchStage::kPattern: rank = 0; break;
      case EdictSearchStage::kContingent: rank = 1; break;
      case EdictSearchStage::kAdaptive: rank = 2; break;
      default: throw EdictSearchError("Invalid EDICT presentation stage");
    }
    if (phase.begin > priority.size() || phase.begin < previous || (i == 0 && phase.begin != 0) ||
        (phase.new_pass && rank != 0) ||
        (i && !phase.new_pass && rank <= previous_rank) ||
        (phase.quiet_empty && phase.stage != EdictSearchStage::kContingent))
      throw EdictSearchError("Invalid EDICT presentation boundaries");
    previous = phase.begin;
    previous_rank = rank;
  }

  // A stable insertion boundary reproduces move_primary in linear work. In
  // unmarked advanced searches it stays behind the earlier priority marker.
  std::list<EdictPresentationItem> display;
  auto boundary = display.end();
  std::size_t primary = 0, ordinary = 0;
  for (std::size_t s = 0; s < phases.size(); ++s) {
    const auto& phase = phases[s];
    const auto end = s + 1 < phases.size() ? phases[s + 1].begin : priority.size();
    const bool contingent = phase.stage == EdictSearchStage::kContingent;
    const bool marked_advanced = phase.stage == EdictSearchStage::kAdaptive && options.advanced_separator;
    if ((contingent && !(phase.quiet_empty && phase.begin == end)) || marked_advanced)
      display.push_back({contingent ? EdictPresentationKind::kContingent : EdictPresentationKind::kAdaptive, 0});
    if (s == 0 || phase.new_pass || contingent || marked_advanced) {
      boundary = display.end();
      primary = ordinary = 0;
    }
    for (std::size_t i = phase.begin; i < end; ++i) {
      if (options.priority_first && priority[i]) {
        display.insert(boundary, {EdictPresentationKind::kEntry, i});
        ++primary;
      } else {
        const auto inserted = display.insert(display.end(), {EdictPresentationKind::kEntry, i});
        if (ordinary++ == 0) boundary = inserted;
      }
    }
    if (options.priority_first && options.priority_separator && primary && ordinary &&
        (phase.stage != EdictSearchStage::kAdaptive || options.advanced_separator))
      display.insert(boundary, {EdictPresentationKind::kPriorityEnd, 0});
  }
  return {display.begin(), display.end()};
}

}  // namespace jwpqt::core
