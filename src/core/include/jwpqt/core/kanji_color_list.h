// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "jwpqt/core/jwp_document.h"
#include "jwpqt/core/kanji_color.h"

#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace jwpqt::core {

class KanjiColorListError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

class KanjiColorList {
 public:
  static KanjiColorList parse(std::string_view bytes);
  std::string serialize() const;

  bool contains(JisCode code) const noexcept;
  bool add(JisCode code) noexcept;
  bool remove(JisCode code) noexcept;
  void clear() noexcept;

  std::size_t add_text(const JwpText& text) noexcept;
  std::size_t add_document(const JwpDocument& document) noexcept;

  std::size_t size() const noexcept;
  bool empty() const noexcept;
  std::vector<JisCode> codes() const;

 private:
  std::array<bool, kKanjiColorListLastIndex + 1> entries_{};
  std::size_t size_ = 0;
};

}  // namespace jwpqt::core
