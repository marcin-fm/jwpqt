// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdexcept>
#include <string>
#include <string_view>

#include "jwpqt/core/edict_pattern.h"

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

void test_direct_normalization_and_boundaries() {
  using namespace jwpqt::core;
  const EdictSearchPlan ascii =
      prepare_edict_search_plan({' ', 'C', 'A', 'T', '\t'});
  require(ascii.kind == EdictSearchPlanKind::kDirect &&
              ascii.anchor.key == JwpText({'c', 'a', 't'}) &&
              !ascii.force_open_end && !ascii.force_closed_boundaries,
          "Direct ASCII plan was not trimmed and normalized");

  const EdictSearchPlan kana = prepare_edict_search_plan({0x2522});
  require(kana.anchor.key == JwpText({0x2422}) &&
              kana.force_closed_boundaries,
          "Single kana did not force closed boundaries");

  const EdictSearchPlan ascii_wildcard =
      prepare_edict_search_plan({'C', 'A', 'T', '*'});
  require(ascii_wildcard.kind == EdictSearchPlanKind::kDirect &&
              ascii_wildcard.anchor.key == JwpText({'c', 'a', 't', '*'}),
          "ASCII wildcard syntax was not disabled like the source");

  EdictPatternOptions jascii;
  jascii.jascii_to_ascii = true;
  const EdictSearchPlan wide =
      prepare_edict_search_plan({0x2343, 0x2341, 0x2354}, jascii);
  require(wide.anchor.key == JwpText({'c', 'a', 't'}),
          "JASCII option did not normalize fullwidth letters");
  const EdictSearchPlan invalid_wide =
      prepare_edict_search_plan({0x2321, 0x2341, 0x2354}, jascii);
  require(invalid_wide.anchor.kind == EdictQueryKind::kAscii &&
              invalid_wide.anchor.key == JwpText({0, 'a', 't'}),
          "Unsupported JASCII was not classified like the source");
  require(prepare_edict_search_plan({0x2343, 0x2341, 0x2354})
                  .anchor.kind == EdictQueryKind::kJapanese,
          "Disabled JASCII conversion changed query kind");
}

void test_version_id_exceptions() {
  using namespace jwpqt::core;
  const EdictSearchPlan version = prepare_edict_search_plan(
      {0x2129, 0x2129, 0x2129, 0x2129});
  require(version.kind == EdictSearchPlanKind::kDirect &&
              version.anchor.key ==
                  JwpText({0x2129, 0x2129, 0x2129, 0x2129}),
          "Fullwidth version marker was interpreted as a wildcard");

  const EdictSearchPlan dated = prepare_edict_search_plan(
      {0x2121, 0x2129, 0x2129, 0x2129});
  require(dated.anchor.key ==
              JwpText({0x2121, 0x2129, 0x2129, 0x2129}),
          "Leading-space version marker was trimmed or patterned");

  const EdictSearchPlan padded = prepare_edict_search_plan(
      {' ', 0x2129, 0x2129, 0x2129, 0x2129, '\t'});
  require(padded.anchor.key ==
              JwpText({0x2129, 0x2129, 0x2129, 0x2129}),
          "Padded version marker was not recognized after trimming");
  require_throws(
      [] {
        prepare_edict_search_plan(
            {'\t', 0x2121, 0x2129, 0x2129, 0x2129, ' '});
      },
      "Non-exact leading-space version marker bypassed legacy trimming");
}

void test_fixed_prefix_and_japanese_patterns() {
  using namespace jwpqt::core;
  const EdictSearchPlan english = prepare_edict_search_plan(
      {'T', 'o', ' ', 'S', 'w', 'i', 'm'});
  require(english.kind == EdictSearchPlanKind::kPattern &&
              english.prefix == JwpText({'t', 'o', ' '}) &&
              english.anchor.key == JwpText({'s', 'w', 'i', 'm'}) &&
              english.postfix.empty(),
          "English infinitive prefix was not split from its anchor");
  const EdictSearchPlan japanese_infinitive =
      prepare_edict_search_plan({'t', 'o', ' ', 0x3021});
  require(japanese_infinitive.kind == EdictSearchPlanKind::kPattern &&
              japanese_infinitive.prefix == JwpText({'t', 'o', ' '}) &&
              japanese_infinitive.anchor.key == JwpText({0x3021}) &&
              japanese_infinitive.ascii_boundaries &&
              japanese_infinitive.adaptive_disabled,
          "English prefix with Japanese anchor was rejected");

  const EdictSearchPlan ascii = prepare_edict_search_plan({'c', 'a', 't'});
  require(ascii.ascii_boundaries && ascii.adaptive_disabled,
          "ASCII search state was not retained explicitly");

  const EdictSearchPlan wildcard =
      prepare_edict_search_plan({0x2176, 0x2522, 0x3021, 0x2424, 0x2129});
  require(wildcard.kind == EdictSearchPlanKind::kPattern &&
              wildcard.prefix == JwpText({'*', 0x2422}) &&
              wildcard.anchor.key == JwpText({0x3021, 0x2424}) &&
              wildcard.postfix == JwpText({'?'}),
          "Japanese wildcard plan did not normalize and split its anchor");

  const EdictSearchPlan bracket = prepare_edict_search_plan(
      {0x214e, 0x3021, 0x214f, 0x3022, 0x3023});
  require(bracket.prefix == JwpText({'['}) &&
              bracket.anchor.key == JwpText({0x3021}) &&
              bracket.postfix == JwpText({']'}) &&
              bracket.adaptive_disabled && bracket.input_truncated,
          "Closing bracket did not terminate and constrain the plan");
}

