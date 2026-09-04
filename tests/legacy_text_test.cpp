// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/legacy_text.h"

#include <array>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "jwpqt/core/jis_encoding.h"
#include "jwpqt/core/jis_unicode.h"

namespace {

using jwpqt::core::EncodedPair;
using jwpqt::core::JisCode;
using jwpqt::core::LegacyEncoding;
using jwpqt::core::LegacyTextError;

void require(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

template <typename Function>
void require_error(Function&& function, std::string_view message) {
  try {
    function();
  } catch (const LegacyTextError&) {
    return;
  }
  throw std::runtime_error(std::string(message));
}

template <typename Function>
void require_error_contains(Function&& function, std::string_view expected,
                            std::string_view message) {
  try {
    function();
  } catch (const LegacyTextError& error) {
    require(std::string_view(error.what()).find(expected) !=
                std::string_view::npos,
            message);
    return;
  }
  throw std::runtime_error(std::string(message));
}

std::string bytes(std::initializer_list<unsigned int> values) {
  std::string output;
  output.reserve(values.size());
  for (const unsigned int value : values) {
    output.push_back(static_cast<char>(value));
  }
  return output;
}

void check_known_text() {
  const std::u32string text = U"ASCII あ日本語\n";
  const std::string euc =
      "ASCII " + bytes({0xa4, 0xa2, 0xc6, 0xfc, 0xcb, 0xdc, 0xb8, 0xec}) +
      "\n";
  const std::string shift_jis =
      "ASCII " + bytes({0x82, 0xa0, 0x93, 0xfa, 0x96, 0x7b, 0x8c, 0xea}) +
      "\n";

  require(jwpqt::core::decode_legacy_text(euc, LegacyEncoding::kEucJp) ==
              text,
          "Known EUC-JP text must decode");
  require(jwpqt::core::encode_legacy_text(text, LegacyEncoding::kEucJp) ==
              euc,
          "Known EUC-JP text must encode");
  require(jwpqt::core::decode_legacy_text(
              shift_jis, LegacyEncoding::kShiftJis) == text,
          "Known Shift-JIS text must decode");
  require(jwpqt::core::encode_legacy_text(
              text, LegacyEncoding::kShiftJis) == shift_jis,
          "Known Shift-JIS text must encode");
}

void check_ascii() {
  std::string ascii;
  std::u32string unicode;
  for (unsigned int value = 0; value <= 0x7fU; ++value) {
    ascii.push_back(static_cast<char>(value));
    unicode.push_back(static_cast<char32_t>(value));
  }

  for (const LegacyEncoding encoding : {LegacyEncoding::kEucJp,
                                        LegacyEncoding::kShiftJis}) {
    require(jwpqt::core::decode_legacy_text(ascii, encoding) == unicode,
            "All ASCII bytes must decode unchanged");
    require(jwpqt::core::encode_legacy_text(unicode, encoding) == ascii,
            "All ASCII characters must encode unchanged");
  }
}

void check_mapped_repertoire() {
  std::size_t mapped = 0;
  for (unsigned int row = 0x21; row <= 0x7e; ++row) {
    for (unsigned int cell = 0x21; cell <= 0x7e; ++cell) {
      const JisCode jis = static_cast<JisCode>((row << 8U) | cell);
      const std::optional<char32_t> code_point =
          jwpqt::core::jis_x0208_to_unicode(jis);
      if (!code_point.has_value()) {
        for (const LegacyEncoding encoding : {LegacyEncoding::kEucJp,
                                              LegacyEncoding::kShiftJis}) {
          const EncodedPair pair =
              encoding == LegacyEncoding::kEucJp
                  ? jwpqt::core::encode_euc_jp_pair(jis)
                  : jwpqt::core::encode_shift_jis_pair(jis);
          const std::string encoded = bytes({pair.lead, pair.trail});
          require_error(
              [&] { jwpqt::core::decode_legacy_text(encoded, encoding); },
              "Every unmapped structural JIS character must fail");
        }
        continue;
      }
      ++mapped;
      const std::u32string text(1, *code_point);

      for (const LegacyEncoding encoding : {LegacyEncoding::kEucJp,
                                            LegacyEncoding::kShiftJis}) {
        const EncodedPair pair = encoding == LegacyEncoding::kEucJp
                                     ? jwpqt::core::encode_euc_jp_pair(jis)
                                     : jwpqt::core::encode_shift_jis_pair(jis);
        const std::string encoded = bytes({pair.lead, pair.trail});
        require(jwpqt::core::decode_legacy_text(encoded, encoding) == text,
                "Every mapped JIS character must decode");

        const std::string canonical =
            jwpqt::core::encode_legacy_text(text, encoding);
        require(jwpqt::core::decode_legacy_text(canonical, encoding) == text,
                "Every mapped Unicode character must encode canonically");
      }
    }
  }
  require(mapped == 6892, "Mapped repertoire count must remain stable");
}

void check_canonical_duplicates() {
  struct Fixture {
    char32_t code_point;
    JisCode canonical_jis;
  };
  constexpr std::array<Fixture, 12> fixtures{{
      {0xff1a, 0x2127}, {0xff1b, 0x2128}, {0xff1c, 0x2163},
      {0xff1d, 0x2161}, {0xff1e, 0x2164}, {0xff1f, 0x2129},
      {0xff20, 0x2177}, {0xff3b, 0x214e}, {0xff3d, 0x214f},
      {0xff3e, 0x2130}, {0xff3f, 0x2132}, {0xff40, 0x212e},
  }};

  for (const Fixture& fixture : fixtures) {
    const std::u32string text(1, fixture.code_point);
    for (const LegacyEncoding encoding : {LegacyEncoding::kEucJp,
                                          LegacyEncoding::kShiftJis}) {
      const EncodedPair expected = encoding == LegacyEncoding::kEucJp
                                       ? jwpqt::core::encode_euc_jp_pair(
                                             fixture.canonical_jis)
                                       : jwpqt::core::encode_shift_jis_pair(
                                             fixture.canonical_jis);
      require(jwpqt::core::encode_legacy_text(text, encoding) ==
                  bytes({expected.lead, expected.trail}),
              "Duplicate Unicode must encode to the legacy canonical pair");
    }
  }
}

void check_rejections() {
  require_error(
      [] { jwpqt::core::decode_legacy_text(bytes({0xa4}),
                                               LegacyEncoding::kEucJp); },
      "Truncated EUC-JP must fail");
  require_error(
      [] { jwpqt::core::decode_legacy_text(bytes({0xa4, 0x20}),
                                               LegacyEncoding::kEucJp); },
      "Bad EUC-JP trail must fail");
  require_error(
      [] { jwpqt::core::decode_legacy_text(bytes({0x8e, 0xa6}),
                                               LegacyEncoding::kEucJp); },
      "EUC-JP halfwidth kana must fail explicitly");
  require_error(
      [] { jwpqt::core::decode_legacy_text(bytes({0x8f, 0xa1, 0xa1}),
                                               LegacyEncoding::kEucJp); },
      "JIS X 0212 must fail explicitly");
  require_error(
      [] { jwpqt::core::decode_legacy_text(bytes({0xcf, 0xd4}),
                                               LegacyEncoding::kEucJp); },
      "Unmapped EUC-JP character must fail");

  require_error(
      [] { jwpqt::core::decode_legacy_text(bytes({0x82}),
                                               LegacyEncoding::kShiftJis); },
      "Truncated Shift-JIS must fail");
  require_error(
      [] { jwpqt::core::decode_legacy_text(bytes({0x82, 0x7f}),
                                               LegacyEncoding::kShiftJis); },
      "Bad Shift-JIS trail must fail");
  require_error(
      [] { jwpqt::core::decode_legacy_text(bytes({0xa6}),
                                               LegacyEncoding::kShiftJis); },
      "Shift-JIS halfwidth kana must fail explicitly");
  require_error(
      [] { jwpqt::core::decode_legacy_text(bytes({0xf0, 0x40}),
                                               LegacyEncoding::kShiftJis); },
      "Shift-JIS extension lead must fail");

  require_error(
      [] { jwpqt::core::encode_legacy_text(U"😀", LegacyEncoding::kEucJp); },
      "Unmappable Unicode must fail for EUC-JP");
  require_error(
      [] {
        jwpqt::core::encode_legacy_text(U"😀", LegacyEncoding::kShiftJis);
      },
      "Unmappable Unicode must fail for Shift-JIS");

  const auto unknown = static_cast<LegacyEncoding>(999);
  require_error(
      [unknown] { jwpqt::core::decode_legacy_text("", unknown); },
      "Unknown decode encoding must fail");
  require_error(
      [unknown] { jwpqt::core::encode_legacy_text(U"", unknown); },
      "Unknown encode encoding must fail");

  require_error_contains(
      [] {
        jwpqt::core::decode_legacy_text("A" + bytes({0xa4}),
                                        LegacyEncoding::kEucJp);
      },
      "byte 1", "Decode errors must identify the source byte offset");
  require_error_contains(
      [] {
        jwpqt::core::encode_legacy_text(U"A😀", LegacyEncoding::kShiftJis);
      },
      "character 1", "Encode errors must identify the character offset");
}

}  // namespace

int main() {
  try {
    check_known_text();
    check_ascii();
    check_mapped_repertoire();
    check_canonical_duplicates();
    check_rejections();
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
