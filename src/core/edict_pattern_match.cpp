// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/edict_pattern_match.h"

#include "edict_pattern_match_internal.h"

#include <cstdint>
#include <limits>
#include <string_view>
#include <vector>

#include "jwpqt/core/jis_unicode.h"
#include "jwpqt/core/utf8.h"

namespace jwpqt::core {
namespace {

constexpr std::uint16_t kUnmappedToken = 0xffff;
constexpr std::uint16_t kKanjiRepetition = 0x2139;
constexpr std::size_t kMaximumPatternLength = 100;

struct SourceToken {
  std::uint16_t value = 0;
  std::size_t byte_begin = 0;
  std::size_t byte_end = 0;
};

[[noreturn]] void fail(std::string_view reason) {
  throw EdictPatternError(std::string(reason));
}

class WorkBudget {
 public:
  explicit WorkBudget(std::size_t limit) : limit_(limit) {}

  void consume() {
    if (steps_ >= limit_) {
      fail("EDICT pattern match exceeds its work limit");
    }
    ++steps_;
  }

  std::size_t steps() const noexcept { return steps_; }

 private:
  std::size_t limit_;
  std::size_t steps_ = 0;
};

std::size_t utf8_width(unsigned char lead) {
  if ((lead & 0x80U) == 0) {
    return 1;
  }
  if ((lead & 0xe0U) == 0xc0U) {
    return 2;
  }
  if ((lead & 0xf0U) == 0xe0U) {
    return 3;
  }
  if ((lead & 0xf8U) == 0xf0U) {
    return 4;
  }
  fail("EDICT pattern source has an invalid UTF-8 lead byte");
}

std::uint16_t unicode_token(char32_t code_point,
                            LegacyCodePage code_page) {
  if (code_point > U'\0' && code_point <= U'\u007e') {
    return static_cast<std::uint16_t>(code_point);
  }
  if (code_point == U'\u201a' || code_point == U'\u0192' ||
      code_point == U'\u201e') {
    return kUnmappedToken;
  }
  const bool prefer_extension =
      (code_page == LegacyCodePage::k1251 &&
       ((code_point >= U'\u0410' && code_point <= U'\u044f') ||
        code_point == U'\u0401' || code_point == U'\u0451')) ||
      (code_page == LegacyCodePage::k1253 &&
       code_point >= U'\u0391' && code_point <= U'\u03c9');
  if (!prefer_extension) {
    if (const std::optional<JisCode> jis =
            unicode_to_jis_x0208(code_point)) {
      return *jis;
    }
  }
  if (const std::optional<std::uint8_t> extended =
          unicode_to_legacy_byte(code_point, code_page)) {
    return *extended;
  }
  if (prefer_extension) {
    if (const std::optional<JisCode> jis =
            unicode_to_jis_x0208(code_point)) {
      return *jis;
    }
  }
  return kUnmappedToken;
}

std::vector<SourceToken> decode_record(
    const EdictDictionary& dictionary, const EdictRecord& record,
    LegacyCodePage utf8_code_page, WorkBudget& budget) {
  const std::string_view source = dictionary.source_bytes();
  if (record.byte_offset > source.size() ||
      record.byte_length > source.size() - record.byte_offset) {
    fail("EDICT pattern record span is invalid");
  }

  std::vector<SourceToken> result;
  std::size_t offset = record.byte_offset;
  const std::size_t end = record.byte_offset + record.byte_length;
  while (offset < end) {
    budget.consume();
    const std::size_t begin = offset;
    std::uint16_t value = 0;
    if (dictionary.encoding() == EdictEncoding::kUtf8) {
      const std::size_t width =
          utf8_width(static_cast<unsigned char>(source[offset]));
      if (width > end - offset) {
        fail("EDICT pattern UTF-8 character is truncated");
      }
      try {
        const std::u32string decoded = decode_utf8(source.substr(offset, width));
        if (decoded.size() != 1) {
          fail("EDICT pattern UTF-8 character is invalid");
        }
        value = unicode_token(decoded.front(), utf8_code_page);
      } catch (const EdictPatternError&) {
        throw;
      } catch (const std::exception& error) {
        throw EdictPatternError(error.what());
      }
      offset += width;
    } else if (dictionary.encoding() == EdictEncoding::kEucJp) {
      const auto first = static_cast<unsigned char>(source[offset++]);
      if ((first & 0x80U) == 0) {
        value = first;
      } else if (first == 0x8fU) {
        if (end - offset < 2) {
          fail("EDICT pattern JIS X 0212 character is truncated");
        }
        const std::optional<std::uint8_t> mapped = edict_euc_0212_byte(
            static_cast<std::uint8_t>(source[offset]),
            static_cast<std::uint8_t>(source[offset + 1]));
        if (!mapped.has_value()) {
          fail("EDICT pattern JIS X 0212 character is unsupported");
        }
        value = *mapped;
        offset += 2;
      } else {
        if (offset == end) {
          fail("EDICT pattern EUC-JP character is truncated");
        }
        const auto second = static_cast<unsigned char>(source[offset++]);
        const std::uint16_t encoded = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(first) << 8U) |
            static_cast<std::uint16_t>(second));
        value = static_cast<std::uint16_t>(
            encoded & static_cast<std::uint16_t>(0x7f7fU));
      }
    } else {
      fail("EDICT pattern dictionary encoding is unsupported");
    }
    result.push_back({value, begin, offset});
  }
  return result;
}

