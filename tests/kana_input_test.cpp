// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/kana_input.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using jwpqt::core::JisCode;
using jwpqt::core::KanaInputComposer;
using jwpqt::core::KanaInputEvent;
using jwpqt::core::KanaInputKind;

void require(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

std::vector<KanaInputEvent> type(KanaInputComposer& composer,
                                 const std::string& input) {
  std::vector<KanaInputEvent> result;
  for (const char value : input) {
    auto events = composer.push_ascii(value);
    result.insert(result.end(), events.begin(), events.end());
  }
  return result;
}

KanaInputEvent event(KanaInputKind kind,
                     std::initializer_list<JisCode> text) {
  return {kind, jwpqt::core::JwpText(text)};
}

void test_direct_case_semantics() {
  KanaInputComposer composer;
  require(type(composer, "ka") ==
              std::vector{event(KanaInputKind::kKanaContinue, {0x242b})},
          "lowercase direct kana mismatch");
  require(type(composer, "KA") ==
              std::vector{event(KanaInputKind::kText, {0x252b})},
          "uppercase direct kana mismatch");
  require(type(composer, "Ka") ==
              std::vector{event(KanaInputKind::kKanaStart, {0x242b})},
          "mixed-case start mismatch");
  require(type(composer, "kA") ==
              std::vector{event(KanaInputKind::kKanaContinue, {0x242b})},
          "mixed-case continuation mismatch");
}

void test_compounds_and_aliases() {
  KanaInputComposer composer;
  require(type(composer, "sha") ==
              std::vector({event(KanaInputKind::kKanaContinue, {0x2437}),
                           event(KanaInputKind::kKanaContinue, {0x2463})}),
          "sha mismatch");
  require(type(composer, "ci") ==
              std::vector{event(KanaInputKind::kKanaContinue, {0x2441})},
          "ci alias mismatch");
  require(type(composer, "dji") ==
              std::vector{event(KanaInputKind::kKanaContinue, {0x2442})},
          "dji alias mismatch");
  require(type(composer, "kya") ==
              std::vector({event(KanaInputKind::kKanaContinue, {0x242d}),
                           event(KanaInputKind::kKanaContinue, {0x2463})}),
          "regular y compound mismatch");
}

void test_pending_input() {
  KanaInputComposer composer;
  require(composer.push_ascii('n').empty() && composer.pending(),
          "lowercase n was not pending");
  require(composer.pending_ambiguous(),
          "lowercase n was not treated as ambiguous");
  require(composer.flush() ==
              std::vector{event(KanaInputKind::kKanaContinue, {0x2473})},
          "lowercase n flush mismatch");

  require(composer.push_ascii('N').empty() && composer.pending_ambiguous(),
          "uppercase N ambiguity mismatch");
  require(composer.flush() ==
              std::vector{event(KanaInputKind::kText, {0x2573})},
          "uppercase N flush mismatch");
}

void test_vowel_quote_modes() {
  KanaInputComposer composer;
  require(composer.push_ascii('A').empty(), "uppercase vowel was not pending");
  require(composer.push_ascii('\'') ==
              std::vector{event(KanaInputKind::kText, {0x2522})},
          "modern katakana-vowel quote mismatch");

  KanaInputComposer old({true});
  require(old.push_ascii('A').empty(), "old-mode vowel was not pending");
  require(old.push_ascii('\'') ==
              std::vector({event(KanaInputKind::kText, {0x2522}),
                           event(KanaInputKind::kText, {0x2157})}),
          "old katakana-vowel quote mismatch");
}

void test_doubled_consonants_and_assimilation() {
  KanaInputComposer composer;
  require(type(composer, "kka") ==
              std::vector({event(KanaInputKind::kKanaContinue, {0x2443}),
                           event(KanaInputKind::kKanaContinue, {0x242b})}),
          "lowercase doubled consonant mismatch");
  require(type(composer, "KKA") ==
              std::vector({event(KanaInputKind::kText, {0x2543}),
                           event(KanaInputKind::kText, {0x252b})}),
          "uppercase doubled consonant mismatch");
  require(type(composer, "mpa") ==
              std::vector({event(KanaInputKind::kKanaContinue, {0x2473}),
                           event(KanaInputKind::kKanaContinue, {0x2451})}),
          "m assimilation mismatch");
}

void test_symbols_and_invalid_input() {
  KanaInputComposer composer;
  require(type(composer, "vu") ==
              std::vector{event(KanaInputKind::kText, {0x2574})},
          "vu mismatch");
  require(type(composer, "^#") ==
              std::vector{event(KanaInputKind::kText, {0x2139})},
          "complex symbol mismatch");
  require(composer.push_ascii('!') ==
              std::vector{event(KanaInputKind::kText, {0x212a})},
          "JASCII punctuation mismatch");
  require(composer.push_ascii('x') ==
              std::vector{event(KanaInputKind::kText, {0x215f})},
          "special x mismatch");
}

void test_state_replay_and_rejection() {
  KanaInputComposer composer;
  require(composer.push_ascii('k').empty(), "partial k unexpectedly emitted");
  require(composer.push_ascii('n').empty() && composer.pending(),
          "reserved input was not replayed from the base state");
  require(composer.flush() ==
              std::vector{event(KanaInputKind::kKanaContinue, {0x2473})},
          "replayed n mismatch");

  require(composer.push_ascii('k').empty(), "partial k unexpectedly emitted");
  bool incomplete_threw = false;
  try {
    static_cast<void>(composer.flush());
  } catch (const jwpqt::core::KanaInputError&) {
    incomplete_threw = true;
  }
  require(incomplete_threw && composer.pending(),
          "incomplete flush did not retain input");
  require(composer.push_ascii('a') ==
              std::vector{event(KanaInputKind::kKanaContinue, {0x242b})},
          "retained input did not resume");

  bool invalid_threw = false;
  try {
    static_cast<void>(composer.push_ascii(static_cast<char>(0x80)));
  } catch (const jwpqt::core::KanaInputError&) {
    invalid_threw = true;
  }
  require(invalid_threw && !composer.pending(),
          "high-bit input rejection mutated state");
}

void test_discard_and_options() {
  KanaInputComposer composer;
  require(!composer.discard(), "empty discard reported input");
  static_cast<void>(composer.push_ascii('A'));
  bool option_threw = false;
  try {
    composer.set_options({true});
  } catch (const jwpqt::core::KanaInputError&) {
    option_threw = true;
  }
  require(option_threw && composer.pending(),
          "option change did not preserve composition");
  require(composer.discard() && !composer.pending(),
          "discard did not clear composition");
  composer.set_options({true});
}

void test_jascii_mode_mapping() {
  using jwpqt::core::ascii_to_jascii;
  for (int value = 0; value <= 0xff; ++value) {
    require(ascii_to_jascii(static_cast<char>(value), true).has_value() ==
                (value >= 0x20 && value <= 0x7e),
            "JASCII mapping must cover exactly printable ASCII");
  }
  for (char input = 'a'; input <= 'z'; ++input) {
    require(ascii_to_jascii(input, true) == 0x2361 + input - 'a' &&
                ascii_to_jascii(input - 'a' + 'A', true) == 0x2341 + input - 'a',
            "JASCII alphabet mapping differs from the recovered table");
  }
  require(ascii_to_jascii(' ', true) == 0x2121 &&
              ascii_to_jascii('9', true) == 0x2339 &&
              ascii_to_jascii(',', true) == 0x2124 &&
              ascii_to_jascii('.', true) == 0x2125 &&
              ascii_to_jascii('-', true) == 0x213d &&
              ascii_to_jascii(',') == 0x2122 &&
              ascii_to_jascii('.') == 0x2123 &&
              ascii_to_jascii('-') == 0x213c,
          "JASCII-specific punctuation changed kana-mode punctuation");
}

void test_character_information_spellings() {
  using jwpqt::core::kana_input_spellings;
  using Spellings = std::vector<std::string_view>;
  require(kana_input_spellings(0x2437) == Spellings{"shi", "si"} &&
              kana_input_spellings(0x2541) == Spellings{"chi", "ci", "ti"} &&
              kana_input_spellings(0x2444) == Spellings{"tsu", "tu"} &&
              kana_input_spellings(0x2445) == Spellings{"dzu", "du"} &&
              kana_input_spellings(0x2442) == Spellings{"dji", "dzi", "di"} &&
              kana_input_spellings(0x2473) == Spellings{"n", "n'"} &&
              kana_input_spellings(0x242b) == Spellings{"ka"} &&
              kana_input_spellings(0x2574) == Spellings{"vu"} &&
              kana_input_spellings(0x2575) == Spellings{"+ka"} &&
              kana_input_spellings(0x2576) == Spellings{"+ke"},
          "Character Information lost visible aliases or exposed hidden input");
  for (unsigned cell = 0x21; cell <= 0x73; ++cell) {
    const auto hiragana = kana_input_spellings(0x2400 | cell);
    require(!hiragana.empty() &&
                hiragana == kana_input_spellings(0x2500 | cell) &&
                hiragana.back() == *jwpqt::core::romaji_for_kana(0x2400 | cell),
            "Kana information spellings differ between scripts");
  }
  for (const auto code : {0U, 0x41U, 0x2420U, 0x2474U, 0x2577U, 0x3021U})
    require(kana_input_spellings(code).empty(),
            "Invalid or non-kana cell has kana input spellings");
}

}  // namespace

int main() {
  try {
    test_direct_case_semantics();
    test_compounds_and_aliases();
    test_pending_input();
    test_vowel_quote_modes();
    test_doubled_consonants_and_assimilation();
    test_symbols_and_invalid_input();
    test_state_replay_and_rejection();
    test_discard_and_options();
    test_jascii_mode_mapping();
    test_character_information_spellings();
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
