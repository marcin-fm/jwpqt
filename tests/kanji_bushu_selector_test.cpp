// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include "jwpqt/core/kanji_bushu_selector.h"
#include "jwpqt/core/kanji_spahn_selector.h"

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

void test_spahn_choices() {
  using jwpqt::core::kanji_spahn_choices;
  const auto all = kanji_spahn_choices(0, true);
  const auto reduced = kanji_spahn_choices(0, false);
  require(all.size() == 116 && reduced.size() == 79 &&
              all.back().sprite_index == 115 && all.back().radical_strokes == 11 &&
              all.back().radical == 1 && reduced.back().sprite_index == 115,
          "Spahn selector omitted a recovered variant or canonical radical");
  constexpr std::size_t counts[]{0, 0, 30, 27, 22, 10, 7, 6, 10, 1, 1, 2};
  constexpr std::size_t canonical_counts[]{0, 0, 19, 18, 13, 9, 6, 5, 5, 1, 1, 2};
  for (std::uint8_t strokes = 1; strokes <= 11; ++strokes) {
    const auto variants = kanji_spahn_choices(strokes, true);
    const auto canonical = kanji_spahn_choices(strokes, false);
    require(variants.size() == counts[strokes] && canonical.size() == canonical_counts[strokes],
            "Spahn stroke group sizes are wrong");
    for (const auto& choice : variants) {
      require(choice.radical_strokes == strokes && choice.sprite_index < all.size() &&
                  choice.radical < 20 && choice.radical != 11 &&
                  choice.radical == all[choice.sprite_index].radical,
              "Spahn variant has an invalid stroke group, sprite or letter");
      require(std::count_if(canonical.begin(), canonical.end(), [&](const auto& entry) {
                return entry.radical == choice.radical;
              }) == 1,
              "Spahn variant does not have exactly one canonical radical");
    }
    for (std::size_t i = 0; i < canonical.size(); ++i)
      require(canonical[i].radical == (i < 11 ? i : i + 1),
              "Spahn radical letters did not skip l");
  }
  try { (void)kanji_spahn_choices(12, true); }
  catch (const std::invalid_argument&) { return; }
  require(false, "Out-of-range Spahn selector strokes were accepted");
}

}  // namespace

int main() {
  test_full_and_reduced_sets();
  test_stroke_groups_and_variants();
  test_invalid_strokes();
  test_spahn_choices();
  return EXIT_SUCCESS;
}