std::size_t token_boundary(const std::vector<SourceToken>& tokens,
                           std::size_t byte_offset,
                           std::size_t record_end, WorkBudget& budget) {
  if (byte_offset == record_end) {
    return tokens.size();
  }
  std::size_t lower = 0;
  std::size_t upper = tokens.size();
  while (lower < upper) {
    budget.consume();
    const std::size_t middle = lower + (upper - lower) / 2;
    if (tokens[middle].byte_begin < byte_offset) {
      lower = middle + 1;
    } else {
      upper = middle;
    }
  }
  if (lower < tokens.size() && tokens[lower].byte_begin == byte_offset) {
    return lower;
  }
  fail("EDICT pattern match is not at a character boundary");
}

bool beginning_delimiter(std::uint16_t token) noexcept {
  return token == '[' || token == ' ' || token == '/';
}

bool ending_delimiter(std::uint16_t token) noexcept {
  return token == ']' || token == ' ' || token == '/';
}

bool kanji_token(std::uint16_t token) noexcept {
  return token >= 0x3000U || token == kKanjiRepetition;
}

std::uint16_t normalize_anchor_token(std::uint16_t token) noexcept {
  if (token >= 'A' && token <= 'Z') {
    return static_cast<std::uint16_t>(token + ('a' - 'A'));
  }
  if ((token & 0xff00U) == 0x2500U) {
    return static_cast<std::uint16_t>(0x2400U | (token & 0x00ffU));
  }
  return token;
}

std::uint16_t normalize_literal_token(std::uint16_t token,
                                      EdictEncoding encoding) noexcept {
  if (encoding == EdictEncoding::kEucJp && token >= 'A' && token <= 'Z') {
    return static_cast<std::uint16_t>(token + ('a' - 'A'));
  }
  return token;
}

class Matcher {
 public:
  Matcher(const std::vector<SourceToken>& tokens, EdictEncoding encoding,
          WorkBudget& budget)
      : tokens_(tokens), encoding_(encoding), budget_(budget) {}

  std::optional<std::size_t> prefix(const JwpText& pattern,
                                    std::size_t cursor) {
    return prefix_at(pattern, pattern.size(), cursor);
  }

  std::optional<std::size_t> postfix(const JwpText& pattern,
                                     std::size_t cursor) {
    return postfix_at(pattern, 0, cursor);
  }

 private:
  void consume() {
    budget_.consume();
  }

  std::optional<std::size_t> postfix_at(const JwpText& pattern,
                                        std::size_t index,
                                        std::size_t cursor) {
    consume();
    if (index == pattern.size()) {
      return cursor;
    }

    const std::uint16_t expected = pattern[index];
    if (expected == '[') {
      return std::nullopt;
    }
    if (expected == ']') {
      if (cursor == tokens_.size() ||
          ending_delimiter(tokens_[cursor].value)) {
        return postfix_at(pattern, index + 1, cursor);
      }
      return std::nullopt;
    }
    if (expected == '*') {
      std::size_t farthest = cursor;
      while (farthest < tokens_.size() &&
             !ending_delimiter(tokens_[farthest].value)) {
        consume();
        ++farthest;
      }
      for (std::size_t candidate = farthest;; --candidate) {
        if (const std::optional<std::size_t> matched =
                postfix_at(pattern, index + 1, candidate)) {
          return matched;
        }
        if (candidate == cursor) {
          break;
        }
      }
      return std::nullopt;
    }
    if (cursor == tokens_.size()) {
      return std::nullopt;
    }
    const std::uint16_t actual = tokens_[cursor].value;
    const bool matched =
        expected == '?'
            ? !ending_delimiter(actual)
            : expected == '#'
                  ? !ending_delimiter(actual) && kanji_token(actual)
                  : normalize_literal_token(actual, encoding_) == expected;
    if (matched) {
      return postfix_at(pattern, index + 1, cursor + 1);
    }
    return std::nullopt;
  }

