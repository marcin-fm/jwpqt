// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/wnn_preferences.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "jwpqt/core/byte_io.h"

namespace jwpqt::core {
namespace {

constexpr JisCode kHiraganaBase = 0x2400;
constexpr JisCode kKatakanaBase = 0x2500;
constexpr JisCode kFirstUnsupportedKatakana = 0x2574;

std::size_t checked_capacity(std::size_t capacity) {
  if (capacity == 0 || capacity > kWnnMaximumPreferenceCapacity) {
    throw WnnPreferenceError("WNN preference capacity is out of range");
  }
  return capacity;
}

std::optional<std::array<std::uint8_t, kWnnPreferenceKeySize>> preference_key(
    const JwpText& input) {
  if (input.empty() || input.size() > kWnnPreferenceKeySize) {
    return std::nullopt;
  }

  std::array<std::uint8_t, kWnnPreferenceKeySize> key{};
  for (std::size_t index = 0; index < input.size(); ++index) {
    const JisCode kana = input[index];
    const JisCode page = static_cast<JisCode>(kana & 0xff00U);
    if (page != kHiraganaBase &&
        (page != kKatakanaBase || kana >= kFirstUnsupportedKatakana)) {
      throw WnnPreferenceError("WNN preference key contains non-kana text");
    }
    key[index] = static_cast<std::uint8_t>((kana & 0x00ffU) | 0x80U);
  }
  return key;
}

}  // namespace

bool WnnPreferenceEntry::operator==(
    const WnnPreferenceEntry& other) const noexcept {
  return key == other.key && selected_offset == other.selected_offset;
}

WnnPreferences::WnnPreferences(std::size_t capacity)
    : entries_(checked_capacity(capacity)) {}

WnnPreferences::WnnPreferences(std::vector<WnnPreferenceEntry> entries)
    : entries_(std::move(entries)) {}

WnnPreferences WnnPreferences::parse(std::string_view bytes,
                                     std::size_t capacity) {
  capacity = checked_capacity(capacity);
  std::string padded(capacity * kWnnPreferenceRecordSize, '\0');
  std::copy_n(bytes.begin(), std::min(bytes.size(), padded.size()),
              padded.begin());
  std::vector<WnnPreferenceEntry> entries(capacity);
  ByteReader reader(padded);
  try {
    for (std::size_t index = 0; index < capacity; ++index) {
      for (std::uint8_t& byte : entries[index].key) {
        byte = reader.read_u8();
      }
      entries[index].selected_offset = reader.read_i16_le();
    }
  } catch (const BinaryError& error) {
    throw WnnPreferenceError(error.what());
  }
  return WnnPreferences(std::move(entries));
}

std::string WnnPreferences::serialize() const {
  ByteWriter writer;
  for (const WnnPreferenceEntry& entry : entries_) {
    for (const std::uint8_t byte : entry.key) {
      writer.write_u8(byte);
    }
    writer.write_i16_le(entry.selected_offset);
  }
  return writer.take_bytes();
}

std::optional<std::size_t> WnnPreferences::preferred_candidate(
    const JwpText& input, const WnnLookupResult& result) const {
  if (result.candidates.empty()) {
    return std::nullopt;
  }
  const auto key = preference_key(input);
  if (!key) {
    return 0;
  }

  const auto entry =
      std::find_if(entries_.begin(), entries_.end(), [&key](const auto& item) {
        return item.key == *key;
      });
  if (entry == entries_.end() || entry->selected_offset < 0) {
    return 0;
  }
  const std::size_t offset =
      static_cast<std::size_t>(entry->selected_offset);
  const auto candidate = std::find_if(
      result.candidates.begin(), result.candidates.end(),
      [offset](const WnnCandidate& item) {
        return item.legacy_cell_offset == offset;
      });
  if (candidate == result.candidates.end()) {
    return 0;
  }
  return static_cast<std::size_t>(candidate - result.candidates.begin());
}

bool WnnPreferences::remember(const JwpText& input,
                              const WnnLookupResult& result,
                              std::size_t candidate_index) {
  if (candidate_index >= result.candidates.size()) {
    throw WnnPreferenceError("WNN preference candidate is out of range");
  }
  const auto key = preference_key(input);
  if (!key || candidate_index + 1 == result.candidates.size()) {
    return false;
  }

  const std::size_t offset =
      result.candidates[candidate_index].legacy_cell_offset;
  if (offset > static_cast<std::size_t>(
                   std::numeric_limits<std::int16_t>::max())) {
    throw WnnPreferenceError(
        "WNN preference candidate offset exceeds the wire format");
  }
  const std::int16_t selected_offset = static_cast<std::int16_t>(offset);
  auto entry =
      std::find_if(entries_.begin(), entries_.end(), [&key](const auto& item) {
        return item.key == *key;
      });
  if (entry != entries_.end()) {
    if (entry->selected_offset == selected_offset) {
      return false;
    }
    entry->selected_offset = selected_offset;
    changed_ = true;
    return true;
  }
  if (selected_offset == 0) {
    return false;
  }

  std::move(entries_.begin() + 1, entries_.end(), entries_.begin());
  entries_.back() = {*key, selected_offset};
  changed_ = true;
  return true;
}

void WnnPreferences::resize(std::size_t capacity) {
  capacity = checked_capacity(capacity);
  if (capacity == entries_.size()) {
    return;
  }
  entries_.resize(capacity);
  changed_ = true;
}

std::size_t WnnPreferences::capacity() const noexcept { return entries_.size(); }

const std::vector<WnnPreferenceEntry>& WnnPreferences::entries() const noexcept {
  return entries_;
}

bool WnnPreferences::changed() const noexcept { return changed_; }

void WnnPreferences::mark_saved() noexcept { changed_ = false; }

}  // namespace jwpqt::core
