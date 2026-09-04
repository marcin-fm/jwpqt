// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/jis_unicode.h"
#include "jwpqt/core/jwp_text_codec.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

namespace {

using jwpqt::core::JisCode;
using jwpqt::core::JwpText;
using jwpqt::core::JwpTextCodecError;
using jwpqt::core::LegacyCodePage;

void require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAILED: " << message << '\n';
    std::exit(1);
  }
}

template <typename Callable>
void require_codec_error(Callable callable, const std::string& message) {
  try {
    callable();
  } catch (const JwpTextCodecError&) {
    return;
  }
  require(false, message);
}

void test_mixed_text() {
  const JwpText text = {'A', 0xe9, 0x2422, 0x467c, 0x2331};
  const std::u32string unicode =
      jwpqt::core::decode_jwp_text(text, LegacyCodePage::k1252);
  require(unicode == U"A\u00e9\u3042\u65e5\uff11",
          "Mixed JWP text did not decode");
  require(jwpqt::core::encode_jwp_text(unicode,
                                       LegacyCodePage::k1252) == text,
          "Mixed JWP text did not round-trip");
}

void test_code_page_preferences() {
  require(jwpqt::core::encode_jwp_text(U"\u0410",
                                       LegacyCodePage::k1251) ==
              JwpText{0xc0},
          "CP1251 Cyrillic did not prefer its extended byte");
  require(jwpqt::core::encode_jwp_text(U"\u0410",
                                       LegacyCodePage::k1252) ==
              JwpText{0x2721},
          "Non-Cyrillic code page did not use JIS Cyrillic");
  require(jwpqt::core::encode_jwp_text(U"\u0391",
                                       LegacyCodePage::k1253) ==
              JwpText{0xc1},
          "CP1253 Greek did not prefer its extended byte");
  require(jwpqt::core::encode_jwp_text(U"\u0391",
                                       LegacyCodePage::k1252) ==
              JwpText{0x2621},
          "Non-Greek code page did not use JIS Greek");

  const std::u32string cyrillic = jwpqt::core::decode_jwp_text(
      JwpText{0x2721}, LegacyCodePage::k1251);
  require(jwpqt::core::encode_jwp_text(cyrillic,
                                       LegacyCodePage::k1251) ==
              JwpText{0xc0},
          "CP1251 did not canonicalize JIS Cyrillic to an extended byte");
  const std::u32string greek = jwpqt::core::decode_jwp_text(
      JwpText{0x2621}, LegacyCodePage::k1253);
  require(jwpqt::core::encode_jwp_text(greek,
                                       LegacyCodePage::k1253) ==
              JwpText{0xc1},
          "CP1253 did not canonicalize JIS Greek to an extended byte");

  require(jwpqt::core::encode_jwp_text(U"\uff1a") == JwpText{0x2127},
          "Duplicate fullwidth punctuation lost legacy first-match order");
}

void test_all_jis_mappings_preserve_text() {
  for (unsigned int row = 0x21; row <= 0x7e; ++row) {
    for (unsigned int cell = 0x21; cell <= 0x7e; ++cell) {
      const JisCode jis = static_cast<JisCode>((row << 8U) | cell);
      const std::optional<char32_t> code_point =
          jwpqt::core::jis_x0208_to_unicode(jis);
      if (!code_point.has_value()) {
        continue;
      }
      const JwpText encoded = jwpqt::core::encode_jwp_text(
          std::u32string(1, *code_point), LegacyCodePage::k1252);
      require(jwpqt::core::decode_jwp_text(
                  encoded, LegacyCodePage::k1252) ==
                  std::u32string(1, *code_point),
              "Mapped JIS code point changed through canonical encoding");
    }
  }
}

void test_all_code_pages_preserve_text() {
  constexpr std::array<LegacyCodePage, 9> code_pages = {
      LegacyCodePage::k1250, LegacyCodePage::k1251,
      LegacyCodePage::k1252, LegacyCodePage::k1253,
      LegacyCodePage::k1254, LegacyCodePage::k1255,
      LegacyCodePage::k1256, LegacyCodePage::k1257,
      LegacyCodePage::k1258,
  };
  for (const LegacyCodePage code_page : code_pages) {
    for (unsigned int value = 0x80U; value <= 0xffU; ++value) {
      const JwpText source{static_cast<JisCode>(value)};
      const std::optional<char32_t> mapped =
          jwpqt::core::legacy_byte_to_unicode(
              static_cast<std::uint8_t>(value), code_page);
      if (!mapped.has_value()) {
        require_codec_error(
            [&] { jwpqt::core::decode_jwp_text(source, code_page); },
            "Undefined extended byte decoded");
        continue;
      }

      const std::u32string decoded =
          jwpqt::core::decode_jwp_text(source, code_page);
      require(decoded == std::u32string(1, *mapped),
              "Extended byte decoded to the wrong code point");
      if (*mapped == U'\u201a' || *mapped == U'\u0192' ||
          *mapped == U'\u201e') {
        require_codec_error(
            [&] { jwpqt::core::encode_jwp_text(decoded, code_page); },
            "Legacy-misconstrued code point encoded");
        continue;
      }
      const JwpText encoded =
          jwpqt::core::encode_jwp_text(decoded, code_page);
      require(jwpqt::core::decode_jwp_text(encoded, code_page) == decoded,
              "Extended code point changed through canonical encoding");
    }
  }
}

void test_errors() {
  require_codec_error(
      [] { jwpqt::core::decode_jwp_text(JwpText{0x7f}); },
      "JWP DEL token was accepted");
  require_codec_error(
      [] { jwpqt::core::decode_jwp_text(JwpText{0x80}); },
      "Undefined CP1252 token was accepted");
  require_codec_error(
      [] { jwpqt::core::decode_jwp_text(JwpText{0x2921}); },
      "Unmapped JIS token was accepted");
  require_codec_error(
      [] {
        jwpqt::core::encode_jwp_text(std::u32string(1, U'\0'));
      },
      "Unicode NUL was accepted");
  require_codec_error(
      [] { jwpqt::core::encode_jwp_text(U"\U0001f5fe"); },
      "Unrepresentable Unicode was accepted");
  require_codec_error(
      [] { jwpqt::core::encode_jwp_text(U"\u201a"); },
      "Legacy-misconstrued U+201A was accepted");
  require_codec_error(
      [] { jwpqt::core::encode_jwp_text(U"\u0192"); },
      "Legacy-misconstrued U+0192 was accepted");
  require_codec_error(
      [] { jwpqt::core::encode_jwp_text(U"\u201e"); },
      "Legacy-misconstrued U+201E was accepted");
}

}  // namespace

int main() {
  test_mixed_text();
  test_code_page_preferences();
  test_all_jis_mappings_preserve_text();
  test_all_code_pages_preserve_text();
  test_errors();
  std::cout << "All JWP text codec tests passed\n";
}