  std::optional<std::size_t> prefix_at(const JwpText& pattern,
                                       std::size_t count,
                                       std::size_t cursor) {
    consume();
    if (count == 0) {
      return cursor;
    }

    const std::uint16_t expected = pattern[count - 1];
    if (expected == ']') {
      return std::nullopt;
    }
    if (expected == '[') {
      if (cursor == 0 || beginning_delimiter(tokens_[cursor - 1].value)) {
        return prefix_at(pattern, count - 1, cursor);
      }
      return std::nullopt;
    }
    if (expected == '*') {
      std::size_t farthest = cursor;
      while (farthest > 0 &&
             !beginning_delimiter(tokens_[farthest - 1].value)) {
        consume();
        --farthest;
      }
      for (std::size_t candidate = farthest; candidate <= cursor;
           ++candidate) {
        if (const std::optional<std::size_t> matched =
                prefix_at(pattern, count - 1, candidate)) {
          return matched;
        }
      }
      return std::nullopt;
    }
    if (cursor == 0) {
      return std::nullopt;
    }
    const std::uint16_t actual = tokens_[cursor - 1].value;
    const bool matched =
        expected == '?'
            ? !beginning_delimiter(actual)
            : expected == '#'
                  ? !beginning_delimiter(actual) && kanji_token(actual)
                  : normalize_literal_token(actual, encoding_) == expected;
    if (matched) {
      return prefix_at(pattern, count - 1, cursor - 1);
    }
    return std::nullopt;
  }

  const std::vector<SourceToken>& tokens_;
  EdictEncoding encoding_;
  WorkBudget& budget_;
};

}  // namespace

bool EdictPatternMatch::operator==(
    const EdictPatternMatch& other) const noexcept {
  return byte_offset == other.byte_offset &&
         byte_length == other.byte_length &&
         record_index == other.record_index;
}

void validate_edict_pattern_plan(const EdictSearchPlan& plan) {
  if (plan.kind != EdictSearchPlanKind::kPattern || plan.anchor.key.empty()) {
    fail("EDICT pattern matcher requires a pattern search plan");
  }
  if (plan.anchor.key.size() > kMaximumPatternLength ||
      plan.prefix.size() > kMaximumPatternLength - plan.anchor.key.size() ||
      plan.postfix.size() >
          kMaximumPatternLength - plan.anchor.key.size() - plan.prefix.size()) {
    fail("EDICT pattern plan exceeds its source input limit");
  }
}

EdictPatternMatchReport match_edict_pattern_report(
    const EdictDictionary& dictionary, const EdictIndexMatch& anchor,
    const EdictSearchPlan& plan, const EdictPatternMatchOptions& options) {
  validate_edict_pattern_plan(plan);
  if (anchor.record_index >= dictionary.records().size()) {
    fail("EDICT pattern match has an invalid record index");
  }
  const EdictRecord& record = dictionary.records()[anchor.record_index];
  if (anchor.byte_length == 0 ||
      anchor.byte_offset < record.byte_offset ||
      anchor.byte_offset >
          std::numeric_limits<std::size_t>::max() - anchor.byte_length ||
      anchor.byte_offset + anchor.byte_length >
          record.byte_offset + record.byte_length) {
    fail("EDICT pattern anchor span is invalid");
  }

  WorkBudget budget(options.work_steps);
  const std::vector<SourceToken> tokens = decode_record(
      dictionary, record, options.utf8_code_page, budget);
  const std::size_t record_end = record.byte_offset + record.byte_length;
  const std::size_t anchor_begin =
      token_boundary(tokens, anchor.byte_offset, record_end, budget);
  const std::size_t anchor_end = token_boundary(
      tokens, anchor.byte_offset + anchor.byte_length, record_end, budget);
  if (anchor_end - anchor_begin != plan.anchor.key.size()) {
    fail("EDICT pattern anchor length does not match its search plan");
  }
  for (std::size_t i = 0; i < plan.anchor.key.size(); ++i) {
    budget.consume();
    if (normalize_anchor_token(tokens[anchor_begin + i].value) !=
        plan.anchor.key[i]) {
      fail("EDICT pattern anchor content does not match its search plan");
    }
  }

  Matcher matcher(tokens, dictionary.encoding(), budget);
  const std::optional<std::size_t> begin =
      matcher.prefix(plan.prefix, anchor_begin);
  if (!begin.has_value()) {
    return {std::nullopt, budget.steps()};
  }
  const std::optional<std::size_t> end =
      matcher.postfix(plan.postfix, anchor_end);
  if (!end.has_value()) {
    return {std::nullopt, budget.steps()};
  }

  const std::size_t byte_begin =
      *begin == tokens.size() ? record_end : tokens[*begin].byte_begin;
  const std::size_t byte_end =
      *end == 0 ? record.byte_offset : tokens[*end - 1].byte_end;
  return {EdictPatternMatch{byte_begin, byte_end - byte_begin,
                            anchor.record_index},
          budget.steps()};
}

std::optional<EdictPatternMatch> match_edict_pattern(
    const EdictDictionary& dictionary, const EdictIndexMatch& anchor,
    const EdictSearchPlan& plan, const EdictPatternMatchOptions& options) {
  return match_edict_pattern_report(dictionary, anchor, plan, options).match;
}

}  // namespace jwpqt::core
