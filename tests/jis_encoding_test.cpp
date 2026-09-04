// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "jwpqt/core/jis_encoding.h"

namespace {

using jwpqt::core::EncodedPair;
using jwpqt::core::JisCode;

void require(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

void require_pair(EncodedPair actual, EncodedPair expected,
                  std::string_view message) {
  require(actual.lead == expected.lead && actual.trail == expected.trail,
          message);
}

template <typename Operation>
void expect_invalid(Operation operation) {
  try {
    operation();
  } catch (const jwpqt::core::JisEncodingError&) {
    return;
  }
  throw std::runtime_error("Invalid encoded pair was accepted");
}

void test_known_characters() {
  struct Fixture {
    JisCode jis;
    EncodedPair euc_jp;
    EncodedPair shift_jis;
  };

  // Ideographic space, あ, 日, 本, 語.
  constexpr std::array<Fixture, 5> fixtures = {{
      {0x2121, {0xa1, 0xa1}, {0x81, 0x40}},
      {0x2422, {0xa4, 0xa2}, {0x82, 0xa0}},
      {0x467c, {0xc6, 0xfc}, {0x93, 0xfa}},
      {0x4b5c, {0xcb, 0xdc}, {0x96, 0x7b}},
      {0x386c, {0xb8, 0xec}, {0x8c, 0xea}},
  }};

  for (const Fixture& fixture : fixtures) {
    require_pair(jwpqt::core::encode_euc_jp_pair(fixture.jis),
                 fixture.euc_jp, "Known EUC-JP encoding did not match");
    require(jwpqt::core::decode_euc_jp_pair(fixture.euc_jp) == fixture.jis,
            "Known EUC-JP decoding did not match");
    require_pair(jwpqt::core::encode_shift_jis_pair(fixture.jis),
                 fixture.shift_jis,
                 "Known Shift-JIS encoding did not match");
    require(jwpqt::core::decode_shift_jis_pair(fixture.shift_jis) ==
                fixture.jis,
            "Known Shift-JIS decoding did not match");
  }
}

void test_shift_jis_boundaries() {
  struct Fixture {
    JisCode jis;
    EncodedPair euc_jp;
    EncodedPair shift_jis;
  };

  constexpr std::array<Fixture, 6> fixtures = {{
      {0x215f, {0xa1, 0xdf}, {0x81, 0x7e}},
      {0x2160, {0xa1, 0xe0}, {0x81, 0x80}},
      {0x2221, {0xa2, 0xa1}, {0x81, 0x9f}},
      {0x5e21, {0xde, 0xa1}, {0x9f, 0x9f}},
      {0x5f21, {0xdf, 0xa1}, {0xe0, 0x40}},
      {0x7e7e, {0xfe, 0xfe}, {0xef, 0xfc}},
  }};

  for (const Fixture& fixture : fixtures) {
    require_pair(jwpqt::core::encode_euc_jp_pair(fixture.jis),
                 fixture.euc_jp, "Boundary EUC-JP encoding did not match");
    require_pair(jwpqt::core::encode_shift_jis_pair(fixture.jis),
                 fixture.shift_jis,
                 "Boundary Shift-JIS encoding did not match");
    require(jwpqt::core::decode_shift_jis_pair(fixture.shift_jis) ==
                fixture.jis,
            "Boundary Shift-JIS decoding did not match");
  }
}

void test_every_jis_x0208_pair_round_trips() {
  for (std::uint16_t row = 0x21; row <= 0x7e; ++row) {
    for (std::uint16_t cell = 0x21; cell <= 0x7e; ++cell) {
      const JisCode code = static_cast<JisCode>((row << 8U) | cell);
      require(jwpqt::core::is_jis_x0208_pair(code),
              "Valid JIS X 0208 pair was rejected");
      require(jwpqt::core::decode_euc_jp_pair(
                  jwpqt::core::encode_euc_jp_pair(code)) == code,
              "EUC-JP pair did not round-trip");
      require(jwpqt::core::decode_shift_jis_pair(
                  jwpqt::core::encode_shift_jis_pair(code)) == code,
              "Shift-JIS pair did not round-trip");
    }
  }
}

void test_invalid_jis_codes() {
  constexpr std::array<JisCode, 5> invalid = {
      0x0000, 0x2021, 0x2120, 0x7f21, 0x217f,
  };
  for (const JisCode code : invalid) {
    require(!jwpqt::core::is_jis_x0208_pair(code),
            "Invalid JIS X 0208 code was classified as valid");
    expect_invalid([code] {
      static_cast<void>(jwpqt::core::encode_euc_jp_pair(code));
    });
    expect_invalid([code] {
      static_cast<void>(jwpqt::core::encode_shift_jis_pair(code));
    });
  }
}

void test_invalid_encoded_pairs() {
  constexpr std::array<EncodedPair, 4> invalid_euc_jp = {{
      {0xa0, 0xa1}, {0xa1, 0xa0}, {0xff, 0xa1}, {0x8e, 0xa1},
  }};
  for (const EncodedPair bytes : invalid_euc_jp) {
    expect_invalid([bytes] {
      static_cast<void>(jwpqt::core::decode_euc_jp_pair(bytes));
    });
  }

  constexpr std::array<EncodedPair, 6> invalid_shift_jis = {{
      {0x80, 0x40}, {0xa0, 0x40}, {0xf0, 0x40},
      {0x81, 0x3f}, {0x81, 0x7f}, {0x81, 0xfd},
  }};
  for (const EncodedPair bytes : invalid_shift_jis) {
    expect_invalid([bytes] {
      static_cast<void>(jwpqt::core::decode_shift_jis_pair(bytes));
    });
  }
}

}  // namespace

int main() {
  try {
    test_known_characters();
    test_shift_jis_boundaries();
    test_every_jis_x0208_pair_round_trips();
    test_invalid_jis_codes();
    test_invalid_encoded_pairs();
    std::cout << "All JIS encoding tests passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
