#pragma once

#include "jwpqt/core/text_file.h"

#include <string_view>
#include <vector>

namespace jwpqt::core {

enum class DetectionConfidence {
  kCertain,
  kAmbiguous,
  kAsciiOnly,
  kUnknown,
};

struct TextEncodingDetection {
  DetectionConfidence confidence = DetectionConfidence::kUnknown;
  std::vector<TextEncoding> candidates;
};

// Detects viable encodings without choosing among ambiguous byte streams.
TextEncodingDetection detect_text_encoding(std::string_view bytes);

}  // namespace jwpqt::core
