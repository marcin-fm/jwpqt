// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/jwp_word_selection.h"

#include <stdexcept>

namespace jwpqt::core {
namespace {

constexpr JisCode kJapaneseSpace = 0x2121;
constexpr JisCode kLongVowel = 0x213c;
constexpr JisCode kKatakanaRepeat = 0x2133;
constexpr JisCode kKatakanaRepeatVoiced = 0x2134;
constexpr JisCode kHiraganaRepeat = 0x2135;
constexpr JisCode kHiraganaRepeatVoiced = 0x2136;
constexpr JisCode kKanjiRepeat = 0x2139;
constexpr JisCode kHiraganaWo = 0x2472;

bool ascii_alphanumeric(JisCode code) noexcept {
  return (code >= '0' && code <= '9') ||
         (code >= 'A' && code <= 'Z') ||
         (code >= 'a' && code <= 'z') || code == '_' || code == '@' ||
         code == '#' || code == '$' || code == '%';
}

std::size_t move_left_word(const JwpText& text, std::size_t cursor) {
  if (cursor == 0) return 0;

  std::size_t position = cursor;
  do {
    --position;
    if (position == 0 ||
        !same_jwp_word_class(text[position], static_cast<JisCode>(' '))) {
      break;
    }
  } while (position != 0);

  if (position == 0) return 0;

  JisCode previous = text[position];
  JisCode reference = previous;
  while (position != 0) {
    --position;
    const JisCode current = text[position];
    if (reference == kLongVowel) reference = current;
    if (!same_jwp_word_class(previous, current) ||
        !same_jwp_word_class(reference, current)) {
      ++position;
      break;
    }
    previous = current;
  }
  return position;
}

std::size_t move_right_word(const JwpText& text, std::size_t cursor) {
  if (cursor >= text.size()) return text.size();

  std::size_t position = cursor;
  JisCode previous = text[position];
  JisCode reference = previous;
  while (position < text.size()) {
    ++position;
    if (position == text.size()) break;
    const JisCode current = text[position];
    if (reference == kLongVowel) reference = current;
    if ((!same_jwp_word_class(previous, current) ||
         !same_jwp_word_class(reference, current)) &&
        jwp_word_class(current) != JwpWordClass::kSpace) {
      break;
    }
    previous = current;
  }
  return position;
}

}  // namespace

JwpWordClass jwp_word_class(JisCode code) noexcept {
  if (code == 0) return JwpWordClass::kJunk;
  if (code >= 0x3000) return JwpWordClass::kKanji;
  if (code == ' ' || code == '\t' || code == kJapaneseSpace) {
    return JwpWordClass::kSpace;
  }
  switch (code & 0x7f00U) {
    case 0x2500:
      return JwpWordClass::kKatakana;
    case 0x2400:
      return JwpWordClass::kHiragana;
    case 0x2300:
      return JwpWordClass::kJascii;
    default:
      break;
  }
  if (code > 0x00ffU) return JwpWordClass::kJapanesePunctuation;
  if (ascii_alphanumeric(code)) return JwpWordClass::kAscii;
  return JwpWordClass::kAsciiPunctuation;
}

bool same_jwp_word_class(JisCode first, JisCode second) noexcept {
  if (first == kHiraganaWo || second == kHiraganaWo) return false;

  const JwpWordClass first_class = jwp_word_class(first);
  const JwpWordClass second_class = jwp_word_class(second);
  if (first_class == second_class) return true;
  if ((first_class == JwpWordClass::kSpace &&
       second_class == JwpWordClass::kJunk) ||
      (second_class == JwpWordClass::kSpace &&
       first_class == JwpWordClass::kJunk)) {
    return true;
  }
  if (first == kLongVowel || second == kLongVowel) {
    const JwpWordClass other = first == kLongVowel ? second_class : first_class;
    if (other == JwpWordClass::kKatakana ||
        other == JwpWordClass::kHiragana) {
      return true;
    }
  }
  if (first == kKanjiRepeat || second == kKanjiRepeat) {
    const JwpWordClass other = first == kKanjiRepeat ? second_class : first_class;
    if (other == JwpWordClass::kKanji) return true;
  }
  if (first == kKatakanaRepeat || first == kKatakanaRepeatVoiced ||
      second == kKatakanaRepeat || second == kKatakanaRepeatVoiced) {
    const JwpWordClass other =
        first == kKatakanaRepeat || first == kKatakanaRepeatVoiced
            ? second_class
            : first_class;
    if (other == JwpWordClass::kKatakana) return true;
  }
  if (first == kHiraganaRepeat || first == kHiraganaRepeatVoiced ||
      second == kHiraganaRepeat || second == kHiraganaRepeatVoiced) {
    const JwpWordClass other =
        first == kHiraganaRepeat || first == kHiraganaRepeatVoiced
            ? second_class
            : first_class;
    if (other == JwpWordClass::kHiragana) return true;
  }
  return false;
}

std::size_t previous_jwp_word_position(const JwpText& text,
                                       std::size_t cursor,
                                       std::size_t visual_line_begin) {
  if (cursor > text.size() || visual_line_begin > cursor) {
    throw std::out_of_range("JWP word-navigation position is out of range");
  }

  const bool began_at_line_start = cursor == visual_line_begin;
  std::size_t position = cursor;
  while (position != 0) {
    --position;
    if ((!began_at_line_start && position == visual_line_begin) ||
        !same_jwp_word_class(text[position], static_cast<JisCode>(' '))) {
      break;
    }
  }
  if ((!began_at_line_start && position == visual_line_begin) || position == 0) {
    return position;
  }

  JisCode previous = text[position];
  JisCode reference = previous;
  JisCode current = previous;
  while (position != 0) {
    --position;
    current = text[position];
    if (reference == kLongVowel) reference = current;
    if (!same_jwp_word_class(previous, current) ||
        !same_jwp_word_class(reference, current)) {
      return position + 1;
    }
    previous = current;
  }
  return same_jwp_word_class(current, static_cast<JisCode>(' ')) ? 1U : 0U;
}

std::size_t next_jwp_word_position(const JwpText& text,
                                   std::size_t cursor) {
  if (cursor > text.size()) {
    throw std::out_of_range("JWP word-navigation position is out of range");
  }
  if (cursor == text.size()) return cursor;

  JisCode previous = text[cursor];
  JisCode reference = previous;
  std::size_t position = cursor;
  while (position != text.size()) {
    ++position;
    const JisCode current = position == text.size() ? 0 : text[position];
    if (reference == kLongVowel) reference = current;
    if ((!same_jwp_word_class(previous, current) ||
         !same_jwp_word_class(reference, current)) &&
        !same_jwp_word_class(current, static_cast<JisCode>(' '))) {
      break;
    }
    previous = current;
  }
  return position;
}

JwpWordRange select_jwp_word(const JwpText& text, std::size_t cursor,
                             bool preceding_selection) {
  if (cursor > text.size()) {
    throw std::out_of_range("JWP word-selection cursor is outside the paragraph");
  }
  if (text.empty()) return {cursor, cursor};

  std::size_t begin = cursor;
  if (begin == text.size()) --begin;

  if (same_jwp_word_class(text[begin], static_cast<JisCode>(' '))) {
    begin = move_right_word(text, begin);
  } else if (!preceding_selection && begin != 0 &&
             same_jwp_word_class(text[begin - 1], text[begin])) {
    begin = move_left_word(text, begin);
  }

  std::size_t end = move_right_word(text, begin);
  while (end > begin && jwp_word_class(text[end - 1]) == JwpWordClass::kSpace) {
    --end;
  }
  return {begin, end};
}

}  // namespace jwpqt::core
