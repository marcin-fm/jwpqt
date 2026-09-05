// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "jwpqt/core/jis_encoding.h"

namespace jwpqt::core {

inline constexpr std::size_t kRadicalListGroups = 241;
inline constexpr std::size_t kStrokeListGroups = 30;

class KanjiLookupListError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

struct KanjiLookupListLimits {
  std::size_t encoded_bytes = 64U * 1024U;
  std::size_t groups = 512;
  std::size_t memberships = 100'000;
};

class KanjiLookupLists {
 public:
  static KanjiLookupLists parse(
      std::string_view bytes, std::size_t group_count,
      const KanjiLookupListLimits& limits = KanjiLookupListLimits{});

  std::size_t group_count() const noexcept;
  std::size_t membership_count() const noexcept;
  const std::vector<JisCode>& group(std::size_t index) const;

 private:
  std::vector<std::vector<JisCode>> groups_;
  std::size_t membership_count_ = 0;
};

KanjiLookupLists parse_radical_lists(
    std::string_view bytes,
    const KanjiLookupListLimits& limits = KanjiLookupListLimits{});
KanjiLookupLists parse_stroke_lists(
    std::string_view bytes,
    const KanjiLookupListLimits& limits = KanjiLookupListLimits{});

}  // namespace jwpqt::core
