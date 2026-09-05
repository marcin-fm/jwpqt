// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>

#include "jwpqt/core/kanji_reading_search.h"

namespace {

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
  } catch (const jwpqt::core::KanjiInfoError&) {
    return;
  }
  require(false, message);
}

void append_u16(std::string& bytes, std::uint16_t value) {
  bytes.push_back(static_cast<char>(value & 0xffU));
  bytes.push_back(static_cast<char>((value >> 8U) & 0xffU));
}

void append_u32(std::string& bytes, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8)
    bytes.push_back(static_cast<char>((value >> shift) & 0xffU));
}

void put_u16(std::string& bytes, std::size_t offset, std::uint16_t value) {
  bytes[offset] = static_cast<char>(value & 0xffU);
  bytes[offset + 1] = static_cast<char>((value >> 8U) & 0xffU);
}

void put_u32(std::string& bytes, std::size_t offset, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8)
    bytes[offset + shift / 8] = static_cast<char>((value >> shift) & 0xffU);
}

void append_string(std::string& bytes, std::string_view value) {
  bytes.append(value);
  bytes.push_back('\0');
}

jwpqt::core::KanjiInfoDatabase database() {
  constexpr std::size_t count = 2;
  constexpr std::size_t variable = 12 + count * 16;
  std::string bytes;
  append_u32(bytes, jwpqt::core::kKanjiInfoMagic);
  append_u32(bytes, 0x3fU);
  append_u16(bytes, count);
  append_u16(bytes, 0x3022U);
  bytes.resize(variable, '\0');

  put_u16(bytes, 12, (4U << 8U) | (1U << 13U));
  put_u16(bytes, 14, 2U << 4U);
  put_u16(bytes, 16, (1U << 5U) | (1U << 6U) | (1U << 11U));
  put_u16(bytes, 20, 1U);
  put_u32(bytes, 24, static_cast<std::uint32_t>(bytes.size()) << 8U);
  append_string(bytes, "han");
  append_string(bytes, std::string("m\xe0", 2));
  append_string(bytes, "red apple");
  append_string(bytes, "fruit, food");
  append_string(bytes, std::string("\x22", 1));
  append_string(bytes, std::string("\x22\xa4", 2));
  append_string(bytes, std::string("\x2a", 1));

  put_u16(bytes, 28, (7U << 8U) | (1U << 13U));
  put_u16(bytes, 30, 1U << 4U);
  put_u32(bytes, 40, static_cast<std::uint32_t>(bytes.size()) << 8U);
  append_string(bytes, "pineapple");
  append_string(bytes, std::string("\x1f\x23", 2));
  return jwpqt::core::KanjiInfoDatabase::parse(bytes);
}

void test_kana_modes() {
  const auto source = database();
  jwpqt::core::KanjiReadingQuery query;
  query.kind = jwpqt::core::KanjiReadingKind::kOn;
  query.text = U"あ";
  auto report = jwpqt::core::search_kanji_readings(source, query);
  require(report.matches.size() == 1 && report.matches[0].code == 0x3021U,
          "On reading did not match katakana by its shared kana cell");
  query.text = U"―ぃ";
  report = jwpqt::core::search_kanji_readings(source, query);
  require(report.matches.size() == 1 && report.matches[0].code == 0x3022U,
          "Japanese dash did not match a compressed long-vowel marker");
  query.text = U"ぃ";
  require(jwpqt::core::search_kanji_readings(source, query).matches.size() == 1,
          "On reading did not preserve leading long-vowel retry semantics");

  query.kind = jwpqt::core::KanjiReadingKind::kKun;
  query.text = U"あ";
  report = jwpqt::core::search_kanji_readings(source, query);
  require(report.matches.size() == 1,
          "Kun stem did not stop at the okurigana boundary");
  query.text = U"あい";
  require(jwpqt::core::search_kanji_readings(source, query).matches.empty(),
          "Strict kun matching ignored the okurigana boundary bit");
  query.flexible_kun = true;
  require(jwpqt::core::search_kanji_readings(source, query).matches.size() == 1,
          "Flexible kun matching did not ignore okurigana marking");
  query.flexible_kun = false;
  query.text = U"あ(い)";
  require(jwpqt::core::search_kanji_readings(source, query).matches.size() == 1,
          "Explicit okurigana query did not match its reading");

  query.kind = jwpqt::core::KanjiReadingKind::kOnOrKun;
  query.text = U"あい";
  query.flexible_kun = true;
  require(jwpqt::core::search_kanji_readings(source, query).matches.size() == 1,
          "Combined reading lookup did not apply flexible matching");

  query.kind = jwpqt::core::KanjiReadingKind::kNanori;
  query.flexible_kun = false;
  query.text = U"お";
  require(jwpqt::core::search_kanji_readings(source, query).matches.size() == 1,
          "Nanori lookup did not search the nanori list");
}

