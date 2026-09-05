// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_CORE_UTF7_H
#define JWPQT_CORE_UTF7_H

#include <stdexcept>
#include <string>
#include <string_view>

namespace jwpqt::core {

class Utf7Error : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

std::u32string decode_utf7(std::string_view bytes);
std::string encode_utf7(std::u32string_view text);

}  // namespace jwpqt::core

#endif
