// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/edict_pattern_match.h"

#include <cstdlib>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using jwpqt::core::EdictDictionary;
using jwpqt::core::EdictEncoding;
using jwpqt::core::EdictIndexMatch;
using jwpqt::core::EdictPatternError;
using jwpqt::core::EdictPatternMatch;
using jwpqt::core::EdictPatternMatchOptions;
using jwpqt::core::EdictSearchPlan;
using jwpqt::core::EdictSearchPlanKind;
using jwpqt::core::JwpText;

void require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void expect_error(const std::function<void()>& operation,
                  const char* message) {
  try {
    operation();
  } catch (const EdictPatternError&) {
    return;
  }
  throw std::runtime_error(message);
}

std::string euc(std::uint16_t token) {
  std::string result;
  result.push_back(static_cast<char>((token >> 8U) | 0x80U));
  result.push_back(static_cast<char>((token & 0xffU) | 0x80U));
  return result;
}

EdictSearchPlan pattern_plan(JwpText anchor, JwpText prefix = {},
                             JwpText postfix = {}) {
  EdictSearchPlan plan;
  plan.kind = EdictSearchPlanKind::kPattern;
  plan.anchor.key = std::move(anchor);
  plan.prefix = std::move(prefix);
  plan.postfix = std::move(postfix);
  return plan;
}

void test_expands_prefix_and_greedy_postfix() {
  const std::string source =
      "to " + euc(0x3021) + "xbxb /definition/\n";
  const EdictDictionary dictionary =
      EdictDictionary::parse(source, EdictEncoding::kEucJp);
  const EdictIndexMatch anchor{3, 2, 0};
  const EdictSearchPlan plan =
      pattern_plan({0x3021}, {'t', 'o', ' '}, {'*', 'x', 'b', ']'});

  const auto report =
      jwpqt::core::match_edict_pattern_report(dictionary, anchor, plan);
  require(report.match == EdictPatternMatch{0, 9, 0},
          "Pattern did not expand the exact greedy byte span");
  require(report.work_steps != 0,
          "Pattern matcher did not report comparison work");
}

void test_assertions_and_character_classes() {
  const std::string source =
      "x/" + euc(0x3021) + euc(0x3022) + " /definition/\n";
  const EdictDictionary dictionary =
      EdictDictionary::parse(source, EdictEncoding::kEucJp);
  const EdictIndexMatch anchor{2, 2, 0};

  require(jwpqt::core::match_edict_pattern(
              dictionary, anchor,
              pattern_plan({0x3021}, {'['}, {'#', ']'})) ==
              EdictPatternMatch{2, 4, 0},
          "Pattern assertions or kanji class did not match");
  require(!jwpqt::core::match_edict_pattern(
               dictionary, anchor,
               pattern_plan({0x3021}, {'?'}, {'#'}))
               .has_value(),
          "Prefix question crossed a beginning delimiter");

  const std::string repetition_source =
      euc(0x3021) + euc(0x2139) + " /definition/\n";
  const EdictDictionary repetition =
      EdictDictionary::parse(repetition_source, EdictEncoding::kEucJp);
  require(jwpqt::core::match_edict_pattern(
              repetition, {0, 2, 0},
              pattern_plan({0x3021}, {}, {'#', ']'}))
              .has_value(),
          "Kanji repetition mark did not satisfy the kanji class");

  const std::string kana_source =
      euc(0x3021) + euc(0x2422) + " /definition/\n";
  const EdictDictionary kana =
      EdictDictionary::parse(kana_source, EdictEncoding::kEucJp);
  require(!jwpqt::core::match_edict_pattern(
               kana, {0, 2, 0},
               pattern_plan({0x3021}, {}, {'#', ']'}))
               .has_value(),
          "Kana incorrectly satisfied the kanji class");
}

void test_record_local_and_jis_x0212_matching() {
  const std::string source =
      euc(0x3021) + std::string("\x8f\xa2\xed", 3) +
      " /definition/\n";
  const EdictDictionary dictionary =
      EdictDictionary::parse(source, EdictEncoding::kEucJp);
  require(jwpqt::core::match_edict_pattern(
              dictionary, {0, 2, 0},
              pattern_plan({0x3021}, {}, {'?', ']'})) ==
              EdictPatternMatch{0, 5, 0},
          "JIS X 0212 sequence was not matched as one character");

  require(!jwpqt::core::match_edict_pattern(
               dictionary, {0, 2, 0},
               pattern_plan({0x3021}, {}, {'*', 'x'}))
               .has_value(),
          "Star crossed the record delimiter while searching for a literal");
}

