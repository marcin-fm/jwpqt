// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <stdexcept>
#include <vector>

#include "jwpqt/core/edict_dictionary.h"
#include "jwpqt/core/edict_index.h"
#include "jwpqt/core/jwp_document.h"

namespace jwpqt::core {

class EdictSearchError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

enum class EdictQueryKind {
  kAscii,
  kJapanese,
};

struct EdictQuery {
  JwpText key;
  EdictQueryKind kind = EdictQueryKind::kJapanese;
  bool truncated = false;
};

struct EdictDirectSearchOptions {
  bool require_beginning = false;
  bool require_end = false;
  bool full_ascii_boundaries = false;
  std::size_t results = 1'000'000;
  std::size_t candidate_matches = 1'000'000;
  std::size_t lookup_steps = 64U * 1024U * 1024U;
};

struct EdictDirectSearchReport {
  std::vector<EdictIndexMatch> matches;
  std::size_t candidate_matches = 0;
  std::size_t lookup_steps = 0;
};

EdictQuery prepare_edict_query(const JwpText& input);

std::vector<EdictIndexMatch> search_edict_direct(
    const EdictDictionary& dictionary, const EdictIndex& index,
    const EdictQuery& query,
    const EdictDirectSearchOptions& options = EdictDirectSearchOptions{});

EdictDirectSearchReport search_edict_direct_report(
    const EdictDictionary& dictionary, const EdictIndex& index,
    const EdictQuery& query,
    const EdictDirectSearchOptions& options = EdictDirectSearchOptions{});

}  // namespace jwpqt::core
