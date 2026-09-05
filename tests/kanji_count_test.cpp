// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <iostream>

#include "jwpqt/core/kanji_count.h"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

template <typename Function>
void require_error(Function&& function, const char* message) {
  try {
    function();
  } catch (const jwpqt::core::KanjiCountError&) {
    return;
  }
  require(false, message);
}

jwpqt::core::JwpDocument first_document() {
  jwpqt::core::JwpDocument document;
  jwpqt::core::JwpParagraph paragraph;
  paragraph.text = {'A', 0x2422U, 0x2522U, 0x213cU, 0x2341U,
                    0x3021U, 0x3021U, 0x3022U, 0x2121U};
  document.paragraphs.push_back(std::move(paragraph));
  return document;
}

void test_counts_and_order() {
  const auto document = first_document();
  jwpqt::core::KanjiColorList color_list;
  color_list.add(0x3021U);
  const auto report =
      jwpqt::core::count_kanji({&document}, color_list);
  const auto& summary = report.summary;
  require(summary.total == 9 && summary.hiragana == 1 &&
              summary.katakana == 2 && summary.ascii == 1 &&
              summary.jascii == 1 && summary.other == 1,
          "Raw JWP character classes were counted incorrectly");
  require(summary.kanji_on_color_list == 2 &&
              summary.kanji_off_color_list == 1 &&
              summary.unique_kanji_on_color_list == 1 &&
              summary.unique_kanji_off_color_list == 1,
          "Kanji color-list totals were counted incorrectly");
  require(report.entries.size() == 2 &&
              report.entries[0].code == 0x3021U &&
              report.entries[0].count == 2 &&
              report.entries[0].on_color_list &&
              report.entries[1].code == 0x3022U,
          "Kanji frequencies were not ordered or classified correctly");
}

void test_multiple_documents_and_filters() {
  const auto first = first_document();
  jwpqt::core::JwpDocument second;
  second.paragraphs.push_back({});
  second.paragraphs[0].text = {0x3022U, 0x3023U};
  jwpqt::core::KanjiColorList color_list;
  color_list.add(0x3021U);

  auto report = jwpqt::core::count_kanji(
      {&first, &second}, color_list,
      jwpqt::core::KanjiCountFilter::kColorListOnly);
  require(report.summary.total == 11 && report.entries.size() == 1 &&
              report.entries[0].code == 0x3021U,
          "All-document color-list filtering was incorrect");
  report = jwpqt::core::count_kanji(
      {&first, &second}, color_list,
      jwpqt::core::KanjiCountFilter::kExcludeColorList);
  require(report.entries.size() == 2 && report.entries[0].code == 0x3022U &&
              report.entries[0].count == 2 &&
              report.entries[1].code == 0x3023U,
          "Color-list exclusion changed count order or membership");
}

void test_validation_and_limits() {
  const auto document = first_document();
  jwpqt::core::KanjiColorList color_list;
  jwpqt::core::KanjiCountLimits limits;
  limits.characters = 1;
  require_error(
      [&] { (void)jwpqt::core::count_kanji({&document}, color_list,
                                           jwpqt::core::KanjiCountFilter::kAll,
                                           limits); },
      "Character budget was not enforced");
  limits = {};
  limits.results = 1;
  const auto truncated = jwpqt::core::count_kanji(
      {&document}, color_list, jwpqt::core::KanjiCountFilter::kAll, limits);
  require(truncated.entries.size() == 1 && truncated.truncated &&
              truncated.entries[0].code == 0x3021U &&
              truncated.entries[0].count == 2,
          "Result truncation did not retain the most frequent kanji");
  require_error(
      [&] { (void)jwpqt::core::count_kanji({nullptr}, color_list); },
      "Null document was accepted");
  require_error(
      [&] {
        (void)jwpqt::core::count_kanji(
            {&document}, color_list,
            static_cast<jwpqt::core::KanjiCountFilter>(99));
      },
      "Invalid count filter was accepted");
}

}  // namespace

int main() {
  test_counts_and_order();
  test_multiple_documents_and_filters();
  test_validation_and_limits();
  return EXIT_SUCCESS;
}
