// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_CORE_EDICT_ENGINE_H
#define JWPQT_CORE_EDICT_ENGINE_H

#include <cstddef>
#include <vector>

#include "jwpqt/core/edict_deinflection.h"
#include "jwpqt/core/edict_filter.h"
#include "jwpqt/core/edict_search.h"

namespace jwpqt::core {

enum class EdictSearchStage {
  kDirect,
  kAdaptive,
};

struct EdictSearchResult {
  EdictRecord record;
  EdictIndexMatch match;
  JwpText query;
  EdictSearchStage stage = EdictSearchStage::kDirect;
  std::size_t adaptive_pass = 0;
};

struct EdictSearchOptions {
  EdictDirectSearchOptions direct;
  EdictNameFilterOptions name_filter;
  EdictDeinflectionOptions deinflection;
  bool adaptive = false;
  bool adaptive_always = true;
  bool adaptive_show_all = false;
  std::size_t queries = 1024;
  std::size_t candidate_matches = 1'000'000;
  std::size_t results = 1'000'000;
  std::size_t lookup_steps = 64U * 1024U * 1024U;
};

struct EdictSearchReport {
  std::vector<EdictSearchResult> results;
  std::size_t rejected = 0;
  std::size_t candidate_matches = 0;
  std::size_t queries = 0;
  std::size_t lookup_steps = 0;
};

EdictSearchReport search_edict(
    const EdictDictionary& dictionary, const EdictIndex& index,
    const EdictQuery& query,
    const EdictSearchOptions& options = EdictSearchOptions{});

}  // namespace jwpqt::core

#endif
