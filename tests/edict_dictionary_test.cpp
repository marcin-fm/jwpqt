// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/edict_dictionary.h"

#include <cstdlib>
#include <iostream>
#include <string>

#include "jwpqt/core/legacy_text.h"
#include "jwpqt/core/utf8.h"

namespace {

using jwpqt::core::EdictDictionary;
using jwpqt::core::EdictDictionaryError;
using jwpqt::core::EdictEncoding;
using jwpqt::core::EdictParseLimits;

void require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

template <typename Function>
void expect_error(Function&& function, const char* context) {
  try {
    function();
  } catch (const EdictDictionaryError&) {
    return;
  }
  throw std::runtime_error(std::string("Expected EDICT error for ") + context);
}

void test_utf8_records_and_boundaries() {
  const std::u32string first =
      U"日本 [にほん] [にっぽん] /(n) Japan/(P)/";
  const std::u32string second = U"かな /(n) kana/phonetic script/";
  const std::u32string third = U"語 [ご] /(n) language/";
  const std::u32string fourth = U"猫 [ねこ] /(n) cat/";
  const std::string bytes =
      std::string("\xef\xbb\xbf") + jwpqt::core::encode_utf8(first) + "\r\n" +
      jwpqt::core::encode_utf8(second) + "\n\r" +
      jwpqt::core::encode_utf8(third) + "\r" +
      jwpqt::core::encode_utf8(fourth) + "\n";

  const EdictDictionary dictionary =
      EdictDictionary::parse(bytes, EdictEncoding::kUtf8);
  require(dictionary.encoding() == EdictEncoding::kUtf8 &&
              dictionary.source_bytes() == bytes &&
              dictionary.records().size() == 4 &&
              dictionary.definition_count() == 6 &&
              dictionary.decoded_code_points() ==
                  first.size() + second.size() + third.size() + fourth.size(),
          "UTF-8 EDICT ownership or record count is wrong");
  const auto& record = dictionary.records().front();
  require(record.byte_offset == 0 &&
              record.byte_length == 3 + jwpqt::core::encode_utf8(first).size() &&
              record.headword == U"日本" &&
              record.readings ==
                  std::vector<std::u32string>({U"にほん", U"にっぽん"}) &&
              record.definitions ==
                  std::vector<std::u32string>({U"(n) Japan", U"(P)"}),
          "UTF-8 EDICT record fields are wrong");
  require(dictionary.records()[1].headword == U"かな" &&
              dictionary.records()[1].readings.empty() &&
              dictionary.records()[1].definitions.size() == 2,
          "Kana-only EDICT record was parsed incorrectly");
}

void test_euc_jp_record() {
  const std::u32string line = U"日本 [にほん] /(n) Japan/(P)/";
  const std::string bytes = jwpqt::core::encode_legacy_text(
                                line, jwpqt::core::LegacyEncoding::kEucJp) +
                            "\n";
  const EdictDictionary dictionary =
      EdictDictionary::parse(bytes, EdictEncoding::kEucJp);
  require(dictionary.records().size() == 1 &&
              dictionary.records()[0].headword == U"日本" &&
              dictionary.records()[0].readings ==
                  std::vector<std::u32string>({U"にほん"}),
          "EUC-JP EDICT record was parsed incorrectly");
}

void test_recovered_euc_jis_x_0212_subset() {
  const std::string bytes =
      std::string("\x8f\xaa\xa1 /capital A acute/\n") +
      "mark /\x8f\xa2\xed/\n";
  const EdictDictionary dictionary =
      EdictDictionary::parse(bytes, EdictEncoding::kEucJp);
  require(dictionary.records().size() == 2 &&
              dictionary.records()[0].headword == U"\u00c1" &&
              dictionary.records()[1].definitions ==
                  std::vector<std::u32string>{U"\u00a9"},
          "Recovered JWP JIS X 0212 mappings decoded incorrectly");

  expect_error(
      [] {
        EdictDictionary::parse(std::string("\x8f\xa1\xa1 /unknown/\n"),
                               EdictEncoding::kEucJp);
      },
      "unmapped JIS X 0212 sequence");
  expect_error(
      [] {
        EdictDictionary::parse(std::string("word /\x8f\xaa/\n"),
                               EdictEncoding::kEucJp);
      },
      "truncated JIS X 0212 sequence");
}

void test_mixed_record_definitions() {
  const std::string prefix = jwpqt::core::encode_legacy_text(
      U"日本 [にほん]", jwpqt::core::LegacyEncoding::kEucJp);
  const std::string bytes = prefix + " /caf\xe9/letters \xc0\xc1/\n";
  const EdictDictionary western = EdictDictionary::parse(
      bytes, EdictEncoding::kMixed, EdictParseLimits{},
      jwpqt::core::LegacyCodePage::k1252);
  require(western.encoding() == EdictEncoding::kMixed &&
              western.mixed_code_page() ==
                  jwpqt::core::LegacyCodePage::k1252 &&
              western.records().size() == 1 &&
              western.records()[0].headword == U"日本" &&
              western.records()[0].readings ==
                  std::vector<std::u32string>{U"にほん"} &&
              western.records()[0].definitions ==
                  std::vector<std::u32string>{U"café", U"letters ÀÁ"},
          "Mixed EDICT headword or CP1252 definitions decoded incorrectly");

  const EdictDictionary cyrillic = EdictDictionary::parse(
      prefix + " /\x8f\xc0\xc1/\n", EdictEncoding::kMixed, EdictParseLimits{},
      jwpqt::core::LegacyCodePage::k1251);
  require(cyrillic.mixed_code_page() ==
                  jwpqt::core::LegacyCodePage::k1251 &&
              cyrillic.records()[0].definitions ==
                  std::vector<std::u32string>{U"ЏАБ"},
          "Mixed EDICT did not honor the selected legacy code page");

  EdictParseLimits exact_spaced;
  exact_spaced.decoded_code_points = 8;
  require(EdictDictionary::parse("word /x/\n", EdictEncoding::kMixed,
                                 exact_spaced)
                  .records()[0]
                  .headword == U"word",
          "Mixed EDICT charged a synthetic spaced-record separator");
  EdictParseLimits exact_compact;
  exact_compact.decoded_code_points = 7;
  require(EdictDictionary::parse("word/x/\n", EdictEncoding::kMixed,
                                 exact_compact)
                  .records()[0]
                  .headword == U"word",
          "Mixed EDICT charged a synthetic compact-record separator");

  const EdictDictionary fallback = EdictDictionary::parse(
      prefix + " /caf\xe9/\n", EdictEncoding::kMixed, EdictParseLimits{},
      static_cast<jwpqt::core::LegacyCodePage>(9999));
  require(fallback.mixed_code_page() ==
                  jwpqt::core::kDefaultLegacyCodePage &&
              fallback.records()[0].definitions ==
                  std::vector<std::u32string>{U"café"},
          "Mixed EDICT did not apply the recovered unknown-page fallback");
}

void test_invalid_mixed_records() {
  expect_error(
      [] {
        EdictDictionary::parse(std::string("word /bad \x81/\n"),
                               EdictEncoding::kMixed);
      },
      "undefined mixed definition byte");
  expect_error(
      [] {
        EdictDictionary::parse(std::string("\xa4 /value/\n"),
                               EdictEncoding::kMixed);
      },
      "malformed mixed EUC headword");
  expect_error(
      [] {
        EdictDictionary::parse(std::string("word\0 /value/\n", 14),
                               EdictEncoding::kMixed);
      },
      "embedded NUL in mixed headword");
  expect_error(
      [] {
        EdictDictionary::parse(std::string("\x8f\xa2\xaf /value/\n"),
                               EdictEncoding::kMixed);
      },
      "unrepresentable 0x8f mixed headword pair");
  expect_error(
      [] {
        EdictDictionary::parse(std::string("word definition\n"),
                               EdictEncoding::kMixed);
      },
      "mixed record without definition delimiter");
  expect_error(
      [] {
        EdictDictionary::parse({}, static_cast<EdictEncoding>(99));
      },
      "invalid encoding in an empty dictionary");

  const EdictDictionary compact =
      EdictDictionary::parse("word/definition/\n", EdictEncoding::kMixed);
  require(compact.records().size() == 1 &&
              compact.records()[0].headword == U"word" &&
              compact.records()[0].definitions ==
                  std::vector<std::u32string>{U"definition"},
          "Mixed EDICT did not accept the recovered first-slash boundary");
}

void test_invalid_records() {
  const auto parse = [](std::string_view bytes) {
    return EdictDictionary::parse(bytes, EdictEncoding::kUtf8);
  };
  expect_error([&] { parse("word /(n) value/"); }, "unterminated record");
  expect_error([&] { parse("\n"); }, "empty record");
  expect_error([&] { parse("word/(n) value/\n"); }, "missing separator");
  expect_error([&] { parse(" /value/\n"); }, "empty headword");
  expect_error([&] { parse("word [] /value/\n"); }, "empty reading");
  expect_error([&] { parse("word [read /value/\n"); }, "unterminated reading");
  expect_error([&] { parse("word /value\n"); }, "unterminated definitions");
  expect_error([&] { parse("word //\n"); }, "empty definition");
  expect_error([&] { parse("word /one//two/\n"); }, "empty middle definition");
  expect_error([&] { parse(std::string("word /\xff/\n")); },
               "malformed UTF-8");
  expect_error(
      [] {
        EdictDictionary::parse(std::string("word /\x8f/\n"),
                               EdictEncoding::kEucJp);
      },
      "malformed EUC-JP");
}

void test_delimiter_and_line_break_boundaries() {
  const EdictDictionary dictionary = EdictDictionary::parse(
      "word /definition with / punctuation/good/\n",
      EdictEncoding::kUtf8);
  require(dictionary.records().size() == 1 &&
              dictionary.records()[0].definitions ==
                  std::vector<std::u32string>{U"definition with ",
                                              U" punctuation", U"good"},
          "Space-slash definition text changed record parsing");

  const auto parse = [](std::string_view bytes) {
    return EdictDictionary::parse(bytes, EdictEncoding::kUtf8);
  };
  expect_error([&] { parse("word /unterminated /tail\n"); },
                "stray definition tail");
  expect_error([&] { parse("word [broken /definition/ /tail/\n"); },
               "malformed prefix before valid definitions");
  const EdictDictionary padded =
      parse("word [reading]  /definition/\n");
  require(padded.records().size() == 1 &&
              padded.records()[0].headword == U"word" &&
              padded.records()[0].readings ==
                  std::vector<std::u32string>{U"reading"},
          "Trailing space after a reading changed record parsing");
  const EdictDictionary embedded =
      parse("word /one/ two /three/\n");
  require(embedded.records()[0].headword == U"word" &&
              embedded.records()[0].definitions ==
                  std::vector<std::u32string>{U"one", U" two ", U"three"},
          "A definition-space-slash sequence changed the record delimiter");
  expect_error([&] { parse("word // /valid/\n"); },
               "empty definition before a later valid suffix");
  expect_error([&] { parse("one /1/\n\r\rtwo /2/\n"); },
               "standalone opposite newline");
}

void test_resource_limits() {
  EdictParseLimits limits;
  limits.encoded_bytes = 8;
  expect_error(
      [&] { EdictDictionary::parse("word /x/\n", EdictEncoding::kUtf8, limits); },
      "encoded size limit");

  limits = EdictParseLimits{};
  limits.line_bytes = 4;
  expect_error(
      [&] { EdictDictionary::parse("word /x/\n", EdictEncoding::kUtf8, limits); },
      "line size limit");

  limits = EdictParseLimits{};
  limits.records = 1;
  expect_error(
      [&] {
        EdictDictionary::parse("one /1/\ntwo /2/\n", EdictEncoding::kUtf8,
                               limits);
      },
      "record count limit");

  limits = EdictParseLimits{};
  limits.definitions = 1;
  expect_error(
      [&] {
        EdictDictionary::parse("word /one/two/\n", EdictEncoding::kUtf8,
                               limits);
      },
      "definition count limit");
  expect_error(
      [&] {
        EdictDictionary::parse("word /one/ two /three/\n",
                               EdictEncoding::kUtf8, limits);
      },
      "definition count limit through an alternate delimiter");

  limits = EdictParseLimits{};
  limits.decoded_code_points = 4;
  expect_error(
      [&] { EdictDictionary::parse("word /x/\n", EdictEncoding::kUtf8, limits); },
      "decoded code-point limit");
}

}  // namespace

int main() {
  try {
    test_utf8_records_and_boundaries();
    test_euc_jp_record();
    test_recovered_euc_jis_x_0212_subset();
    test_mixed_record_definitions();
    test_invalid_mixed_records();
    test_invalid_records();
    test_delimiter_and_line_break_boundaries();
    test_resource_limits();
  } catch (const std::exception& error) {
    std::cerr << "edict_dictionary_test: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
