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
#include "jwpqt/core/edict_pattern.h"
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
          const std::vector<std::size_t>& indexed_offsets,
          const jwpqt::core::EdictIndexOptions& index_options = {})
      : source(std::move(source_value)),
        dictionary(jwpqt::core::EdictDictionary::parse(
            source, jwpqt::core::EdictEncoding::kUtf8)),
        index(jwpqt::core::EdictIndex::parse(
            index_bytes(source.size(), indexed_offsets), dictionary,
            index_options)) {}

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

void test_pattern_search_expands_and_filters_records() {
  const std::string first = "to swim /(s) name/\n";
  const std::string second = "to swim /word/\n";
  const std::string source = first + second;
  const std::size_t first_anchor = offset_of(source, "swim");
  const std::size_t second_anchor = offset_of(source, "swim", first_anchor + 1);
  const Fixture fixture(source, {first_anchor, second_anchor});
  const jwpqt::core::EdictSearchPlan plan =
      jwpqt::core::prepare_edict_search_plan(
          jwpqt::core::encode_jwp_text(U"to swim"));

  jwpqt::core::EdictSearchOptions options;
  options.direct.require_beginning = true;
  options.direct.require_end = true;
  options.name_filter.reject_personal_names = true;
  const jwpqt::core::EdictSearchReport report =
      jwpqt::core::search_edict_pattern(fixture.dictionary, fixture.index,
                                        plan, options);
  require(report.results.size() == 1 && report.rejected == 1 &&
              report.candidate_matches == 2 && report.queries == 1 &&
              report.lookup_steps > 0 &&
              report.results[0].stage ==
                  jwpqt::core::EdictSearchStage::kPattern &&
              report.results[0].match.byte_offset == first.size() &&
              report.results[0].match.byte_length == 7 &&
              report.results[0].record.definitions ==
                  std::vector<std::u32string>{U"word"},
          "Pattern EDICT orchestration did not expand/filter its match");
}

void test_pattern_search_applies_expanded_ascii_boundaries() {
  const std::string source = "not to swim /word/\n";
  const std::size_t anchor = offset_of(source, "swim");
  const Fixture fixture(source, {anchor});
  const jwpqt::core::EdictSearchPlan plan =
      jwpqt::core::prepare_edict_search_plan(
          jwpqt::core::encode_jwp_text(U"to swim"));

  jwpqt::core::EdictSearchOptions options;
  options.direct.require_beginning = true;
  const jwpqt::core::EdictSearchReport normal =
      jwpqt::core::search_edict_pattern(fixture.dictionary, fixture.index,
                                        plan, options);
  require(normal.results.size() == 1,
          "Normal ASCII boundaries rejected an expanded pattern word");

  options.direct.full_ascii_boundaries = true;
  const jwpqt::core::EdictSearchReport full =
      jwpqt::core::search_edict_pattern(fixture.dictionary, fixture.index,
                                        plan, options);
  require(full.results.empty() && full.rejected == 1,
          "Full ASCII boundaries ignored the expanded pattern span");
}

void test_pattern_search_limits_and_occurrence_identity() {
  const std::string source = "\xe4\xba\x9c\xe6\x97\xa5 /word/\n";
  const std::size_t anchor = offset_of(source, "\xe4\xba\x9c");
  const Fixture fixture(source, {anchor, anchor});
  const jwpqt::core::EdictSearchPlan plan =
      jwpqt::core::prepare_edict_search_plan(
          jwpqt::core::encode_jwp_text(U"[\u4e9c?]"));

  jwpqt::core::EdictSearchOptions options;
  const jwpqt::core::EdictSearchReport report =
      jwpqt::core::search_edict_pattern(fixture.dictionary, fixture.index,
                                        plan, options);
  require(report.results.size() == 2 &&
              report.results[0].match == report.results[1].match,
          "Pattern EDICT search deduplicated distinct index occurrences");

  options.candidate_matches = 1;
  require_throws(
      [&] { search_edict_pattern(fixture.dictionary, fixture.index, plan,
                                 options); },
      "Pattern EDICT search ignored its global candidate limit");
  options.candidate_matches = 10;
  options.results = 1;
  require_throws(
      [&] { search_edict_pattern(fixture.dictionary, fixture.index, plan,
                                 options); },
      "Pattern EDICT search ignored its global result limit");
  options.results = 10;
  options.lookup_steps = 1;
  require_throws(
      [&] { search_edict_pattern(fixture.dictionary, fixture.index, plan,
                                 options); },
      "Pattern EDICT search ignored its shared work limit");
  options.lookup_steps = 1'000;
  options.pattern.work_steps = 1;
  require_throws(
      [&] { search_edict_pattern(fixture.dictionary, fixture.index, plan,
                                 options); },
      "Pattern EDICT search ignored its matcher work limit");

  const jwpqt::core::EdictSearchPlan direct =
      jwpqt::core::prepare_edict_search_plan({'c', 'a', 't'});
  require_throws(
      [&] { search_edict_pattern(fixture.dictionary, fixture.index, direct); },
      "Pattern EDICT orchestration accepted a direct search plan");
}

