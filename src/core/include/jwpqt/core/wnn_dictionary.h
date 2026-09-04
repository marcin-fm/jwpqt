// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "jwpqt/core/jwp_document.h"

namespace jwpqt::core {

constexpr std::size_t kWnnIndexKeySize = 3;
constexpr std::size_t kWnnMaximumKeySize = 19;
constexpr std::size_t kWnnMaximumRecordCount = 65'536;
constexpr std::size_t kWnnMaximumCandidateCount = 262'144;
constexpr std::size_t kWnnMaximumCandidateCells = 1'048'576;

class WnnDictionaryError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

struct WnnRecord {
  std::uint32_t data_offset = 0;
  std::vector<std::uint8_t> key;
  char ending = '*';
  std::vector<JwpText> candidates;
};

struct WnnIndexEntry {
  std::array<std::uint8_t, kWnnIndexKeySize> key{};
  std::uint32_t data_offset = 0;
  std::size_t record_index = 0;
};

class WnnDictionary {
 public:
  static WnnDictionary parse(std::string_view index_bytes,
                             std::string_view data_bytes);

  const std::vector<WnnIndexEntry>& index() const noexcept;
  const std::vector<WnnRecord>& records() const noexcept;

 private:
  std::vector<WnnIndexEntry> index_;
  std::vector<WnnRecord> records_;
};

}  // namespace jwpqt::core
