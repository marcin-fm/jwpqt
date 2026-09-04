// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/legacy_code_page.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

namespace {

using jwpqt::core::LegacyCodePage;

void require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAILED: " << message << '\n';
    std::exit(1);
  }
}

struct Fixture {
  LegacyCodePage code_page;
  std::uint8_t byte;
  char32_t code_point;
};

constexpr std::array<LegacyCodePage, 9> kCodePages = {
    LegacyCodePage::k1250, LegacyCodePage::k1251, LegacyCodePage::k1252,
    LegacyCodePage::k1253, LegacyCodePage::k1254, LegacyCodePage::k1255,
    LegacyCodePage::k1256, LegacyCodePage::k1257, LegacyCodePage::k1258,
};

void test_ascii() {
  for (const LegacyCodePage code_page : kCodePages) {
    for (unsigned int value = 0; value <= 0x7eU; ++value) {
      const auto byte = static_cast<std::uint8_t>(value);
      require(jwpqt::core::legacy_byte_to_unicode(byte, code_page) == value,
              "ASCII byte mapping failed");
      require(jwpqt::core::unicode_to_legacy_byte(value, code_page) == byte,
              "ASCII inverse mapping failed");
    }
    require(!jwpqt::core::legacy_byte_to_unicode(0x7f, code_page),
            "Legacy DEL byte must remain unmapped");
    require(!jwpqt::core::unicode_to_legacy_byte(U'\u007f', code_page),
            "Unicode DEL must remain unmapped");
  }
}

void test_recovered_tables() {
  constexpr std::array<Fixture, 18> fixtures = {{
      {LegacyCodePage::k1250, 0xc0, U'\u0154'},
      {LegacyCodePage::k1250, 0xd0, U'\u0110'},
      {LegacyCodePage::k1251, 0x80, U'\u0402'},
      {LegacyCodePage::k1251, 0xc0, U'\u0410'},
      {LegacyCodePage::k1252, 0x82, U'\u201a'},
      {LegacyCodePage::k1252, 0xc0, U'\u00c0'},
      {LegacyCodePage::k1253, 0xc0, U'\u0390'},
      {LegacyCodePage::k1253, 0xd0, U'\u03a0'},
      {LegacyCodePage::k1254, 0xd0, U'\u011e'},
      {LegacyCodePage::k1254, 0xdd, U'\u0130'},
      {LegacyCodePage::k1255, 0xc0, U'\u05b0'},
      {LegacyCodePage::k1255, 0xe0, U'\u05d0'},
      {LegacyCodePage::k1256, 0x81, U'\u067e'},
      {LegacyCodePage::k1256, 0xd0, U'\u0630'},
      {LegacyCodePage::k1257, 0xc0, U'\u0104'},
      {LegacyCodePage::k1257, 0xd0, U'\u0160'},
      {LegacyCodePage::k1258, 0xd0, U'\u0110'},
      {LegacyCodePage::k1258, 0xff, U'\u00ff'},
  }};

  for (const Fixture& fixture : fixtures) {
    require(jwpqt::core::legacy_byte_to_unicode(fixture.byte,
                                                 fixture.code_page) ==
                fixture.code_point,
            "Recovered code-page fixture did not decode");
    require(jwpqt::core::unicode_to_legacy_byte(fixture.code_point,
                                                fixture.code_page) ==
                fixture.byte,
            "Recovered code-page fixture did not encode");
  }
}

void test_undefined_and_inverse_stability() {
  constexpr std::array<std::size_t, 9> expected_undefined = {
      7, 2, 8, 18, 8, 30, 9, 13, 10,
  };

  for (std::size_t page_index = 0; page_index < kCodePages.size();
       ++page_index) {
    const LegacyCodePage code_page = kCodePages[page_index];
    std::size_t undefined = 0;
    for (unsigned int value = 0x80U; value <= 0xffU; ++value) {
      const auto byte = static_cast<std::uint8_t>(value);
      const std::optional<char32_t> code_point =
          jwpqt::core::legacy_byte_to_unicode(byte, code_page);
      if (!code_point.has_value()) {
        ++undefined;
        continue;
      }
      const std::optional<std::uint8_t> canonical =
          jwpqt::core::unicode_to_legacy_byte(*code_point, code_page);
      require(canonical.has_value(), "Mapped code point lacked an inverse");
      require(jwpqt::core::legacy_byte_to_unicode(*canonical, code_page) ==
                  code_point,
              "Canonical inverse changed the code point");
    }
    require(undefined == expected_undefined[page_index],
            "Undefined table-entry count changed");
  }

  require(!jwpqt::core::unicode_to_legacy_byte(U'\uc23b',
                                                LegacyCodePage::k1252),
          "Recovered undefined marker must not become text");
}

