// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "jwpqt/core/edict_search.h"

namespace jwpqt::core {

EdictQuery prepare_edict_adaptive_query(const JwpText& input);

EdictDirectSearchReport search_edict_adaptive_report(
    const EdictDictionary& dictionary, const EdictIndex& index,
    const EdictQuery& query, const EdictDirectSearchOptions& options);

EdictDirectSearchReport search_edict_direct_linear_report(
    const EdictDictionary& dictionary, const EdictQuery& query,
    const EdictDirectSearchOptions& options);

EdictDirectSearchReport search_edict_adaptive_linear_report(
    const EdictDictionary& dictionary, const EdictQuery& query,
    const EdictDirectSearchOptions& options);

bool edict_match_boundaries(const EdictDictionary& dictionary,
                            const EdictIndexMatch& match,
                            EdictQueryKind query_kind,
                            const EdictDirectSearchOptions& options);

}  // namespace jwpqt::core
