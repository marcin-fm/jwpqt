// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/kanji_count.h"

#include <algorithm>
#include <limits>
#include <unordered_map>
#include <utility>

namespace jwpqt::core {
namespace {

constexpr JisCode kPageMask = 0x7f00U;
constexpr JisCode kJasciiPage = 0x2300U;
constexpr JisCode kHiraganaPage = 0x2400U;
constexpr JisCode kKatakanaPage = 0x2500U;
constexpr JisCode kLongVowel = 0x213cU;
constexpr JisCode kKanjiBase = 0x3000U;

void validate_limits(const KanjiCountLimits& limits) {
  if (limits.documents == 0 || limits.characters == 0 ||
      limits.unique_kanji == 0 || limits.results == 0) {
    throw KanjiCountError("Kanji count limits must be positive");
  }
}

void increment(std::size_t& value) {
  if (value == std::numeric_limits<std::size_t>::max())
    throw KanjiCountError("Kanji count overflows");
  ++value;
}

bool include_entry(bool on_color_list, KanjiCountFilter filter) {
  switch (filter) {
    case KanjiCountFilter::kAll:
      return true;
    case KanjiCountFilter::kExcludeColorList:
      return !on_color_list;
    case KanjiCountFilter::kColorListOnly:
      return on_color_list;
  }
  throw KanjiCountError("Kanji count filter is invalid");
}

}  // namespace

KanjiCountReport count_kanji(const std::vector<const JwpDocument*>& documents,
                             const KanjiColorList& color_list,
                             KanjiCountFilter filter,
                             const KanjiCountLimits& limits) {
  validate_limits(limits);
  if (documents.size() > limits.documents)
    throw KanjiCountError("Kanji count document limit exceeded");
  (void)include_entry(false, filter);

  KanjiCountReport report;
  std::unordered_map<JisCode, std::size_t> frequencies;
  frequencies.reserve(std::min(limits.unique_kanji, std::size_t{1024}));
  std::size_t work = 0;
  for (const JwpDocument* document : documents) {
    if (document == nullptr)
      throw KanjiCountError("Kanji count document is null");
    for (const JwpParagraph& paragraph : document->paragraphs) {
      for (const JisCode code : paragraph.text) {
        if (work >= limits.characters)
          throw KanjiCountError("Kanji count character limit exceeded");
        ++work;
        increment(report.summary.total);
        const JisCode page = static_cast<JisCode>(code & kPageMask);
        if (page == kHiraganaPage) {
          increment(report.summary.hiragana);
        } else if (page == kKatakanaPage || code == kLongVowel) {
          increment(report.summary.katakana);
        } else if (page == 0) {
          increment(report.summary.ascii);
        } else if (page == kJasciiPage) {
          increment(report.summary.jascii);
        } else if (code >= kKanjiBase) {
          const auto found = frequencies.find(code);
          if (found == frequencies.end()) {
            if (frequencies.size() >= limits.unique_kanji)
              throw KanjiCountError("Kanji count unique-character limit exceeded");
            frequencies.emplace(code, 1);
          } else {
            increment(found->second);
          }
        } else {
          increment(report.summary.other);
        }
      }
    }
  }

  report.entries.reserve(frequencies.size());
  for (const auto& [code, count] : frequencies) {
    const bool listed = color_list.contains(code);
    if (listed) {
      report.summary.kanji_on_color_list += count;
      increment(report.summary.unique_kanji_on_color_list);
    } else {
      report.summary.kanji_off_color_list += count;
      increment(report.summary.unique_kanji_off_color_list);
    }
    if (!include_entry(listed, filter))
      continue;
    report.entries.push_back({code, count, listed});
  }
  std::sort(report.entries.begin(), report.entries.end(),
            [](const KanjiCountEntry& left, const KanjiCountEntry& right) {
              if (left.count != right.count)
                return left.count > right.count;
              return left.code < right.code;
            });
  if (report.entries.size() > limits.results) {
    report.entries.resize(limits.results);
    report.truncated = true;
  }
  return report;
}

}  // namespace jwpqt::core
