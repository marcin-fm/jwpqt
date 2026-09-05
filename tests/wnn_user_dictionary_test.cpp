// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/wnn_user_dictionary.h"

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
using jwpqt::core::WnnUserDictionary;
using jwpqt::core::WnnUserDictionaryError;
using jwpqt::core::WnnUserEntry;

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

}  // namespace

int main() {
  try {
    test_empty_and_ordered_round_trip();
    test_all_inflection_endings();
    test_long_stem_and_imported_inflected_candidates();
    test_invalid_wire_data();
    test_invalid_entry_model();
    test_parse_resource_limits();
  } catch (const std::exception& error) {
    std::cerr << "wnn_user_dictionary_test: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
