// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/kanji_lookup.h"

#include <algorithm>
#include <array>
#include <unordered_set>

#include "jwpqt/core/kanji_bushu_selector.h"

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

std::vector<std::size_t> linked_kanji_radicals(std::size_t radical) {
  if (radical >= 241) throw KanjiLookupListError("Selected radical is out of range");
  constexpr std::array<std::array<std::size_t, 2>, 15> pairs{{
      {{186, 32}}, {{193, 33}}, {{51, 52}}, {{54, 55}}, {{164, 73}},
      {{80, 81}}, {{86, 87}}, {{97, 96}}, {{118, 113}}, {{149, 115}},
      {{154, 116}}, {{146, 140}}, {{236, 199}}, {{238, 219}}, {{239, 226}}}};
  if (radical >= 64 && radical <= 66) return {64, 65, 66};
  for (const auto& pair : pairs)
    if (pair[0] == radical || pair[1] == radical) return {pair[0], pair[1]};
  return {radical};
}

std::size_t kanji_radical_stroke_estimate(const std::vector<std::size_t>& radicals) {
  if (radicals.size() > 241) throw KanjiLookupListError("Too many selected radicals");
  std::array<bool, 241> selected{};
  for (const auto radical : radicals) {
    if (radical >= selected.size()) throw KanjiLookupListError("Selected radical is out of range");
    selected[radical] = true;
  }
  std::size_t estimate = 0;
  for (std::uint8_t strokes = 1; strokes <= kMaximumBushuRadicalStrokes; ++strokes)
    for (const auto choice : kanji_bushu_choices(strokes, true))
      if (selected[choice.sprite_index]) estimate += strokes;
  return estimate;
}

int step_kanji_strokes(int count, int steps, std::size_t radical_strokes) {
  if (count < 0 || count > 30) throw KanjiLookupListError("Kanji stroke count is invalid");
  if (steps == 0) return count;
  // The estimate is only a stepping hint: overlapping variants may sum above 30.
  const int minimum = static_cast<int>(std::clamp<std::size_t>(radical_strokes, 1, 30));
  const int cycle = 32 - minimum;
  int index = 0;
  if (count >= minimum) index = count - minimum + 1;
  else if (count != 0 && steps < 0) index = 1;
  index = (index + steps % cycle + cycle) % cycle;
  return index == 0 ? 0 : index + minimum - 1;
}

std::vector<std::size_t> kanji_radicals_for_character(
    const KanjiLookupLists& radical_lists, JisCode code, std::size_t work_limit) {
  if (!is_jis_x0208_pair(code) || code < 0x3000)
    throw KanjiLookupListError("Radical extraction requires a JIS kanji");
  std::vector<std::size_t> result;
  std::size_t work = 0;
  for (std::size_t index = 0; index < radical_lists.group_count(); ++index) {
    if (work == work_limit) throw KanjiLookupListError("Radical extraction work budget is exceeded");
    ++work;
    for (const auto member : radical_lists.group(index)) {
      if (work == work_limit) throw KanjiLookupListError("Radical extraction work budget is exceeded");
      ++work;
      if (member == code) {
        result.push_back(index);
        break;
      }
    }
  }
  return result;
}

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
