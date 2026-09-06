// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>

#include "jwpqt/core/edict_dictionary.h"
#include "jwpqt/core/jfc_text.h"
#include "jwpqt/core/legacy_code_page.h"
#include "jwpqt/core/utf8.h"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

void require_error(const std::function<void()>& operation,
                   const char* message) {
  try {
    operation();
  } catch (const jwpqt::core::JfcTextError&) {
    return;
  }
  require(false, message);
}

void test_utf8_preference() {
  using namespace jwpqt::core;
  require(decode_jfc_text("").empty(), "Empty JFC did not decode");
  require(decode_jfc_text("Question\tAnswer\r\n") == U"Question\tAnswer\r\n",
          "JFC changed ASCII text or line endings");
  require(decode_jfc_text("\xc3\xa9") == U"\u00e9",
          "JFC did not prefer UTF-8 over an ambiguous EUC pair");
  const std::u32string text = U"\u65e5\u672c\u8a9e\t\U0001f600\n";
  const std::string utf8 = encode_utf8(text);
  require(decode_jfc_text(utf8) == text,
          "JFC did not decode UTF-8 including supplementary scalars");
  require(decode_jfc_text(std::string("\xef\xbb\xbf") + utf8) == text,
          "JFC did not consume the UTF-8 BOM");
  require(encode_jfc_text(text) == utf8,
          "JFC output was not plain UTF-8 without a BOM");
  const std::string embedded_nul("A\0B", 3);
  require(encode_jfc_text(decode_jfc_text(embedded_nul)) == embedded_nul,
          "JFC truncated an embedded NUL");
}

void test_old_euc() {
  using namespace jwpqt::core;
  const std::string bytes = "\xc6\xfc\xcb\xdc\xb8\xec\tanswer\r\n";
  const std::u32string text = U"\u65e5\u672c\u8a9e\tanswer\r\n";
  require(decode_jfc_text(bytes) == text, "JFC JIS X 0208 decoding failed");
  require(encode_jfc_text(decode_jfc_text(bytes)) == encode_utf8(text),
          "Old-EUC JFC was not saved as canonical UTF-8");
  require(decode_jfc_text("\x8e\x02\x8e\x26\x8e\xa6") ==
              U"\u201a\u00a6\u00a6",
          "JFC SS2 bytes used halfwidth kana instead of JWP code-page bytes");
  require(decode_jfc_text("\x8f\xa2\xed\x8f\xa9\xad\x8f\xab\xb1") ==
              U"\u00a9\u0152\u00e9",
          "JFC JIS X 0212 compatibility decoding failed");
  // The prefix is valid UTF-8 alone, but old EUC must decode the whole file.
  require(decode_jfc_text("\xc3\xa9\x8e\x26") == U"\u8fbf\u00a6",
          "JFC mixed UTF-8 and EUC within a file");
}

void test_recovered_subset() {
  using namespace jwpqt::core;
  std::size_t count = 0;
  std::size_t undefined_count = 0;
  for (unsigned int first = 0xa1; first <= 0xfe; ++first) {
    for (unsigned int second = 0xa1; second <= 0xfe; ++second) {
      const std::string bytes{static_cast<char>(0x8f),
                              static_cast<char>(first),
                              static_cast<char>(second)};
      const auto mapped = edict_euc_0212_byte(
          static_cast<std::uint8_t>(first), static_cast<std::uint8_t>(second));
      if (mapped.has_value()) {
        const auto unicode =
            legacy_byte_to_unicode(*mapped, LegacyCodePage::k1252);
        if (unicode.has_value()) {
          require(decode_jfc_text(bytes) == std::u32string(1, *unicode),
                  "JFC disagrees with the recovered JIS X 0212 subset");
        } else {
          require_error([&] { (void)decode_jfc_text(bytes); },
                        "JFC accepted an undefined recovered CP1252 byte");
          ++undefined_count;
        }
        ++count;
      } else {
        require_error([&] { (void)decode_jfc_text(bytes); },
                      "JFC accepted an unsupported JIS X 0212 extension");
      }
    }
  }
  require(count == 77, "Recovered JIS X 0212 subset no longer has 77 entries");
  require(undefined_count == 2,
          "Recovered CP1252 no longer rejects the two Z-caron mappings");
}

void test_errors() {
  using namespace jwpqt::core;
  require_error([] { (void)decode_jfc_text(std::string("\x8e\0", 2)); },
                "Undefined recovered CP1252 byte 0x80 was accepted");
  for (const std::string bytes : {"\x80", "\xff\xff", "\xc6", "\xc6 ",
                                  "\xfe\xfe", "\x8e", "\x8e\x01",
                                  "\x8e\x81", "\x8f", "\x8f\xa2",
                                  "\x8f\xa2 ", "\x8f\xb0\xa1",
                                  "\xc0\x80", "\xed\xa0\x80"}) {
    require_error([&] { (void)decode_jfc_text(bytes); },
                  "Malformed or unmapped JFC input was accepted");
  }
  for (const char32_t invalid : {char32_t{0xd800}, char32_t{0x110000}}) {
    require_error([&] { (void)encode_jfc_text(std::u32string(1, invalid)); },
                  "Invalid Unicode scalar was JFC encoded");
  }
}

}  // namespace

int main() {
  test_utf8_preference();
  test_old_euc();
  test_recovered_subset();
  test_errors();
  return EXIT_SUCCESS;
}
