// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <iostream>
#include <string>

#include "jwpqt/core/kanji_code_search.h"

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

jwpqt::core::KanjiInfoDatabase database() {
  constexpr std::size_t count = 2;
  constexpr std::size_t variable = 12 + count * 16;
  std::string bytes;
  append_u32(bytes, jwpqt::core::kKanjiInfoMagic);
  append_u32(bytes, 0x28U);
  append_u16(bytes, count);
  append_u16(bytes, 0x3022U);
  bytes.resize(variable, '\0');

  put_u16(bytes, 12, 23U | (3U << 8U));
  put_u16(bytes, 14, (1U << 8U) | (2U << 11U));
  put_u16(bytes, 16, 3U);
  put_u16(bytes, 18, 1U);
  put_u32(bytes, 24,
          25U | (static_cast<std::uint32_t>(variable) << 8U));
  append_u16(bytes, 0U);
  append_u32(bytes,
             (2U << 17U) | (4U << 22U) | (5U << 27U));
  append_u32(bytes, 7U | 1234U << 6U | 5U << 20U | 7U << 24U);
  const std::uint16_t miss =
      static_cast<std::uint16_t>((1U << 13U) | (4U << 10U) | (5U << 5U) |
                                 6U);
  bytes.push_back('Q');
  append_u16(bytes, 9876U);
  bytes.push_back('z');
  append_u16(bytes, miss);
  bytes.push_back('\0');

  put_u16(bytes, 28, 35U | (4U << 8U));
  put_u16(bytes, 30, (2U << 8U) | (3U << 11U));
  put_u16(bytes, 32, 4U);
  put_u32(bytes, 40, static_cast<std::uint32_t>(bytes.size()) << 8U);
  return jwpqt::core::KanjiInfoDatabase::parse(bytes);
}

void test_skip() {
  const auto source = database();
  require(source.record(0x3021U).references.size() == 2,
          "Kanji code fixture references decoded incorrectly");
  jwpqt::core::KanjiSkipQuery query;
  query.type = {1, 1};
  query.first = {2, 2};
  query.second = {3, 3};
  auto report = jwpqt::core::search_kanji_skip(source, query);
  require(report.matches.size() == 1 &&
              report.matches[0].code == 0x3021U &&
              !report.matches[0].alternate,
          "Primary SKIP search returned wrong results");
  query.type = {4, 4};
  query.first = {5, 5};
  query.second = {6, 6};
  query.include_misclassifications = true;
  report = jwpqt::core::search_kanji_skip(source, query);
  require(report.matches.size() == 1 && report.matches[0].alternate,
          "Alternate SKIP code was not searched");
}

void test_four_corner() {
  const auto source = database();
  jwpqt::core::KanjiFourCornerQuery primary;
  primary.digits = {1, 2, 3, 4, 5};
  auto report = jwpqt::core::search_kanji_four_corner(source, primary);
  require(report.matches.size() == 1 &&
              report.matches[0].code == 0x3021U &&
              !report.matches[0].alternate,
          "Primary four-corner search returned wrong results");
  jwpqt::core::KanjiFourCornerQuery secondary;
  secondary.digits = {9, 8, 7, 6, 7};
  report = jwpqt::core::search_kanji_four_corner(source, secondary);
  require(report.matches.size() == 1 && report.matches[0].alternate,
          "Secondary four-corner code was not searched");
}

void test_bushu() {
  const auto source = database();
  jwpqt::core::KanjiBushuQuery query;
  query.radical = {22, 22};
  query.strokes = {3, 3};
  query.classical = false;
  auto report = jwpqt::core::search_kanji_bushu(source, query);
  require(report.matches.size() == 1 &&
              report.matches[0].code == 0x3021U,
          "Nelson Bushu aliases were not normalized");

  query.radical = {34, 34};
  query.strokes = {3, 4};
  query.nelson = false;
  query.classical = true;
  report = jwpqt::core::search_kanji_bushu(source, query);
  require(report.matches.size() == 2 &&
              report.matches[0].code == 0x3021U &&
              report.matches[1].code == 0x3022U,
          "Classical Bushu aliases or Nelson fallback are wrong");

  query.nelson = false;
  query.classical = false;
  require_error(
      [&] { (void)jwpqt::core::search_kanji_bushu(source, query); },
      "Bushu search without a radical system was accepted");
}

void test_spahn() {
  const auto source = database();
  jwpqt::core::KanjiSpahnQuery query;
  query.radical_strokes = {2, 2};
  query.radical = {4, 4};
  query.other_strokes = {5, 5};
  query.index = {7, 7};
  const auto report = jwpqt::core::search_kanji_spahn(source, query);
  require(report.matches.size() == 1 &&
              report.matches[0].code == 0x3021U &&
              !report.matches[0].alternate,
          "Spahn-Hadamitzky metadata search returned wrong results");
}

