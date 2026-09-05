// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>

#include "jwpqt/core/utf7.h"

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
  } catch (const jwpqt::core::Utf7Error&) {
    return;
  }
  require(false, message);
}

void test_known_sequences() {
  require(jwpqt::core::decode_utf7("ASCII+-+ZeVnLIqe-") ==
              U"ASCII+\U000065e5\U0000672c\U00008a9e",
          "Known UTF-7 sequence decoded incorrectly");
  require(jwpqt::core::encode_utf7(
              U"ASCII+\U000065e5\U0000672c\U00008a9e") ==
              "ASCII+-+ZeVnLIqe-",
          "Known UTF-7 sequence encoded incorrectly");
  require(jwpqt::core::encode_utf7(U"A!B") == "A+ACE-B" &&
              jwpqt::core::decode_utf7("A+ACE-B") == U"A!B",
          "JWP direct-character policy was not preserved");
}

void test_unicode_round_trip() {
  const std::u32string text =
      U"Tab\t newline\n \U000003a9 \U0001f600 end";
  require(jwpqt::core::decode_utf7(jwpqt::core::encode_utf7(text)) == text,
          "UTF-7 supplementary-scalar round trip failed");
  require(jwpqt::core::decode_utf7("+2D3eAA-") ==
              std::u32string{static_cast<char32_t>(0x1f600U)},
          "UTF-7 surrogate pair decoded incorrectly");
  require(jwpqt::core::decode_utf7("+ZeVnLIqe!") ==
              U"\U000065e5\U0000672c\U00008a9e!",
          "Implicit UTF-7 shift termination failed");
  require(jwpqt::core::decode_utf7("++") == U"+",
          "JWP double-plus compatibility failed");
}

void test_malformed_input() {
  require_error([] { (void)jwpqt::core::decode_utf7("+"); },
                "Incomplete UTF-7 shift was accepted");
  require_error([] { (void)jwpqt::core::decode_utf7("+A-"); },
                "Short UTF-7 shift was accepted");
  require_error([] { (void)jwpqt::core::decode_utf7("+AAF-"); },
                "Nonzero UTF-7 residual bits were accepted");
  require_error([] { (void)jwpqt::core::decode_utf7("+AAAA-"); },
                "Impossible UTF-7 residual length was accepted");
  require_error([] { (void)jwpqt::core::decode_utf7("+2AA-"); },
                "Unpaired UTF-7 high surrogate was accepted");
  require_error([] { (void)jwpqt::core::decode_utf7("+3gA-"); },
                "Unpaired UTF-7 low surrogate was accepted");
  require_error(
      [] { (void)jwpqt::core::decode_utf7(std::string("A\x80", 2)); },
      "High-bit UTF-7 byte was accepted");
  require_error(
      [] {
        (void)jwpqt::core::encode_utf7(
            std::u32string{U'A', static_cast<char32_t>(0xd800U)});
      },
                "Invalid Unicode scalar was UTF-7 encoded");
}

}  // namespace

int main() {
  test_known_sequences();
  test_unicode_round_trip();
  test_malformed_input();
  return EXIT_SUCCESS;
}
