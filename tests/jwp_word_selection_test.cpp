// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/jwp_word_selection.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {

using jwpqt::core::JwpText;
using jwpqt::core::JwpWordRange;

void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

void require_range(JwpWordRange range, std::size_t begin, std::size_t end,
                   const char* message) {
  require(range.begin == begin && range.end == end, message);
}

void test_source_classes() {
  const JwpText text{'a', 'b', '_', '1', '!', '?', ' ', ' ', 'x', 'y'};
  require_range(jwpqt::core::select_jwp_word(text, 2), 0, 4,
                "ASCII word selection is wrong");
  require_range(jwpqt::core::select_jwp_word(text, 4), 4, 6,
                "ASCII punctuation selection is wrong");
  require_range(jwpqt::core::select_jwp_word(text, 6), 8, 10,
                "Whitespace did not advance to the next word");
  require_range(jwpqt::core::select_jwp_word(text, text.size()), 8, 10,
                "End-of-line selection is wrong");
  require_range(jwpqt::core::select_jwp_word(text, 2, true), 2, 4,
                "Preceding selection did not keep its active endpoint");

  const JwpText raw{0x80, 0x81, 0x2341, 0x2342, 0x2121, 0x3021};
  require_range(jwpqt::core::select_jwp_word(raw, 1), 0, 2,
                "Raw high-byte punctuation selection is wrong");
  require_range(jwpqt::core::select_jwp_word(raw, 2), 2, 4,
                "JASCII selection is wrong");
  require_range(jwpqt::core::select_jwp_word(raw, 4), 5, 6,
                "Japanese space did not advance to kanji");
}

void test_source_leniencies() {
  const JwpText katakana{0x2522, 0x213c, 0x2523, 0x2133};
  require_range(jwpqt::core::select_jwp_word(katakana, 2), 0, 4,
                "Katakana leniencies were not selected together");

  const JwpText kanji{0x3021, 0x2139, 0x3022};
  require_range(jwpqt::core::select_jwp_word(kanji, 1), 0, 3,
                "Kanji repetition mark was not selected with kanji");

  const JwpText hiragana{0x2422, 0x2135, 0x2423, 0x2472, 0x2424};
  require_range(jwpqt::core::select_jwp_word(hiragana, 1), 0, 3,
                "Hiragana repetition mark was not selected with hiragana");
  require_range(jwpqt::core::select_jwp_word(hiragana, 3), 3, 4,
                "Hiragana wo did not remain a word boundary");
}

void test_boundaries() {
  require_range(jwpqt::core::select_jwp_word({}, 0), 0, 0,
                "Empty text selection is wrong");
  bool threw = false;
  try {
    (void)jwpqt::core::select_jwp_word(JwpText{'a'}, 2);
  } catch (const std::out_of_range&) {
    threw = true;
  }
  require(threw, "Out-of-range cursor was accepted");
}

}  // namespace

int main() {
  try {
    test_source_classes();
    test_source_leniencies();
    test_boundaries();
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
