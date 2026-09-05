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

}  // namespace jwpqt::core
