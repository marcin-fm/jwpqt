// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_CORE_TEXT_FILE_H
#define JWPQT_CORE_TEXT_FILE_H

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace jwpqt::core {

enum class TextEncoding {
  kUtf8,
  kUtf7,
  kEucJp,
  kShiftJis,
  kNewJis,
  kOldJis,
  kNecJis,
};

struct TextFile {
  std::u32string text;
  TextEncoding encoding = TextEncoding::kUtf8;
  bool has_byte_order_mark = false;
};

class TextFileError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

std::string_view text_encoding_name(TextEncoding encoding);
std::optional<TextEncoding> parse_text_encoding(std::string_view name) noexcept;
TextFile decode_text_file(std::string_view bytes, TextEncoding encoding);
std::string encode_text_file(const TextFile& file);

}  // namespace jwpqt::core

#endif
