// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <cstddef>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "jwpqt/core/jis_encoding.h"
#include "jwpqt/core/jis_text.h"
#include "jwpqt/core/jis_unicode.h"

namespace {

using jwpqt::core::JisCode;
using jwpqt::core::JisTextEncoding;
using jwpqt::core::JisTextError;

constexpr std::array<JisTextEncoding, 3> kEncodings{
    JisTextEncoding::kNewJis,
    JisTextEncoding::kOldJis,
    JisTextEncoding::kNecJis,
};

void require(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

template <typename Function>
void require_error(Function&& function, std::string_view message) {
  try {
    function();
  } catch (const JisTextError&) {
    return;
  }
  throw std::runtime_error(std::string(message));
}

template <typename Function>
void require_error_contains(Function&& function, std::string_view expected,
                            std::string_view message) {
  try {
    function();
  } catch (const JisTextError& error) {
    require(std::string_view(error.what()).find(expected) !=
                std::string_view::npos,
            message);
    return;
  }
  throw std::runtime_error(std::string(message));
}

std::string enter(JisTextEncoding encoding) {
  switch (encoding) {
    case JisTextEncoding::kNewJis:
      return "\x1b$B";
    case JisTextEncoding::kOldJis:
      return "\x1b$@";
    case JisTextEncoding::kNecJis:
      return "\x1bK";
  }
  throw std::runtime_error("Unknown test encoding");
}

std::string leave(JisTextEncoding encoding) {
  switch (encoding) {
    case JisTextEncoding::kNewJis:
    case JisTextEncoding::kOldJis:
      return "\x1b(J";
    case JisTextEncoding::kNecJis:
      return "\x1bH";
  }
  throw std::runtime_error("Unknown test encoding");
}

std::string encoded_pair(JisTextEncoding encoding, JisCode jis) {
  std::string output = enter(encoding);
  output.push_back(static_cast<char>(jis >> 8U));
  output.push_back(static_cast<char>(jis & 0xffU));
  output += leave(encoding);
  return output;
}

void check_known_text() {
  const std::u32string text = U"ASCII あ日本語\n";
  const std::string pairs = "\x24\x22\x46\x7c\x4b\x5c\x38\x6c";

  for (const JisTextEncoding encoding : kEncodings) {
    const std::string expected = "ASCII " + enter(encoding) + pairs +
                                 leave(encoding) + "\n";
    require(jwpqt::core::decode_jis_text(expected, encoding) == text,
            "Known JIS text must decode");
    require(jwpqt::core::encode_jis_text(text, encoding) == expected,
            "Known JIS text must encode with the historical escapes");
  }
}

void check_ascii_and_line_reset() {
  std::string ascii;
  std::u32string unicode;
  for (unsigned int value = 0; value <= 0x7fU; ++value) {
    if (value == 0x1bU) {
      continue;
    }
    ascii.push_back(static_cast<char>(value));
    unicode.push_back(static_cast<char32_t>(value));
  }

  for (const JisTextEncoding encoding : kEncodings) {
    require(jwpqt::core::decode_jis_text(ascii, encoding) == unicode,
            "ASCII except ESC must decode unchanged");
    require(jwpqt::core::encode_jis_text(unicode, encoding) == ascii,
            "ASCII except ESC must encode unchanged");

    const std::string implicit_reset =
        enter(encoding) + "\x24\x22\nASCII";
    require(jwpqt::core::decode_jis_text(implicit_reset, encoding) ==
                U"あ\nASCII",
            "Line boundaries must reset JIS mode like the legacy reader");
    const std::string cr_reset_and_reentry =
        enter(encoding) + "\x24\x22\rASCII" + enter(encoding) +
        "\x24\x24";
    require(jwpqt::core::decode_jis_text(cr_reset_and_reentry, encoding) ==
                U"あ\rASCIIい",
            "CR must reset JIS mode and allow explicit re-entry");

    const std::string encoded_lines =
        enter(encoding) + "\x24\x22" + leave(encoding) + "\n" +
        enter(encoding) + "\x24\x24" + leave(encoding);
    require(jwpqt::core::encode_jis_text(U"あ\nい", encoding) ==
                encoded_lines,
            "Encoder must leave JIS mode at LF and terminate final JIS text");
    require(jwpqt::core::encode_jis_text(U"あ", encoding) ==
                encoded_pair(encoding, 0x2422),
            "Final JIS character must have a byte-exact closing escape");
  }
}

void check_mapped_repertoire() {
  std::size_t mapped = 0;
  for (unsigned int row = 0x21; row <= 0x7e; ++row) {
    for (unsigned int cell = 0x21; cell <= 0x7e; ++cell) {
      const JisCode jis = static_cast<JisCode>((row << 8U) | cell);
      const std::optional<char32_t> code_point =
          jwpqt::core::jis_x0208_to_unicode(jis);
      for (const JisTextEncoding encoding : kEncodings) {
        const std::string bytes = encoded_pair(encoding, jis);
        if (!code_point.has_value()) {
          require_error(
              [&] { jwpqt::core::decode_jis_text(bytes, encoding); },
              "Every unmapped structural JIS character must fail");
          continue;
        }
        const std::u32string text(1, *code_point);
        require(jwpqt::core::decode_jis_text(bytes, encoding) == text,
                "Every mapped JIS character must decode");
        const std::string canonical =
            jwpqt::core::encode_jis_text(text, encoding);
        require(jwpqt::core::decode_jis_text(canonical, encoding) == text,
                "Every mapped Unicode character must encode canonically");
      }
      if (code_point.has_value()) {
        ++mapped;
      }
    }
  }
  require(mapped == 6892, "Mapped repertoire count must remain stable");
}

void check_escape_compatibility() {
  for (const JisTextEncoding encoding : kEncodings) {
    require(jwpqt::core::decode_jis_text(
                "\x1b$B\x24\x22\x1b(B\\~", encoding) == U"あ\\~",
            "ASCII ESC ( B must be accepted");
    require(jwpqt::core::decode_jis_text(
                "\x1b$@\x24\x22\x1b(J\\~", encoding) == U"あ\\~",
            "Roman ESC ( J must be accepted with JWP ASCII semantics");
    require(jwpqt::core::decode_jis_text(
                "\x1bK\x24\x22\x1bH", encoding) == U"あ",
            "NEC escape transitions must be accepted by every JIS reader");
  }
}

void check_rejections() {
  for (const JisTextEncoding encoding : kEncodings) {
    require_error(
        [encoding] { jwpqt::core::decode_jis_text("\x1b", encoding); },
        "Truncated escape sequence must fail");
    require_error(
        [encoding] { jwpqt::core::decode_jis_text("\x1bX", encoding); },
        "Unsupported escape sequence must fail");
    require_error(
        [encoding] { jwpqt::core::decode_jis_text("\x1b$", encoding); },
        "Truncated dollar escape sequence must fail");
    require_error(
        [encoding] { jwpqt::core::decode_jis_text("\x1b(", encoding); },
        "Truncated parenthesis escape sequence must fail");
    require_error(
        [encoding] { jwpqt::core::decode_jis_text("\x1b$I", encoding); },
        "Unsupported JIS designation must fail");
    require_error(
        [encoding] {
          jwpqt::core::decode_jis_text(enter(encoding) + "\x24", encoding);
        },
        "Truncated JIS pair must fail");
    require_error(
        [encoding] {
          jwpqt::core::decode_jis_text(enter(encoding) + "\x20\x21",
                                       encoding);
        },
        "Invalid JIS pair must fail");
    require_error(
        [encoding] {
          jwpqt::core::decode_jis_text(enter(encoding) + "\x4f\x54",
                                       encoding);
        },
        "Unmapped JIS character must fail");
    require_error(
        [encoding] {
          jwpqt::core::decode_jis_text(std::string(1, '\x80'), encoding);
        },
        "High-bit byte outside JIS mode must fail");
    require_error(
        [encoding] { jwpqt::core::encode_jis_text(U"\x1b", encoding); },
        "Literal ESC must be rejected because it cannot round-trip");
    require_error(
        [encoding] { jwpqt::core::encode_jis_text(U"😀", encoding); },
        "Unmapped Unicode must fail");
  }

  const auto unknown = static_cast<JisTextEncoding>(999);
  require_error(
      [unknown] { jwpqt::core::decode_jis_text("", unknown); },
      "Unknown decode encoding must fail");
  require_error(
      [unknown] { jwpqt::core::encode_jis_text(U"", unknown); },
      "Unknown encode encoding must fail");

  require_error_contains(
      [] {
        jwpqt::core::decode_jis_text("A\x1b$B\x24",
                                     JisTextEncoding::kNewJis);
      },
      "byte 4", "Decode errors must identify the source byte offset");
  require_error_contains(
      [] {
        jwpqt::core::encode_jis_text(U"A😀",
                                     JisTextEncoding::kNewJis);
      },
      "character 1", "Encode errors must identify the character offset");
}

}  // namespace

int main() {
  try {
    check_known_text();
    check_ascii_and_line_reset();
    check_mapped_repertoire();
    check_escape_compatibility();
    check_rejections();
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
