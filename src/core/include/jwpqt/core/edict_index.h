// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "jwpqt/core/edict_dictionary.h"
#include "jwpqt/core/jwp_document.h"
#include "jwpqt/core/legacy_code_page.h"

namespace jwpqt::core {

class EdictIndexError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

struct EdictIndexOptions {
  std::size_t encoded_bytes = 32U * 1024U * 1024U;
  std::size_t entries = 4'000'000;
  std::size_t lookup_steps = 64U * 1024U * 1024U;
  std::size_t matches = 1'000'000;
  LegacyCodePage utf8_code_page = LegacyCodePage::k1252;
};

struct EdictIndexEntry {
  std::size_t byte_offset = 0;
  std::size_t record_index = 0;

  bool operator==(const EdictIndexEntry& other) const noexcept;
};

class EdictIndex {
 public:
  static EdictIndex parse(
      std::string_view bytes, const EdictDictionary& dictionary,
      const EdictIndexOptions& options = EdictIndexOptions{});

  std::string serialize() const;
  std::uint32_t source_extent() const noexcept;
  const std::vector<EdictIndexEntry>& entries() const noexcept;
  std::vector<EdictIndexEntry> find(const JwpText& key) const;

 private:
  int compare_with_key(std::size_t byte_offset,
                       const JwpText& normalized_key,
                       std::size_t& steps) const;

  EdictEncoding encoding_ = EdictEncoding::kEucJp;
  LegacyCodePage utf8_code_page_ = LegacyCodePage::k1252;
  std::string source_bytes_;
  std::uint32_t source_extent_ = 0;
  std::size_t lookup_steps_ = 0;
  std::size_t matches_ = 0;
  std::vector<EdictIndexEntry> entries_;
};

}  // namespace jwpqt::core
