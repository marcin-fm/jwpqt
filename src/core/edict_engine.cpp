// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/edict_engine.h"

#include "edict_search_internal.h"
#include "edict_pattern_match_internal.h"

#include "jwpqt/core/edict_pattern_match.h"

#include <algorithm>
#include <limits>
#include <optional>
#include <utility>

namespace jwpqt::core {
namespace {

constexpr std::uint16_t kHiraganaI = 0x2424;
constexpr std::uint16_t kHiraganaO = 0x242a;
constexpr std::uint16_t kHiraganaGo = 0x2434;
constexpr std::uint16_t kHiraganaTa = 0x243f;
constexpr std::uint16_t kHiraganaDa = 0x2440;
constexpr std::uint16_t kHiraganaSmallTsu = 0x2443;
constexpr std::uint16_t kHiraganaTe = 0x2446;
constexpr std::uint16_t kHiraganaDe = 0x2447;
constexpr std::uint16_t kHiraganaN = 0x2473;
constexpr std::uint16_t kKanjiGoGyo = 0x3866;

bool is_pattern_token(std::uint16_t token) noexcept {
  return token == '*' || token == '?' || token == '#' || token == '[' ||
         token == ']' || token == 0x2129 || token == 0x214e ||
         token == 0x214f || token == 0x2174 || token == 0x2176;
}

bool is_hiragana(std::uint16_t token) noexcept {
  return (token & 0x7f00U) == 0x2400U;
}

bool likely_conjugated(const EdictQuery& query, std::size_t kana_count) {
  if (kana_count == 0 || query.key.size() < 2 ||
      !is_hiragana(query.key.back())) {
    return false;
  }
  const std::uint16_t last = query.key.back();
  const std::uint16_t penultimate = query.key[query.key.size() - 2];
  return last == kHiraganaSmallTsu || last == kHiraganaTa ||
         last == kHiraganaTe ||
         ((penultimate == kHiraganaN || penultimate == kHiraganaI) &&
          (last == kHiraganaDe || last == kHiraganaDa));
}

struct ContingentDecision {
  bool limited = false;
  bool kana_only = false;
  bool honorific = false;
};

std::optional<ContingentDecision> contingent_decision(
    const EdictQuery& query, const EdictSearchOptions& options) {
  const bool forced = options.contingent.forced;
  if ((!options.contingent.enabled && !forced) ||
      options.contingent.names_mode || query.truncated ||
      query.kind != EdictQueryKind::kJapanese || query.key.size() < 2 ||
      !options.direct.require_beginning || !options.direct.require_end ||
      (options.adaptive && !options.adaptive_always && !forced) ||
      std::any_of(query.key.begin(), query.key.end(), is_pattern_token)) {
    return std::nullopt;
  }

  std::size_t kana_count = 0;
  std::size_t kanji_count = 0;
  for (const std::uint16_t token : query.key) {
    if (token >= 0x3000U) {
      ++kanji_count;
    } else if (is_hiragana(token)) {
      ++kana_count;
    }
  }
  if ((kana_count == 0 && kanji_count < 2) ||
      (kanji_count == 0 && kana_count < 3)) {
    return std::nullopt;
  }

  ContingentDecision decision;
  decision.kana_only = kanji_count == 0;
  decision.limited =
      (decision.kana_only && kana_count < 4) ||
      (kanji_count == 1 && kana_count < 2);
  if (options.adaptive && likely_conjugated(query, kana_count)) {
    if (!forced && decision.kana_only && kana_count < 4) {
      return std::nullopt;
    }
    decision.limited = true;
  }
  if (forced) {
    decision.limited = false;
  }
  const std::uint16_t first = query.key.front();
  decision.honorific = first == kHiraganaO || first == kHiraganaGo ||
                       first == kKanjiGoGyo;
  return decision;
}

class SearchCollector {
 public:
  SearchCollector(const EdictDictionary& dictionary, const EdictIndex& index,
                   const EdictSearchOptions& options)
      : dictionary_(dictionary), index_(&index), options_(options) {}

  SearchCollector(const EdictDictionary& dictionary,
                  const EdictSearchOptions& options)
      : dictionary_(dictionary), index_(nullptr), options_(options) {}

  void run(const EdictQuery& query, EdictSearchStage stage,
           std::size_t adaptive_pass,
           std::optional<EdictDirectSearchOptions> direct_override =
               std::nullopt,
           bool reject_names = false) {
    consume_query();

    EdictDirectSearchOptions direct =
        direct_override.value_or(options_.direct);
    const std::size_t accepted_limit = direct.results;
    direct.results = std::numeric_limits<std::size_t>::max();
    direct.candidate_matches =
        std::min(direct.candidate_matches,
                 options_.candidate_matches - report_.candidate_matches);
    direct.lookup_steps =
        std::min(direct.lookup_steps,
                 options_.lookup_steps - report_.lookup_steps);
    EdictDirectSearchReport found;
    if (index_ != nullptr) {
      found = stage == EdictSearchStage::kAdaptive
                  ? search_edict_adaptive_report(dictionary_, *index_, query,
                                                 direct)
                  : search_edict_direct_report(dictionary_, *index_, query,
                                               direct);
    } else {
      found = stage == EdictSearchStage::kAdaptive
                  ? search_edict_adaptive_linear_report(dictionary_, query,
                                                        direct)
                  : search_edict_direct_linear_report(dictionary_, query,
                                                      direct);
    }
    report_.lookup_steps += found.lookup_steps;
    report_.candidate_matches += found.candidate_matches;

    std::size_t accepted = 0;
    for (const EdictIndexMatch& match : found.matches) {
      accepted += append(match, query.key, stage, adaptive_pass, accepted,
                          accepted_limit, reject_names)
                       ? 1U
                       : 0U;
    }
  }

