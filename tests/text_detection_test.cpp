#include "jwpqt/core/text_detection.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

using jwpqt::core::DetectionConfidence;
using jwpqt::core::TextEncoding;
using jwpqt::core::TextEncodingDetection;

void fail(const std::string& message) {
  std::cerr << message << '\n';
  std::exit(1);
}

void expect_detection(std::string_view bytes, DetectionConfidence confidence,
                      const std::vector<TextEncoding>& candidates,
                      const std::string& context) {
  const TextEncodingDetection detection =
      jwpqt::core::detect_text_encoding(bytes);
  if (detection.confidence != confidence ||
      detection.candidates != candidates) {
    fail(context);
  }
}

std::string bytes(std::initializer_list<unsigned int> values) {
  std::string result;
  result.reserve(values.size());
  for (const unsigned int value : values) {
    result.push_back(static_cast<char>(value));
  }
  return result;
}

}  // namespace

int main() {
  const std::vector<TextEncoding> all_encodings = {
      TextEncoding::kUtf8,    TextEncoding::kEucJp,
      TextEncoding::kShiftJis, TextEncoding::kNewJis,
      TextEncoding::kOldJis,  TextEncoding::kNecJis,
  };

  expect_detection("ASCII only\n", DetectionConfidence::kAsciiOnly,
                   all_encodings, "ASCII-only detection mismatch");
  expect_detection("", DetectionConfidence::kAsciiOnly, all_encodings,
                   "empty-file detection mismatch");
  expect_detection(bytes({0x1b}), DetectionConfidence::kAsciiOnly,
                   {TextEncoding::kUtf8, TextEncoding::kEucJp,
                    TextEncoding::kShiftJis},
                   "truncated ASCII escape candidates mismatch");
  expect_detection(bytes({0x1b, 'X'}), DetectionConfidence::kAsciiOnly,
                   {TextEncoding::kUtf8, TextEncoding::kEucJp,
                    TextEncoding::kShiftJis},
                   "unknown ASCII escape candidates mismatch");

  expect_detection(bytes({0xef, 0xbb, 0xbf, 0xe3, 0x81, 0x82}),
                   DetectionConfidence::kCertain, {TextEncoding::kUtf8},
                   "BOM UTF-8 detection mismatch");
  expect_detection(bytes({0xe3, 0x81, 0x82}), DetectionConfidence::kCertain,
                   {TextEncoding::kUtf8}, "UTF-8 detection mismatch");
  expect_detection(bytes({0xa4, 0xa2, 0xc6, 0xfc}),
                   DetectionConfidence::kCertain, {TextEncoding::kEucJp},
                   "EUC-JP detection mismatch");
  expect_detection(bytes({0x82, 0xa0, 0x93, 0xfa}),
                   DetectionConfidence::kCertain, {TextEncoding::kShiftJis},
                   "Shift-JIS detection mismatch");

  expect_detection(bytes({0x1b, '$', 'B', 0x24, 0x22, 0x1b, '(', 'J'}),
                   DetectionConfidence::kCertain, {TextEncoding::kNewJis},
                   "New JIS detection mismatch");
  expect_detection(bytes({0x1b, '$', '@', 0x24, 0x22, 0x1b, '(', 'J'}),
                   DetectionConfidence::kCertain, {TextEncoding::kOldJis},
                   "Old JIS detection mismatch");
  expect_detection(bytes({0x1b, 'K', 0x24, 0x22, 0x1b, 'H'}),
                   DetectionConfidence::kCertain, {TextEncoding::kNecJis},
                   "NEC JIS detection mismatch");

  expect_detection(
      bytes({0x1b, '$', 'B', 0x24, 0x22, 0x1b, '(', 'J',
             0x1b, '$', '@', 0x24, 0x22, 0x1b, '(', 'J'}),
      DetectionConfidence::kAmbiguous,
      {TextEncoding::kNewJis, TextEncoding::kOldJis},
      "mixed JIS detection mismatch");
  expect_detection(bytes({0xe0, 0xa1}), DetectionConfidence::kAmbiguous,
                   {TextEncoding::kEucJp, TextEncoding::kShiftJis},
                   "ambiguous EUC-JP/Shift-JIS detection mismatch");

  expect_detection(bytes({0xff}), DetectionConfidence::kUnknown, {},
                   "invalid high-byte detection mismatch");
  expect_detection(bytes({0xef, 0xbb, 0xbf, 0xff}),
                   DetectionConfidence::kUnknown, {},
                   "malformed BOM UTF-8 detection mismatch");
  expect_detection(bytes({0x1b, '$', 'B', 0x22, 0x2f}),
                   DetectionConfidence::kUnknown, {},
                   "unmapped JIS detection mismatch");

  return 0;
}
