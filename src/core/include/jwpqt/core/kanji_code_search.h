// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "jwpqt/core/kanji_info.h"

namespace jwpqt::core {

struct KanjiCodeSearchLimits {
  std::size_t results = 3000;
  std::size_t work = 1'000'000;
};

struct KanjiCodeMatch {
  JisCode code = 0;
  bool alternate = false;
};

struct KanjiCodeSearchReport {
  std::vector<KanjiCodeMatch> matches;
  std::size_t work = 0;
  bool truncated = false;
};

struct KanjiNumericRange {
  std::uint8_t minimum = 0;
  std::uint8_t maximum = 0;
};

struct KanjiSkipQuery {
  KanjiNumericRange type{0, 4};
  KanjiNumericRange first{0, 20};
  KanjiNumericRange second{0, 24};
  bool include_misclassifications = false;
};

struct KanjiFourCornerQuery {
  // -1 is a wildcard; otherwise every value is a decimal digit 0..9.
  std::array<std::int8_t, 5> digits{-1, -1, -1, -1, -1};
};

struct KanjiBushuQuery {
  KanjiNumericRange radical{0, 255};
  KanjiNumericRange strokes{0, 30};
  bool nelson = true;
  bool classical = true;
};

struct KanjiSpahnQuery {
  KanjiNumericRange radical_strokes{0, 11};
  KanjiNumericRange radical{0, 19};
  KanjiNumericRange other_strokes{0, 26};
  KanjiNumericRange index{0, 47};
};

KanjiCodeSearchReport search_kanji_skip(
    const KanjiInfoDatabase& information, const KanjiSkipQuery& query,
    const KanjiCodeSearchLimits& limits = KanjiCodeSearchLimits{});
KanjiCodeSearchReport search_kanji_four_corner(
    const KanjiInfoDatabase& information, const KanjiFourCornerQuery& query,
    const KanjiCodeSearchLimits& limits = KanjiCodeSearchLimits{});
KanjiCodeSearchReport search_kanji_bushu(
    const KanjiInfoDatabase& information, const KanjiBushuQuery& query,
    const KanjiCodeSearchLimits& limits = KanjiCodeSearchLimits{});
KanjiCodeSearchReport search_kanji_spahn(
    const KanjiInfoDatabase& information, const KanjiSpahnQuery& query,
    const KanjiCodeSearchLimits& limits = KanjiCodeSearchLimits{});

}  // namespace jwpqt::core