void test_text_modes_and_strokes() {
  const auto source = database();
  jwpqt::core::KanjiReadingQuery query;
  query.kind = jwpqt::core::KanjiReadingKind::kMeaning;
  query.text = U"APPLE";
  auto report = jwpqt::core::search_kanji_readings(source, query);
  require(report.matches.size() == 1 && report.matches[0].code == 0x3021U,
          "Whole-word meaning lookup returned wrong results");
  query.partial_words = true;
  report = jwpqt::core::search_kanji_readings(source, query);
  require(report.matches.size() == 2,
          "Partial meaning lookup did not match inside words");
  query.partial_words = false;
  query.text = U"fruit";
  require(jwpqt::core::search_kanji_readings(source, query).matches.size() == 1,
          "Meaning lookup did not accept a comma word boundary");

  query.kind = jwpqt::core::KanjiReadingKind::kPinyin;
  query.text = U"ma3";
  require(jwpqt::core::search_kanji_readings(source, query).matches.size() == 1,
          "Pinyin tone expansion did not match CP1252 data");
  query.text = U"a";
  query.partial_words = true;
  require(jwpqt::core::search_kanji_readings(source, query).matches.empty(),
          "Partial-word option incorrectly affected pinyin lookup");
  query.kind = jwpqt::core::KanjiReadingKind::kKorean;
  query.text = U"HAN";
  require(jwpqt::core::search_kanji_readings(source, query).matches.size() == 1,
          "Korean lookup did not fold ASCII case");

  query.strokes = {7, 7};
  require(jwpqt::core::search_kanji_readings(source, query).matches.empty(),
          "Stroke filter did not exclude the Korean match");
}

void test_validation_and_limits() {
  const auto source = database();
  jwpqt::core::KanjiReadingQuery query;
  query.text = U"ASCII";
  require_error(
      [&] { (void)jwpqt::core::search_kanji_readings(source, query); },
      "Non-kana reading query was accepted");
  query.kind = jwpqt::core::KanjiReadingKind::kMeaning;
  query.text = U"日本";
  require_error(
      [&] { (void)jwpqt::core::search_kanji_readings(source, query); },
      "Multibyte meaning query was accepted");
  query.text.assign(1, U'\0');
  require_error(
      [&] { (void)jwpqt::core::search_kanji_readings(source, query); },
      "NUL meaning query was accepted");
  query.text = U"apple";
  query.strokes = {8, 7};
  require_error(
      [&] { (void)jwpqt::core::search_kanji_readings(source, query); },
      "Invalid reading stroke range was accepted");

  query.strokes = {0, 30};
  query.partial_words = true;
  jwpqt::core::KanjiReadingSearchLimits limits;
  limits.results = 1;
  const auto truncated =
      jwpqt::core::search_kanji_readings(source, query, limits);
  require(truncated.matches.size() == 1 && truncated.truncated,
          "Reading result limit did not report truncation");
  limits.results = 10;
  limits.work = 1;
  require_error(
      [&] { (void)jwpqt::core::search_kanji_readings(source, query, limits); },
      "Reading search work limit was not enforced");
  limits.work = 1000;
  limits.query_code_points = 4;
  require_error(
      [&] { (void)jwpqt::core::search_kanji_readings(source, query, limits); },
      "Reading query length limit was not enforced");
}

}  // namespace

int main() {
  test_kana_modes();
  test_text_modes_and_strokes();
  test_validation_and_limits();
  return EXIT_SUCCESS;
}
