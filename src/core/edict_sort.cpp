// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/edict_sort.h"

#include <algorithm>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "jwpqt/core/jwp_text_codec.h"

namespace jwpqt::core {

std::vector<std::size_t> sort_edict_records(
    const std::vector<std::reference_wrapper<const EdictRecord>>& records,
    const EdictSortOptions& options, const EdictSortLimits& limits) {
  switch (options.mode) {
    case EdictSortMode::kReading:
    case EdictSortMode::kLength:
    case EdictSortMode::kEntry:
    case EdictSortMode::kDefinition: break;
    default: throw EdictSortError("Unknown dictionary sort mode");
  }
  const int page = static_cast<int>(options.code_page);
  if (page < 1250 || page > 1258) throw EdictSortError("Unknown dictionary sort code page");
  if (records.size() > limits.records) throw EdictSortError("Dictionary sort record limit exceeded");
  std::size_t cells = limits.text_cells;
  std::size_t comparisons = limits.comparisons;
  std::size_t work = limits.work;
  const auto charge = [](std::size_t amount, std::size_t& remaining) {
    if (amount > remaining) throw EdictSortError("Dictionary sort work limit exceeded");
    remaining -= amount;
  };
  struct Entry {
    std::u32string codes;
    std::size_t reading_begin = 0, reading_end = 0;
    std::size_t headword_end = 0, definition_begin = 0;
  };
  std::vector<Entry> entries;
  entries.reserve(records.size());
  for (const auto& reference : records) {
    const auto& record = reference.get();
    Entry entry;
    const auto append = [&](char32_t character) {
      if (character == 0 || character > 0x10ffffU ||
          (character >= 0xd800U && character <= 0xdfffU)) {
        throw EdictSortError("Dictionary sort entry contains invalid Unicode");
      }
      if (cells == 0) throw EdictSortError("Dictionary sort text limit exceeded");
      --cells;
      charge(1, work);
      const auto code = unicode_to_jwp_code(character, options.code_page);
      // Unmapped Unicode stays distinct and follows the legacy repertoire.
      entry.codes.push_back(code ? static_cast<char32_t>(*code) : 0x110000U + character);
    };
    const auto prefix = [&](std::u32string_view text) {
      for (const auto character : text) {
        if (character == U' ') append(U'\t');
        else if (character == U'[') append(U'\u3010');
        else if (character == U']') append(U'\u3011');
        else append(character);
      }
    };
    prefix(record.headword);
    for (const auto& reading : record.readings) {
      append(U'\t');
      append(U'\u3010');
      prefix(reading);
      append(U'\u3011');
    }
    append(U'\t');
    for (std::size_t i = 0; i < record.definitions.size(); ++i) {
      if (i != 0) { append(U','); append(U' '); }
      for (const auto character : record.definitions[i]) append(character);
    }
    const auto is_left = [](char32_t value) { return value == U'[' || value == 0x215aU; };
    const auto is_right = [](char32_t value) { return value == U']' || value == 0x215bU; };
    std::size_t position = 0;
    while (position < entry.codes.size() && !is_left(entry.codes[position])) {
      charge(1, work); ++position;
    }
    if (position < entry.codes.size()) {
      entry.reading_begin = ++position;
      while (position < entry.codes.size() && !is_right(entry.codes[position])) {
        charge(1, work); ++position;
      }
      entry.reading_end = position;
    } else {
      position = 0;
      while (position < entry.codes.size() && entry.codes[position] != U'\t') {
        charge(1, work); ++position;
      }
      entry.reading_end = position;
    }
    position = entry.reading_end;
    while (position < entry.codes.size() && entry.codes[position] != U'\t') {
      charge(1, work); ++position;
    }
    entry.definition_begin = position < entry.codes.size() ? position + 1 : position;
    position = 0;
    while (position < entry.codes.size() && entry.codes[position] != U'\t' &&
           entry.codes[position] != U'/' && !is_left(entry.codes[position])) {
      charge(1, work); ++position;
    }
    entry.headword_end = position;
    entries.push_back(std::move(entry));
  }

  const auto compare = [&](std::u32string_view left, std::u32string_view right,
                           bool low_byte, bool priority) {
    std::size_t i = 0;
    for (; i < left.size() && i < right.size(); ++i) {
      charge(1, work);
      const auto a = low_byte && left[i] <= 0xffffU ? left[i] & 0xffU : left[i];
      const auto b = low_byte && right[i] <= 0xffffU ? right[i] & 0xffU : right[i];
      if (a != b) return a < b ? -1 : 1;
    }
    if (priority && left.size() != right.size()) {
      constexpr std::u32string_view suffix = U", (P)";
      const auto remainder = left.size() > right.size() ? left.substr(i) : right.substr(i);
      if (remainder.size() == suffix.size()) {
        charge(suffix.size(), work);
        if (remainder == suffix) return left.size() > right.size() ? -1 : 1;
      }
    }
    return left.size() == right.size() ? 0 : left.size() < right.size() ? -1 : 1;
  };
  const auto compare_lists = [&](const auto& left, const auto& right) {
    for (std::size_t i = 0; i < left.size() && i < right.size(); ++i) {
      charge(1, work);
      const int result = compare(left[i], right[i], false, false);
      if (result != 0) return result;
    }
    return left.size() == right.size() ? 0 : left.size() < right.size() ? -1 : 1;
  };
  const auto identity_less = [&](std::size_t a, std::size_t b) {
    charge(1, comparisons);
    const auto& left = records[a].get();
    const auto& right = records[b].get();
    int result = compare(left.headword, right.headword, false, false);
    if (result == 0) result = compare_lists(left.readings, right.readings);
    if (result == 0) result = compare_lists(left.definitions, right.definitions);
    return result < 0;
  };
  std::set<std::size_t, decltype(identity_less)> unique(identity_less);
  std::vector<std::size_t> order;
  order.reserve(records.size());
  for (std::size_t i = 0; i < records.size(); ++i) {
    if (unique.insert(i).second) order.push_back(i);
  }
  const auto compare_entries = [&](std::size_t a, std::size_t b) {
    charge(1, comparisons);
    const auto& left = entries[a];
    const auto& right = entries[b];
    const std::u32string_view full_left(left.codes), full_right(right.codes);
    const auto reading_left = full_left.substr(left.reading_begin, left.reading_end - left.reading_begin);
    const auto reading_right = full_right.substr(right.reading_begin, right.reading_end - right.reading_begin);
    const auto entry_order = [&] { return compare(full_left, full_right, false, true); };
    const auto length_order = [&](std::size_t size_left, std::size_t size_right) {
      const int result = size_left == size_right ? 0 : size_left > size_right ? -1 : 1;
      return options.reverse ? -result : result;
    };
    int result = 0;
    switch (options.mode) {
      case EdictSortMode::kReading:
        result = compare(reading_left, reading_right, true, false);
        if (result == 0) result = compare(reading_left, reading_right, false, false);
        if (result == 0) result = compare(full_left, full_right, false, false);
        break;
      case EdictSortMode::kEntry: result = entry_order(); break;
      case EdictSortMode::kDefinition:
        result = compare(full_left.substr(left.definition_begin),
                         full_right.substr(right.definition_begin), false, true);
        if (result == 0) result = entry_order();
        break;
      case EdictSortMode::kLength:
        if (options.headword_length) {
          result = length_order(left.headword_end, right.headword_end);
          if (result != 0) return result;
          if (compare(full_left.substr(0, left.headword_end),
                      full_right.substr(0, right.headword_end), false, false) != 0) return entry_order();
        }
        if (reading_left.size() == reading_right.size() &&
            compare(reading_left, reading_right, false, false) == 0) {
          result = length_order(left.headword_end, right.headword_end);
        } else {
          result = length_order(reading_left.size(), reading_right.size());
        }
        return result != 0 ? result : entry_order();
    }
    return options.reverse ? -result : result;
  };
  // The priority and conditional-length rules can cycle. std::sort requires a
  // strict weak order; retain the source's deterministic selection-and-move loop.
  for (std::size_t i = 0; i < order.size(); ++i) {
    std::size_t best = i;
    for (std::size_t j = i + 1; j < order.size(); ++j) {
      if (compare_entries(order[best], order[j]) > 0) best = j;
    }
    if (best != i) {
      charge(best - i + 1, work);
      std::rotate(order.begin() + static_cast<std::ptrdiff_t>(i),
                  order.begin() + static_cast<std::ptrdiff_t>(best),
                  order.begin() + static_cast<std::ptrdiff_t>(best + 1));
    }
  }
  return order;
}

}  // namespace jwpqt::core
