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

}  // namespace

int main() {
  test_canonical_round_trip();
  test_import_line_endings_and_empty_meaning();
  test_new_entry_validation();
  test_malformed_and_code_page_failures();
  test_limits_are_atomic();
  return EXIT_SUCCESS;
}
