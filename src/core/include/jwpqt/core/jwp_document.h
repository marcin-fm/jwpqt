#pragma once

#include "jwpqt/core/jis_encoding.h"

#include <array>
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
};

class JwpFormatError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

JwpDocument decode_jwp_document(std::string_view bytes);
std::string encode_jwp_document(const JwpDocument& document);

}  // namespace jwpqt::core
