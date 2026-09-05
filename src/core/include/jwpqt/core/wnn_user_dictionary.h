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

WnnUserEntry make_wnn_user_entry(
    JwpText reading, std::vector<JwpText> candidates,
    WnnUserInflection inflection = WnnUserInflection::kUninflected);

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

}  // namespace jwpqt::core
