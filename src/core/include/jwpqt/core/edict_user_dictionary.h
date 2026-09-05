// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "jwpqt/core/jwp_document.h"
#include "jwpqt/core/legacy_code_page.h"

namespace jwpqt::core {

struct EdictUserEntry {
  JwpText headword;
  JwpText reading;
  std::u32string meaning;

  bool operator==(const EdictUserEntry& other) const noexcept;
};

struct EdictUserDictionaryLimits {
  std::size_t encoded_bytes = 8U * 1024U * 1024U;
  std::size_t line_bytes = 1024U * 1024U;
  std::size_t records = 100'000;
  std::size_t jwp_cells = 8U * 1024U * 1024U;
  std::size_t meaning_code_points = 8U * 1024U * 1024U;
};

class EdictUserDictionaryError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

inline constexpr std::size_t kEdictUserMaximumSortComparisonSteps = 5'000'000;

EdictUserEntry make_edict_user_entry(JwpText reading, JwpText headword,
                                     std::u32string meaning);
std::u32string render_edict_user_entry(const EdictUserEntry& entry);
std::vector<EdictUserEntry> sort_edict_user_entries(
    std::vector<EdictUserEntry> entries);

class EdictUserDictionary {
 public:
  static EdictUserDictionary parse(
      std::string_view bytes,
      LegacyCodePage code_page = kDefaultLegacyCodePage,
      const EdictUserDictionaryLimits& limits =
          EdictUserDictionaryLimits{});
  static EdictUserDictionary from_entries(
      std::vector<EdictUserEntry> entries,
      const EdictUserDictionaryLimits& limits =
          EdictUserDictionaryLimits{});

  std::string serialize(
      LegacyCodePage code_page = kDefaultLegacyCodePage,
      const EdictUserDictionaryLimits& limits =
          EdictUserDictionaryLimits{}) const;
  const std::vector<EdictUserEntry>& entries() const noexcept;

 private:
  std::vector<EdictUserEntry> entries_;
};

class EdictUserDictionaryEditor {
 public:
  explicit EdictUserDictionaryEditor(const EdictUserDictionary& dictionary);

  const std::vector<EdictUserEntry>& entries() const noexcept;
  std::size_t add(EdictUserEntry entry);
  void replace(std::size_t index, EdictUserEntry entry);
  void erase(std::size_t index);
  bool move_up(std::size_t index);
  bool move_down(std::size_t index);
  void sort();
  EdictUserDictionary dictionary() const;

 private:
  void publish(std::vector<EdictUserEntry> candidate);

  std::vector<EdictUserEntry> entries_;
};

}  // namespace jwpqt::core
