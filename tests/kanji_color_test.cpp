#include "jwpqt/core/kanji_color.h"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>

namespace {

using jwpqt::core::JisCode;
using jwpqt::core::KanjiColorPolicy;
using jwpqt::core::KanjiListColorMode;
using jwpqt::core::RgbColor;
using jwpqt::core::decode_legacy_color_ref;
using jwpqt::core::kanji_color_list_index;
using jwpqt::core::kanji_foreground_color;

constexpr RgbColor kList{1, 2, 3};
constexpr RgbColor kUncommon{4, 5, 6};

void expect(bool condition, std::string_view message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

void expect_color(const std::optional<RgbColor>& actual,
                  std::optional<RgbColor> expected,
                  std::string_view message) {
  expect(actual == expected, message);
}

KanjiColorPolicy policy(KanjiListColorMode mode,
                        bool colorize_uncommon = true) {
  return {mode, kList, colorize_uncommon, kUncommon};
}

void test_legacy_list_indexing() {
  expect(!kanji_color_list_index(0x3020).has_value(),
         "code below list base was indexable");
  expect(kanji_color_list_index(0x3021) == 0,
         "first list code has wrong index");
  expect(kanji_color_list_index(0x307e) == 93,
         "last cell in first row has wrong index");
  expect(kanji_color_list_index(0x3121) == 94,
         "first cell in second row has wrong index");
  expect(kanji_color_list_index(0x745b) == 6450,
         "last legacy list code has wrong index");
  expect(!kanji_color_list_index(0x745c).has_value(),
         "code above legacy list capacity was indexable");

  // The source subtracts 0x3021 and folds bytes arithmetically, so malformed
  // 0x307f aliases canonical 0x3121. Preserve that wire-visible behavior.
  expect(kanji_color_list_index(0x307f) ==
             kanji_color_list_index(0x3121),
         "legacy malformed-cell alias changed");
}

void test_legacy_color_decoding() {
  expect(decode_legacy_color_ref(0x00332211U, kUncommon) ==
             RgbColor{0x11, 0x22, 0x33},
         "COLORREF byte order was decoded incorrectly");
  expect(decode_legacy_color_ref(0x01000000U, kUncommon) == kUncommon,
         "special COLORREF did not use fallback");
}

void test_excluded_character_classes() {
  const KanjiColorPolicy colors = policy(KanjiListColorMode::kNoMatch);
  for (JisCode code : {JisCode{'A'}, JisCode{0x2341}, JisCode{0x2422},
                       JisCode{0x2522}, JisCode{0x2621}, JisCode{0x2721},
                       JisCode{0x2fff}}) {
    expect_color(kanji_foreground_color(code, false, colors), std::nullopt,
                 "non-kanji token was colorized");
  }
}

void test_match_mode_precedence() {
  const KanjiColorPolicy colors = policy(KanjiListColorMode::kMatch);
  expect_color(kanji_foreground_color(0x3021, true, colors), kList,
               "listed common kanji did not use list color");
  expect_color(kanji_foreground_color(0x3021, false, colors), std::nullopt,
               "unlisted common kanji was colorized in match mode");
  expect_color(kanji_foreground_color(0x5021, true, colors), kList,
               "list color did not precede uncommon color");
  expect_color(kanji_foreground_color(0x5021, false, colors), kUncommon,
               "unlisted uncommon kanji did not use uncommon color");
}

void test_no_match_mode_precedence() {
  const KanjiColorPolicy colors = policy(KanjiListColorMode::kNoMatch);
  expect_color(kanji_foreground_color(0x3021, false, colors), kList,
               "unlisted common kanji did not use non-match color");
  expect_color(kanji_foreground_color(0x3021, true, colors), std::nullopt,
               "listed common kanji was colorized in non-match mode");
  expect_color(kanji_foreground_color(0x5021, false, colors), kList,
               "non-match color did not precede uncommon color");
  expect_color(kanji_foreground_color(0x5021, true, colors), kUncommon,
               "listed uncommon kanji did not use uncommon color");
}

void test_off_and_non_indexable_behavior() {
  expect_color(kanji_foreground_color(
                   0x5021, true, policy(KanjiListColorMode::kOff)),
               kUncommon, "off mode suppressed uncommon color");
  expect_color(kanji_foreground_color(
                   0x5021, false,
                   policy(KanjiListColorMode::kOff, false)),
               std::nullopt, "disabled uncommon color was still applied");

  expect_color(kanji_foreground_color(
                   0x8000, true, policy(KanjiListColorMode::kMatch)),
               kUncommon,
               "non-indexable raw kanji was treated as a list member");
  expect_color(kanji_foreground_color(
                   0x8000, true, policy(KanjiListColorMode::kNoMatch)),
               kList,
               "non-indexable raw kanji was not treated as a non-member");

  KanjiColorPolicy unknown_mode =
      policy(static_cast<KanjiListColorMode>(3));
  expect_color(kanji_foreground_color(0x3021, false, unknown_mode), kList,
               "unknown nonzero mode did not follow legacy non-match mode");
  expect_color(kanji_foreground_color(0x3021, true, unknown_mode),
               std::nullopt,
               "unknown nonzero mode colorized a legacy list member");
}

}  // namespace

int main() {
  test_legacy_list_indexing();
  test_legacy_color_decoding();
  test_excluded_character_classes();
  test_match_mode_precedence();
  test_no_match_mode_precedence();
  test_off_and_non_indexable_behavior();
  std::cout << "kanji color tests passed\n";
  return 0;
}