void test_pattern_search_uses_japanese_boundaries_and_index_code_page() {
  const std::string embedded = "\xe6\x97\xa5\xe4\xba\x9c\xe6\x97\xa5";
  const std::string leading = "\xe4\xba\x9c\xe6\x97\xa5";
  const std::string source =
      embedded + " /embedded/\n" + leading + " /leading/\n";
  const std::size_t first_anchor = offset_of(source, "\xe4\xba\x9c");
  const std::size_t second_anchor =
      offset_of(source, "\xe4\xba\x9c", first_anchor + 1);
  const Fixture fixture(source, {first_anchor, second_anchor});
  const jwpqt::core::EdictSearchPlan plan =
      jwpqt::core::prepare_edict_search_plan(
          jwpqt::core::encode_jwp_text(U"\u4e9c?"));
  jwpqt::core::EdictSearchOptions options;
  options.direct.require_beginning = true;
  const jwpqt::core::EdictSearchReport report =
      jwpqt::core::search_edict_pattern(fixture.dictionary, fixture.index,
                                        plan, options);
  require(report.results.size() == 1 && report.rejected == 1 &&
              report.results[0].record.definitions ==
                  std::vector<std::u32string>{U"leading"},
          "Pattern EDICT search ignored Japanese expanded-span boundaries");

  const std::string cp_source =
      "\xe4\xba\x9c\xe6\x97\xa5\xd0\x91 /cp1251/\n";
  jwpqt::core::EdictIndexOptions cp_options;
  cp_options.utf8_code_page = jwpqt::core::LegacyCodePage::k1251;
  const Fixture cp_fixture(cp_source, {0}, cp_options);
  jwpqt::core::EdictSearchPlan cp_plan;
  cp_plan.kind = jwpqt::core::EdictSearchPlanKind::kPattern;
  cp_plan.anchor = query(U"\u4e9c");
  cp_plan.postfix = {'?'};
  const jwpqt::core::JwpText cyrillic = jwpqt::core::encode_jwp_text(
      U"\u0411", jwpqt::core::LegacyCodePage::k1251);
  cp_plan.postfix.insert(cp_plan.postfix.end(), cyrillic.begin(),
                         cyrillic.end());
  cp_plan.postfix.push_back(']');
  const jwpqt::core::EdictSearchReport cp_report =
      jwpqt::core::search_edict_pattern(cp_fixture.dictionary,
                                        cp_fixture.index, cp_plan);
  require(cp_report.results.size() == 1 &&
              cp_report.results[0].match.byte_length == 8,
          "Pattern EDICT search ignored its index-owned UTF code page");
}

void test_contingent_kanji_search_and_name_filtering() {
  const std::string word =
      jwpqt::core::encode_utf8(U"\u65e5\u672c");
  const std::string term =
      jwpqt::core::encode_utf8(U"\u4e9c\u65e5\u672c\u8a9e");
  const std::string first = term + " /(s) name/\n";
  const std::string second = term + " /word/\n";
  const std::string source = first + second;
  const std::size_t first_anchor = offset_of(source, word);
  const std::size_t second_anchor = offset_of(source, word, first_anchor + 1);
  const Fixture fixture(source, {first_anchor, second_anchor});

  jwpqt::core::EdictSearchOptions options;
  options.contingent.enabled = true;
  options.direct.require_beginning = true;
  options.direct.require_end = true;
  const jwpqt::core::EdictSearchReport report = jwpqt::core::search_edict(
      fixture.dictionary, fixture.index, query(U"\u65e5\u672c"), options);
  require(report.results.size() == 1 && report.queries == 2 &&
              report.candidate_matches == 4 && report.rejected == 1 &&
              report.results[0].stage ==
                  jwpqt::core::EdictSearchStage::kContingent &&
              report.results[0].record.definitions ==
                  std::vector<std::u32string>{U"word"},
          "Contingent kanji search did not expand/filter its retry");

  options.contingent.names_mode = true;
  const jwpqt::core::EdictSearchReport names = jwpqt::core::search_edict(
      fixture.dictionary, fixture.index, query(U"\u65e5\u672c"), options);
  require(names.results.empty() && names.queries == 1,
          "Contingent search ran in explicit names mode");

  options.contingent.names_mode = false;
  options.queries = 1;
  require_throws(
      [&] {
        search_edict(fixture.dictionary, fixture.index,
                     query(U"\u65e5\u672c"),
                     options);
      },
      "Contingent search escaped the shared query limit");
}

