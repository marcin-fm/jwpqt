// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <iostream>

#include "jwpqt/core/jis_table.h"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

void test_descriptions() {
  const auto entry = jwpqt::core::describe_jis_character(0x2422U);
  require(entry.has_value() && entry->unicode == U'\u3042' &&
              entry->euc.lead == 0xa4U && entry->euc.trail == 0xa2U &&
              entry->shift_jis.lead == 0x82U &&
              entry->shift_jis.trail == 0xa0U,
          "JIS table descriptor returned wrong encodings");
  require(jwpqt::core::describe_unicode_character(U'\u3042') == entry,
          "Unicode JIS table lookup did not round trip");
  require(!jwpqt::core::describe_jis_character(0x222fU).has_value() &&
              !jwpqt::core::describe_unicode_character(U'\U0001f600')
                   .has_value(),
          "JIS table accepted an unassigned character");
}

void test_pages() {
  const auto hiragana = jwpqt::core::jis_table_page(0x24U);
  require(!hiragana.empty() && hiragana.front().jis == 0x2421U,
          "JIS table page omitted assigned hiragana");
  for (std::size_t index = 1; index < hiragana.size(); ++index)
    require(hiragana[index - 1].jis < hiragana[index].jis,
            "JIS table page is not ordered by cell");
  require(jwpqt::core::jis_table_page(0x20U).empty() &&
              jwpqt::core::jis_table_page(0x75U).empty(),
          "JIS table accepted a page outside the browsable range");
}

}  // namespace

int main() {
  test_descriptions();
  test_pages();
  return EXIT_SUCCESS;
}