void test_trailing_star_and_invalid_patterns() {
  using namespace jwpqt::core;
  const EdictSearchPlan open =
      prepare_edict_search_plan({0x2522, 0x2424, '*'});
  require(open.kind == EdictSearchPlanKind::kDirect &&
              open.anchor.key == JwpText({0x2422, 0x2424}) &&
              open.force_open_end && open.adaptive_disabled,
          "Kana trailing-star shortcut was not preserved");
  const EdictSearchPlan closed = prepare_edict_search_plan({0x2422, '*'});
  require(closed.force_closed_boundaries && !closed.force_open_end,
          "One-kana trailing star did not restore closed boundaries");

  require_throws(
      [] { prepare_edict_search_plan({'*', 0x2422, '?'}); },
      "Kanji-free Japanese wildcard pattern was accepted");
  require_throws([] { prepare_edict_search_plan({'*'}); },
                 "Anchorless trailing wildcard was accepted");
  require_throws([] { prepare_edict_search_plan({' ', '\t', 0x2121}); },
                 "Whitespace-only EDICT plan was accepted");
  require_throws(
      [] { prepare_edict_search_plan({'x', 0x3021, '*'}); },
      "Mixed ASCII/Japanese pattern was accepted");
}

void test_query_length_bound() {
  using namespace jwpqt::core;
  JwpText long_query(101, 0x3021);
  const EdictSearchPlan plan = prepare_edict_search_plan(long_query);
  require(plan.input_truncated && plan.anchor.key.size() == 100,
          "Pattern preprocessing did not preserve the search-box bound");
}

void test_contingent_kanji_plans() {
  using namespace jwpqt::core;
  const EdictQuery query = prepare_edict_query({0x2522, 0x3021, 0x2424});
  const EdictSearchPlan open =
      prepare_edict_contingent_plan(query, EdictContingentMode::kOpen);
  require(open.kind == EdictSearchPlanKind::kPattern &&
              open.prefix == JwpText({'*', 0x2422}) &&
              open.anchor.key == JwpText({0x3021, 0x2424}) &&
              open.postfix == JwpText({'*'}) &&
              !open.adaptive_disabled && !open.input_truncated,
          "Open contingent plan did not wrap and normalize the query");

  const EdictSearchPlan limited =
      prepare_edict_contingent_plan(query, EdictContingentMode::kLimited);
  require(limited.prefix == JwpText({'[', 0x2422}) &&
              limited.anchor.key == open.anchor.key &&
              limited.anchor.kind == open.anchor.kind &&
              limited.postfix == open.postfix,
          "Limited contingent plan did not preserve its left boundary");

  EdictSearchPlan owned;
  {
    const EdictQuery temporary = prepare_edict_query({0x3021, 0x2422});
    owned =
        prepare_edict_contingent_plan(temporary, EdictContingentMode::kOpen);
  }
  require(owned.prefix == JwpText({'*'}) &&
              owned.anchor.key == JwpText({0x3021, 0x2422}) &&
              owned.postfix == JwpText({'*'}),
          "Contingent plan borrowed its source query");
}

void test_contingent_plan_validation() {
  using namespace jwpqt::core;
  JwpText maximum(98, 0x2422);
  maximum[40] = 0x3021;
  const EdictSearchPlan accepted = prepare_edict_contingent_plan(
      prepare_edict_query(maximum), EdictContingentMode::kOpen);
  require(accepted.prefix.size() + accepted.anchor.key.size() +
                  accepted.postfix.size() ==
              100 &&
              !accepted.input_truncated,
          "Maximum contingent query did not fit the legacy buffer");

  JwpText too_long(99, 0x2422);
  too_long[40] = 0x3021;
  require_throws(
      [&too_long] {
        prepare_edict_contingent_plan(
            prepare_edict_query(too_long), EdictContingentMode::kOpen);
      },
      "Oversized contingent query was accepted");
  JwpText truncated(101, 0x2422);
  truncated[40] = 0x3021;
  require_throws(
      [&truncated] {
        prepare_edict_contingent_plan(
            prepare_edict_query(truncated), EdictContingentMode::kOpen);
      },
      "Truncated contingent query was accepted");
  require_throws(
      [] {
        prepare_edict_contingent_plan(prepare_edict_query({0x3021}),
                                      EdictContingentMode::kOpen);
      },
      "Single-character contingent query was accepted");
  require_throws(
      [] {
        prepare_edict_contingent_plan(prepare_edict_query({0x2422, 0x2424}),
                                      EdictContingentMode::kOpen);
      },
      "Kanji-free contingent query was accepted");
  require_throws(
      [] {
        prepare_edict_contingent_plan(
            prepare_edict_query({'c', 'a', 't'}),
            EdictContingentMode::kOpen);
      },
      "ASCII contingent query was accepted");
  require_throws(
      [] {
        prepare_edict_contingent_plan(
            EdictQuery{{0x3021, '*'}, EdictQueryKind::kJapanese},
            EdictContingentMode::kOpen);
      },
      "Pattern-bearing contingent query was accepted");
  require_throws(
      [] {
        prepare_edict_contingent_plan(
            prepare_edict_query({0x3021, 0x2176}),
            EdictContingentMode::kOpen);
      },
      "Fullwidth pattern-bearing contingent query was accepted");
  require_throws(
      [] {
        prepare_edict_contingent_plan(
            prepare_edict_query({0x3021, 0x2422}),
            static_cast<EdictContingentMode>(99));
      },
      "Invalid contingent mode was accepted");
}

}  // namespace

int main() {
  test_direct_normalization_and_boundaries();
  test_version_id_exceptions();
  test_fixed_prefix_and_japanese_patterns();
  test_trailing_star_and_invalid_patterns();
  test_query_length_bound();
  test_contingent_kanji_plans();
  test_contingent_plan_validation();
}
