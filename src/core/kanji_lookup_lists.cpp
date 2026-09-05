// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/kanji_lookup_lists.h"

#include <limits>

namespace jwpqt::core {
namespace {

std::uint16_t read_u16(std::string_view bytes, std::size_t offset) {
  if (offset > bytes.size() || bytes.size() - offset < 2) {
    throw KanjiLookupListError("Kanji lookup list is truncated");
  }
  return static_cast<std::uint16_t>(
      static_cast<unsigned char>(bytes[offset]) |
      (static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[offset + 1]))
       << 8U));
}

void validate_limits_impl(const KanjiLookupListLimits& limits) {
  if (limits.encoded_bytes == 0 || limits.groups == 0 ||
      limits.memberships == 0) {
    throw KanjiLookupListError("Kanji lookup list limits must be positive");
  }
}

}  // namespace

KanjiLookupLists KanjiLookupLists::parse(
    std::string_view bytes, std::size_t group_count,
    const KanjiLookupListLimits& limits) {
  validate_kanji_lookup_list_limits(limits);
  if (bytes.size() > limits.encoded_bytes) {
    throw KanjiLookupListError("Kanji lookup list exceeds its byte limit");
  }
  if (group_count == 0 || group_count > limits.groups ||
      group_count > std::numeric_limits<std::size_t>::max() / 4U) {
    throw KanjiLookupListError("Kanji lookup group count is invalid");
  }
  const std::size_t index_bytes = group_count * 4U;
  if (index_bytes > bytes.size()) {
    throw KanjiLookupListError("Kanji lookup index is truncated");
  }

  KanjiLookupLists result;
  result.groups_.reserve(group_count);
  std::size_t expected_offset = index_bytes;
  for (std::size_t group_index = 0; group_index < group_count; ++group_index) {
    const std::size_t index_offset = group_index * 4U;
    const std::size_t offset = read_u16(bytes, index_offset);
    const std::size_t count = read_u16(bytes, index_offset + 2U);
    if (offset != expected_offset || (offset & 1U) != 0) {
      throw KanjiLookupListError(
          "Kanji lookup groups are not contiguous and ordered");
    }
    if (count > limits.memberships - result.membership_count_ ||
        count > (bytes.size() - offset) / 2U) {
      throw KanjiLookupListError("Kanji lookup membership budget is exceeded");
    }
    std::vector<JisCode> group;
    group.reserve(count);
    for (std::size_t member = 0; member < count; ++member) {
      const JisCode code = read_u16(bytes, offset + member * 2U);
      if (!is_jis_x0208_pair(code) || code < 0x3000U) {
        throw KanjiLookupListError(
            "Kanji lookup list contains an invalid character");
      }
      group.push_back(code);
    }
    result.membership_count_ += count;
    expected_offset += count * 2U;
    result.groups_.push_back(std::move(group));
  }
  if (expected_offset != bytes.size()) {
    throw KanjiLookupListError("Kanji lookup list contains trailing bytes");
  }
  return result;
}

void validate_kanji_lookup_list_limits(
    const KanjiLookupListLimits& limits) {
  validate_limits_impl(limits);
}

std::size_t KanjiLookupLists::group_count() const noexcept {
  return groups_.size();
}

std::size_t KanjiLookupLists::membership_count() const noexcept {
  return membership_count_;
}

const std::vector<JisCode>& KanjiLookupLists::group(std::size_t index) const {
  if (index >= groups_.size()) {
    throw KanjiLookupListError("Kanji lookup group is out of range");
  }
  return groups_[index];
}

KanjiLookupLists parse_radical_lists(std::string_view bytes,
                                     const KanjiLookupListLimits& limits) {
  return KanjiLookupLists::parse(bytes, kRadicalListGroups, limits);
}

KanjiLookupLists parse_stroke_lists(std::string_view bytes,
                                    const KanjiLookupListLimits& limits) {
  return KanjiLookupLists::parse(bytes, kStrokeListGroups, limits);
}

}  // namespace jwpqt::core
