// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <stdexcept>
#include <vector>

#include "jwpqt/core/jwp_document.h"
#include "jwpqt/core/kanji_color_list.h"

namespace jwpqt::core {

class KanjiCountError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

enum class KanjiCountFilter {
  kAll,
  kExcludeColorList,
  kColorListOnly,
};

struct KanjiCountLimits {
  std::size_t documents = 1024;
  std::size_t characters = 32U * 1024U * 1024U;
  std::size_t unique_kanji = 7'690;
  std::size_t results = 7'690;
};

struct KanjiCountSummary {
  std::size_t total = 0;
  std::size_t hiragana = 0;
  std::size_t katakana = 0;
  std::size_t ascii = 0;
  std::size_t jascii = 0;
  std::size_t kanji_on_color_list = 0;
  std::size_t kanji_off_color_list = 0;
  std::size_t unique_kanji_on_color_list = 0;
  std::size_t unique_kanji_off_color_list = 0;
  std::size_t other = 0;
};

struct KanjiCountEntry {
  JisCode code = 0;
  std::size_t count = 0;
  bool on_color_list = false;
};

struct KanjiCountReport {
  KanjiCountSummary summary;
  std::vector<KanjiCountEntry> entries;
  bool truncated = false;
};

KanjiCountReport count_kanji(
    const std::vector<const JwpDocument*>& documents,
    const KanjiColorList& color_list,
    KanjiCountFilter filter = KanjiCountFilter::kAll,
    const KanjiCountLimits& limits = KanjiCountLimits{});

}  // namespace jwpqt::core
