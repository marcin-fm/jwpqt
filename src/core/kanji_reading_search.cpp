// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/kanji_reading_search.h"

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

#include "jwpqt/core/jis_unicode.h"
#include "jwpqt/core/legacy_code_page.h"

namespace jwpqt::core {
namespace {

void validate(const KanjiReadingQuery& query,
              const KanjiReadingSearchLimits& limits) {
  if (limits.results == 0 || limits.work == 0 ||
      limits.query_code_points == 0) {
    throw KanjiInfoError("Kanji reading search limits must be positive");
  }
  if (query.text.size() > limits.query_code_points) {
    throw KanjiInfoError("Kanji reading query exceeds its length limit");
  }
  if (query.strokes.minimum > query.strokes.maximum ||
      query.strokes.maximum > 30) {
    throw KanjiInfoError("Kanji reading stroke range is invalid");
  }
  switch (query.kind) {
    case KanjiReadingKind::kOn:
    case KanjiReadingKind::kKun:
    case KanjiReadingKind::kOnOrKun:
    case KanjiReadingKind::kMeaning:
    case KanjiReadingKind::kNanori:
    case KanjiReadingKind::kPinyin:
    case KanjiReadingKind::kKorean:
      return;
  }
  throw KanjiInfoError("Kanji reading search type is invalid");
}

void charge(KanjiCodeSearchReport& report,
            const KanjiReadingSearchLimits& limits) {
  if (report.work >= limits.work) {
    throw KanjiInfoError("Kanji reading search work budget is exceeded");
  }
  ++report.work;
}

void charge(KanjiCodeSearchReport& report,
            const KanjiReadingSearchLimits& limits, std::size_t amount) {
  if (amount > limits.work || report.work > limits.work - amount) {
    throw KanjiInfoError("Kanji reading search work budget is exceeded");
  }
  report.work += amount;
}

bool append(KanjiCodeSearchReport& report,
            const KanjiReadingSearchLimits& limits, JisCode code) {
  if (report.matches.size() >= limits.results) {
    report.truncated = true;
    return false;
  }
  report.matches.push_back({code, false});
  return true;
}

JisCode code_at(std::size_t index) {
  return static_cast<JisCode>(((0x30U + index / 94U) << 8U) |
                              (0x21U + index % 94U));
}

char32_t ascii_lower(char32_t value) {
  if (value >= U'A' && value <= U'Z') return value + (U'a' - U'A');
  return value;
}

std::u32string normalize_ascii_query(const KanjiReadingQuery& query) {
  std::u32string result;
  result.reserve(query.text.size());
  const auto tone = [](char32_t vowel, unsigned number) {
    if (number == 0 || number == 4) return vowel;
    switch (vowel) {
      case U'a': {
        constexpr std::array<char32_t, 4> values{
            U'a', U'\u00e1', U'\u00e2', U'\u00e0'};
        return values[number];
      }
      case U'e': {
        constexpr std::array<char32_t, 4> values{
            U'e', U'\u00e9', U'\u00ea', U'\u00e8'};
        return values[number];
      }
      case U'i': {
        constexpr std::array<char32_t, 4> values{
            U'i', U'\u00ed', U'\u00ee', U'\u00ec'};
        return values[number];
      }
      case U'o': {
        constexpr std::array<char32_t, 4> values{
            U'o', U'\u00f3', U'\u00f4', U'\u00f2'};
        return values[number];
      }
      case U'u': {
        constexpr std::array<char32_t, 4> values{
            U'u', U'\u00fa', U'\u00fb', U'\u00f9'};
        return values[number];
      }
      default:
        return vowel;
    }
  };
  for (std::size_t index = 0; index < query.text.size(); ++index) {
    char32_t value = ascii_lower(query.text[index]);
    if (value == U'\0' ||
        !unicode_to_legacy_byte(value, kDefaultLegacyCodePage).has_value()) {
      throw KanjiInfoError("Kanji reading text query is not single-byte text");
    }
    if (query.kind == KanjiReadingKind::kPinyin &&
        index + 1 < query.text.size() && query.text[index + 1] >= U'0' &&
        query.text[index + 1] <= U'9') {
      const unsigned number = static_cast<unsigned>(query.text[index + 1] - U'0');
      value = tone(value, number <= 4 ? number : 0);
      ++index;
    }
    result.push_back(value);
  }
  return result;
}

std::vector<std::uint8_t> reading_bytes(std::u32string_view text) {
  std::vector<std::uint8_t> result;
  result.reserve(text.size());
  bool in_okurigana = false;
  bool mark_okurigana = false;
  for (const char32_t value : text) {
    if (value == U'-' || value == U'\u30fc' || value == U'\u2015') {
      result.push_back(0x1fU);
      continue;
    }
    if (value == U'(' || value == U'\uff08') {
      if (in_okurigana) {
        throw KanjiInfoError("Kanji reading has nested okurigana markers");
      }
      in_okurigana = true;
      mark_okurigana = true;
      continue;
    }
    if (value == U')' || value == U'\uff09') {
      if (!in_okurigana || mark_okurigana) {
        throw KanjiInfoError("Kanji reading has an empty okurigana marker");
      }
      in_okurigana = false;
      continue;
    }
    const auto code = unicode_to_jis_x0208(value);
    if (!code.has_value() ||
        ((*code & 0xff00U) != 0x2400U && (*code & 0xff00U) != 0x2500U)) {
      throw KanjiInfoError("Kanji reading query contains a non-kana character");
    }
    std::uint8_t encoded = static_cast<std::uint8_t>(*code & 0xffU);
    if (mark_okurigana) {
      encoded = static_cast<std::uint8_t>(encoded | 0x80U);
      mark_okurigana = false;
    }
    result.push_back(encoded);
  }
  if (in_okurigana) {
    throw KanjiInfoError("Kanji reading has an unterminated okurigana marker");
  }
  return result;
}

bool kana_matches(const std::vector<std::uint8_t>& query,
                  const std::vector<std::uint8_t>& candidate, bool flexible,
                  KanjiCodeSearchReport& report,
                  const KanjiReadingSearchLimits& limits) {
  const std::uint8_t mask = flexible ? 0x7fU : 0xffU;
  std::size_t start = 0;
  while (true) {
    std::size_t index = 0;
    while (start + index < candidate.size() && index < query.size()) {
      charge(report, limits);
      if ((candidate[start + index] & mask) != (query[index] & mask)) break;
      ++index;
    }
    if (index == query.size()) {
      if (start + index == candidate.size() ||
          (candidate[start + index] & 0x80U) != 0 ||
          (flexible && candidate[start + index] == 0x1fU)) {
        return true;
      }
    }
    if (start >= candidate.size() || candidate[start] != 0x1fU) return false;
    ++start;
  }
}

bool ascii_prefix(std::u32string_view query, std::u32string_view candidate,
                  std::size_t start, KanjiCodeSearchReport& report,
                  const KanjiReadingSearchLimits& limits) {
  if (query.size() > candidate.size() - start) return false;
  for (std::size_t index = 0; index < query.size(); ++index) {
    charge(report, limits);
    if (query[index] != ascii_lower(candidate[start + index])) return false;
  }
  return true;
}

bool ascii_matches(std::u32string_view query, std::u32string_view candidate,
                   bool fragments, KanjiCodeSearchReport& report,
                   const KanjiReadingSearchLimits& limits) {
  charge(report, limits, candidate.size());
  if (fragments) {
    for (std::size_t start = 0; start <= candidate.size(); ++start) {
      charge(report, limits);
      if (ascii_prefix(query, candidate, start, report, limits)) return true;
    }
    return false;
  }
  std::size_t start = 0;
  while (start <= candidate.size()) {
    charge(report, limits);
    if (ascii_prefix(query, candidate, start, report, limits)) {
      const std::size_t end = start + query.size();
      if (end == candidate.size() || candidate[end] == U',' ||
          candidate[end] == U' ') {
        return true;
      }
    }
    const std::size_t space = candidate.find(U' ', start);
    if (space == std::u32string_view::npos) return false;
    start = space + 1;
  }
  return false;
}

bool reading_list_matches(const std::vector<std::u32string>& readings,
                          const std::vector<std::uint8_t>& query,
                          bool flexible, KanjiCodeSearchReport& report,
                          const KanjiReadingSearchLimits& limits) {
  for (const std::u32string& reading : readings) {
    charge(report, limits);
    charge(report, limits, reading.size());
    const std::vector<std::uint8_t> candidate = reading_bytes(reading);
    if (kana_matches(query, candidate, flexible, report, limits)) return true;
  }
  return false;
}

}  // namespace

KanjiCodeSearchReport search_kanji_readings(
    const KanjiInfoDatabase& information, const KanjiReadingQuery& query,
    const KanjiReadingSearchLimits& limits) {
  validate(query, limits);
  const bool ascii = query.kind == KanjiReadingKind::kMeaning ||
                     query.kind == KanjiReadingKind::kPinyin ||
                     query.kind == KanjiReadingKind::kKorean;
  const std::u32string ascii_query =
      ascii ? normalize_ascii_query(query) : std::u32string{};
  const std::vector<std::uint8_t> kana_query =
      ascii ? std::vector<std::uint8_t>{} : reading_bytes(query.text);

  KanjiCodeSearchReport report;
  for (std::size_t index = 0; index < information.count(); ++index) {
    charge(report, limits);
    const JisCode code = code_at(index);
    const KanjiInfoRecord record = information.record(code);
    if (record.fixed.strokes < query.strokes.minimum ||
        record.fixed.strokes > query.strokes.maximum) {
      continue;
    }

    bool match = false;
    switch (query.kind) {
      case KanjiReadingKind::kOn:
        match = reading_list_matches(record.on_readings, kana_query, false,
                                    report, limits);
        break;
      case KanjiReadingKind::kKun:
        match = reading_list_matches(record.kun_readings, kana_query,
                                    query.flexible_kun, report, limits);
        break;
      case KanjiReadingKind::kOnOrKun:
        match = reading_list_matches(record.on_readings, kana_query,
                                    query.flexible_kun, report, limits) ||
                reading_list_matches(record.kun_readings, kana_query,
                                    query.flexible_kun, report, limits);
        break;
      case KanjiReadingKind::kMeaning:
        for (const std::u32string& meaning : record.meanings) {
          charge(report, limits);
          if (ascii_matches(ascii_query, meaning, query.partial_words, report,
                            limits)) {
            match = true;
            break;
          }
        }
        break;
      case KanjiReadingKind::kNanori:
        match = reading_list_matches(record.nanori, kana_query, false, report,
                                    limits);
        break;
      case KanjiReadingKind::kPinyin:
        match = ascii_matches(ascii_query, record.pinyin, false, report, limits);
        break;
      case KanjiReadingKind::kKorean:
        match = ascii_matches(ascii_query, record.korean, false, report, limits);
        break;
    }
    if (match && !append(report, limits, code)) return report;
  }
  return report;
}

}  // namespace jwpqt::core