void test_preserves_euc_and_utf_literal_behavior() {
  const std::string euc_source =
      "TO " + euc(0x3021) + " /definition/\n";
  const EdictDictionary euc_dictionary =
      EdictDictionary::parse(euc_source, EdictEncoding::kEucJp);
  const EdictSearchPlan plan =
      pattern_plan({0x3021}, {'t', 'o', ' '}, {']'});
  require(jwpqt::core::match_edict_pattern(euc_dictionary, {3, 2, 0}, plan)
              .has_value(),
          "EUC pattern literals did not lowercase dictionary ASCII");

  const std::string utf_source = "TO \xe4\xba\x9c /definition/\n";
  const EdictDictionary utf_dictionary =
      EdictDictionary::parse(utf_source, EdictEncoding::kUtf8);
  require(!jwpqt::core::match_edict_pattern(utf_dictionary, {3, 3, 0}, plan)
               .has_value(),
          "UTF pattern literals unexpectedly lowercased dictionary ASCII");

  const EdictDictionary mixed_dictionary = EdictDictionary::parse(
      euc_source, EdictEncoding::kMixed);
  require(jwpqt::core::match_edict_pattern(mixed_dictionary, {3, 2, 0}, plan)
              .has_value(),
          "Mixed pattern literals did not preserve legacy ASCII folding");

  const std::string cyrillic_source =
      "\xd0\x90\xe4\xba\x9c /definition/\n";
  const EdictDictionary cyrillic_dictionary =
      EdictDictionary::parse(cyrillic_source, EdictEncoding::kUtf8);
  EdictPatternMatchOptions cp1251;
  cp1251.utf8_code_page = jwpqt::core::LegacyCodePage::k1251;
  require(jwpqt::core::match_edict_pattern(
              cyrillic_dictionary, {2, 3, 0},
              pattern_plan({0x3021}, {0xc0}, {']'}), cp1251)
              .has_value(),
          "UTF matcher ignored CP1251 extension precedence");
  require(jwpqt::core::match_edict_pattern(
              cyrillic_dictionary, {2, 3, 0},
              pattern_plan({0x3021}, {0x2721}, {']'}))
              .has_value(),
          "UTF matcher lost JIS Cyrillic under a non-Cyrillic code page");

  const std::string misconstrued_source =
      "\xe2\x80\x9a\xe4\xba\x9c /definition/\n";
  const EdictDictionary misconstrued_dictionary =
      EdictDictionary::parse(misconstrued_source, EdictEncoding::kUtf8);
  require(!jwpqt::core::match_edict_pattern(
               misconstrued_dictionary, {3, 3, 0},
               pattern_plan({0x3021}, {0x82}, {']'}))
               .has_value(),
          "UTF matcher accepted a recovered Windows misconstruction");
}

void test_mixed_dictionary_definition_bytes() {
  const std::string source = "word /a\x8f\xa1/\n";
  const EdictDictionary dictionary = EdictDictionary::parse(
      source, EdictEncoding::kMixed, jwpqt::core::EdictParseLimits{},
      jwpqt::core::LegacyCodePage::k1251);
  const std::size_t anchor_offset = source.find('a');
  const EdictSearchPlan plan = pattern_plan({'a'}, {}, {'?', ']'});
  require(jwpqt::core::match_edict_pattern(
              dictionary, {anchor_offset, 1, 0}, plan) ==
              EdictPatternMatch{anchor_offset, 3, 0},
          "Mixed pattern matcher did not preserve recovered high-bit pair stepping");
}

void test_rejects_invalid_anchors_and_bounds_work() {
  const std::string source = euc(0x3021) + "aaaa /definition/\n";
  const EdictDictionary dictionary =
      EdictDictionary::parse(source, EdictEncoding::kEucJp);
  const EdictSearchPlan plan =
      pattern_plan({0x3021}, {}, {'*', 'z'});

  expect_error(
      [&] {
        jwpqt::core::match_edict_pattern(dictionary, {1, 1, 0}, plan);
      },
      "Mid-character pattern anchor was accepted");
  expect_error(
      [&] {
        jwpqt::core::match_edict_pattern(dictionary, {0, 2, 1}, plan);
      },
      "Out-of-range pattern record was accepted");
  expect_error(
      [&] {
        EdictSearchPlan direct = plan;
        direct.kind = EdictSearchPlanKind::kDirect;
        jwpqt::core::match_edict_pattern(dictionary, {0, 2, 0}, direct);
      },
      "Direct plan was accepted by the pattern matcher");
  expect_error(
      [&] {
        EdictPatternMatchOptions options;
        options.work_steps = 3;
        jwpqt::core::match_edict_pattern(dictionary, {0, 2, 0}, plan,
                                         options);
      },
      "Pattern work limit was not enforced");

  EdictSearchPlan oversized = plan;
  oversized.postfix.assign(101, '?');
  expect_error(
      [&] {
        jwpqt::core::match_edict_pattern(dictionary, {0, 2, 0}, oversized);
      },
      "Oversized forged pattern plan was accepted");
}

}  // namespace

int main() {
  try {
    test_expands_prefix_and_greedy_postfix();
    test_assertions_and_character_classes();
    test_record_local_and_jis_x0212_matching();
    test_preserves_euc_and_utf_literal_behavior();
    test_mixed_dictionary_definition_bytes();
    test_rejects_invalid_anchors_and_bounds_work();
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
