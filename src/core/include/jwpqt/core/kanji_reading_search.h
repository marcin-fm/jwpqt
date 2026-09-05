// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <string>

#include "jwpqt/core/kanji_code_search.h"

namespace jwpqt::core {

enum class KanjiReadingKind {
  kOn,
  kKun,
  kOnOrKun,
  kMeaning,
  kNanori,
  kPinyin,
  kKorean,
};

struct KanjiReadingQuery {
  KanjiReadingKind kind = KanjiReadingKind::kOnOrKun;
  std::u32string text;
  KanjiNumericRange strokes{0, 30};
  bool flexible_kun = false;
  bool partial_words = false;
};

struct KanjiReadingSearchLimits {
  std::size_t results = 3000;
  std::size_t work = 2'000'000;
  std::size_t query_code_points = 1024;
};

KanjiCodeSearchReport search_kanji_readings(
    const KanjiInfoDatabase& information, const KanjiReadingQuery& query,
    const KanjiReadingSearchLimits& limits = KanjiReadingSearchLimits{});

}  // namespace jwpqt::core