std::uint64_t mapping_fingerprint(LegacyCodePage code_page) {
  constexpr std::uint64_t kOffset = 14695981039346656037ULL;
  constexpr std::uint64_t kPrime = 1099511628211ULL;
  std::uint64_t hash = kOffset;
  for (unsigned int value = 0x80U; value <= 0xffU; ++value) {
    const std::optional<char32_t> code_point =
        jwpqt::core::legacy_byte_to_unicode(
            static_cast<std::uint8_t>(value), code_page);
    const std::uint32_t mapped = code_point.has_value()
                                     ? static_cast<std::uint32_t>(*code_point)
                                     : 0xffffffffU;
    for (unsigned int shift = 0; shift < 32U; shift += 8U) {
      hash ^= (mapped >> shift) & 0xffU;
      hash *= kPrime;
    }
  }
  return hash;
}

void test_recovered_table_fingerprints() {
  // Independently calculated from the nine recovered jwp_cp125*.dat files.
  constexpr std::array<std::uint64_t, 9> expected = {
      0x0df86f5b0a5ed95aULL, 0xabb3d0cb537c7cc3ULL,
      0xc2111cba887ffb19ULL, 0xc5ddc9f1f9b60ae4ULL,
      0x31329389c4294778ULL, 0x99db309a72953f23ULL,
      0x6a42a7cdde29dc72ULL, 0x1e4a55c0d6103e70ULL,
      0xeb67e7b038908910ULL,
  };
  for (std::size_t index = 0; index < kCodePages.size(); ++index) {
    require(mapping_fingerprint(kCodePages[index]) == expected[index],
            "Recovered code-page table fingerprint changed");
  }
}

void test_unknown_code_page() {
  const auto unknown = static_cast<LegacyCodePage>(9999);
  require(!jwpqt::core::legacy_byte_to_unicode(0x80, unknown),
          "Unknown code page decoded an extended byte");
  require(!jwpqt::core::unicode_to_legacy_byte(U'\u00e9', unknown),
          "Unknown code page encoded an extended character");
  require(jwpqt::core::legacy_byte_to_unicode('A', unknown) == U'A',
          "ASCII should remain independent of the code page");
  require(jwpqt::core::kDefaultLegacyCodePage == LegacyCodePage::k1252,
          "Legacy fallback code page changed");
}

void test_code_page_names() {
  for (std::size_t index = 0; index < kCodePages.size(); ++index) {
    const LegacyCodePage code_page = kCodePages[index];
    const std::string_view display =
        jwpqt::core::legacy_code_page_name(code_page);
    require(display == "windows-125" + std::to_string(index),
            "Code-page display name changed");
    require(jwpqt::core::parse_legacy_code_page(display) == code_page,
            "Canonical code-page name did not round-trip");

    const std::string number = "125" + std::to_string(index);
    require(jwpqt::core::parse_legacy_code_page(number) == code_page,
            "Numeric code-page name was not parsed");
    require(jwpqt::core::parse_legacy_code_page("cp" + number) == code_page,
            "CP-prefixed code-page name was not parsed");
    require(jwpqt::core::parse_legacy_code_page("windows-" + number) ==
                code_page,
            "Windows-prefixed code-page name was not parsed");
  }

  require(jwpqt::core::legacy_code_page_name(
              static_cast<LegacyCodePage>(9999)) == "Unknown",
          "Unknown code-page name changed");
  require(!jwpqt::core::parse_legacy_code_page("1259"),
          "Unsupported code page was accepted");
  require(!jwpqt::core::parse_legacy_code_page("Windows-1252"),
          "Uppercase alias was unexpectedly accepted");
  require(!jwpqt::core::parse_legacy_code_page("cp1252extra"),
          "Trailing code-page text was accepted");
  require(!jwpqt::core::parse_legacy_code_page("windows1252"),
          "Missing code-page separator was accepted");
  require(!jwpqt::core::parse_legacy_code_page("cp-1252"),
          "Extra code-page separator was accepted");
  require(!jwpqt::core::parse_legacy_code_page("1252 "),
          "Code-page trailing whitespace was accepted");
  require(!jwpqt::core::parse_legacy_code_page("cp"),
          "Truncated code-page name was accepted");
  require(!jwpqt::core::parse_legacy_code_page(""),
          "Empty code-page name was accepted");
}

}  // namespace

int main() {
  test_ascii();
  test_recovered_tables();
  test_undefined_and_inverse_stability();
  test_recovered_table_fingerprints();
  test_unknown_code_page();
  test_code_page_names();
  std::cout << "All legacy code-page tests passed\n";
}
