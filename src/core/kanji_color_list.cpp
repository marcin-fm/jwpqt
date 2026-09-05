// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/kanji_color_list.h"

#include <cstdint>

namespace jwpqt::core {
namespace {

JisCode code_for_index(std::size_t index) {
  const std::size_t row = index / 94U;
  const std::size_t cell = index % 94U;
  return static_cast<JisCode>(
      kKanjiColorListBase + static_cast<JisCode>((row << 8U) | cell));
}

}  // namespace

KanjiColorList KanjiColorList::parse(std::string_view bytes) {
  if (bytes.size() % 2U != 0U) {
    throw KanjiColorListError("Kanji color list has a truncated EUC pair");
  }

  KanjiColorList result;
  for (std::size_t offset = 0; offset < bytes.size(); offset += 2U) {
    const auto lead = static_cast<std::uint8_t>(bytes[offset]);
    const auto trail = static_cast<std::uint8_t>(bytes[offset + 1U]);
    if (lead < 0xa1U || lead > 0xfeU || trail < 0xa1U || trail > 0xfeU) {
      throw KanjiColorListError(
          "Kanji color list contains a non-EUC code");
    }
    const auto row = static_cast<std::uint16_t>(lead & 0x7fU);
    const auto cell = static_cast<std::uint16_t>(trail & 0x7fU);
    const JisCode code = static_cast<JisCode>(
        (static_cast<std::uint32_t>(row) << 8U) |
        static_cast<std::uint32_t>(cell));
    if (!kanji_color_list_index(code).has_value()) {
      throw KanjiColorListError(
          "Kanji color list code is outside the supported range");
    }
    result.add(code);
  }
  return result;
}

std::string KanjiColorList::serialize() const {
  std::string result;
  result.reserve(size_ * 2U);
  for (std::size_t index = 0; index < entries_.size(); ++index) {
    if (!entries_[index]) {
      continue;
    }
    const JisCode code = code_for_index(index);
    result.push_back(static_cast<char>(((code >> 8U) & 0x7fU) | 0x80U));
    result.push_back(static_cast<char>((code & 0x7fU) | 0x80U));
  }
  return result;
}

bool KanjiColorList::contains(JisCode code) const noexcept {
  const auto index = kanji_color_list_index(code);
  return index.has_value() && entries_[*index];
}

bool KanjiColorList::add(JisCode code) noexcept {
  const auto index = kanji_color_list_index(code);
  if (!index.has_value() || entries_[*index]) {
    return false;
  }
  entries_[*index] = true;
  ++size_;
  return true;
}

bool KanjiColorList::remove(JisCode code) noexcept {
  const auto index = kanji_color_list_index(code);
  if (!index.has_value() || !entries_[*index]) {
    return false;
  }
  entries_[*index] = false;
  --size_;
  return true;
}

void KanjiColorList::clear() noexcept {
  entries_.fill(false);
  size_ = 0;
}

std::size_t KanjiColorList::add_text(const JwpText& text) noexcept {
  const std::size_t before = size_;
  for (const JisCode code : text) {
    if (code >= 0x3000U) {
      add(code);
    }
  }
  return size_ - before;
}

std::size_t KanjiColorList::add_document(
    const JwpDocument& document) noexcept {
  const std::size_t before = size_;
  for (const JwpParagraph& paragraph : document.paragraphs) {
    add_text(paragraph.text);
  }
  return size_ - before;
}

std::size_t KanjiColorList::size() const noexcept { return size_; }

bool KanjiColorList::empty() const noexcept { return size_ == 0; }

std::vector<JisCode> KanjiColorList::codes() const {
  std::vector<JisCode> result;
  result.reserve(size_);
  for (std::size_t index = 0; index < entries_.size(); ++index) {
    if (entries_[index]) {
      result.push_back(code_for_index(index));
    }
  }
  return result;
}

}  // namespace jwpqt::core
