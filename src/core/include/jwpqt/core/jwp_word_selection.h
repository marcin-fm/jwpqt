// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "jwpqt/core/jwp_document.h"

#include <cstddef>

namespace jwpqt::core {

enum class JwpWordClass {
  kAscii,
  kKatakana,
  kHiragana,
  kJascii,
  kSpace,
  kAsciiPunctuation,
  kJapanesePunctuation,
  kKanji,
  kJunk,
};

struct JwpWordRange {
  std::size_t begin = 0;
  std::size_t end = 0;
};

JwpWordClass jwp_word_class(JisCode code) noexcept;
bool same_jwp_word_class(JisCode first, JisCode second) noexcept;

// Returns the source Ctrl+W range within one paragraph. When an existing
// selection ends at cursor, selection starts there instead of moving left.
JwpWordRange select_jwp_word(const JwpText& text, std::size_t cursor,
                             bool preceding_selection = false);

}  // namespace jwpqt::core
