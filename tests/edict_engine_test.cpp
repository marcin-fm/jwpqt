// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "jwpqt/core/edict_dictionary.h"
#include "jwpqt/core/edict_engine.h"
#include "jwpqt/core/edict_index.h"
#include "jwpqt/core/jwp_text_codec.h"
#include "jwpqt/core/utf8.h"

namespace {

void require(bool condition, std::string_view message) {
  if (!condition) throw std::runtime_error(std::string(message));
}

template <typename Function>
void require_throws(Function&& function, std::string_view message) {
  try {
    function();
  } catch (const std::exception&) {
    return;
  }
  throw std::runtime_error(std::string(message));
}

void append_u32_le(std::string& bytes, std::uint32_t value) {
  for (int shift = 0; shift < 32; shift += 8) {
    bytes.push_back(static_cast<char>((value >> shift) & 0xffU));
  }
}

std::string index_bytes(std::size_t source_size,
                        const std::vector<std::size_t>& offsets) {
  std::string bytes;
  append_u32_le(bytes, static_cast<std::uint32_t>(source_size));
  for (const std::size_t offset : offsets) {
    append_u32_le(bytes, static_cast<std::uint32_t>(offset + 1));
  }
  return bytes;
}

std::size_t offset_of(const std::string& source, std::string_view term,
                      std::size_t from = 0) {
  const std::size_t offset = source.find(term, from);
  require(offset != std::string::npos, "Synthetic EDICT term is missing");
  return offset;
}

struct Fixture {
  Fixture(std::string source_value,
          const std::vector<std::size_t>& indexed_offsets)
      : source(std::move(source_value)),
        dictionary(jwpqt::core::EdictDictionary::parse(
            source, jwpqt::core::EdictEncoding::kUtf8)),
        index(jwpqt::core::EdictIndex::parse(
            index_bytes(source.size(), indexed_offsets), dictionary)) {}

