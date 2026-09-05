// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_CORE_EDICT_ENGINE_H
#define JWPQT_CORE_EDICT_ENGINE_H

#include <cstddef>
#include <vector>

#include "jwpqt/core/edict_deinflection.h"
#include "jwpqt/core/edict_filter.h"
#include "jwpqt/core/edict_pattern.h"
#include "jwpqt/core/edict_pattern_match.h"
#include "jwpqt/core/edict_search.h"

namespace jwpqt::core {

enum class EdictSearchStage {
  kDirect,
  kAdaptive,
  kPattern,
  kContingent,
};

struct EdictSearchResult {
  EdictRecord record;
  EdictIndexMatch match;
  JwpText query;
  EdictSearchStage stage = EdictSearchStage::kDirect;
  std::size_t adaptive_pass = 0;
  std::size_t source_index = 0;
};

struct EdictSearchSource {
  const EdictDictionary* dictionary = nullptr;
  const EdictIndex* index = nullptr;
};

struct EdictSearchOptions {
  struct Contingent {
    bool enabled = false;
    bool forced = false;
    bool names_mode = false;
  };
  EdictDirectSearchOptions direct;
  EdictNameFilterOptions name_filter;
  EdictDeinflectionOptions deinflection;
  EdictPatternMatchOptions pattern;
  bool adaptive = false;
  bool adaptive_always = true;
  bool adaptive_show_all = false;
  std::size_t queries = 1024;
  std::size_t candidate_matches = 1'000'000;
  std::size_t results = 1'000'000;
  std::size_t lookup_steps = 64U * 1024U * 1024U;
  Contingent contingent;
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

EdictSearchReport search_edict_pattern(
    const EdictDictionary& dictionary, const EdictIndex& index,
    const EdictSearchPlan& plan,
    const EdictSearchOptions& options = EdictSearchOptions{});

EdictSearchReport search_edict_linear(
    const EdictDictionary& dictionary, const EdictQuery& query,
    const EdictSearchOptions& options = EdictSearchOptions{});

EdictSearchReport search_edict_pattern_linear(
    const EdictDictionary& dictionary, const EdictSearchPlan& plan,
    const EdictSearchOptions& options = EdictSearchOptions{});

EdictSearchReport search_edict_sources(
    const std::vector<EdictSearchSource>& sources, const EdictQuery& query,
    const EdictSearchOptions& options = EdictSearchOptions{});

EdictSearchReport search_edict_pattern_sources(
    const std::vector<EdictSearchSource>& sources,
    const EdictSearchPlan& plan,
    const EdictSearchOptions& options = EdictSearchOptions{});

}  // namespace jwpqt::core

#endif
