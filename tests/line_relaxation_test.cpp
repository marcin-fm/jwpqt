// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/line_relaxation.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

using jwpqt::core::JisCode;
using jwpqt::core::JwpText;
using jwpqt::core::LineRelaxationOptions;

void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

void test_character_sets() {
  LineRelaxationOptions options;
  for (const JisCode code : {JisCode{0x2122}, JisCode{0x2123}, JisCode{0x214b},
                             JisCode{0x2157}, JisCode{0x2159}}) {
    require(jwpqt::core::is_relaxable_margin_character(code, options),
            "Source punctuation was not relaxable");
  }
  for (const std::uint8_t cell :
       {0x21, 0x23, 0x25, 0x27, 0x29, 0x43, 0x63, 0x65, 0x67}) {
    require(jwpqt::core::is_relaxable_margin_character(
                static_cast<JisCode>(0x2400U | cell), options) &&
                jwpqt::core::is_relaxable_margin_character(
                    static_cast<JisCode>(0x2500U | cell), options),
            "Source small kana was not relaxable");
  }
  require(!jwpqt::core::is_relaxable_margin_character(0x2124, options) &&
              !jwpqt::core::is_relaxable_margin_character(0x2422, options) &&
              !jwpqt::core::is_relaxable_margin_character(0x22, options),
          "Non-source or raw-byte character was relaxable");
  options.punctuation = false;
  require(!jwpqt::core::is_relaxable_margin_character(0x2122, options),
          "Disabled punctuation remained relaxable");
  options.small_kana = false;
  require(!jwpqt::core::is_relaxable_margin_character(0x2421, options),
          "Disabled small kana remained relaxable");
}

void test_source_line_planning() {
  const JwpText text = {0x467c, 0x467c, 0x467c, 0x467c, 0x2123,
                        0x467c, 0x467c, 0x467c, 0x467c, 0x2463};
  const std::vector<std::int64_t> advances(text.size(), 10);
  LineRelaxationOptions options;
  options.first_line_width = 40;
  options.continuation_line_width = 40;
  options.jis_advance = 10;
  require(jwpqt::core::plan_line_relaxation(text, advances, options) ==
              std::vector<std::size_t>({4, 9}),
          "Planner did not retain exactly one source character per line");

  options.punctuation = false;
  require(jwpqt::core::plan_line_relaxation(text, advances, options).empty(),
          "Disabled punctuation did not restore source line wrapping");
  const JwpText small_text = {0x467c, 0x467c, 0x467c, 0x467c, 0x2463};
  require(jwpqt::core::plan_line_relaxation(
              small_text, std::vector<std::int64_t>(small_text.size(), 10),
              options) == std::vector<std::size_t>({4}),
          "Small-kana policy was not independently applied");
  options.small_kana = false;
  require(jwpqt::core::plan_line_relaxation(text, advances, options).empty(),
          "Disabled policies still relaxed characters");

  options.punctuation = true;
  const JwpText consecutive = {0x467c, 0x467c, 0x467c, 0x467c,
                               0x2123, 0x2122};
  require(jwpqt::core::plan_line_relaxation(
              consecutive,
              std::vector<std::int64_t>(consecutive.size(), 10), options) ==
              std::vector<std::size_t>({4}),
          "Planner relaxed more than one character on a source line");
}

void test_words_spaces_and_tabs() {
  const JwpText text = {'a', 'b', 'c', ' ', 0x467c, 0x467c, 0x2123,
                        '\t', 0x467c, 0x467c, 0x2122};
  const std::vector<std::int64_t> advances = {4, 4, 4, 3, 10, 10, 10,
                                               1, 10, 10, 10};
  LineRelaxationOptions options;
  options.first_line_width = 19;
  options.continuation_line_width = 19;
  options.jis_advance = 10;
  const auto positions =
      jwpqt::core::plan_line_relaxation(text, advances, options);
  require(positions == std::vector<std::size_t>({6, 10}),
          "ASCII word/space/tab planning lost source line boundaries");
}

void test_validation() {
  LineRelaxationOptions options;
  bool rejected = false;
  try {
    (void)jwpqt::core::plan_line_relaxation({0x2123}, {}, options);
  } catch (const std::invalid_argument&) { rejected = true; }
  require(rejected, "Mismatched advances were accepted");

  rejected = false;
  try {
    (void)jwpqt::core::plan_line_relaxation({0x2123}, {0}, options);
  } catch (const std::invalid_argument&) { rejected = true; }
  require(rejected, "Nonpositive advance was accepted");

  options.max_characters = 0;
  rejected = false;
  try {
    (void)jwpqt::core::plan_line_relaxation({0x2123}, {1}, options);
  } catch (const std::length_error&) { rejected = true; }
  require(rejected, "Character bound was not enforced");
}

}  // namespace

int main() {
  try {
    test_character_sets();
    test_source_line_planning();
    test_words_spaces_and_tabs();
    test_validation();
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
