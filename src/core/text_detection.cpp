#include "jwpqt/core/text_detection.h"

#include "jwpqt/core/jis_text.h"
#include "jwpqt/core/legacy_text.h"
#include "jwpqt/core/utf8.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace jwpqt::core {
namespace {

constexpr std::array<TextEncoding, 6> kAllEncodings = {
    TextEncoding::kUtf8,
    TextEncoding::kEucJp,
    TextEncoding::kShiftJis,
    TextEncoding::kNewJis,
    TextEncoding::kOldJis,
    TextEncoding::kNecJis,
};

bool has_utf8_bom(std::string_view bytes) {
  return bytes.size() >= 3 &&
         static_cast<unsigned char>(bytes[0]) == 0xef &&
         static_cast<unsigned char>(bytes[1]) == 0xbb &&
         static_cast<unsigned char>(bytes[2]) == 0xbf;
}

bool is_ascii_only(std::string_view bytes) {
  return std::all_of(bytes.begin(), bytes.end(), [](char byte) {
    return static_cast<unsigned char>(byte) < 0x80;
  });
}

void add_jis_designations(std::string_view bytes,
                          std::array<bool, 3>& designations) {
  constexpr unsigned char kEscape = 0x1b;

  for (std::size_t index = 0; index < bytes.size(); ++index) {
    if (static_cast<unsigned char>(bytes[index]) != kEscape) {
      continue;
    }

    if (index + 2 < bytes.size() && bytes[index + 1] == '$') {
      if (bytes[index + 2] == 'B') {
        designations[0] = true;
      } else if (bytes[index + 2] == '@') {
        designations[1] = true;
      }
    } else if (index + 1 < bytes.size() && bytes[index + 1] == 'K') {
      designations[2] = true;
    }
  }
}

bool decodes_as(std::string_view bytes, TextEncoding encoding) {
  try {
    static_cast<void>(decode_text_file(bytes, encoding));
    return true;
  } catch (const Utf8Error&) {
    return false;
  } catch (const LegacyTextError&) {
    return false;
  } catch (const JisTextError&) {
    return false;
  } catch (const TextFileError&) {
    return false;
  }
}

}  // namespace

TextEncodingDetection detect_text_encoding(std::string_view bytes) {
  if (has_utf8_bom(bytes)) {
    if (decodes_as(bytes, TextEncoding::kUtf8)) {
      return {DetectionConfidence::kCertain, {TextEncoding::kUtf8}};
    }
    return {};
  }

  std::array<bool, 3> jis_designations{};
  add_jis_designations(bytes, jis_designations);
  const auto designation_count =
      static_cast<std::size_t>(std::count(jis_designations.begin(),
                                          jis_designations.end(), true));
  if (designation_count != 0) {
    try {
      static_cast<void>(decode_jis_text(bytes, JisTextEncoding::kNewJis));
    } catch (const JisTextError&) {
      return {};
    }

    TextEncodingDetection detection;
    detection.confidence = designation_count == 1
                               ? DetectionConfidence::kCertain
                               : DetectionConfidence::kAmbiguous;
    if (jis_designations[0]) {
      detection.candidates.push_back(TextEncoding::kNewJis);
    }
    if (jis_designations[1]) {
      detection.candidates.push_back(TextEncoding::kOldJis);
    }
    if (jis_designations[2]) {
      detection.candidates.push_back(TextEncoding::kNecJis);
    }
    return detection;
  }

  if (is_ascii_only(bytes)) {
    TextEncodingDetection detection;
    detection.confidence = DetectionConfidence::kAsciiOnly;
    for (const auto encoding : kAllEncodings) {
      if (decodes_as(bytes, encoding)) {
        detection.candidates.push_back(encoding);
      }
    }
    return detection;
  }

  TextEncodingDetection detection;
  for (const auto encoding : {TextEncoding::kUtf8, TextEncoding::kEucJp,
                              TextEncoding::kShiftJis}) {
    if (decodes_as(bytes, encoding)) {
      detection.candidates.push_back(encoding);
    }
  }

  if (detection.candidates.empty()) {
    detection.confidence = DetectionConfidence::kUnknown;
  } else if (detection.candidates.size() == 1) {
    detection.confidence = DetectionConfidence::kCertain;
  } else {
    detection.confidence = DetectionConfidence::kAmbiguous;
  }
  return detection;
}

}  // namespace jwpqt::core
