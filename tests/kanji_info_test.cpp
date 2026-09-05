// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <functional>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include "jwpqt/core/kanji_info.h"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

void append_u16(std::string& bytes, std::uint16_t value) {
  bytes.push_back(static_cast<char>(value & 0xffU));
  bytes.push_back(static_cast<char>((value >> 8U) & 0xffU));
}

void append_u32(std::string& bytes, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8) {
    bytes.push_back(static_cast<char>((value >> shift) & 0xffU));
  }
}

void put_u16(std::string& bytes, std::size_t offset, std::uint16_t value) {
  bytes[offset] = static_cast<char>(value & 0xffU);
  bytes[offset + 1] = static_cast<char>((value >> 8U) & 0xffU);
}

void put_u32(std::string& bytes, std::size_t offset, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8) {
    bytes[offset + shift / 8] = static_cast<char>((value >> shift) & 0xffU);
  }
}

std::string fixture() {
  constexpr std::uint16_t count = 2;
  constexpr std::size_t variable = 12 + count * 16;
  std::string bytes;
  append_u32(bytes, jwpqt::core::kKanjiInfoMagic);
  append_u32(bytes, 0x3fU);
  append_u16(bytes, count);
  append_u16(bytes, 0x3022U);
  bytes.resize(variable, '\0');

  const std::uint16_t word0 = 1U | (7U << 8U) | (1U << 13U);
  const std::uint16_t word1 = 2U | (1U << 4U) | (3U << 8U) | (4U << 11U);
  const std::uint16_t word2 = 5U | (1U << 5U) | (1U << 6U) | (1U << 11U);
  const std::uint16_t word3 = 1U | (123U << 1U);
  const std::uint16_t word4 = 1U | (456U << 1U);
  put_u16(bytes, 12, word0);
  put_u16(bytes, 14, word1);
  put_u16(bytes, 16, word2);
  put_u16(bytes, 18, word3);
  put_u16(bytes, 20, word4);
  put_u16(bytes, 22, 789U);
  put_u32(bytes, 24, 214U | (static_cast<std::uint32_t>(variable) << 8U));

  bytes.append("han\0", 4);
  bytes.append("pin\0", 4);
  bytes.append("meaning\0", 8);
  bytes.append("\x22\0", 2);
  bytes.append("\x23\x20\x24\0", 4);
  bytes.append("\x25\0", 2);
  append_u16(bytes, 1000U);
  append_u32(bytes, 2U | (300U << 4U) | (3U << 17U) | (4U << 22U) |
                        (5U << 27U));
  append_u32(bytes, 6U | (1234U << 6U) | (7U << 20U) | (8U << 24U) |
                        (1U << 28U));
  bytes.push_back('F');
  append_u16(bytes, 42U);
  bytes.push_back('n');
  append_u16(bytes, 43U);
  bytes.push_back('\0');

  put_u32(bytes, 40, static_cast<std::uint32_t>(bytes.size()) << 8U);
  return bytes;
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

void test_parse_record() {
  const auto database = jwpqt::core::KanjiInfoDatabase::parse(fixture());
  require(database.flags() == 0x3fU && database.count() == 2 &&
              database.maximum_code() == 0x3022U &&
              database.contains(0xb0a1U) && !database.contains(0x3023U),
          "Kanji information header or normalized lookup is wrong");
  const auto record = database.record(0x3021U);
  require(database.stroke_count(0x3021U) == 7,
          "Kanji fixed-table stroke lookup is wrong");
  require(record.fixed.bushu == 1 && record.fixed.strokes == 7 &&
              record.fixed.grade == 2 && record.fixed.skip.type == 3 &&
              record.fixed.skip.first == 4 && record.fixed.skip.second == 5 &&
              record.fixed.halpern == 123 && record.fixed.nelson == 456 &&
              record.fixed.haig == 789 &&
              record.fixed.classical_bushu == 214,
          "Kanji fixed bit fields decoded incorrectly");
  require(record.korean == U"han" && record.pinyin == U"pin" &&
              record.meanings == std::vector<std::u32string>{U"meaning"} &&
              record.on_readings.size() == 1 &&
              record.kun_readings.size() == 1 && record.nanori.size() == 1,
          "Kanji variable strings decoded incorrectly");
  require(record.has_extended && record.extended.morohashi_long == 1000 &&
              record.extended.morohashi_volume == 2 &&
              record.extended.morohashi_index == 300 &&
              record.extended.spahn_radical_strokes == 3 &&
              record.extended.spahn_radical == 4 &&
              record.extended.spahn_other_strokes == 5 &&
              record.extended.spahn_index == 6 &&
              record.extended.four_corner == 1234 &&
              record.extended.four_corner_index == 7 &&
              record.references.size() == 2 &&
              record.references[0].kind == 'F' &&
              record.references[0].value == 42,
          "Kanji extended data decoded incorrectly");
}

void test_malformed_and_limits() {
  std::string bytes = fixture();
  bytes[0] = 0;
  require_error([&] { (void)jwpqt::core::KanjiInfoDatabase::parse(bytes); },
                "Bad kanji information magic was accepted");
  bytes = fixture();
  bytes.pop_back();
  require_error(
      [&] {
        auto database = jwpqt::core::KanjiInfoDatabase::parse(bytes);
        (void)database.record(0x3021U);
      },
      "Unterminated kanji reference list was accepted");
  jwpqt::core::KanjiInfoLimits limits;
  limits.encoded_bytes = fixture().size() - 1;
  require_error(
      [&] { (void)jwpqt::core::KanjiInfoDatabase::parse(fixture(), limits); },
      "Kanji information byte budget was not enforced");
  require_error(
      [&] {
        auto database = jwpqt::core::KanjiInfoDatabase::parse(fixture());
        (void)database.record(0x2f21U);
      },
      "Out-of-range kanji lookup was accepted");
}

}  // namespace

int main(int argc, char* argv[]) {
  test_parse_record();
  test_malformed_and_limits();
  if (argc == 2) {
    std::ifstream input(argv[1], std::ios::binary);
    require(static_cast<bool>(input), "Could not open kanji information file");
    const std::string bytes{std::istreambuf_iterator<char>(input),
                            std::istreambuf_iterator<char>()};
    const auto database = jwpqt::core::KanjiInfoDatabase::parse(bytes);
    require(database.count() == 6398 && database.maximum_code() == 0x7426U &&
                database.record(0x3021U).code == 0x3021U,
            "Recovered kanji information file has unexpected metadata");
    for (std::size_t index = 0; index < database.count(); ++index) {
      const auto code = static_cast<jwpqt::core::JisCode>(
          ((0x30U + index / 94U) << 8U) | (0x21U + index % 94U));
      (void)database.record(code);
    }
  }
  return EXIT_SUCCESS;
}
