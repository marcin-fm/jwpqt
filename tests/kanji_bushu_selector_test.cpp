// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include "jwpqt/core/kanji_bushu_selector.h"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

void test_full_and_reduced_sets() {
  const auto full = jwpqt::core::kanji_bushu_choices(0, true);
  const auto reduced = jwpqt::core::kanji_bushu_choices(0, false);
  require(full.size() == 241 && reduced.size() == 211 &&
              full.front() == jwpqt::core::KanjiBushuChoice{0, 1} &&
              full.back() == jwpqt::core::KanjiBushuChoice{240, 214},
          "Bushu full/reduced selector sizes or endpoints are wrong");
  require(std::none_of(reduced.begin(), reduced.end(), [](const auto& choice) {
            return choice.bushu == 255;
          }),
          "Reduced Bushu choices retained the source sentinel radical");
}

void test_stroke_groups_and_variants() {
  const auto stroke_two = jwpqt::core::kanji_bushu_choices(2, true);
  require(stroke_two.size() == 28 && stroke_two.front().sprite_index == 6 &&
              stroke_two.back().sprite_index == 33 &&
              stroke_two[2].bushu == 9 && stroke_two[3].bushu == 9,
          "Two-stroke Bushu variants are wrong");
  const auto reduced_two = jwpqt::core::kanji_bushu_choices(2, false);
  require(reduced_two.size() == 23 &&
              reduced_two[2] == jwpqt::core::KanjiBushuChoice{8, 9} &&
              reduced_two[3] == jwpqt::core::KanjiBushuChoice{11, 10},
          "Two-stroke reduced Bushu choices are not canonical");
  const auto stroke_four = jwpqt::core::kanji_bushu_choices(4, true);
  const auto reduced_four = jwpqt::core::kanji_bushu_choices(4, false);
  require(stroke_four.size() == 43 && stroke_four[1].bushu == 255 &&
              reduced_four.size() == 34 && reduced_four.front().bushu == 61 &&
              reduced_four[1].bushu == 62,
          "Four-stroke sentinel or reduced choices are wrong");
  const auto stroke_seventeen = jwpqt::core::kanji_bushu_choices(17, false);
  require(stroke_seventeen ==
              std::vector<jwpqt::core::KanjiBushuChoice>{{239, 213}},
          "Recovered seventeen-stroke reduced Bushu choice is wrong");
  const auto stroke_seventeen_variants =
      jwpqt::core::kanji_bushu_choices(17, true);
  require(stroke_seventeen_variants ==
              std::vector<jwpqt::core::KanjiBushuChoice>{{240, 214}},
          "Seventeen-stroke variant Bushu choice is wrong");
}

void test_invalid_strokes() {
  try {
    (void)jwpqt::core::kanji_bushu_choices(18, true);
  } catch (const std::invalid_argument&) {
    return;
  }
  require(false, "Out-of-range Bushu stroke count was accepted");
}

}  // namespace

int main() {
  test_full_and_reduced_sets();
  test_stroke_groups_and_variants();
  test_invalid_strokes();
  return EXIT_SUCCESS;
}