void test_contingent_kana_limits_and_forced_mode() {
  const std::string key =
      jwpqt::core::encode_utf8(U"\u3042\u3044\u3046");
  const std::string leading =
      jwpqt::core::encode_utf8(U"\u3042\u3044\u3046\u3048");
  const std::string embedded =
      jwpqt::core::encode_utf8(U"\u304b\u3042\u3044\u3046\u3048");
  const std::string source = leading + " /leading/\n" + embedded +
                             " /embedded/\n";
  const std::size_t first_anchor = offset_of(source, key);
  const std::size_t second_anchor = offset_of(source, key, first_anchor + 1);
  const Fixture fixture(source, {first_anchor, second_anchor});

  jwpqt::core::EdictSearchOptions options;
  options.contingent.enabled = true;
  options.direct.require_beginning = true;
  options.direct.require_end = true;
  const jwpqt::core::EdictSearchReport limited = jwpqt::core::search_edict(
      fixture.dictionary, fixture.index,
      query(U"\u3042\u3044\u3046"), options);
  require(limited.results.size() == 1 &&
              limited.results[0].record.definitions ==
                  std::vector<std::u32string>{U"leading"},
          "Automatic three-kana contingent search did not retain Beginning");

  options.contingent.forced = true;
  const jwpqt::core::EdictSearchReport forced = jwpqt::core::search_edict(
      fixture.dictionary, fixture.index,
      query(U"\u3042\u3044\u3046"), options);
  require(forced.results.size() == 2 &&
              forced.results[1].record.definitions ==
                  std::vector<std::u32string>{U"embedded"},
          "Forced contingent search did not clear heuristic boundaries");
}

void test_contingent_honorific_retry() {
  const std::string suffix =
      jwpqt::core::encode_utf8(U"\u65e5\u672c");
  const std::string source = suffix + " /country/\n";
  const Fixture fixture(source, {0});
  jwpqt::core::EdictSearchOptions options;
  options.contingent.enabled = true;
  options.direct.require_beginning = true;
  options.direct.require_end = true;
  const jwpqt::core::EdictSearchReport report = jwpqt::core::search_edict(
      fixture.dictionary, fixture.index,
      query(U"\u304a\u65e5\u672c"), options);
  require(report.results.size() == 1 && report.queries == 2 &&
              report.results[0].query == query(U"\u65e5\u672c").key &&
              report.results[0].stage ==
                  jwpqt::core::EdictSearchStage::kContingent,
          "Contingent search did not stop after its honorific suffix retry");
}

void test_contingent_filtered_honorific_falls_through() {
  const std::string suffix =
      jwpqt::core::encode_utf8(U"\u65e5\u672c");
  const std::string honorific =
      jwpqt::core::encode_utf8(U"\u304a\u65e5\u672c\u8a9e");
  const std::string first = suffix + " /(s) name/\n";
  const std::string second = honorific + " /word/\n";
  const std::string source = first + second;
  const std::size_t honorific_start = first.size();
  const std::size_t suffix_in_honorific = offset_of(source, suffix, first.size());
  const Fixture fixture(source,
                        {honorific_start, 0, suffix_in_honorific});
  jwpqt::core::EdictSearchOptions options;
  options.contingent.enabled = true;
  options.direct.require_beginning = true;
  options.direct.require_end = true;
  const jwpqt::core::EdictQuery lookup =
      query(U"\u304a\u65e5\u672c");
  const jwpqt::core::EdictSearchReport report = jwpqt::core::search_edict(
      fixture.dictionary, fixture.index, lookup, options);
  require(report.results.size() == 1 && report.queries == 3 &&
              report.results[0].record.definitions ==
                  std::vector<std::u32string>{U"word"} &&
              report.results[0].stage ==
                  jwpqt::core::EdictSearchStage::kContingent,
          "Filtered honorific retry did not fall through to contingent search");

  options.queries = 2;
  require_throws(
      [&] { search_edict(fixture.dictionary, fixture.index, lookup, options); },
      "Honorific and contingent retries escaped the shared query limit");
}

