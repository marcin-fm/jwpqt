// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "jwpqt/core/wnn_dictionary.h"

namespace jwpqt::core {

class WnnUserDictionaryError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

struct WnnUserEntry {
  JwpText reading;
  char ending = '*';
  std::vector<JwpText> candidates;

  bool operator==(const WnnUserEntry& other) const noexcept;
};

enum class WnnUserInflection {
  kUninflected,
  kGodan,
  kIchidan,
  kIAdjective,
};

inline constexpr std::size_t kWnnMaximumSortComparisonSteps = 5'000'000;

WnnUserEntry make_wnn_user_entry(
    JwpText reading, std::vector<JwpText> candidates,
    WnnUserInflection inflection = WnnUserInflection::kUninflected);

std::vector<WnnUserEntry> sort_wnn_user_entries(
    std::vector<WnnUserEntry> entries);

class WnnUserDictionary {
 public:
  static WnnUserDictionary parse(std::string_view bytes);
  static WnnUserDictionary from_entries(std::vector<WnnUserEntry> entries);

  std::string serialize() const;
  const std::vector<WnnUserEntry>& entries() const noexcept;
  std::vector<WnnRecord> lookup_records() const;

 private:
  std::vector<WnnUserEntry> entries_;
};

class WnnUserDictionaryEditor {
 public:
  explicit WnnUserDictionaryEditor(const WnnUserDictionary& dictionary);

  const std::vector<WnnUserEntry>& entries() const noexcept;
  std::size_t add(WnnUserEntry entry);
  void replace(std::size_t index, WnnUserEntry entry);
  void erase(std::size_t index);
  bool move_up(std::size_t index);
  bool move_down(std::size_t index);
  void sort();
  WnnUserDictionary dictionary() const;

 private:
  void publish(std::vector<WnnUserEntry> candidate);

  std::vector<WnnUserEntry> entries_;
};

}  // namespace jwpqt::core
