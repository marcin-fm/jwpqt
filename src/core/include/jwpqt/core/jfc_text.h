// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_CORE_JFC_TEXT_H
#define JWPQT_CORE_JFC_TEXT_H

#include <stdexcept>
#include <string>
#include <string_view>

namespace jwpqt::core {

class JfcTextError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

// JFC is UTF-8 (preferred) or old JWP EUC text, not a structured card format.
// Old EUC supports JIS X 0208 and the recovered CP1252 extensions. Saves are
// always UTF-8 without a byte-order mark, including after old-EUC reads.
std::u32string decode_jfc_text(std::string_view bytes);
std::string encode_jfc_text(std::u32string_view text);

}  // namespace jwpqt::core

#endif
