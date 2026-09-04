// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "jwpqt/core/jis_unicode.h"

namespace {

using jwpqt::core::JisCode;

void require(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

void require_mapping(JisCode jis, char32_t unicode) {
  require(jwpqt::core::jis_x0208_to_unicode(jis) == unicode,
          "JIS-to-Unicode mapping did not match");
  require(jwpqt::core::unicode_to_jis_x0208(unicode).has_value(),
          "Unicode-to-JIS mapping was missing");
  const JisCode reverse = *jwpqt::core::unicode_to_jis_x0208(unicode);
  require(reverse == jis, "Unicode-to-JIS canonical mapping did not match");
}

void test_representative_mappings() {
  require_mapping(0x2121, U'\u3000');  // Ideographic space.
  require_mapping(0x2330, U'\uff10');  // Fullwidth zero.
  require_mapping(0x2422, U'\u3042');  // あ.
  require_mapping(0x2522, U'\u30a2');  // ア.
  require_mapping(0x2621, U'\u0391');  // Greek alpha.
  require_mapping(0x2727, U'\u0401');  // Cyrillic IO.
  require_mapping(0x467c, U'\u65e5');  // 日.
  require_mapping(0x4b5c, U'\u672c');  // 本.
  require_mapping(0x386c, U'\u8a9e');  // 語.
}

void test_table_boundaries() {
  require_mapping(0x217e, U'\u25c7');
  require_mapping(0x2221, U'\u25c6');
  require_mapping(0x227e, U'\u25ef');
  require_mapping(0x2821, U'\u2500');
  require_mapping(0x2840, U'\u2542');
  require_mapping(0x3021, U'\u4e9c');
  require_mapping(0x307e, U'\u852d');
  require_mapping(0x3121, U'\u9662');
  require_mapping(0x737e, U'\u9fa0');
  require_mapping(0x7421, U'\u582f');
  require_mapping(0x7426, U'\u7199');
}

void test_formula_boundaries_and_holes() {
  require_mapping(0x237a, U'\uff5a');
  require_mapping(0x2421, U'\u3041');
  require_mapping(0x2473, U'\u3093');
  require_mapping(0x2521, U'\u30a1');
  require_mapping(0x2576, U'\u30f6');
  require_mapping(0x2631, U'\u03a1');
  require_mapping(0x2632, U'\u03a3');
  require_mapping(0x2638, U'\u03a9');
  require_mapping(0x2641, U'\u03b1');
  require_mapping(0x2651, U'\u03c1');
  require_mapping(0x2652, U'\u03c3');
  require_mapping(0x2658, U'\u03c9');
  require_mapping(0x2726, U'\u0415');
  require_mapping(0x2728, U'\u0416');
  require_mapping(0x2741, U'\u042f');
  require_mapping(0x2751, U'\u0430');
  require_mapping(0x2758, U'\u0436');
  require_mapping(0x2771, U'\u044f');

  require(!jwpqt::core::jis_x0208_to_unicode(0x237b),
          "Code beyond the fullwidth range was mapped");
  require(!jwpqt::core::jis_x0208_to_unicode(0x2474),
          "Code beyond the hiragana range was mapped");
  require(!jwpqt::core::jis_x0208_to_unicode(0x2639),
          "Greek row gap was mapped");
  require(!jwpqt::core::jis_x0208_to_unicode(0x2640),
          "Greek row gap was mapped");
  require(!jwpqt::core::unicode_to_jis_x0208(U'\u03a2'),
          "Unassigned Greek capital was mapped");
  require(!jwpqt::core::unicode_to_jis_x0208(U'\u03c2'),
          "Greek final sigma was mapped");
}

void test_every_legacy_mapping_is_stable() {
  std::size_t mapped = 0;
  for (std::uint16_t row = 0x21; row <= 0x7e; ++row) {
    for (std::uint16_t cell = 0x21; cell <= 0x7e; ++cell) {
      const JisCode jis = static_cast<JisCode>((row << 8U) | cell);
      const std::optional<char32_t> unicode =
          jwpqt::core::jis_x0208_to_unicode(jis);
      if (!unicode) {
        continue;
      }
      ++mapped;
      const std::optional<JisCode> reverse =
          jwpqt::core::unicode_to_jis_x0208(*unicode);
      require(reverse.has_value(), "Mapped Unicode character had no inverse");
      require(jwpqt::core::jis_x0208_to_unicode(*reverse) == unicode,
              "Duplicate mapping did not resolve to a stable character");
    }
  }
  require(mapped == 6892, "Unexpected number of legacy JIS mappings");
}

void test_first_mapping_wins() {
  struct DuplicateFixture {
    char32_t unicode;
    JisCode canonical_jis;
  };

  // JWPxp scans its symbol table before the contiguous fullwidth row.
  constexpr std::array<DuplicateFixture, 12> fixtures = {{
      {U'\uff1a', 0x2127}, {U'\uff1b', 0x2128}, {U'\uff1c', 0x2163},
      {U'\uff1d', 0x2161}, {U'\uff1e', 0x2164}, {U'\uff1f', 0x2129},
      {U'\uff20', 0x2177}, {U'\uff3b', 0x214e}, {U'\uff3d', 0x214f},
      {U'\uff3e', 0x2130}, {U'\uff3f', 0x2132}, {U'\uff40', 0x212e},
  }};
  for (const DuplicateFixture& fixture : fixtures) {
    require(jwpqt::core::unicode_to_jis_x0208(fixture.unicode) ==
                fixture.canonical_jis,
            "Duplicate mapping did not retain legacy first-match behavior");
  }
}

void test_unmapped_values() {
  require(!jwpqt::core::jis_x0208_to_unicode(0x2021),
          "Structurally invalid JIS code was mapped");
  require(!jwpqt::core::jis_x0208_to_unicode(0x2921),
          "Unassigned JIS pair was mapped");
  require(!jwpqt::core::jis_x0208_to_unicode(0x7427),
          "Code beyond the legacy kanji table was mapped");
  require(!jwpqt::core::jis_x0208_to_unicode(0x4f54),
          "Zero entry in the kanji table was mapped");
  require(!jwpqt::core::jis_x0208_to_unicode(0x222f),
          "Zero entry in the symbol table was mapped");
  require(!jwpqt::core::unicode_to_jis_x0208(U'\0'),
          "Unicode NUL was mapped through a table sentinel");
  require(!jwpqt::core::unicode_to_jis_x0208(U'\U0001f5fe'),
          "Non-JIS Unicode character was mapped");
  require(!jwpqt::core::unicode_to_jis_x0208(U'\uff71'),
          "Halfwidth katakana was mapped by the fullwidth-only layer");
}

}  // namespace

int main() {
  try {
    test_representative_mappings();
    test_table_boundaries();
    test_formula_boundaries_and_holes();
    test_every_legacy_mapping_is_stable();
    test_first_mapping_wins();
    test_unmapped_values();
    std::cout << "All JIS Unicode tests passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
