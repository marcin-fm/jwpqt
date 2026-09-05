// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/kanji_lookup.h"

#include <algorithm>
#include <unordered_set>

namespace jwpqt::core {
namespace {

void validate_options(const KanjiLookupLists& radicals,
                      const KanjiLookupLists& strokes,
                      const KanjiLookupOptions& options) {
  if (strokes.group_count() < 30 || options.minimum_strokes < 1 ||
      options.maximum_strokes > 30 ||
      options.minimum_strokes > options.maximum_strokes ||
      options.results == 0 || options.work == 0) {
    throw KanjiLookupListError("Kanji lookup options are invalid");
  }
  for (const std::size_t index : options.radicals) {
    if (index >= radicals.group_count()) {
      throw KanjiLookupListError("Selected radical is out of range");
    }
  }
}

void charge_work(KanjiLookupReport& report, const KanjiLookupOptions& options,
                 std::size_t amount = 1) {
  if (amount > options.work - report.work) {
    throw KanjiLookupListError("Kanji lookup work budget is exceeded");
  }
  report.work += amount;
}

void append_result(KanjiLookupReport& report,
                   const KanjiLookupOptions& options, JisCode code,
                   std::uint8_t strokes) {
  if (report.results.size() >= options.results) {
    report.truncated = true;
    return;
  }
  report.results.push_back({code, strokes});
}

}  // namespace

KanjiLookupReport search_kanji_radicals(
    const KanjiLookupLists& radical_lists,
    const KanjiLookupLists& stroke_lists,
    const KanjiInfoDatabase* information,
    const KanjiLookupOptions& options) {
  validate_options(radical_lists, stroke_lists, options);
  KanjiLookupReport report;
  std::vector<JisCode> candidates;
  bool has_radicals = false;

  for (const std::size_t index : options.radicals) {
    const std::vector<JisCode>& selected = radical_lists.group(index);
    if (selected.empty()) continue;
    if (!has_radicals) {
      candidates = selected;
      has_radicals = true;
      continue;
    }
    const std::unordered_set<JisCode> members(selected.begin(), selected.end());
    for (JisCode& candidate : candidates) {
      if (candidate == 0) continue;
      charge_work(report, options);
      if (members.find(candidate) == members.end()) candidate = 0;
    }
  }

  const bool unrestricted_strokes =
      options.minimum_strokes == 1 && options.maximum_strokes == 30;
  if (has_radicals) {
    if (!unrestricted_strokes && information == nullptr) {
      throw KanjiLookupListError(
          "Stroke-filtered radical lookup requires kanji information");
    }
    for (const JisCode candidate : candidates) {
      if (candidate == 0) continue;
      charge_work(report, options);
      std::uint8_t strokes = 0;
      if (information != nullptr) {
        strokes = information->stroke_count(candidate);
      }
      if (!unrestricted_strokes &&
          (strokes < options.minimum_strokes ||
           strokes > options.maximum_strokes)) {
        continue;
      }
      append_result(report, options, candidate, strokes);
    }
  } else {
    for (std::uint8_t strokes = options.minimum_strokes;
         strokes <= options.maximum_strokes; ++strokes) {
      for (const JisCode candidate :
           stroke_lists.group(static_cast<std::size_t>(strokes - 1U))) {
        charge_work(report, options);
        append_result(report, options, candidate, strokes);
      }
    }
  }

  if (options.rare_last &&
      (has_radicals || options.minimum_strokes != options.maximum_strokes)) {
    std::stable_partition(report.results.begin(), report.results.end(),
                          [](const KanjiLookupResult& result) {
                            return result.code < 0x5000U;
                          });
  }
  return report;
}

}  // namespace jwpqt::core
