// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/edict_engine.h"

#include "edict_search_internal.h"

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
    if (report_.queries >= options_.queries) {
      throw EdictSearchError("EDICT search exceeds its query limit");
    }
    ++report_.queries;

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

    const std::vector<EdictRecord>& records = dictionary_.records();
    std::size_t accepted = 0;
    for (const EdictIndexMatch& match : found.matches) {
      if (match.record_index >= records.size()) {
        throw EdictSearchError("EDICT search result record is invalid");
      }
      std::optional<EdictRecord> record =
          filter_edict_name_types(records[match.record_index],
                                  options_.name_filter);
      if (!record.has_value()) {
        if (report_.rejected == std::numeric_limits<std::size_t>::max()) {
          throw EdictSearchError("EDICT rejected-result count overflows");
        }
        ++report_.rejected;
        continue;
      }
      if (accepted >= accepted_limit) {
        throw EdictSearchError("EDICT search exceeds its result limit");
      }
      if (report_.results.size() >= options_.results) {
        throw EdictSearchError("EDICT search exceeds its result limit");
      }
      report_.results.push_back({std::move(*record), match, query.key, stage,
                                 adaptive_pass});
      ++accepted;
    }
  }

  const EdictSearchReport& report() const noexcept { return report_; }
  EdictSearchReport finish() { return std::move(report_); }

 private:
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

}  // namespace jwpqt::core
