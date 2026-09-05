// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "jwpqt/core/jis_encoding.h"

namespace jwpqt::core {

inline constexpr std::uint32_t kKanjiInfoMagic = 0x34a5b4d4U;

class KanjiInfoError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

struct KanjiInfoLimits {
  std::size_t encoded_bytes = 32U * 1024U * 1024U;
  std::size_t records = 7'680;
  std::size_t strings = 100'000;
  std::size_t string_bytes = 1024U * 1024U;
  std::size_t codes = 100'000;
};

void validate_kanji_info_limits(const KanjiInfoLimits& limits);

struct KanjiInfoSkipCode {
  std::uint8_t type = 0;
  std::uint8_t first = 0;
  std::uint8_t second = 0;
};

struct KanjiInfoFixed {
  std::uint8_t bushu = 0;
  std::uint8_t strokes = 0;
  std::uint8_t grade = 0;
  KanjiInfoSkipCode skip;
  std::uint16_t halpern = 0;
  std::uint16_t nelson = 0;
  std::uint16_t haig = 0;
  std::uint8_t classical_bushu = 0;
};

struct KanjiInfoExtended {
  std::uint16_t morohashi_long = 0;
  std::uint8_t morohashi_volume = 0;
  std::uint16_t morohashi_index = 0;
  std::uint8_t spahn_radical_strokes = 0;
  std::uint8_t spahn_radical = 0;
  std::uint8_t spahn_other_strokes = 0;
  std::uint8_t spahn_index = 0;
  std::uint16_t four_corner = 0;
  std::uint8_t four_corner_index = 0;
  std::uint8_t four_corner_second_index = 0;
  bool morohashi_page = false;
  bool morohashi_cross = false;
};

struct KanjiInfoCode {
  char kind = 0;
  std::uint16_t value = 0;
};

struct KanjiInfoRecord {
  JisCode code = 0;
  KanjiInfoFixed fixed;
  std::u32string korean;
  std::u32string pinyin;
  std::vector<std::u32string> meanings;
  std::vector<std::u32string> on_readings;
  std::vector<std::u32string> kun_readings;
  std::vector<std::u32string> nanori;
  bool has_extended = false;
  KanjiInfoExtended extended;
  std::vector<KanjiInfoCode> references;
};

class KanjiInfoDatabase {
 public:
  static KanjiInfoDatabase parse(
      std::string_view bytes,
      const KanjiInfoLimits& limits = KanjiInfoLimits{});

  std::uint32_t flags() const noexcept;
  std::uint16_t count() const noexcept;
  JisCode maximum_code() const noexcept;
  bool contains(JisCode code) const noexcept;
  KanjiInfoRecord record(JisCode code) const;

 private:
  std::string bytes_;
  std::uint32_t flags_ = 0;
  std::uint16_t count_ = 0;
  JisCode maximum_code_ = 0;
  KanjiInfoLimits limits_;
};

}  // namespace jwpqt::core
