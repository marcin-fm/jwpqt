#pragma once

#include "jwpqt/core/jis_encoding.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace jwpqt::core {

enum class JwpVersion {
  kJ120,
  kB2,
  kB1,
};

using JwpText = std::vector<JisCode>;

struct JwpParagraph {
  JwpText text;
  std::int16_t line_spacing = 100;
  std::int8_t first_indent = 0;
  std::uint8_t left_indent = 0;
  std::uint8_t right_indent = 0;
  bool page_break = false;

  bool operator==(const JwpParagraph& other) const noexcept {
    return text == other.text && line_spacing == other.line_spacing &&
           first_indent == other.first_indent &&
           left_indent == other.left_indent &&
           right_indent == other.right_indent &&
           page_break == other.page_break;
  }

  bool operator!=(const JwpParagraph& other) const noexcept {
    return !(*this == other);
  }
};

struct JwpDocument {
  JwpVersion source_version = JwpVersion::kJ120;
  std::array<float, 4> margins{};
  bool landscape = false;
  bool separate_left_right_headers = false;
  bool suppress_first_page_headers = false;
  bool vertical = false;
  std::array<JwpText, 5> summary;
  std::array<std::array<JwpText, 3>, 4> headers;
  std::vector<JwpParagraph> paragraphs;

  bool operator==(const JwpDocument& other) const noexcept {
    for (std::size_t index = 0; index < margins.size(); ++index) {
      if (margins[index] != other.margins[index] &&
          !(std::isnan(margins[index]) &&
            std::isnan(other.margins[index]))) {
        return false;
      }
    }
    return source_version == other.source_version &&
           landscape == other.landscape &&
           separate_left_right_headers ==
               other.separate_left_right_headers &&
           suppress_first_page_headers == other.suppress_first_page_headers &&
           vertical == other.vertical && summary == other.summary &&
           headers == other.headers && paragraphs == other.paragraphs;
  }

  bool operator!=(const JwpDocument& other) const noexcept {
    return !(*this == other);
  }
};

class JwpFormatError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

bool has_jwp_document_magic(std::string_view bytes) noexcept;
JwpDocument decode_jwp_document(std::string_view bytes);
std::string encode_jwp_document(const JwpDocument& document);

}  // namespace jwpqt::core