  std::string source;
  jwpqt::core::EdictDictionary dictionary;
  jwpqt::core::EdictIndex index;
};

jwpqt::core::EdictQuery query(std::u32string_view text) {
  return jwpqt::core::prepare_edict_query(jwpqt::core::encode_jwp_text(text));
}

jwpqt::core::EdictSearchOptions adaptive_options() {
  jwpqt::core::EdictSearchOptions options;
  options.adaptive = true;
  options.adaptive_always = false;
  options.direct.require_beginning = true;
  options.direct.require_end = true;
  return options;
}

void test_direct_and_disabled_adaptive_search() {
  const std::string source = "cat /animal/\n";
  const Fixture fixture(source, {offset_of(source, "cat")});
  const jwpqt::core::EdictSearchReport report = jwpqt::core::search_edict(
      fixture.dictionary, fixture.index,
      jwpqt::core::prepare_edict_query({'C', 'A', 'T'}));
  require(report.results.size() == 1 && report.queries == 1 &&
              report.candidate_matches == 1 && report.rejected == 0 &&
              report.lookup_steps > 0 &&
              report.results[0].stage ==
                  jwpqt::core::EdictSearchStage::kDirect &&
              report.results[0].record.definitions ==
                  std::vector<std::u32string>{U"animal"},
          "Direct EDICT orchestration did not preserve its result/report");
}

void test_direct_candidate_and_result_limits_are_distinct() {
  const std::string source = "bobcat /prefix/\ncat /word/\n";
  const std::size_t embedded = offset_of(source, "cat");
  const std::size_t word = offset_of(source, "cat", embedded + 1);
  const Fixture fixture(source, {embedded, word});
  jwpqt::core::EdictDirectSearchOptions options;
  options.require_beginning = true;
  options.results = 1;
  const jwpqt::core::EdictDirectSearchReport report =
      jwpqt::core::search_edict_direct_report(
          fixture.dictionary, fixture.index, query(U"cat"), options);
  require(report.matches.size() == 1 &&
              report.matches[0].byte_offset == word &&
              report.candidate_matches == 2,
          "Raw candidates consumed the accepted direct-result limit");

  jwpqt::core::EdictSearchOptions coordinated;
  coordinated.direct = options;
  coordinated.candidate_matches = 1;
  require_throws(
      [&] {
        search_edict(fixture.dictionary, fixture.index, query(U"cat"),
                     coordinated);
      },
      "Boundary-rejected candidates escaped the global candidate limit");
}

void test_name_filter_precedes_coordinator_result_limit() {
  const std::string source = "cat /(s) name/\ncat /animal/\n";
  const std::size_t first = offset_of(source, "cat");
  const std::size_t second = offset_of(source, "cat", first + 1);
  const Fixture fixture(source, {first, second});
  jwpqt::core::EdictSearchOptions options;
  options.direct.require_beginning = true;
  options.direct.require_end = true;
  options.direct.results = 1;
  options.name_filter.reject_personal_names = true;
  const jwpqt::core::EdictSearchReport report = jwpqt::core::search_edict(
      fixture.dictionary, fixture.index, query(U"cat"), options);
  require(report.results.size() == 1 && report.rejected == 1 &&
              report.results[0].record.definitions ==
                  std::vector<std::u32string>{U"animal"},
          "Name filtering consumed the coordinator accepted-result limit");
}

void test_adaptive_passes_finish_all_sibling_queries() {
  const std::string aku = "\xe3\x81\x82\xe3\x81\x8f";
  const std::string agu = "\xe3\x81\x82\xe3\x81\x90";
  const std::string source = aku + " /first/\n" + agu + " /second/\n";
  const Fixture fixture(source,
                        {offset_of(source, aku), offset_of(source, agu)});
  const jwpqt::core::EdictSearchReport report = jwpqt::core::search_edict(
      fixture.dictionary, fixture.index, query(U"\u3042\u3044"),
      adaptive_options());
  require(report.results.size() == 2 && report.queries == 6 &&
              report.results[0].stage ==
                  jwpqt::core::EdictSearchStage::kAdaptive &&
              report.results[0].adaptive_pass == 0 &&
              report.results[1].adaptive_pass == 0,
          "Adaptive EDICT search stopped inside a sibling ending pass");
}

void test_filtering_controls_adaptive_continuation() {
  const std::string aiku =
      "\xe3\x81\x82\xe3\x81\x84\xe3\x81\x8f";
  const std::string aku = "\xe3\x81\x82\xe3\x81\x8f";
  const std::string source = aiku + " /(s) name/\n" + aku + " /exist/\n";
  const Fixture fixture(source,
                        {offset_of(source, aiku), offset_of(source, aku)});
  jwpqt::core::EdictSearchOptions options = adaptive_options();
  options.name_filter.reject_personal_names = true;
  const jwpqt::core::EdictSearchReport report = jwpqt::core::search_edict(
      fixture.dictionary, fixture.index, query(U"\u3042\u3044\u3044"),
      options);
  require(report.results.size() == 1 && report.rejected == 1 &&
              report.results[0].record.definitions ==
                  std::vector<std::u32string>{U"exist"} &&
              report.results[0].adaptive_pass == 1,
          "Rejected EDICT results incorrectly stopped adaptive search");
}

void test_always_and_show_all_policies() {
  const std::string ai = "\xe3\x81\x82\xe3\x81\x84";
  const std::string aku = "\xe3\x81\x82\xe3\x81\x8f";
  const std::string aru = "\xe3\x81\x82\xe3\x82\x8b";
  const std::string source = ai + " /direct/\n" + aku + " /first/\n" +
                             aru + " /later/\n";
  const Fixture fixture(source, {offset_of(source, ai), offset_of(source, aku),
                                 offset_of(source, aru)});

  jwpqt::core::EdictSearchOptions options = adaptive_options();
  const jwpqt::core::EdictSearchReport direct_only =
      jwpqt::core::search_edict(fixture.dictionary, fixture.index,
                                query(U"\u3042\u3044"), options);
  require(direct_only.results.size() == 1 && direct_only.queries == 1,
          "Adaptive EDICT search ignored disabled Always policy");

  options.adaptive_always = true;
  const jwpqt::core::EdictSearchReport first_pass =
      jwpqt::core::search_edict(fixture.dictionary, fixture.index,
                                query(U"\u3042\u3044"), options);
  require(first_pass.results.size() == 2 && first_pass.queries == 6,
          "Always EDICT search did not execute exactly one adaptive pass");

  options.adaptive_show_all = true;
  const jwpqt::core::EdictSearchReport all = jwpqt::core::search_edict(
      fixture.dictionary, fixture.index, query(U"\u3042\u3044"), options);
  require(all.results.size() >= 3 && all.queries > first_pass.queries &&
              all.results.back().adaptive_pass > 0,
          "Keep Searching did not continue through later adaptive passes");

  options.direct.require_end = false;
  const jwpqt::core::EdictSearchReport open = jwpqt::core::search_edict(
      fixture.dictionary, fixture.index, query(U"\u3042\u3044"), options);
  require(open.queries == first_pass.queries,
          "Keep Searching ignored the recovered closed-end requirement");
}

void test_adaptive_search_preserves_101_token_candidates() {
  std::u32string lookup_word(100, U'\u4e9c');
  lookup_word.push_back(U'\u308b');
  const std::string source =
      jwpqt::core::encode_utf8(lookup_word) + " /long/\n";
  const Fixture fixture(source, {0});
  const jwpqt::core::EdictSearchReport report = jwpqt::core::search_edict(
      fixture.dictionary, fixture.index,
      query(std::u32string(100, U'\u4e9c')), adaptive_options());
  require(report.results.size() == 1 &&
              report.results[0].stage ==
                  jwpqt::core::EdictSearchStage::kAdaptive &&
              report.results[0].query.size() == 101 &&
              report.results[0].record.definitions ==
                  std::vector<std::u32string>{U"long"},
          "Adaptive EDICT search truncated a recovered 101-token variant");

  jwpqt::core::EdictQuery forged = query(std::u32string(100, U'\u4e9c'));
  forged.key.push_back(jwpqt::core::encode_jwp_text(U"\u308b")[0]);
  jwpqt::core::EdictDirectSearchOptions direct;
  direct.require_end = true;
  require(jwpqt::core::search_edict_direct(fixture.dictionary, fixture.index,
                                           forged, direct)
              .empty(),
          "Public direct EDICT search accepted a forged adaptive query");
}

void test_global_budgets_and_occurrence_identity() {
  const std::string source = "cat cat /two/\n";
  const std::size_t first = offset_of(source, "cat");
  const std::size_t second = offset_of(source, "cat", first + 1);
  const Fixture repeated(source, {first, first, second});
  const jwpqt::core::EdictQuery cat =
      jwpqt::core::prepare_edict_query({'c', 'a', 't'});
  jwpqt::core::EdictSearchOptions options;
  const jwpqt::core::EdictSearchReport report =
      jwpqt::core::search_edict(repeated.dictionary, repeated.index, cat,
                                options);
  require(report.results.size() == 3 &&
              report.results[0].match.byte_offset ==
                  report.results[1].match.byte_offset &&
              report.results[1].match.byte_offset !=
                  report.results[2].match.byte_offset,
          "Distinct repeated index occurrences were deduplicated");

  options.results = 1;
  require_throws(
      [&] { search_edict(repeated.dictionary, repeated.index, cat, options); },
      "Global EDICT result limit was not enforced");
  options.results = 10;
  options.candidate_matches = 1;
  require_throws(
      [&] { search_edict(repeated.dictionary, repeated.index, cat, options); },
      "Global EDICT candidate limit was not enforced");
  options.candidate_matches = 10;
  options.lookup_steps = 1;
  require_throws(
      [&] { search_edict(repeated.dictionary, repeated.index, cat, options); },
      "Global EDICT lookup-work limit was not enforced");

  const std::string no_match_source = "dog /animal/\n";
  const Fixture no_match(no_match_source,
                         {offset_of(no_match_source, "dog")});
  options.lookup_steps = 100;
  options.queries = 1;
  options.adaptive = true;
  options.adaptive_always = false;
  require_throws(
      [&] { search_edict(no_match.dictionary, no_match.index,
                         query(U"\u3042\u3044"), options); },
      "Global EDICT query limit was not enforced across adaptive search");
}

}  // namespace

int main() {
  test_direct_and_disabled_adaptive_search();
  test_direct_candidate_and_result_limits_are_distinct();
  test_name_filter_precedes_coordinator_result_limit();
  test_adaptive_passes_finish_all_sibling_queries();
  test_filtering_controls_adaptive_continuation();
  test_always_and_show_all_policies();
  test_adaptive_search_preserves_101_token_candidates();
  test_global_budgets_and_occurrence_identity();
  return 0;
}
