// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/edict_deinflection.h"

#include <optional>
#include <string_view>
#include <utility>

#include "jwpqt/core/kana_input.h"

namespace jwpqt::core {
namespace {

constexpr JisCode kHiraganaBase = 0x2400;
constexpr JisCode kHiraganaI = 0x2424;
constexpr JisCode kHiraganaU = 0x2426;
constexpr JisCode kHiraganaKu = 0x242f;
constexpr JisCode kHiraganaGu = 0x2430;
constexpr JisCode kHiraganaSu = 0x2439;
constexpr JisCode kSmallTsu = 0x2443;
constexpr JisCode kHiraganaTsu = 0x2444;
constexpr JisCode kHiraganaNu = 0x244c;
constexpr JisCode kHiraganaBu = 0x2456;
constexpr JisCode kHiraganaMu = 0x2460;
constexpr JisCode kHiraganaRu = 0x246b;
constexpr JisCode kHiraganaWa = 0x246f;
constexpr JisCode kHiraganaN = 0x2473;
constexpr JisCode kJapaneseAsterisk = 0x2176;

bool is_pattern(JisCode token) noexcept {
  switch (token & 0x7f7fU) {
    case 0x2129:  // Question mark.
    case 0x214e:  // Left bracket.
    case 0x214f:  // Right bracket.
    case 0x2174:  // Number sign.
    case kJapaneseAsterisk:
      return true;
    default:
      return false;
  }
}

bool is_hiragana(JisCode token) noexcept {
  return (token & 0x7f00U) == kHiraganaBase;
}

bool is_single_kana(const JwpText& key) noexcept {
  return key.size() == 1 && is_hiragana(key.front());
}

std::optional<JisCode> godan_ending(char initial) noexcept {
  switch (initial) {
    case 'u':
      return kHiraganaU;
    case 'k':
      return kHiraganaKu;
    case 'g':
      return kHiraganaGu;
    case 's':
      return kHiraganaSu;
    case 't':
      return kHiraganaTsu;
    case 'n':
      return kHiraganaNu;
    case 'b':
      return kHiraganaBu;
    case 'm':
      return kHiraganaMu;
    case 'r':
      return kHiraganaRu;
    default:
      return std::nullopt;
  }
}

class Generator {
 public:
  explicit Generator(const EdictDeinflectionOptions& options)
      : options_(options) {}

  std::vector<EdictDeinflectionQuery> run(JwpText key) {
    bool first = true;
    JisCode truncated = 0;
    std::size_t pass = 0;
    while (!key.empty()) {
      pass_ = pass++;
      spend_work();
      if (!first && !is_single_kana(key) &&
          truncated != kJapaneseAsterisk) {
        emit(key);
      }
      first = false;
      emit_endings(key);
      truncated = key.back();
      key.pop_back();
    }
    return std::move(results_);
  }

 private:
  void spend_work() {
    if (work_steps_ >= options_.work_steps) {
      throw EdictDeinflectionError(
          "EDICT deinflection work exceeds the configured limit");
    }
    ++work_steps_;
  }

  void emit(const JwpText& key) {
    spend_work();
    if (results_.size() >= options_.queries) {
      throw EdictDeinflectionError(
          "EDICT deinflection queries exceed the configured limit");
    }
    results_.push_back({key, pass_});
  }

  void append(const JwpText& key, JisCode ending) {
    JwpText candidate = key;
    candidate.push_back(ending);
    emit(candidate);
  }

  void replace_end(const JwpText& key, JisCode ending) {
    if (key.size() <= 1 || is_single_kana(key)) {
      return;
    }
    JwpText candidate = key;
    candidate.back() = ending;
    emit(candidate);
  }

  void emit_general_endings(const JwpText& key, JisCode last) {
    const JisCode spelling_token = last == kHiraganaWa ? 0x2422 : last;
    const std::optional<std::string_view> spelling =
        romaji_for_kana(spelling_token);
    if (!spelling || spelling->empty()) {
      return;
    }

    const char final = spelling->back();
    if (final == 'i' || final == 'e') {
      append(key, kHiraganaRu);
    }
    if (final != 'u') {
      const char initial = spelling->size() == 1 ? 'u' : spelling->front();
      if (const std::optional<JisCode> ending = godan_ending(initial)) {
        replace_end(key, *ending);
      }
    }
    if (options_.include_i_adjectives) {
      append(key, kHiraganaI);
    }
  }

  void emit_endings(const JwpText& key) {
    const JisCode last = key.back();
    if (!is_hiragana(last)) {
      append(key, kHiraganaRu);
      if (options_.include_i_adjectives) {
        append(key, kHiraganaI);
      }
      return;
    }

    if (last == kHiraganaI) {
      replace_end(key, kHiraganaKu);
      replace_end(key, kHiraganaGu);
    } else if (last == kSmallTsu) {
      replace_end(key, kHiraganaU);
      replace_end(key, kHiraganaTsu);
      replace_end(key, kHiraganaRu);
      return;
    } else if (last == kHiraganaN) {
      replace_end(key, kHiraganaNu);
      replace_end(key, kHiraganaBu);
      replace_end(key, kHiraganaMu);
      return;
    }
    emit_general_endings(key, last);
  }

  const EdictDeinflectionOptions& options_;
  std::size_t work_steps_ = 0;
  std::size_t pass_ = 0;
  std::vector<EdictDeinflectionQuery> results_;
};

}  // namespace

bool EdictDeinflectionQuery::operator==(
    const EdictDeinflectionQuery& other) const noexcept {
  return key == other.key && pass == other.pass;
}

std::vector<EdictDeinflectionQuery> generate_edict_deinflection_steps(
    const EdictQuery& query, const EdictDeinflectionOptions& options) {
  const EdictQuery validated = prepare_edict_query(query.key);
  if (validated.kind == EdictQueryKind::kAscii) {
    return {};
  }
  for (const JisCode token : validated.key) {
    if (is_pattern(token)) {
      throw EdictDeinflectionError(
          "EDICT pattern queries require the pattern search engine");
    }
  }
  return Generator(options).run(validated.key);
}

std::vector<JwpText> generate_edict_deinflection_queries(
    const EdictQuery& query, const EdictDeinflectionOptions& options) {
  const std::vector<EdictDeinflectionQuery> steps =
      generate_edict_deinflection_steps(query, options);
  std::vector<JwpText> queries;
  queries.reserve(steps.size());
  for (const EdictDeinflectionQuery& step : steps) {
    queries.push_back(step.key);
  }
  return queries;
}

}  // namespace jwpqt::core
