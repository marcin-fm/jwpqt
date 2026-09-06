// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/kanji_code_search.h"

namespace jwpqt::core {
namespace {

void validate_limits(const KanjiCodeSearchLimits& limits) {
  if (limits.results == 0 || limits.work == 0) {
    throw KanjiInfoError("Kanji code search limits must be positive");
  }
}

void validate_range(const KanjiNumericRange& range, std::uint8_t maximum,
                    const char* name) {
  if (range.minimum > range.maximum || range.maximum > maximum) {
    throw KanjiInfoError(std::string("Kanji ") + name +
                         " search range is invalid");
  }
}

JisCode code_at(std::size_t index) {
  return static_cast<JisCode>(((0x30U + index / 94U) << 8U) |
                              (0x21U + index % 94U));
}

void charge(KanjiCodeSearchReport& report,
            const KanjiCodeSearchLimits& limits) {
  if (report.work >= limits.work) {
    throw KanjiInfoError("Kanji code search work budget is exceeded");
  }
  ++report.work;
}

bool append(KanjiCodeSearchReport& report,
            const KanjiCodeSearchLimits& limits, JisCode code,
            bool alternate) {
  if (report.matches.size() >= limits.results) {
    report.truncated = true;
    return false;
  }
  report.matches.push_back({code, alternate});
  return true;
}

bool within(std::uint8_t value, const KanjiNumericRange& range) {
  return value >= range.minimum && value <= range.maximum;
}

bool skip_matches(std::uint8_t type, std::uint8_t first,
                  std::uint8_t second, const KanjiSkipQuery& query) {
  return within(type, query.type) && within(first, query.first) &&
         within(second, query.second);
}

bool four_corner_matches(std::uint16_t main, std::uint8_t index,
                         const KanjiFourCornerQuery& query) {
  if (main == 0x3fffU || main > 9999U || index > 9U) return false;
  for (int position = 3; position >= 0; --position) {
    const std::uint8_t digit = static_cast<std::uint8_t>(main % 10U);
    if (query.digits[static_cast<std::size_t>(position)] >= 0 &&
        query.digits[static_cast<std::size_t>(position)] !=
            static_cast<std::int8_t>(digit)) {
      return false;
    }
    main = static_cast<std::uint16_t>(main / 10U);
  }
  return query.digits[4] < 0 ||
         query.digits[4] == static_cast<std::int8_t>(index);
}

std::uint8_t normalized_nelson_bushu(std::uint8_t value) {
  if (value == 23) return 22;
  if (value == 35) return 34;
  return value;
}

std::uint8_t normalized_classical_bushu(std::uint8_t value,
                                        std::uint8_t nelson) {
  if (value == 23) value = 22;
  if (value == 25) value = 34;
  return value == 0 ? nelson : value;
}

}  // namespace

KanjiCodeSearchReport search_kanji_skip(
    const KanjiInfoDatabase& information, const KanjiSkipQuery& query,
    const KanjiCodeSearchLimits& limits) {
  validate_limits(limits);
  validate_range(query.type, 4, "SKIP type");
  validate_range(query.first, 20, "SKIP first value");
  validate_range(query.second, 24, "SKIP second value");

  KanjiCodeSearchReport report;
  for (std::size_t index = 0; index < information.count(); ++index) {
    charge(report, limits);
    const JisCode code = code_at(index);
    const KanjiInfoRecord record = information.record(code);
    if (skip_matches(record.fixed.skip.type, record.fixed.skip.first,
                     record.fixed.skip.second, query) &&
        !append(report, limits, code, false)) {
      return report;
    }
    if (!query.include_misclassifications) continue;
    for (const KanjiInfoCode& reference : record.references) {
      charge(report, limits);
      if (reference.kind != 'z') continue;
      const std::uint8_t type =
          static_cast<std::uint8_t>((reference.value >> 10U) & 7U);
      const std::uint8_t first =
          static_cast<std::uint8_t>((reference.value >> 5U) & 31U);
      const std::uint8_t second =
          static_cast<std::uint8_t>(reference.value & 31U);
      if (skip_matches(type, first, second, query) &&
          !append(report, limits, code, true)) {
        return report;
      }
    }
  }
  return report;
}

KanjiCodeSearchReport search_kanji_four_corner(
    const KanjiInfoDatabase& information, const KanjiFourCornerQuery& query,
    const KanjiCodeSearchLimits& limits) {
  validate_limits(limits);
  for (const std::int8_t digit : query.digits) {
    if (digit < -1 || digit > 9) {
      throw KanjiInfoError("Four-corner search digit is invalid");
    }
  }

  KanjiCodeSearchReport report;
  for (std::size_t index = 0; index < information.count(); ++index) {
    charge(report, limits);
    const JisCode code = code_at(index);
    const KanjiInfoRecord record = information.record(code);
    if (record.has_extended &&
        four_corner_matches(record.extended.four_corner,
                            record.extended.four_corner_index, query)) {
      if (!append(report, limits, code, false)) return report;
      continue;
    }
    if (!record.has_extended) continue;
    for (const KanjiInfoCode& reference : record.references) {
      charge(report, limits);
      if (reference.kind == 'Q' &&
          four_corner_matches(
              reference.value, record.extended.four_corner_second_index,
              query)) {
        if (!append(report, limits, code, true)) return report;
        break;
      }
    }
  }
  return report;
}

KanjiCodeSearchReport search_kanji_bushu(
    const KanjiInfoDatabase& information, const KanjiBushuQuery& query,
    const KanjiCodeSearchLimits& limits) {
  validate_limits(limits);
  validate_range(query.radical, 255, "Bushu radical");
  validate_range(query.strokes, 30, "Bushu stroke");
  if (!query.nelson && !query.classical) {
    throw KanjiInfoError("Bushu search requires a radical system");
  }

  KanjiCodeSearchReport report;
  for (std::size_t index = 0; index < information.count(); ++index) {
    charge(report, limits);
    const JisCode code = code_at(index);
    const KanjiInfoFixed fixed = information.record(code).fixed;
    const std::uint8_t nelson = normalized_nelson_bushu(fixed.bushu);
    const std::uint8_t classical =
        normalized_classical_bushu(fixed.classical_bushu, nelson);
    if (within(fixed.strokes, query.strokes) &&
        ((query.nelson && within(nelson, query.radical)) ||
         (query.classical && within(classical, query.radical))) &&
        !append(report, limits, code, false)) {
      return report;
    }
  }
  return report;
}

KanjiCodeSearchReport search_kanji_spahn(
    const KanjiInfoDatabase& information, const KanjiSpahnQuery& query,
    const KanjiCodeSearchLimits& limits) {
  validate_limits(limits);
  validate_range(query.radical_strokes, 11, "Spahn radical stroke");
  validate_range(query.radical, 19, "Spahn radical");
  validate_range(query.other_strokes, 26, "Spahn other stroke");
  validate_range(query.index, 47, "Spahn kanji index");

  KanjiCodeSearchReport report;
  for (std::size_t index = 0; index < information.count(); ++index) {
    charge(report, limits);
    const JisCode code = code_at(index);
    const KanjiInfoRecord record = information.record(code);
    if (!record.has_extended) continue;
    const KanjiInfoExtended& extended = record.extended;
    if (within(extended.spahn_radical_strokes, query.radical_strokes) &&
        within(extended.spahn_radical, query.radical) &&
        within(extended.spahn_other_strokes, query.other_strokes) &&
        within(extended.spahn_index, query.index) &&
        !append(report, limits, code, false)) {
      return report;
    }
  }
  return report;
}

KanjiCodeSearchReport search_kanji_index(
    const KanjiInfoDatabase& information, const KanjiIndexQuery& query,
    const KanjiCodeSearchLimits& limits) {
  validate_limits(limits);
  constexpr std::array<char, 21> references{
      0, 0, 0, 0, 0, 0, 'H', 'I', 'E', 'K', 'L', 'O', 'N', 'D', 'F',
      'S', 'T', 'C', 'J', 'B', 'G'};
  const auto type = static_cast<std::size_t>(query.type);
  if (type >= references.size()) throw KanjiInfoError("Unknown kanji index type");
  if (query.index > 0xffffU ||
      (query.type == KanjiIndexType::kMorohashiVolume &&
       (query.volume > 15U || query.index > 8191U)) ||
      (query.type == KanjiIndexType::kBusyPeople &&
       (query.volume > 255U || query.index > 255U))) {
    throw KanjiInfoError("Kanji index search value is out of range");
  }
  const auto wanted = query.type == KanjiIndexType::kBusyPeople
      ? (query.volume << 8U) | query.index : query.index;
  KanjiCodeSearchReport report;
  for (std::size_t index = 0; index < information.count(); ++index) {
    charge(report, limits);
    const JisCode code = code_at(index);
    const auto record = information.record(code);
    std::uint32_t value = 0;
    switch (query.type) {
      case KanjiIndexType::kNelson: value = record.fixed.nelson; break;
      case KanjiIndexType::kHaig: value = record.fixed.haig; break;
      case KanjiIndexType::kHalpern: value = record.fixed.halpern; break;
      case KanjiIndexType::kGrade: value = record.fixed.grade; break;
      case KanjiIndexType::kMorohashiFull: value = record.extended.morohashi_long; break;
      case KanjiIndexType::kMorohashiVolume:
        if (record.extended.morohashi_volume != query.volume) continue;
        value = record.extended.morohashi_index;
        break;
      default:
        // Legacy primary values default to zero; cross-references start at lowercase.
        for (const auto& reference : record.references) {
          charge(report, limits);
          if (reference.kind >= 'a' && reference.kind <= 'z') break;
          if (reference.kind == references[type]) value = reference.value;
        }
        break;
    }
    if (value == wanted && !append(report, limits, code, false)) return report;
  }
  return report;
}

}  // namespace jwpqt::core