  void run_pattern(
      const EdictSearchPlan& plan,
      std::optional<EdictDirectSearchOptions> direct_override = std::nullopt,
      EdictSearchStage stage = EdictSearchStage::kPattern,
      bool reject_names = false) {
    validate_edict_pattern_plan(plan);
    consume_query();

    EdictDirectSearchOptions direct =
        direct_override.value_or(options_.direct);
    const EdictDirectSearchOptions boundaries = direct;
    const std::size_t accepted_limit = direct.results;
    direct.require_beginning = false;
    direct.require_end = false;
    direct.results = std::numeric_limits<std::size_t>::max();
    direct.candidate_matches =
        std::min(direct.candidate_matches,
                 options_.candidate_matches - report_.candidate_matches);
    direct.lookup_steps =
        std::min(direct.lookup_steps,
                 options_.lookup_steps - report_.lookup_steps);
    EdictDirectSearchReport anchors =
        index_ != nullptr
            ? search_edict_direct_report(dictionary_, *index_, plan.anchor,
                                         direct)
            : search_edict_direct_linear_report(dictionary_, plan.anchor,
                                                direct);
    report_.candidate_matches += anchors.candidate_matches;
    report_.lookup_steps += anchors.lookup_steps;

    const EdictQueryKind boundary_kind =
        plan.ascii_boundaries ? EdictQueryKind::kAscii
                              : EdictQueryKind::kJapanese;
    std::optional<JwpText> query;

    std::size_t accepted = 0;
    for (const EdictIndexMatch& anchor : anchors.matches) {
      EdictPatternMatchOptions pattern = options_.pattern;
      pattern.work_steps =
          std::min(pattern.work_steps,
                   options_.lookup_steps - report_.lookup_steps);
      pattern.utf8_code_page =
          index_ != nullptr ? index_->utf8_code_page()
                            : dictionary_.mixed_code_page();
      const EdictPatternMatchReport expanded = match_edict_pattern_report(
          dictionary_, anchor, plan, pattern);
      report_.lookup_steps += expanded.work_steps;
      if (!expanded.match.has_value()) {
        reject();
        continue;
      }

      const EdictIndexMatch match{expanded.match->byte_offset,
                                  expanded.match->byte_length,
                                  expanded.match->record_index};
      if (!edict_match_boundaries(dictionary_, match, boundary_kind,
                                  boundaries)) {
        reject();
        continue;
      }
      if (!query.has_value()) {
        query = plan.prefix;
        query->insert(query->end(), plan.anchor.key.begin(),
                      plan.anchor.key.end());
        query->insert(query->end(), plan.postfix.begin(), plan.postfix.end());
      }
      accepted += append(match, *query, stage, 0, accepted, accepted_limit,
                          reject_names)
                       ? 1U
                       : 0U;
    }
  }

  const EdictSearchReport& report() const noexcept { return report_; }
  EdictSearchReport finish() { return std::move(report_); }

 private:
  void consume_query() {
    if (report_.queries >= options_.queries) {
      throw EdictSearchError("EDICT search exceeds its query limit");
    }
    ++report_.queries;
  }

  void reject() {
    if (report_.rejected == std::numeric_limits<std::size_t>::max()) {
      throw EdictSearchError("EDICT rejected-result count overflows");
    }
    ++report_.rejected;
  }

  bool append(const EdictIndexMatch& match, const JwpText& query,
               EdictSearchStage stage, std::size_t adaptive_pass,
               std::size_t accepted, std::size_t accepted_limit,
               bool reject_names) {
    const std::vector<EdictRecord>& records = dictionary_.records();
    if (match.record_index >= records.size()) {
      throw EdictSearchError("EDICT search result record is invalid");
    }
    EdictNameFilterOptions filter = options_.name_filter;
    if (reject_names) {
      filter.reject_personal_names = true;
      filter.reject_place_names = true;
    }
    std::optional<EdictRecord> record =
        filter_edict_name_types(records[match.record_index], filter);
    if (!record.has_value()) {
      reject();
      return false;
    }
    if (accepted >= accepted_limit ||
        report_.results.size() >= options_.results) {
      throw EdictSearchError("EDICT search exceeds its result limit");
    }
    report_.results.push_back(
        {std::move(*record), match, query, stage, adaptive_pass});
    return true;
  }

