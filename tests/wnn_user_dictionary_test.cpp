// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/wnn_user_dictionary.h"

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using jwpqt::core::JwpText;
using jwpqt::core::JisCode;
using jwpqt::core::WnnUserDictionary;
using jwpqt::core::WnnUserDictionaryEditor;
using jwpqt::core::WnnUserDictionaryError;
using jwpqt::core::WnnUserEntry;
using jwpqt::core::WnnUserInflection;

void require(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

void expect_error(const std::function<void()>& operation,
                  std::string_view context) {
  try {
    operation();
  } catch (const WnnUserDictionaryError&) {
    return;
  }
  throw std::runtime_error("Expected WNN user dictionary error for " +
                           std::string(context));
}

std::string bytes(std::initializer_list<unsigned int> values) {
  std::string result;
  for (const unsigned int value : values) {
    result.push_back(static_cast<char>(value));
  }
  return result;
}

void test_empty_and_ordered_round_trip() {
  require(WnnUserDictionary::parse({}).entries().empty(),
          "Empty user dictionary was not accepted");
  require(WnnUserDictionary::parse({}).serialize().empty(),
          "Empty user dictionary did not serialize empty");

  const std::string input =
      bytes({0xa2, '*', 0xb0, 0xa1, '/', 0xb0, 0xa2, '\n',
             0xa2, 0xa4, '1', 0xb1, 0xa1, '\n',
             0xa2, 'i', 0xb2, 0xa1, '\n',
             0xa2, 'k', 0xb3, 0xa1, '\n'});
  const WnnUserDictionary dictionary = WnnUserDictionary::parse(input);
  require(dictionary.serialize() == input,
          "User dictionary did not round-trip exact canonical bytes");
  require(dictionary.entries().size() == 4,
          "User dictionary entry order was not preserved");
  require(dictionary.entries()[0] ==
              WnnUserEntry{{0x2422}, '*', {{0x3021}, {0x3022}}},
          "Uninflected user entry was decoded incorrectly");
  require(dictionary.entries()[1].reading == JwpText({0x2422, 0x2424, 0x246b}) &&
              dictionary.entries()[1].ending == '1',
          "Ichidan suffix was not reconstructed");
  require(dictionary.entries()[2].reading == JwpText({0x2422, 0x2424}) &&
              dictionary.entries()[2].ending == 'i',
          "I-adjective suffix was not reconstructed");
  require(dictionary.entries()[3].reading == JwpText({0x2422, 0x242f}) &&
              dictionary.entries()[3].ending == 'k',
          "Godan suffix was not reconstructed");

  const auto records = dictionary.lookup_records();
  require(records.size() == 4 &&
              records[1].key == std::vector<std::uint8_t>({0xa2, 0xa4}) &&
              records[1].ending == '1' &&
              records[3].key == std::vector<std::uint8_t>({0xa2}) &&
              records[3].ending == 'k',
          "User entries did not produce lookup stems");
}

void test_all_inflection_endings() {
  const std::vector<std::pair<char, jwpqt::core::JisCode>> endings{
      {'u', 0x2426}, {'k', 0x242f}, {'g', 0x2430}, {'s', 0x2439},
      {'t', 0x2444}, {'n', 0x244c}, {'b', 0x2456}, {'m', 0x2460},
      {'r', 0x246b}, {'1', 0x246b}, {'i', 0x2424}};
  std::vector<WnnUserEntry> entries;
  for (const auto& [ending, suffix] : endings) {
    entries.push_back({{0x2422, suffix}, ending, {{0x3021}}});
  }
  const WnnUserDictionary dictionary =
      WnnUserDictionary::from_entries(entries);
  require(WnnUserDictionary::parse(dictionary.serialize()).entries() == entries,
          "Inflection endings did not round-trip");
}

void test_long_stem_and_imported_inflected_candidates() {
  WnnUserEntry long_inflected{
      JwpText(jwpqt::core::kWnnMaximumKeySize, 0x2422), '1', {{0x3021}}};
  long_inflected.reading.push_back(0x246b);
  const WnnUserDictionary long_dictionary =
      WnnUserDictionary::from_entries({long_inflected});
  require(WnnUserDictionary::parse(long_dictionary.serialize()).entries() ==
              std::vector<WnnUserEntry>{long_inflected},
          "Maximum-length inflected stem did not round-trip");

  const std::string imported =
      bytes({0xa2, '1', 0xb0, 0xa1, '/', 0xb0, 0xa2, '\n'});
  require(WnnUserDictionary::parse(imported).serialize() == imported,
          "Imported multi-candidate inflected entry did not round-trip");
}

void test_invalid_wire_data() {
  expect_error([] { WnnUserDictionary::parse("x"); }, "ASCII key");
  expect_error(
      [] { WnnUserDictionary::parse(bytes({0xa2, '*', 0xb0, 0xa1})); },
      "unterminated record");
  expect_error(
      [] { WnnUserDictionary::parse(bytes({0xa2, '?', 0xb0, 0xa1, '\n'})); },
      "unsupported ending");
  expect_error([] { WnnUserDictionary::parse(bytes({0xa2, '*', '\n'})); },
               "missing candidate");
  expect_error(
      [] {
        WnnUserDictionary::parse(
            bytes({0xa2, '*', '/', 0xb0, 0xa1, '\n'}));
      },
      "leading empty candidate");
  expect_error(
      [] {
        WnnUserDictionary::parse(
            bytes({0xa2, '*', 0xb0, 0xa1, '/', '\n'}));
      },
      "trailing empty candidate");
  expect_error(
      [] { WnnUserDictionary::parse(bytes({0xa2, '*', 0xb0, '\n'})); },
      "partial candidate pair");
  expect_error(
      [] { WnnUserDictionary::parse(bytes({0xa2, '*', 'A', 0xa1, '\n'})); },
      "ASCII candidate byte");
  expect_error(
      [] {
        std::string input(jwpqt::core::kWnnMaximumKeySize + 1,
                          static_cast<char>(0xa2));
        input += bytes({'*', 0xb0, 0xa1, '\n'});
        WnnUserDictionary::parse(input);
      },
      "overlong wire reading");
}

void test_invalid_entry_model() {
  expect_error(
      [] { WnnUserDictionary::from_entries({{{}, '*', {{0x3021}}}}); },
      "empty reading");
  expect_error(
      [] {
        WnnUserDictionary::from_entries(
            {{{0x3021}, '*', {{0x3022}}}});
      },
      "non-hiragana reading");
  expect_error(
      [] {
        WnnUserDictionary::from_entries(
            {{{0x2422}, '1', {{0x3021}}}});
      },
      "missing inflection suffix");
  expect_error(
      [] {
        WnnUserDictionary::from_entries(
            {{{0x2422, 0x2424}, '1', {{0x3021}}}});
      },
      "wrong inflection suffix");
  expect_error(
      [] {
        WnnUserDictionary::from_entries(
            {{{0x2422}, '*', {{0x80e9}}}});
      },
      "lossy candidate code");
  expect_error(
      [] {
        WnnUserDictionary::from_entries(
            {{JwpText(jwpqt::core::kWnnMaximumKeySize + 1, 0x2422),
               '*',
               {{0x3021}}}});
      },
      "overlong reading");

  WnnUserEntry overlong_inflected{
      JwpText(jwpqt::core::kWnnMaximumKeySize + 1, 0x2422),
      '1',
      {{0x3021}}};
  overlong_inflected.reading.push_back(0x246b);
  expect_error(
      [&] {
        WnnUserDictionary::from_entries({std::move(overlong_inflected)});
      },
      "overlong inflected stem");
}

void test_new_entry_factory() {
  require(jwpqt::core::make_wnn_user_entry(
              {0x2422}, {{0x3021}, {0x3022}}) ==
              WnnUserEntry{{0x2422}, '*', {{0x3021}, {0x3022}}},
          "Uninflected entry factory changed candidates");

  const std::vector<std::pair<JisCode, char>> godan_endings{
      {0x2426, 'u'}, {0x242f, 'k'}, {0x2430, 'g'},
      {0x2439, 's'}, {0x2444, 't'}, {0x244c, 'n'},
      {0x2456, 'b'}, {0x2460, 'm'}, {0x246b, 'r'},
  };
  for (const auto& [suffix, ending] : godan_endings) {
    require(jwpqt::core::make_wnn_user_entry(
                {0x2422, suffix}, {{0x3021, suffix}},
                WnnUserInflection::kGodan) ==
                WnnUserEntry{{0x2422, suffix}, ending, {{0x3021}}},
            "Godan entry factory derived the wrong ending");
  }

  require(jwpqt::core::make_wnn_user_entry(
              {0x2424, 0x246b}, {{0x3021, 0x246b}},
              WnnUserInflection::kIchidan) ==
              WnnUserEntry{{0x2424, 0x246b}, '1', {{0x3021}}},
          "Ichidan entry factory did not strip ru");
  require(jwpqt::core::make_wnn_user_entry(
              {0x2428, 0x246b}, {{0x3021}},
              WnnUserInflection::kIchidan) ==
              WnnUserEntry{{0x2428, 0x246b}, '1', {{0x3021}}},
          "Ichidan entry factory changed an unsuffixed candidate");
  require(jwpqt::core::make_wnn_user_entry(
              {0x2422, 0x2424}, {{0x3021, 0x2424}},
              WnnUserInflection::kIAdjective) ==
              WnnUserEntry{{0x2422, 0x2424}, 'i', {{0x3021}}},
          "I-adjective entry factory did not strip i");

  const std::vector<JisCode> ichidan_stems{
      0x2423, 0x2424, 0x2427, 0x2428, 0x242d, 0x242e,
      0x2431, 0x2432, 0x2437, 0x2438, 0x243b, 0x243c,
      0x2441, 0x2442, 0x2446, 0x2447, 0x244b, 0x244d,
      0x2452, 0x2453, 0x2454, 0x2458, 0x2459, 0x245a,
      0x245f, 0x2461, 0x246a, 0x246c, 0x2470, 0x2471,
  };
  for (JisCode stem = 0x2421; stem <= 0x2473; ++stem) {
    const bool accepted = std::find(ichidan_stems.begin(), ichidan_stems.end(),
                                    stem) != ichidan_stems.end();
    if (accepted) {
      require(jwpqt::core::make_wnn_user_entry(
                  {stem, 0x246b}, {{0x3021}},
                  WnnUserInflection::kIchidan)
                      .ending == '1',
              "Valid recovered ichidan stem kana was rejected");
    } else {
      expect_error(
          [stem] {
            jwpqt::core::make_wnn_user_entry(
                {stem, 0x246b}, {{0x3021}},
                WnnUserInflection::kIchidan);
          },
          "invalid recovered ichidan stem kana");
    }
  }

  JwpText maximum_stem(jwpqt::core::kWnnMaximumKeySize, 0x2424);
  maximum_stem.push_back(0x246b);
  require(jwpqt::core::make_wnn_user_entry(
              maximum_stem, {{0x3021}}, WnnUserInflection::kIchidan)
              .reading == maximum_stem,
          "Maximum-length editable inflected stem was rejected");
}

void test_invalid_new_entries() {
  expect_error([] { jwpqt::core::make_wnn_user_entry({}, {{0x3021}}); },
               "empty editable reading");
  expect_error([] { jwpqt::core::make_wnn_user_entry({0x2422}, {}); },
               "empty editable candidate list");
  expect_error([] { jwpqt::core::make_wnn_user_entry({0x2422}, {{}}); },
               "empty editable candidate");
  expect_error(
      [] {
        jwpqt::core::make_wnn_user_entry(
            {0x2422}, {{0x3021}}, WnnUserInflection::kGodan);
      },
      "one-kana inflected entry");
  expect_error(
      [] {
        jwpqt::core::make_wnn_user_entry(
            {0x2422, 0x2424}, {{0x3021}}, WnnUserInflection::kGodan);
      },
      "invalid godan ending");
  expect_error(
      [] {
        jwpqt::core::make_wnn_user_entry(
            {0x2422, 0x246b}, {{0x3021}}, WnnUserInflection::kIchidan);
      },
      "non-i/e ichidan stem");
  expect_error(
      [] {
        jwpqt::core::make_wnn_user_entry(
            {0x2424, 0x2426}, {{0x3021}}, WnnUserInflection::kIchidan);
      },
      "ichidan without ru");
  expect_error(
      [] {
        jwpqt::core::make_wnn_user_entry(
            {0x2422, 0x2426}, {{0x3021}}, WnnUserInflection::kIAdjective);
      },
      "i-adjective without i");
  expect_error(
      [] {
        jwpqt::core::make_wnn_user_entry(
            {0x2422, 0x242f}, {{0x3021}, {0x3022}},
            WnnUserInflection::kGodan);
      },
      "multiple inflected candidates");
  expect_error(
      [] {
        jwpqt::core::make_wnn_user_entry(
            {0x2422, 0x242f}, {{0x242f}}, WnnUserInflection::kGodan);
      },
      "suffix-only inflected candidate");

  expect_error(
      [] {
        JwpText overlong(jwpqt::core::kWnnMaximumKeySize + 1, 0x2424);
        overlong.push_back(0x246b);
        jwpqt::core::make_wnn_user_entry(
            std::move(overlong), {{0x3021}},
            WnnUserInflection::kIchidan);
      },
      "overlong editable inflected stem");
}

void test_legacy_sort_order() {
  const WnnUserEntry a_second{{0x2422}, '*', {{0x3022}}};
  const WnnUserEntry uninflected_kiru{{0x242d, 0x246b}, '*', {{0x3021}}};
  const WnnUserEntry godan_kiru{{0x242d, 0x246b}, 'r', {{0x3021}}};
  const WnnUserEntry a_first{{0x2422}, '*', {{0x3021}}};
  const WnnUserEntry ichidan_kiru{{0x242d, 0x246b}, '1', {{0x3021}}};
  const std::vector<WnnUserEntry> original{
      a_second, uninflected_kiru, godan_kiru, a_first, ichidan_kiru};

  const std::vector<WnnUserEntry> sorted =
      jwpqt::core::sort_wnn_user_entries(original);
  require(original.front() == a_second,
          "User conversion sort mutated its input copy");
  require(sorted == std::vector<WnnUserEntry>({
                        a_first, a_second, uninflected_kiru, ichidan_kiru,
                        godan_kiru}),
          "User conversions did not follow legacy display ordering");

  const std::vector<WnnUserEntry> duplicates{a_first, a_first};
  require(jwpqt::core::sort_wnn_user_entries(duplicates) == duplicates,
          "User conversion sort did not preserve exact duplicates");

  const WnnUserEntry uninflected_ai{{0x2422, 0x2424}, '*', {{0x3021}}};
  const WnnUserEntry adjective_ai{{0x2422, 0x2424}, 'i', {{0x3021}}};
  require(jwpqt::core::sort_wnn_user_entries(
              {adjective_ai, uninflected_ai}) ==
              std::vector<WnnUserEntry>({uninflected_ai, adjective_ai}),
          "User conversion sort did not apply i-adjective marker ordering");

  const WnnUserEntry slash_row{{0x2422}, '*', {{0x3021}, {0x3023}}};
  const WnnUserEntry inner_punctuation{{0x2422}, '*', {{0x3021, 0x2130}}};
  require(jwpqt::core::sort_wnn_user_entries(
              {slash_row, inner_punctuation}) ==
              std::vector<WnnUserEntry>({inner_punctuation, slash_row}),
          "User conversion sort did not use the recovered slash separator");

  const WnnUserEntry plain_candidate{{0x2422}, '*', {{0x3001}}};
  const WnnUserEntry bracket_candidate{{0x2422}, '*', {{'[', 0x222a}}};
  require(jwpqt::core::sort_wnn_user_entries(
              {plain_candidate, bracket_candidate}) ==
              std::vector<WnnUserEntry>({bracket_candidate,
                                         plain_candidate}),
          "User conversion sort did not reproduce pathological legacy order");

  expect_error(
      [] {
        jwpqt::core::sort_wnn_user_entries(
            {WnnUserEntry{{0x2422}, '?', {{0x3021}}}});
      },
      "invalid sortable entry");

  expect_error(
      [] {
        std::vector<WnnUserEntry> pathological(
            1600,
            WnnUserEntry{{0x2422}, '*', {{0x3021}}});
        jwpqt::core::sort_wnn_user_entries(std::move(pathological));
      },
      "interactive sort work limit");
}

void test_parse_resource_limits() {
  expect_error(
      [] {
        WnnUserDictionary::parse(
            std::string(8U * 1024U * 1024U + 1, 'x'));
      },
      "encoded size limit");

  const std::string record = bytes({0xa2, '*', 0xb0, 0xa1, '\n'});
  std::string too_many_records;
  too_many_records.reserve(
      record.size() * (jwpqt::core::kWnnMaximumRecordCount + 1));
  for (std::size_t index = 0;
       index <= jwpqt::core::kWnnMaximumRecordCount; ++index) {
    too_many_records += record;
  }
  expect_error(
      [&] { WnnUserDictionary::parse(too_many_records); },
      "record limit");

  std::string too_many_candidates = bytes({0xa2, '*'});
  too_many_candidates.reserve(
      3 * (jwpqt::core::kWnnMaximumCandidateCount + 1) + 3);
  for (std::size_t index = 0;
       index <= jwpqt::core::kWnnMaximumCandidateCount; ++index) {
    if (index != 0) {
      too_many_candidates.push_back('/');
    }
    too_many_candidates += bytes({0xb0, 0xa1});
  }
  too_many_candidates.push_back('\n');
  expect_error(
      [&] { WnnUserDictionary::parse(too_many_candidates); },
      "candidate limit");

  std::string too_many_cells = bytes({0xa2, '*'});
  too_many_cells.reserve(
      2 * (jwpqt::core::kWnnMaximumCandidateCells + 1) + 3);
  for (std::size_t index = 0;
       index <= jwpqt::core::kWnnMaximumCandidateCells; ++index) {
    too_many_cells += bytes({0xb0, 0xa1});
  }
  too_many_cells.push_back('\n');
  expect_error([&] { WnnUserDictionary::parse(too_many_cells); },
               "candidate-cell limit");
}

void test_ordered_edit_session() {
  const WnnUserEntry first{{0x2422}, '*', {{0x3021}}};
  const WnnUserEntry second{{0x2424}, '*', {{0x3022}}};
  const WnnUserEntry third{{0x2426}, '*', {{0x3023}}};
  const WnnUserEntry replacement{{0x2428}, '*', {{0x3024}}};
  WnnUserDictionaryEditor editor(
      WnnUserDictionary::from_entries({first, second}));

  require(editor.add(third) == 2 &&
              editor.entries() ==
                  std::vector<WnnUserEntry>({first, second, third}),
          "Edit session did not append an entry");
  require(!editor.move_up(0) && !editor.move_down(2),
          "Edit session moved a boundary entry");
  require(editor.move_up(2) &&
              editor.entries() ==
                  std::vector<WnnUserEntry>({first, third, second}),
          "Edit session did not move an entry up");
  require(editor.move_down(1) &&
              editor.entries() ==
                  std::vector<WnnUserEntry>({first, second, third}),
          "Edit session did not move an entry down");

  editor.replace(1, replacement);
  editor.erase(0);
  require(editor.entries() ==
              std::vector<WnnUserEntry>({replacement, third}) &&
              editor.dictionary().entries() == editor.entries(),
          "Edit session did not replace, erase, or publish its dictionary");

  editor.sort();
  require(editor.entries() ==
              jwpqt::core::sort_wnn_user_entries({replacement, third}),
          "Edit session did not apply portable sort order");
}

void test_edit_session_failures_are_atomic() {
  const WnnUserEntry entry{{0x2422}, '*', {{0x3021}}};
  WnnUserDictionaryEditor editor(WnnUserDictionary::from_entries({entry}));
  const auto require_unchanged = [&] {
    require(editor.entries() == std::vector<WnnUserEntry>({entry}),
            "Failed edit session operation changed live entries");
  };

  expect_error([&] { editor.add({{}, '*', {{0x3022}}}); }, "invalid add");
  require_unchanged();
  expect_error([&] { editor.replace(0, {{0x2424}, '*', {}}); },
               "invalid replacement");
  require_unchanged();
  expect_error([&] { editor.replace(1, entry); },
               "out-of-range replacement");
  require_unchanged();
  expect_error([&] { editor.erase(1); }, "out-of-range erase");
  require_unchanged();
  expect_error([&] { editor.move_up(1); }, "out-of-range move up");
  require_unchanged();
  expect_error([&] { editor.move_down(1); }, "out-of-range move down");
  require_unchanged();

  const std::vector<WnnUserEntry> pathological(
      1600, WnnUserEntry{{0x2422}, '*', {{0x3021}}});
  WnnUserDictionaryEditor large_editor(
      WnnUserDictionary::from_entries(pathological));
  expect_error([&] { large_editor.sort(); }, "edit-session sort work limit");
  require(large_editor.entries() == pathological,
          "Failed edit-session sort changed live entries");
}

}  // namespace

int main() {
  try {
    test_empty_and_ordered_round_trip();
    test_all_inflection_endings();
    test_long_stem_and_imported_inflected_candidates();
    test_invalid_wire_data();
    test_invalid_entry_model();
    test_new_entry_factory();
    test_invalid_new_entries();
    test_legacy_sort_order();
    test_parse_resource_limits();
    test_ordered_edit_session();
    test_edit_session_failures_are_atomic();
  } catch (const std::exception& error) {
    std::cerr << "wnn_user_dictionary_test: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