void test_limits_and_validation() {
  const auto source = database();
  jwpqt::core::KanjiSkipQuery skip;
  skip.type = {4, 3};
  require_error([&] { (void)jwpqt::core::search_kanji_skip(source, skip); },
                "Invalid SKIP range was accepted");
  jwpqt::core::KanjiFourCornerQuery corner;
  corner.digits[0] = 10;
  require_error(
      [&] { (void)jwpqt::core::search_kanji_four_corner(source, corner); },
      "Invalid four-corner digit was accepted");
  jwpqt::core::KanjiCodeSearchLimits limits;
  limits.work = 1;
  require_error(
      [&] {
        (void)jwpqt::core::search_kanji_four_corner(source, {}, limits);
      },
      "Kanji code search work limit was not enforced");
  limits.work = 100;
  limits.results = 1;
  const auto truncated = jwpqt::core::search_kanji_skip(source, {}, limits);
  require(truncated.matches.size() == 1 && truncated.truncated,
          "Kanji code result limit did not report truncation");
  jwpqt::core::KanjiSpahnQuery spahn;
  spahn.radical.maximum = 20;
  require_error(
      [&] { (void)jwpqt::core::search_kanji_spahn(source, spahn); },
      "Invalid Spahn-Hadamitzky range was accepted");
}

void test_index_types_and_limits() {
  using namespace jwpqt::core;
  constexpr std::size_t count = 3;
  constexpr std::size_t variable = 12 + count * 16;
  std::string bytes;
  append_u32(bytes, kKanjiInfoMagic);
  append_u32(bytes, 0x38U);
  append_u16(bytes, count);
  append_u16(bytes, 0x3023U);
  bytes.resize(variable, '\0');
  put_u16(bytes, 14, 4);
  put_u16(bytes, 18, (2492U << 1U) | 1U);
  put_u16(bytes, 20, 2829U << 1U);
  put_u16(bytes, 22, 1927);
  put_u32(bytes, 24, variable << 8U);
  append_u16(bytes, 10947);
  append_u32(bytes, 4U | (1123U << 4U));
  append_u32(bytes, 0);
  constexpr std::string_view tags = "HIEKLONDFSTCJBG";
  for (std::size_t i = 0; i < tags.size(); ++i) {
    bytes.push_back(tags[i]);
    append_u16(bytes, tags[i] == 'B' ? (2U << 8U) | 7U : 100U + i);
  }
  bytes.push_back('F');
  append_u16(bytes, 640);
  bytes.push_back('h');
  append_u16(bytes, 777);
  bytes.push_back('H');
  append_u16(bytes, 777);
  bytes.push_back('\0');
  put_u32(bytes, 40, bytes.size() << 8U);
  put_u32(bytes, 56, bytes.size() << 8U);
  const auto source = KanjiInfoDatabase::parse(bytes);
  const std::array<KanjiIndexQuery, 6> fixed{{
      {KanjiIndexType::kNelson, 2829}, {KanjiIndexType::kHaig, 1927},
      {KanjiIndexType::kHalpern, 2492}, {KanjiIndexType::kGrade, 4},
      {KanjiIndexType::kMorohashiFull, 10947}, {KanjiIndexType::kMorohashiVolume, 1123, 4}}};
  for (const auto& query : fixed) {
    const auto report = search_kanji_index(source, query);
    require(report.matches.size() == 1 && report.matches[0].code == 0x3021 &&
                !report.matches[0].alternate && !report.truncated,
            "Fixed or extended dictionary index returned a wrong character");
  }
  for (std::size_t i = 0; i < tags.size(); ++i) {
    const KanjiIndexQuery query{static_cast<KanjiIndexType>(i + 6),
        tags[i] == 'F' ? 640U : tags[i] == 'B' ? 7U : 100U + static_cast<unsigned>(i),
        tags[i] == 'B' ? 2U : 0U};
    const auto report = search_kanji_index(source, query);
    require(report.matches.size() == 1 && report.matches[0].code == 0x3021,
            "A primary reference index used the wrong code or lost the last value");
  }
  require(search_kanji_index(source, {KanjiIndexType::kHalpernLearners, 777}).matches.empty() &&
              search_kanji_index(source, {KanjiIndexType::kFrequency, 108}).matches.empty() &&
              search_kanji_index(source, {KanjiIndexType::kMorohashiVolume, 1123, 3}).matches.empty(),
          "Index search used cross-references, superseded tags, or a wrong volume");
  const auto zero = search_kanji_index(source, {KanjiIndexType::kFrequency, 0});
  require(zero.matches.size() == 2 && zero.matches[0].code == 0x3022 &&
              zero.matches[1].code == 0x3023,
          "Zero index did not match missing primary values in JIS order");
  require(search_kanji_index(source, {KanjiIndexType::kNelson, 0}, {1, 100}).truncated,
          "Index result limit did not report truncation");
  require_error([&] { search_kanji_index(source, {}, {0, 100}); }, "Zero index result budget was accepted");
  require_error([&] { search_kanji_index(source, {KanjiIndexType::kFrequency, 640}, {3, 2}); },
                "Index references bypassed the work budget");
  for (const auto query : {KanjiIndexQuery{static_cast<KanjiIndexType>(99)},
                          KanjiIndexQuery{KanjiIndexType::kNelson, 65536},
                          KanjiIndexQuery{KanjiIndexType::kBusyPeople, 256, 2},
                          KanjiIndexQuery{KanjiIndexType::kBusyPeople, 7, 256},
                          KanjiIndexQuery{KanjiIndexType::kMorohashiVolume, 8192, 4},
                          KanjiIndexQuery{KanjiIndexType::kMorohashiVolume, 1123, 16}}) {
    require_error([&] { search_kanji_index(source, query); }, "Invalid index query was accepted");
  }
}

}  // namespace

int main() {
  test_skip();
  test_four_corner();
  test_bushu();
  test_spahn();
  test_limits_and_validation();
  test_index_types_and_limits();
  return EXIT_SUCCESS;
}
