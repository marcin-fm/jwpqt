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

class SearchCollector {
 public:
  SearchCollector(const EdictDictionary& dictionary, const EdictIndex& index,
                  const EdictSearchOptions& options)
      : dictionary_(dictionary), index_(index), options_(options) {}

  void run(const EdictQuery& query, EdictSearchStage stage,
           std::size_t adaptive_pass) {
    consume_query();

    EdictDirectSearchOptions direct = options_.direct;
    const std::size_t accepted_limit = direct.results;
    direct.results = std::numeric_limits<std::size_t>::max();
    direct.candidate_matches =
        std::min(direct.candidate_matches,
                 options_.candidate_matches - report_.candidate_matches);
    direct.lookup_steps =
        std::min(direct.lookup_steps,
                 options_.lookup_steps - report_.lookup_steps);
    EdictDirectSearchReport found =
        stage == EdictSearchStage::kAdaptive
            ? search_edict_adaptive_report(dictionary_, index_, query, direct)
            : search_edict_direct_report(dictionary_, index_, query, direct);
    report_.lookup_steps += found.lookup_steps;
    report_.candidate_matches += found.candidate_matches;

    std::size_t accepted = 0;
    for (const EdictIndexMatch& match : found.matches) {
      accepted += append(match, query.key, stage, adaptive_pass,
                         accepted, accepted_limit)
                      ? 1U
                      : 0U;
    }
  }

  void run_pattern(const EdictSearchPlan& plan) {
    validate_edict_pattern_plan(plan);
    consume_query();

    EdictDirectSearchOptions direct = options_.direct;
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
        search_edict_direct_report(dictionary_, index_, plan.anchor, direct);
    report_.candidate_matches += anchors.candidate_matches;
    report_.lookup_steps += anchors.lookup_steps;

    EdictDirectSearchOptions boundaries = options_.direct;
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
      pattern.utf8_code_page = index_.utf8_code_page();
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
      accepted += append(match, *query, EdictSearchStage::kPattern, 0,
                         accepted, accepted_limit)
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
              std::size_t accepted, std::size_t accepted_limit) {
    const std::vector<EdictRecord>& records = dictionary_.records();
    if (match.record_index >= records.size()) {
      throw EdictSearchError("EDICT search result record is invalid");
    }
    std::optional<EdictRecord> record =
        filter_edict_name_types(records[match.record_index],
                                options_.name_filter);
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
  const EdictIndex& index_;
  const EdictSearchOptions& options_;
  EdictSearchReport report_;
};

}  // namespace

EdictSearchReport search_edict(const EdictDictionary& dictionary,
                               const EdictIndex& index,
                               const EdictQuery& query,
                               const EdictSearchOptions& options) {
  const EdictQuery validated = prepare_edict_query(query.key);
  SearchCollector collector(dictionary, index, options);
  collector.run(validated, EdictSearchStage::kDirect, 0);

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

}  // namespace jwpqt::core
