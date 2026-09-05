// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_QT_EDICT_RESOURCE_SEARCH_H
#define JWPQT_QT_EDICT_RESOURCE_SEARCH_H

#include <cstddef>
#include <vector>

#include <QString>

#include "edict_resources.h"
#include "jwpqt/core/edict_engine.h"
#include "jwpqt/core/edict_pattern.h"

namespace jwpqt::qt {

struct EdictResourceSearchOptions {
  core::EdictSearchOptions search;
  core::EdictPatternOptions pattern;
  EdictResourceLoadOptions reload;
  bool classical = false;
  bool personal_names = false;
  bool place_names = false;
};

struct EdictResourceSearchResult {
  std::size_t registry_index = 0;
  QString label;
  core::EdictSearchResult result;
};

struct EdictResourceSearchReport {
  std::vector<EdictResourceSearchResult> results;
  std::vector<EdictResourceFailure> failures;
  std::size_t rejected = 0;
  std::size_t candidate_matches = 0;
  std::size_t queries = 0;
  std::size_t lookup_steps = 0;
};

EdictResourceSearchReport search_edict_resources(
    const EdictResourceSet& resources, const QString& config_directory,
    const core::JwpText& input,
    const EdictResourceSearchOptions& options = EdictResourceSearchOptions{});

}  // namespace jwpqt::qt

#endif