void test_contingent_adaptive_selectivity_and_structural_minima() {
  const std::string source = "dog /animal/\n";
  const Fixture fixture(source, {0});
  jwpqt::core::EdictSearchOptions options = adaptive_options();
  options.adaptive_always = true;
  const jwpqt::core::EdictSearchReport baseline = jwpqt::core::search_edict(
      fixture.dictionary, fixture.index,
      query(U"\u3042\u3044\u305f"), options);

  options.contingent.enabled = true;
  const jwpqt::core::EdictSearchReport automatic =
      jwpqt::core::search_edict(fixture.dictionary, fixture.index,
                                query(U"\u3042\u3044\u305f"), options);
  require(automatic.queries == baseline.queries,
          "Likely conjugated three-kana query ran automatic contingent search");

  options.contingent.forced = true;
  const jwpqt::core::EdictSearchReport forced = jwpqt::core::search_edict(
      fixture.dictionary, fixture.index,
      query(U"\u3042\u3044\u305f"), options);
  require(forced.queries == baseline.queries + 1,
          "Forced contingent search did not override adaptive selectivity");

  options.adaptive = false;
  const jwpqt::core::EdictSearchReport too_short = jwpqt::core::search_edict(
      fixture.dictionary, fixture.index, query(U"\u65e5"), options);
  require(too_short.queries == 1,
          "Forced contingent search bypassed structural kanji minima");
}

void test_contingent_long_query_fallback_and_truncation() {
  const std::u32string long_word(99, U'\u4e9c');
  const std::string encoded = jwpqt::core::encode_utf8(long_word);
  const std::string source = jwpqt::core::encode_utf8(U"\u65e5") + encoded +
                             jwpqt::core::encode_utf8(U"\u672c") +
                             " /embedded/\n";
  const Fixture fixture(source, {offset_of(source, encoded)});
  jwpqt::core::EdictSearchOptions options;
  options.contingent.enabled = true;
  options.direct.require_beginning = true;
  options.direct.require_end = true;
  const jwpqt::core::EdictSearchReport report = jwpqt::core::search_edict(
      fixture.dictionary, fixture.index, query(long_word), options);
  require(report.results.size() == 1 && report.queries == 2 &&
              report.results[0].stage ==
                  jwpqt::core::EdictSearchStage::kContingent,
          "Long kanji contingent search did not use its direct fallback");

  jwpqt::core::JwpText oversized(101, 0x3021);
  const jwpqt::core::EdictQuery truncated =
      jwpqt::core::prepare_edict_query(oversized);
  require(truncated.truncated, "Contingent truncation fixture was not truncated");
  const jwpqt::core::EdictSearchReport skipped = jwpqt::core::search_edict(
      fixture.dictionary, fixture.index, truncated, options);
  require(skipped.results.empty() && skipped.queries == 1,
          "Contingent search ignored the caller's truncated-query state");

  jwpqt::core::JwpText limited_key = {0x3021, 0x2422};
  limited_key.insert(limited_key.end(), 97, 0x2122);
  const std::u32string limited_text =
      jwpqt::core::decode_jwp_text(limited_key);
  const std::string limited_source =
      jwpqt::core::encode_utf8(limited_text + U"\u672c") + " /limited/\n";
  const Fixture limited_fixture(limited_source, {0});
  const jwpqt::core::EdictSearchReport limited = jwpqt::core::search_edict(
      limited_fixture.dictionary, limited_fixture.index,
      jwpqt::core::prepare_edict_query(limited_key), options);
  require(limited.results.size() == 1 && limited.queries == 2,
          "Long limited contingent search did not relax only End");
}

