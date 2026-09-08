// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

#include "jwpqt/core/kanji_lookup.h"

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
  } catch (const jwpqt::core::KanjiLookupListError&) {
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

jwpqt::core::KanjiLookupLists lists(
    const std::vector<std::vector<jwpqt::core::JisCode>>& groups) {
  std::string bytes(groups.size() * 4U, '\0');
  std::size_t offset = bytes.size();
  for (std::size_t i = 0; i < groups.size(); ++i) {
    put_u16(bytes, i * 4U, static_cast<std::uint16_t>(offset));
    put_u16(bytes, i * 4U + 2U,
            static_cast<std::uint16_t>(groups[i].size()));
    for (const auto code : groups[i]) append_u16(bytes, code);
    offset = bytes.size();
  }
  return jwpqt::core::KanjiLookupLists::parse(bytes, groups.size());
}

jwpqt::core::KanjiInfoDatabase information() {
  constexpr std::size_t count = 3;
  constexpr std::size_t variable = 12 + count * 16;
  std::string bytes;
  append_u32(bytes, jwpqt::core::kKanjiInfoMagic);
  append_u32(bytes, 0U);
  append_u16(bytes, static_cast<std::uint16_t>(count));
  append_u16(bytes, 0x3023U);
  bytes.resize(variable, '\0');
  for (std::size_t i = 0; i < count; ++i) {
    put_u16(bytes, 12U + i * 16U,
            static_cast<std::uint16_t>((i + 1U) << 8U));
    put_u32(bytes, 12U + i * 16U + 12U,
            static_cast<std::uint32_t>(variable) << 8U);
  }
  return jwpqt::core::KanjiInfoDatabase::parse(bytes);
}

std::vector<std::vector<jwpqt::core::JisCode>> stroke_groups() {
  std::vector<std::vector<jwpqt::core::JisCode>> groups(30);
  groups[0] = {0x3021U};
  groups[1] = {0x3022U};
  groups[2] = {0x3023U};
  return groups;
}

void test_intersection_and_strokes() {
  const auto radicals = lists({{0x3021U, 0x3022U, 0x3023U},
                               {0x3022U, 0x3023U}, {}});
  const auto strokes = lists(stroke_groups());
  const auto info = information();
  jwpqt::core::KanjiLookupOptions options;
  options.radicals = {0, 1};
  auto report =
      jwpqt::core::search_kanji_radicals(radicals, strokes, &info, options);
  require(report.results.size() == 2 &&
              report.results[0].code == 0x3022U &&
              report.results[1].code == 0x3023U,
          "Selected radicals did not intersect in first-list order");

  options.minimum_strokes = 2;
  options.maximum_strokes = 2;
  report =
      jwpqt::core::search_kanji_radicals(radicals, strokes, &info, options);
  require(report.results.size() == 1 && report.results[0].code == 0x3022U &&
              report.results[0].strokes == 2,
          "Radical lookup did not apply information stroke counts");

  options.radicals = {2};
  options.minimum_strokes = 2;
  options.maximum_strokes = 3;
  report =
      jwpqt::core::search_kanji_radicals(radicals, strokes, nullptr, options);
  require(report.results.size() == 2 &&
              report.results[0].code == 0x3022U &&
              report.results[1].code == 0x3023U,
          "False radical did not fall back to stroke-list concatenation");
}

void test_limits_and_validation() {
  const auto radicals = lists({{0x3021U, 0x3022U}});
  const auto strokes = lists(stroke_groups());
  jwpqt::core::KanjiLookupOptions options;
  options.radicals = {0};
  options.minimum_strokes = 1;
  options.maximum_strokes = 1;
  require_error(
      [&] {
        (void)jwpqt::core::search_kanji_radicals(radicals, strokes, nullptr,
                                                 options);
      },
      "Stroke-filtered radical lookup accepted missing information");
  options.radicals = {1};
  require_error(
      [&] {
        (void)jwpqt::core::search_kanji_radicals(radicals, strokes, nullptr,
                                                 options);
      },
      "Out-of-range selected radical was accepted");
  options.radicals.clear();
  options.minimum_strokes = 1;
  options.maximum_strokes = 3;
  options.results = 2;
  const auto truncated =
      jwpqt::core::search_kanji_radicals(radicals, strokes, nullptr, options);
  require(truncated.results.size() == 2 && truncated.truncated,
          "Kanji result limit did not report truncation");
  options.results = 10;
  options.work = 2;
  require_error(
      [&] {
        (void)jwpqt::core::search_kanji_radicals(radicals, strokes, nullptr,
                                                 options);
      },
      "Kanji lookup work limit was not enforced");
}

void test_radical_controls() {
  using namespace jwpqt::core;
  for (std::size_t index = 0; index < 241; ++index) {
    const auto linked = linked_kanji_radicals(index);
    for (const auto variant : linked)
      require(linked_kanji_radicals(variant) == linked, "Radical links are not symmetric");
  }
  require(linked_kanji_radicals(64) == std::vector<std::size_t>({64, 65, 66}) &&
              linked_kanji_radicals(32) == std::vector<std::size_t>({186, 32}) &&
              linked_kanji_radicals(74) == std::vector<std::size_t>({74}),
          "Radical links were confused with Bushu variants");
  require(kanji_radical_stroke_estimate({0, 6, 34, 240}) == 23 &&
              kanji_radical_stroke_estimate({0, 0}) == 1 &&
              kanji_radical_stroke_estimate(linked_kanji_radicals(32)) == 9,
          "Selected radical stroke estimate is incorrect");
  require_error([] { (void)linked_kanji_radicals(241); }, "Invalid radical link accepted");
  require_error([] { (void)kanji_radical_stroke_estimate({241}); }, "Invalid estimate accepted");
  for (int minimum = 1; minimum <= 30; ++minimum) {
    require(step_kanji_strokes(0, 1, minimum) == minimum &&
                step_kanji_strokes(minimum, -1, minimum) == 0 &&
                step_kanji_strokes(30, 1, minimum) == 0 &&
                step_kanji_strokes(0, -1, minimum) == 30,
            "Smart stroke spinner does not cycle through Any and valid counts");
    for (int count = 0; count <= 30; ++count)
      for (int steps = -64; steps <= 64; ++steps) {
        int expected = count;
        for (int remaining = steps; remaining != 0; remaining += remaining > 0 ? -1 : 1) {
          expected += remaining > 0 ? 1 : -1;
          if (expected < 0) expected = 30;
          if (expected > 30) expected = 0;
          if (expected > 0 && expected < minimum) expected = remaining > 0 ? minimum : 0;
        }
        require(step_kanji_strokes(count, steps, minimum) == expected,
                "Multi-step stroke spinner differs from repeated source stepping");
      }
  }
  require(step_kanji_strokes(0, 1, 999) == 30 && step_kanji_strokes(0, 1, 0) == 1 &&
              step_kanji_strokes(0, std::numeric_limits<int>::min(), 30) == 0,
          "Extreme smart step is not safely bounded");
  require_error([] { (void)step_kanji_strokes(31, 1, 0); }, "Invalid stroke count accepted");
  const auto radicals = lists({{0x3021, 0x3022}, {}, {0x3022, 0x3023}, {0x3023}});
  require(kanji_radicals_for_character(radicals, 0x3022) == std::vector<std::size_t>({0, 2}) &&
              kanji_radicals_for_character(radicals, 0x3024).empty(),
          "Radical extraction lost source indices or manufactured matches");
  require_error([&] { (void)kanji_radicals_for_character(radicals, 0x3022, 1); },
                "Radical extraction ignored work limit");
  require_error([&] { (void)kanji_radicals_for_character(radicals, 0x2422); },
                "Kana accepted as a kanji for extraction");
  const auto empty = lists({{}, {}, {}});
  require_error([&] { (void)kanji_radicals_for_character(empty, 0x3021, 2); },
                "Empty groups escaped the radical extraction work budget");
}

}  // namespace

int main() {
  test_intersection_and_strokes();
  test_limits_and_validation();
  test_radical_controls();
  return EXIT_SUCCESS;
}
