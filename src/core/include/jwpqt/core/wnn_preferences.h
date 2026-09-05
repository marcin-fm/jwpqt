// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "jwpqt/core/wnn_lookup.h"

namespace jwpqt::core {

constexpr std::size_t kWnnPreferenceKeySize = 6;
constexpr std::size_t kWnnPreferenceRecordSize = 8;
constexpr std::size_t kWnnDefaultPreferenceCapacity = 200;
constexpr std::size_t kWnnMaximumPreferenceCapacity = 2'000;

class WnnPreferenceError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

struct WnnPreferenceEntry {
  std::array<std::uint8_t, kWnnPreferenceKeySize> key{};
  std::int16_t selected_offset = 0;

  bool operator==(const WnnPreferenceEntry& other) const noexcept;
};

class WnnPreferences {
 public:
  explicit WnnPreferences(
      std::size_t capacity = kWnnDefaultPreferenceCapacity);

  // user.sel is a flat sequence of six key bytes and one little-endian signed
  // 16-bit slash-list offset. Missing bytes are zero-filled and bytes beyond
  // capacity are ignored, matching the recovered loader.
  static WnnPreferences parse(
      std::string_view bytes,
      std::size_t capacity = kWnnDefaultPreferenceCapacity);
  std::string serialize() const;

  // Returns the stored candidate index, or the first candidate when no valid
  // preference exists. An empty result has no selectable candidate.
  std::optional<std::size_t> preferred_candidate(
      const JwpText& input, const WnnLookupResult& result) const;

  // Learns the selected slash-list offset. The final candidate, keys longer
  // than six kana, and a new default (first) choice are not cached. Existing
  // entries may be reset to the default offset.
  bool remember(const JwpText& input, const WnnLookupResult& result,
                std::size_t candidate_index);

  void resize(std::size_t capacity);
  std::size_t capacity() const noexcept;
  const std::vector<WnnPreferenceEntry>& entries() const noexcept;
  bool changed() const noexcept;
  void mark_saved() noexcept;

 private:
  explicit WnnPreferences(std::vector<WnnPreferenceEntry> entries);

  std::vector<WnnPreferenceEntry> entries_;
  bool changed_ = false;
};

}  // namespace jwpqt::core
