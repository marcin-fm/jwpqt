// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdint>
#include <optional>

namespace jwpqt::core {

enum class LegacyCodePage {
  k1250 = 1250,
  k1251 = 1251,
  k1252 = 1252,
  k1253 = 1253,
  k1254 = 1254,
  k1255 = 1255,
  k1256 = 1256,
  k1257 = 1257,
  k1258 = 1258,
};

// JWPxp falls back to its US/Western Europe table when the configured or
// platform code page is unavailable. Native configuration resolves "auto"
// before calling the mapping functions.
inline constexpr LegacyCodePage kDefaultLegacyCodePage =
    LegacyCodePage::k1252;

// Maps the single-byte portion of JWP's configured Windows code page. Values
// that the recovered tables mark undefined have no mapping.
std::optional<char32_t> legacy_byte_to_unicode(
    std::uint8_t byte, LegacyCodePage code_page) noexcept;
std::optional<std::uint8_t> unicode_to_legacy_byte(
    char32_t code_point, LegacyCodePage code_page) noexcept;

}  // namespace jwpqt::core
