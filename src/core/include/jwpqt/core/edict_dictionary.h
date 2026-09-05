// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace jwpqt::core {

enum class EdictEncoding {
  kEucJp,
  kUtf8,
};

struct EdictParseLimits {
  std::size_t encoded_bytes = 64U * 1024U * 1024U;
  std::size_t line_bytes = 1024U * 1024U;
  std::size_t records = 1'000'000;
  std::size_t definitions = 4'000'000;
  std::size_t decoded_code_points = 32U * 1024U * 1024U;
};

class EdictDictionaryError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

struct EdictRecord {
  std::size_t byte_offset = 0;
  std::size_t byte_length = 0;
  std::u32string headword;
  std::vector<std::u32string> readings;
  std::vector<std::u32string> definitions;

  bool operator==(const EdictRecord& other) const noexcept;
};

class EdictDictionary {
 public:
  static EdictDictionary parse(
      std::string_view bytes, EdictEncoding encoding,
      const EdictParseLimits& limits = EdictParseLimits{});

  EdictEncoding encoding() const noexcept;
  std::string_view source_bytes() const noexcept;
  const std::vector<EdictRecord>& records() const noexcept;

 private:
  EdictEncoding encoding_ = EdictEncoding::kEucJp;
  std::string source_bytes_;
  std::vector<EdictRecord> records_;
};

}  // namespace jwpqt::core
