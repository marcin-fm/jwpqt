// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_CORE_LEGACY_TEXT_H
#define JWPQT_CORE_LEGACY_TEXT_H

#include <stdexcept>
#include <string>
#include <string_view>

namespace jwpqt::core {

enum class LegacyEncoding {
  kEucJp,
  kShiftJis,
};

class LegacyTextError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

// Converts ASCII and the mapped JIS X 0208 repertoire. JIS X 0201
// halfwidth kana, JIS X 0212, and vendor extensions are rejected.
std::u32string decode_legacy_text(std::string_view bytes,
                                  LegacyEncoding encoding);
std::string encode_legacy_text(std::u32string_view text,
                               LegacyEncoding encoding);

}  // namespace jwpqt::core

#endif
