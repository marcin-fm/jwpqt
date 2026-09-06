// SPDX-License-Identifier: GPL-2.0-or-later

#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "jwpqt/core/text_file.h"
#include "jwpqt/core/text_detection.h"

namespace {

using jwpqt::core::TextEncoding;
using jwpqt::core::TextFile;

void require(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

template <typename Function>
void require_error(Function function, std::string_view message) {
  try {
    function();
  } catch (const std::exception&) {
    return;
  }
  throw std::runtime_error(std::string(message));
}

template <typename Function>
std::string require_text_file_error(Function function,
                                    std::string_view message) {
  try {
    function();
  } catch (const jwpqt::core::TextFileError& error) {
    return error.what();
  } catch (const std::exception&) {
    throw std::runtime_error(std::string(message) + ": wrong error type");
  }
  throw std::runtime_error(std::string(message) + ": no error");
}

void test_encoding_names() {
  require(jwpqt::core::text_encoding_name(TextEncoding::kUtf8) == "UTF-8",
          "Wrong UTF-8 display name");
  require(jwpqt::core::text_encoding_name(TextEncoding::kUtf7) == "UTF-7",
          "Wrong UTF-7 display name");
  require(jwpqt::core::text_encoding_name(TextEncoding::kJfc) == "JFC",
          "Wrong JFC display name");
  require(jwpqt::core::text_encoding_name(TextEncoding::kEucJp) == "EUC-JP",
          "Wrong EUC-JP display name");
  require(jwpqt::core::text_encoding_name(TextEncoding::kShiftJis) ==
              "Shift-JIS",
           "Wrong Shift-JIS display name");
  require(jwpqt::core::text_encoding_name(TextEncoding::kNewJis) ==
              "New JIS",
          "Wrong New JIS display name");
  require(jwpqt::core::text_encoding_name(TextEncoding::kOldJis) == "Old JIS",
          "Wrong Old JIS display name");
  require(jwpqt::core::text_encoding_name(TextEncoding::kNecJis) == "NEC JIS",
          "Wrong NEC JIS display name");
  require(jwpqt::core::parse_text_encoding("utf-8") == TextEncoding::kUtf8,
          "Could not parse utf-8");
  require(jwpqt::core::parse_text_encoding("utf-7") == TextEncoding::kUtf7,
          "Could not parse utf-7");
  require(jwpqt::core::parse_text_encoding("jfc") == TextEncoding::kJfc,
          "Could not parse jfc");
  require(jwpqt::core::parse_text_encoding("euc-jp") == TextEncoding::kEucJp,
          "Could not parse euc-jp");
  require(jwpqt::core::parse_text_encoding("shift-jis") ==
              TextEncoding::kShiftJis,
          "Could not parse shift-jis");
  require(jwpqt::core::parse_text_encoding("new-jis") ==
              TextEncoding::kNewJis,
          "Could not parse new-jis");
  require(jwpqt::core::parse_text_encoding("old-jis") ==
              TextEncoding::kOldJis,
          "Could not parse old-jis");
  require(jwpqt::core::parse_text_encoding("nec-jis") ==
              TextEncoding::kNecJis,
          "Could not parse nec-jis");
  require(!jwpqt::core::parse_text_encoding("auto").has_value(),
          "Unknown encoding was accepted");
}

void test_utf8_file() {
  const TextFile expected{
      U"ASCII \u65e5\u672c\u8a9e \u3042\U0001f642\n",
      TextEncoding::kUtf8, true};
  const std::string encoded = jwpqt::core::encode_text_file(expected);
  require(encoded.compare(0, 3, "\xef\xbb\xbf") == 0,
          "UTF-8 BOM was not written");
  const TextFile actual =
      jwpqt::core::decode_text_file(encoded, TextEncoding::kUtf8);
  require(actual.text == expected.text, "UTF-8 text did not round-trip");
  require(actual.encoding == expected.encoding, "UTF-8 encoding was not kept");
  require(actual.has_byte_order_mark, "UTF-8 BOM was not kept");
}

void test_utf16_files() {
  using namespace jwpqt::core;
  require(parse_text_encoding("utf-16le") == TextEncoding::kUtf16Le &&
              parse_text_encoding("utf-16be") == TextEncoding::kUtf16Be &&
              text_encoding_name(TextEncoding::kUtf16Le) == "UTF-16LE" &&
              text_encoding_name(TextEncoding::kUtf16Be) == "UTF-16BE",
          "UTF-16 names are not registered");
  for (const auto encoding : {TextEncoding::kUtf16Le, TextEncoding::kUtf16Be}) {
    for (const bool bom : {false, true}) {
      const TextFile file{U"ASCII \u65e5\u672c\U0001f600\n", encoding, bom};
      const auto bytes = encode_text_file(file);
      const auto decoded = decode_text_file(bytes, encoding);
      require(decoded.text == file.text && decoded.encoding == encoding &&
                  decoded.has_byte_order_mark == bom,
              "UTF-16 TextFile did not round trip");
      if (bom) {
        const auto detection = detect_text_encoding(bytes);
        require(detection.confidence == DetectionConfidence::kCertain &&
                    detection.candidates == std::vector<TextEncoding>{encoding},
                "UTF-16 BOM was not detected");
      }
    }
  }
  for (const auto& bytes : {std::string("\xff\xfeX"), std::string("\xfe\xffX"),
                            std::string("\xff\xfe\x3d\xd8"),
                            std::string("\xfe\xff\xd8\x3d")}) {
    require(detect_text_encoding(bytes).candidates.empty(),
            "Malformed UTF-16 BOM fell back to another encoding");
  }
}

void test_legacy_file(TextEncoding encoding, std::string_view expected_bytes) {
  const TextFile expected{U"ASCII \u65e5\u672c\u8a9e\n", encoding, false};
  const std::string encoded = jwpqt::core::encode_text_file(expected);
  require(encoded == expected_bytes, "Legacy file bytes were wrong");
  const TextFile actual = jwpqt::core::decode_text_file(encoded, encoding);
  require(actual.text == expected.text, "Legacy text did not round-trip");
  require(actual.encoding == encoding, "Legacy encoding was not kept");
  require(!actual.has_byte_order_mark, "Legacy file acquired a BOM");
}

void test_jfc_file() {
  const TextFile old_euc = jwpqt::core::decode_text_file(
      "\xc6\xfc\t\x8e\x26\x8f\xab\xb1\n", TextEncoding::kJfc);
  require(old_euc.text == U"\u65e5\t\u00a6\u00e9\n" &&
              old_euc.encoding == TextEncoding::kJfc &&
              !old_euc.has_byte_order_mark,
          "JFC old-EUC decoding lost text or format metadata");
  require(jwpqt::core::encode_text_file(old_euc) ==
              "\xe6\x97\xa5\t\xc2\xa6\xc3\xa9\n",
          "JFC old-EUC file was not saved as UTF-8");
  const TextFile with_bom = jwpqt::core::decode_text_file(
      "\xef\xbb\xbf\xc3\xa9", TextEncoding::kJfc);
  require(with_bom.text == U"\u00e9" && !with_bom.has_byte_order_mark &&
              jwpqt::core::encode_text_file(with_bom) == "\xc3\xa9",
          "JFC preserved an input BOM or did not prefer UTF-8");
  require_text_file_error(
      [] { jwpqt::core::encode_text_file({U"text", TextEncoding::kJfc, true}); },
      "JFC file accepted BOM metadata");
  require_error(
      [] { jwpqt::core::decode_text_file("\x8f\xb0\xa1", TextEncoding::kJfc); },
      "JFC accepted an unsupported extension through TextFile");
}

void test_invalid_metadata() {
  require_error(
      [] {
        jwpqt::core::encode_text_file(
            {U"text", TextEncoding::kEucJp, true});
      },
      "EUC-JP file accepted a BOM");
  const TextEncoding unknown = static_cast<TextEncoding>(999);
  const std::string name_error = require_text_file_error(
      [unknown] { jwpqt::core::text_encoding_name(unknown); },
      "Unknown encoding had a display name");
  const std::string decode_error = require_text_file_error(
      [unknown] { jwpqt::core::decode_text_file("text", unknown); },
      "Unknown encoding was decoded");
  const std::string encode_error = require_text_file_error(
      [unknown] {
        jwpqt::core::encode_text_file({U"text", unknown, false});
      },
      "Unknown encoding was encoded without a BOM");
  const std::string encode_bom_error = require_text_file_error(
      [unknown] { jwpqt::core::encode_text_file({U"text", unknown, true}); },
      "Unknown encoding was encoded with a BOM");
  require(name_error == "Unknown text encoding",
          "Unknown encoding name error was misleading");
  require(decode_error == name_error && encode_error == name_error &&
              encode_bom_error == name_error,
          "Unknown encoding errors were inconsistent");
}

}  // namespace

int main() {
  try {
    test_encoding_names();
    test_utf8_file();
    test_utf16_files();
    test_jfc_file();
    test_legacy_file(TextEncoding::kEucJp,
                     "ASCII \xc6\xfc\xcb\xdc\xb8\xec\n");
    test_legacy_file(TextEncoding::kShiftJis,
                     "ASCII \x93\xfa\x96\x7b\x8c\xea\n");
    test_legacy_file(TextEncoding::kNewJis,
                     "ASCII \x1b$B\x46\x7c\x4b\x5c\x38\x6c\x1b(J\n");
    test_legacy_file(TextEncoding::kOldJis,
                     "ASCII \x1b$@\x46\x7c\x4b\x5c\x38\x6c\x1b(J\n");
    test_legacy_file(TextEncoding::kNecJis,
                     "ASCII \x1bK\x46\x7c\x4b\x5c\x38\x6c\x1bH\n");
    test_legacy_file(TextEncoding::kUtf7, "ASCII +ZeVnLIqe-\n");
    test_invalid_metadata();
    std::cout << "All text file tests passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
