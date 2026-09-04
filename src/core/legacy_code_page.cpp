// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/legacy_code_page.h"

#include <array>
#include <cstddef>

// These tables and their undefined-value marker are retained from Glenn
// Rosenthal's JWPxp jwp_jisc.cpp and jwp_cp125*.dat files.

namespace jwpqt::core {
namespace {

constexpr char32_t kUndefined = 0xc23b;

constexpr std::array<char32_t, 128> kCodePage1250 = {{
#include "jwp_cp1250.dat"
}};
constexpr std::array<char32_t, 128> kCodePage1251 = {{
#include "jwp_cp1251.dat"
}};
constexpr std::array<char32_t, 128> kCodePage1252 = {{
#include "jwp_cp1252.dat"
}};
constexpr std::array<char32_t, 128> kCodePage1253 = {{
#include "jwp_cp1253.dat"
}};
constexpr std::array<char32_t, 128> kCodePage1254 = {{
#include "jwp_cp1254.dat"
}};
constexpr std::array<char32_t, 128> kCodePage1255 = {{
#include "jwp_cp1255.dat"
}};
constexpr std::array<char32_t, 128> kCodePage1256 = {{
#include "jwp_cp1256.dat"
}};
constexpr std::array<char32_t, 128> kCodePage1257 = {{
#include "jwp_cp1257.dat"
}};
constexpr std::array<char32_t, 128> kCodePage1258 = {{
#include "jwp_cp1258.dat"
}};

const std::array<char32_t, 128>* table_for(
    LegacyCodePage code_page) noexcept {
  switch (code_page) {
    case LegacyCodePage::k1250:
      return &kCodePage1250;
    case LegacyCodePage::k1251:
      return &kCodePage1251;
    case LegacyCodePage::k1252:
      return &kCodePage1252;
    case LegacyCodePage::k1253:
      return &kCodePage1253;
    case LegacyCodePage::k1254:
      return &kCodePage1254;
    case LegacyCodePage::k1255:
      return &kCodePage1255;
    case LegacyCodePage::k1256:
      return &kCodePage1256;
    case LegacyCodePage::k1257:
      return &kCodePage1257;
    case LegacyCodePage::k1258:
      return &kCodePage1258;
  }
  return nullptr;
}

}  // namespace

std::optional<char32_t> legacy_byte_to_unicode(
    std::uint8_t byte, LegacyCodePage code_page) noexcept {
  if (byte <= 0x7eU) {
    return static_cast<char32_t>(byte);
  }
  if (byte == 0x7fU) {
    return std::nullopt;
  }
  const std::array<char32_t, 128>* table = table_for(code_page);
  if (table == nullptr) {
    return std::nullopt;
  }
  const char32_t value = (*table)[static_cast<std::size_t>(byte - 0x80U)];
  if (value == kUndefined) {
    return std::nullopt;
  }
  return value;
}

std::optional<std::uint8_t> unicode_to_legacy_byte(
    char32_t code_point, LegacyCodePage code_page) noexcept {
  if (code_point <= 0x7eU) {
    return static_cast<std::uint8_t>(code_point);
  }
  if (code_point == U'\u007f') {
    return std::nullopt;
  }
  if (code_point == kUndefined) {
    return std::nullopt;
  }
  const std::array<char32_t, 128>* table = table_for(code_page);
  if (table == nullptr) {
    return std::nullopt;
  }
  for (std::size_t index = 0; index < table->size(); ++index) {
    if ((*table)[index] == code_point) {
      return static_cast<std::uint8_t>(index + 0x80U);
    }
  }
  return std::nullopt;
}

}  // namespace jwpqt::core
