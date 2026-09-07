// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "jwpqt/core/jwp_document.h"
#include "jwpqt/core/legacy_code_page.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace jwpqt::core {

class JwpTextCodecError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

std::u32string decode_jwp_text(
    const JwpText& text,
    LegacyCodePage code_page = kDefaultLegacyCodePage);
std::optional<JisCode> unicode_to_jwp_code(
    char32_t code_point,
    LegacyCodePage code_page = kDefaultLegacyCodePage) noexcept;
JwpText encode_jwp_text(
    std::u32string_view text,
    LegacyCodePage code_page = kDefaultLegacyCodePage);

}  // namespace jwpqt::core
