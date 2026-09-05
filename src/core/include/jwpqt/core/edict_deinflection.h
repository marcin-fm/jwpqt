// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_CORE_EDICT_DEINFLECTION_H
#define JWPQT_CORE_EDICT_DEINFLECTION_H

#include <cstddef>
#include <stdexcept>
#include <vector>

#include "jwpqt/core/edict_search.h"

namespace jwpqt::core {

class EdictDeinflectionError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

struct EdictDeinflectionOptions {
  bool include_i_adjectives = true;
  std::size_t queries = 1024;
  std::size_t work_steps = 4096;
};

struct EdictDeinflectionQuery {
  JwpText key;
  std::size_t pass = 0;

  bool operator==(const EdictDeinflectionQuery& other) const noexcept;
};

// Returns the recovered non-pattern adaptive-search variants in execution
// order. Only the initial redundant direct-search pass is omitted; the same
// key may be generated again naturally after a later truncation.
std::vector<JwpText> generate_edict_deinflection_queries(
    const EdictQuery& query,
    const EdictDeinflectionOptions& options = EdictDeinflectionOptions{});

std::vector<EdictDeinflectionQuery> generate_edict_deinflection_steps(
    const EdictQuery& query,
    const EdictDeinflectionOptions& options = EdictDeinflectionOptions{});

}  // namespace jwpqt::core

#endif