void test_contingent_does_not_count_jis_punctuation_as_kana() {
  const jwpqt::core::JwpText punctuation = {0x2122, 0x2123, 0x2124};
  const std::u32string decoded =
      jwpqt::core::decode_jwp_text(punctuation) + U"\u65e5";
  const std::string source = jwpqt::core::encode_utf8(decoded) + " /marks/\n";
  const std::string encoded = jwpqt::core::encode_utf8(
      jwpqt::core::decode_jwp_text(punctuation));
  const Fixture fixture(source, {offset_of(source, encoded)});
  jwpqt::core::EdictSearchOptions options;
  options.contingent.enabled = true;
  options.contingent.forced = true;
  options.direct.require_beginning = true;
  options.direct.require_end = true;
  const jwpqt::core::EdictSearchReport report = jwpqt::core::search_edict(
      fixture.dictionary, fixture.index,
      jwpqt::core::prepare_edict_query(punctuation), options);
  require(report.results.empty() && report.queries == 1,
          "JIS punctuation bypassed contingent kana minima");
}

void test_linear_backend_runs_every_search_stage() {
  const std::string direct_source =
      "cat /animal/\n"
      "to swim /verb/\n";
  const jwpqt::core::EdictDictionary direct_dictionary =
      jwpqt::core::EdictDictionary::parse(
          direct_source, jwpqt::core::EdictEncoding::kUtf8);
  const jwpqt::core::EdictSearchReport direct =
      jwpqt::core::search_edict_linear(direct_dictionary, query(U"cat"));
  require(direct.results.size() == 1 && direct.queries == 1 &&
              direct.results[0].record.definitions ==
                  std::vector<std::u32string>{U"animal"},
          "Linear direct orchestration did not return its physical record");

  const jwpqt::core::EdictSearchPlan pattern =
      jwpqt::core::prepare_edict_search_plan(
          jwpqt::core::encode_jwp_text(U"to swim"));
  const jwpqt::core::EdictSearchReport patterned =
      jwpqt::core::search_edict_pattern_linear(direct_dictionary, pattern);
  require(patterned.results.size() == 1 && patterned.queries == 1 &&
              patterned.results[0].stage ==
                  jwpqt::core::EdictSearchStage::kPattern &&
              patterned.results[0].match.byte_length == 7,
          "Linear pattern orchestration did not expand its source span");

  const std::string japanese_source =
      jwpqt::core::encode_utf8(U"\u3042\u304f") + " /adaptive/\n" +
      jwpqt::core::encode_utf8(U"\u4e9c\u65e5\u672c\u8a9e") +
      " /contingent/\n";
  const jwpqt::core::EdictDictionary japanese_dictionary =
      jwpqt::core::EdictDictionary::parse(
          japanese_source, jwpqt::core::EdictEncoding::kUtf8);
  jwpqt::core::EdictSearchOptions adaptive = adaptive_options();
  const jwpqt::core::EdictSearchReport adapted =
      jwpqt::core::search_edict_linear(japanese_dictionary,
                                       query(U"\u3042\u3044"), adaptive);
  require(adapted.results.size() == 1 &&
              adapted.results[0].stage ==
                  jwpqt::core::EdictSearchStage::kAdaptive,
          "Linear adaptive orchestration did not use the linear backend");

  jwpqt::core::EdictSearchOptions contingent;
  contingent.contingent.enabled = true;
  contingent.direct.require_beginning = true;
  contingent.direct.require_end = true;
  const jwpqt::core::EdictSearchReport retried =
      jwpqt::core::search_edict_linear(japanese_dictionary,
                                       query(U"\u65e5\u672c"), contingent);
  require(retried.results.size() == 1 &&
              retried.results[0].stage ==
                  jwpqt::core::EdictSearchStage::kContingent,
          "Linear contingent orchestration did not use wildcard expansion");
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
  test_pattern_search_expands_and_filters_records();
  test_pattern_search_applies_expanded_ascii_boundaries();
  test_pattern_search_limits_and_occurrence_identity();
  test_pattern_search_uses_japanese_boundaries_and_index_code_page();
  test_contingent_kanji_search_and_name_filtering();
  test_contingent_kana_limits_and_forced_mode();
  test_contingent_honorific_retry();
  test_contingent_filtered_honorific_falls_through();
  test_contingent_adaptive_selectivity_and_structural_minima();
  test_contingent_long_query_fallback_and_truncation();
  test_contingent_does_not_count_jis_punctuation_as_kana();
  test_linear_backend_runs_every_search_stage();
  return 0;
}