  const EdictDictionary& dictionary_;
  const EdictIndex* index_;
  const EdictSearchOptions& options_;
  EdictSearchReport report_;
};

void run_contingent(SearchCollector& collector, const EdictQuery& query,
                    const EdictSearchOptions& options,
                    const ContingentDecision& decision) {
  if (decision.honorific) {
    JwpText suffix(query.key.begin() + 1, query.key.end());
    const std::size_t before = collector.report().results.size();
    collector.run(prepare_edict_query(suffix),
                  EdictSearchStage::kContingent, 0, std::nullopt, true);
    if (collector.report().results.size() != before) {
      return;
    }
  }

  if (!decision.kana_only && query.key.size() <= 98) {
    const EdictContingentMode mode =
        decision.limited ? EdictContingentMode::kLimited
                         : EdictContingentMode::kOpen;
    collector.run_pattern(prepare_edict_contingent_plan(query, mode),
                          std::nullopt, EdictSearchStage::kContingent, true);
    return;
  }

  EdictDirectSearchOptions boundaries = options.direct;
  boundaries.require_end = false;
  if (!decision.limited) {
    boundaries.require_beginning = false;
  }
  collector.run(query, EdictSearchStage::kContingent, 0, boundaries, true);
}

}  // namespace

EdictSearchReport search_edict(const EdictDictionary& dictionary,
                               const EdictIndex& index,
                               const EdictQuery& query,
                               const EdictSearchOptions& options) {
  EdictQuery validated = prepare_edict_query(query.key);
  validated.truncated = validated.truncated || query.truncated;
  SearchCollector collector(dictionary, index, options);
  collector.run(validated, EdictSearchStage::kDirect, 0);

  if (collector.report().results.empty()) {
    const std::optional<ContingentDecision> contingent =
        contingent_decision(validated, options);
    if (contingent.has_value()) {
      run_contingent(collector, validated, options, *contingent);
    }
  }

  if (!options.adaptive || validated.kind == EdictQueryKind::kAscii ||
      (!collector.report().results.empty() && !options.adaptive_always)) {
    return collector.finish();
  }

  const std::vector<EdictDeinflectionQuery> steps =
      generate_edict_deinflection_steps(validated, options.deinflection);
  std::size_t cursor = 0;
  bool first_pass = true;
  while (cursor < steps.size()) {
    const bool keep_searching =
        collector.report().results.empty() ||
        (first_pass && options.adaptive_always) ||
        (options.adaptive_show_all && options.direct.require_end);
    if (!keep_searching) {
      break;
    }

    const std::size_t pass = steps[cursor].pass;
    do {
      const EdictQuery candidate =
          prepare_edict_adaptive_query(steps[cursor].key);
      collector.run(candidate, EdictSearchStage::kAdaptive, pass);
      ++cursor;
    } while (cursor < steps.size() && steps[cursor].pass == pass);
    first_pass = false;
  }
  return collector.finish();
}

EdictSearchReport search_edict_pattern(const EdictDictionary& dictionary,
                                       const EdictIndex& index,
                                       const EdictSearchPlan& plan,
                                       const EdictSearchOptions& options) {
  SearchCollector collector(dictionary, index, options);
  collector.run_pattern(plan);
  return collector.finish();
}

EdictSearchReport search_edict_linear(const EdictDictionary& dictionary,
                                      const EdictQuery& query,
                                      const EdictSearchOptions& options) {
  EdictQuery validated = prepare_edict_query(query.key);
  validated.truncated = validated.truncated || query.truncated;
  SearchCollector collector(dictionary, options);
  collector.run(validated, EdictSearchStage::kDirect, 0);

  if (collector.report().results.empty()) {
    const std::optional<ContingentDecision> contingent =
        contingent_decision(validated, options);
    if (contingent.has_value()) {
      run_contingent(collector, validated, options, *contingent);
    }
  }

  if (!options.adaptive || validated.kind == EdictQueryKind::kAscii ||
      (!collector.report().results.empty() && !options.adaptive_always)) {
    return collector.finish();
  }

  const std::vector<EdictDeinflectionQuery> steps =
      generate_edict_deinflection_steps(validated, options.deinflection);
  std::size_t cursor = 0;
  bool first_pass = true;
  while (cursor < steps.size()) {
    const bool keep_searching =
        collector.report().results.empty() ||
        (first_pass && options.adaptive_always) ||
        (options.adaptive_show_all && options.direct.require_end);
    if (!keep_searching) {
      break;
    }
    const std::size_t pass = steps[cursor].pass;
    do {
      collector.run(prepare_edict_adaptive_query(steps[cursor].key),
                    EdictSearchStage::kAdaptive, pass);
      ++cursor;
    } while (cursor < steps.size() && steps[cursor].pass == pass);
    first_pass = false;
  }
  return collector.finish();
}

EdictSearchReport search_edict_pattern_linear(
    const EdictDictionary& dictionary, const EdictSearchPlan& plan,
    const EdictSearchOptions& options) {
  SearchCollector collector(dictionary, options);
  collector.run_pattern(plan);
  return collector.finish();
}

}  // namespace jwpqt::core
