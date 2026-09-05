// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <cstdint>
#include <functional>
#include <iostream>
#include <string>

#include "jwpqt/core/edict_user_dictionary.h"

namespace {

using jwpqt::core::EdictUserDictionary;
using jwpqt::core::EdictUserDictionaryError;
using jwpqt::core::EdictUserDictionaryLimits;
using jwpqt::core::EdictUserEntry;
using jwpqt::core::JwpText;
using jwpqt::core::LegacyCodePage;
using jwpqt::core::make_edict_user_entry;

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

void require_error(const std::function<void()>& operation,
                   const char* message) {
  try {
    operation();
  } catch (const EdictUserDictionaryError&) {
    return;
  }
  require(false, message);
}

std::string euc(std::initializer_list<std::uint16_t> codes) {
  std::string bytes;
  for (const std::uint16_t code : codes) {
    bytes.push_back(static_cast<char>((code >> 8U) | 0x80U));
    bytes.push_back(static_cast<char>((code & 0xffU) | 0x80U));
  }
  return bytes;
}

void test_canonical_round_trip() {
  const std::string source =
      euc({0x2422}) + " /plain/slash/\n" + euc({0x467c}) + " [" +
      euc({0x242b, 0x244a}) + "] /caf\xe9/\n";
  const EdictUserDictionary dictionary =
      EdictUserDictionary::parse(source, LegacyCodePage::k1252);
  require(dictionary.entries() ==
              std::vector<EdictUserEntry>{
                  {{}, {0x2422}, U"plain/slash"},
                  {{0x467c}, {0x242b, 0x244a}, U"caf\u00e9"}},
          "User dictionary parse produced the wrong fields");
  require(dictionary.serialize(LegacyCodePage::k1252) == source,
          "Canonical user dictionary round trip changed bytes");
}

void test_import_line_endings_and_empty_meaning() {
  const std::string source = "\n" + euc({0x2422}) + " //\r\n" +
                              euc({0x2424}) + " /two/\n\r" +
                              euc({0x0041}) + " /last/";
  const EdictUserDictionary dictionary = EdictUserDictionary::parse(source);
  require(dictionary.entries().size() == 3 &&
              dictionary.entries()[0].meaning.empty() &&
              dictionary.entries()[1].meaning == U"two" &&
              dictionary.entries()[2].reading == JwpText{0x0041} &&
              dictionary.entries()[2].meaning == U"last" &&
              dictionary.serialize() == euc({0x2422}) + " //\n" +
                                            euc({0x2424}) + " /two/\n" +
                                            euc({0x0041}) + " /last/\n",
          "Imported line endings, ASCII JWP cells, or empty meanings were not canonicalized");
}

void test_new_entry_validation() {
  const EdictUserEntry entry = jwpqt::core::make_edict_user_entry(
      JwpText{0x2422}, JwpText{0x467c}, U"meaning");
  require(entry.reading == JwpText{0x2422} &&
              entry.headword == JwpText{0x467c},
          "Valid user entry was changed");
  require_error(
      [] {
        (void)jwpqt::core::make_edict_user_entry({}, {}, U"meaning");
      },
      "Empty new-entry reading was accepted");
  require_error(
      [] {
        (void)jwpqt::core::make_edict_user_entry({0x20}, {}, U"meaning");
      },
      "Space-containing new-entry reading was accepted");
  require(jwpqt::core::make_edict_user_entry({0x41}, {}, U"meaning").reading ==
              JwpText{0x41},
          "Source-valid warned ASCII reading was rejected");
  require_error(
      [] {
        (void)jwpqt::core::make_edict_user_entry({0x2422}, {}, U"");
      },
      "Empty new-entry meaning was accepted");
  require_error(
      [] {
        (void)jwpqt::core::make_edict_user_entry({0x2422}, {}, U"bad\nline");
      },
      "Multiline new-entry meaning was accepted");
  require(EdictUserDictionary::from_entries(
              {{{}, {0x2422}, U""}})
              .entries()[0]
              .meaning.empty(),
          "Permissive imported empty meaning was rejected by from_entries");
}

void test_malformed_and_code_page_failures() {
  require(EdictUserDictionary::parse(euc({0x2422}) + " /x/").entries().size() ==
              1,
          "Final user record without a line terminator was rejected");
  require_error(
      [] { (void)EdictUserDictionary::parse(euc({0x2422}) + " x/\n"); },
      "Missing user meaning delimiter was accepted");
  require_error(
      [] { (void)EdictUserDictionary::parse(std::string("\xa4 /x/\n", 7)); },
      "Incomplete EUC user field was accepted");
  require_error(
      [] {
        (void)EdictUserDictionary::parse(std::string("\x80\x80", 2) +
                                         " /x/\n");
      },
      "NUL JWP user field was accepted");
  require_error(
      [] {
        (void)EdictUserDictionary::parse(std::string("\x80\x81", 2) +
                                         " /x/\n");
      },
      "Control JWP user field was accepted");
  require_error(
      [] {
        (void)EdictUserDictionary::parse(std::string("\x80\xff", 2) +
                                         " /x/\n");
      },
      "DEL JWP user field was accepted");
  require_error(
      [] {
        (void)EdictUserDictionary::parse(euc({0x2422}) +
                                         std::string(" /a\0b/\n", 7));
      },
      "NUL user meaning byte was accepted");
  require_error(
      [] {
        (void)EdictUserDictionary::parse(euc({0x2422}) +
                                         std::string(" /\x81/\n", 5));
      },
      "Undefined code-page byte was accepted");
  const EdictUserDictionary unicode = EdictUserDictionary::from_entries(
      {{{}, {0x2422}, U"\u20ac"}});
  require_error(
      [&] { (void)unicode.serialize(LegacyCodePage::k1251); },
      "Unrepresentable code-page meaning was serialized");
  require_error(
      [] {
        (void)EdictUserDictionary::parse(
            {}, static_cast<LegacyCodePage>(9999));
      },
      "Invalid user dictionary code page was accepted");
}

void test_limits_are_atomic() {
  EdictUserDictionaryLimits limits;
  limits.encoded_bytes = 64;
  limits.line_bytes = 16;
  limits.records = 1;
  limits.jwp_cells = 2;
  limits.meaning_code_points = 4;
  require_error(
      [&] {
        (void)EdictUserDictionary::parse(euc({0x2422}) + " /one/\n" +
                                             euc({0x2424}) + " /two/\n",
                                         LegacyCodePage::k1252, limits);
      },
      "User dictionary record limit was not enforced");
  require_error(
      [&] {
        (void)EdictUserDictionary::parse(euc({0x2422}) + " /12345/\n",
                                         LegacyCodePage::k1252, limits);
      },
      "User dictionary meaning limit was not enforced");
}

void test_render_and_legacy_sort() {
  const EdictUserEntry hiragana =
      make_edict_user_entry({0x2422}, {}, U"hiragana");
  const EdictUserEntry katakana =
      make_edict_user_entry({0x2522}, {}, U"katakana");
  const EdictUserEntry kanji =
      make_edict_user_entry({0x2422}, {0x3021}, U"kanji");
  require(jwpqt::core::render_edict_user_entry(kanji) ==
              U"\u4e9c [\u3042]\tkanji",
          "User dictionary display row is wrong");

  const std::vector<EdictUserEntry> sorted =
      jwpqt::core::sort_edict_user_entries({katakana, kanji, hiragana});
  require(sorted == std::vector<EdictUserEntry>({hiragana, kanji, katakana}),
          "User dictionary sort did not follow reading and kana tie breaks");
  const std::vector<EdictUserEntry> duplicates{hiragana, hiragana};
  require(jwpqt::core::sort_edict_user_entries(duplicates) == duplicates,
          "User dictionary sort did not preserve exact duplicates");

  std::vector<EdictUserEntry> pathological(
      1600, make_edict_user_entry({0x2422}, {}, U"same"));
  require_error(
      [&] {
        (void)jwpqt::core::sort_edict_user_entries(std::move(pathological));
      },
      "User dictionary interactive sort work limit was not enforced");
}

void test_candidate_first_editor() {
  using jwpqt::core::EdictUserDictionaryEditor;
  const EdictUserEntry first =
      make_edict_user_entry({0x2424}, {}, U"first");
  const EdictUserEntry second =
      make_edict_user_entry({0x2422}, {}, U"second");
  EdictUserDictionaryEditor editor(
      EdictUserDictionary::from_entries({first}));
  require(editor.add(second) == 1 && editor.entries().size() == 2,
          "User dictionary editor did not append an entry");
  require(editor.move_up(1) && !editor.move_up(0) && editor.move_down(0) &&
              !editor.move_down(1),
          "User dictionary editor move boundaries are wrong");
  editor.replace(1, second);
  editor.sort();
  require(editor.entries().front() == second &&
              editor.dictionary().entries() == editor.entries(),
          "User dictionary editor did not publish replacement or sort");
  editor.erase(1);
  require(editor.entries() == std::vector<EdictUserEntry>{second},
          "User dictionary editor did not erase an entry");

  const std::vector<EdictUserEntry> saved = editor.entries();
  require_error([&] { editor.erase(99); },
                "Out-of-range user dictionary erase was accepted");
  require_error([&] { editor.replace(99, first); },
                "Out-of-range user dictionary replacement was accepted");
  require_error([&] { (void)editor.move_up(99); },
                "Out-of-range user dictionary move was accepted");
  require(editor.entries() == saved,
          "Failed user dictionary edits changed live entries");
}

}  // namespace

int main() {
  test_canonical_round_trip();
  test_import_line_endings_and_empty_meaning();
  test_new_entry_validation();
  test_malformed_and_code_page_failures();
  test_limits_are_atomic();
  test_render_and_legacy_sort();
  test_candidate_first_editor();
  return EXIT_SUCCESS;
}
