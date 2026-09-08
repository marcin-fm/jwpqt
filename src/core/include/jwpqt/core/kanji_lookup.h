// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "jwpqt/core/kanji_info.h"
#include "jwpqt/core/kanji_lookup_lists.h"

namespace jwpqt::core {

struct KanjiLookupOptions {
  std::vector<std::size_t> radicals;
  std::uint8_t minimum_strokes = 1;
  std::uint8_t maximum_strokes = 30;
  bool rare_last = false;
  std::size_t results = 3000;
  std::size_t work = 1'000'000;
};

struct KanjiLookupResult {
  JisCode code = 0;
  std::uint8_t strokes = 0;
};

struct KanjiLookupReport {
  std::vector<KanjiLookupResult> results;
  std::size_t work = 0;
  bool truncated = false;
};

// Zero-based indices in the recovered 241-radical catalog.
std::vector<std::size_t> linked_kanji_radicals(std::size_t radical);
std::size_t kanji_radical_stroke_estimate(const std::vector<std::size_t>& radicals);
int step_kanji_strokes(int count, int steps, std::size_t radical_strokes);
std::vector<std::size_t> kanji_radicals_for_character(
    const KanjiLookupLists& radical_lists, JisCode code,
    std::size_t work_limit = 1'000'000);

KanjiLookupReport search_kanji_radicals(
    const KanjiLookupLists& radical_lists,
    const KanjiLookupLists& stroke_lists,
    const KanjiInfoDatabase* information,
    const KanjiLookupOptions& options = KanjiLookupOptions{});

}  // namespace jwpqt::core
