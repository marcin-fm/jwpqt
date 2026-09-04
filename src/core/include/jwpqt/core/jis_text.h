// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_CORE_JIS_TEXT_H
#define JWPQT_CORE_JIS_TEXT_H

#include <stdexcept>
#include <string>
#include <string_view>

namespace jwpqt::core {

enum class JisTextEncoding {
  kIso2022Jp,
  kOldJis,
  kNecJis,
};

class JisTextError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

// Converts ASCII and the mapped JIS X 0208 repertoire. The variants retain
// JWP's historical escape sequences and reset to ASCII at line boundaries.
std::u32string decode_jis_text(std::string_view bytes,
                               JisTextEncoding encoding);
std::string encode_jis_text(std::u32string_view text,
                            JisTextEncoding encoding);

}  // namespace jwpqt::core

#endif
