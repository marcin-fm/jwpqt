// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/edict_filter.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace jwpqt::core {
namespace {

enum class TagKind {
  kUnknown,
  kOther,
  kPersonalName,
  kPlaceName,
};

constexpr std::array<std::u32string_view, 21> kOtherTags = {
    U"vulg", U"X",    U"col", U"m-sl", U"sl",   U"MA",  U"id",
    U"arch", U"obs",  U"obsc", U"ok",   U"abbr", U"fam", U"pol",
    U"hum",  U"hon",  U"fem", U"male", U"pref", U"suf", U"oK",
};

TagKind classify_tag(std::u32string_view tag) {
  if (tag == U"s" || tag == U"u" || tag == U"g" || tag == U"f" ||
      tag == U"m") {
    return TagKind::kPersonalName;
  }
  if (tag == U"p") {
    return TagKind::kPlaceName;
  }
  for (const std::u32string_view known : kOtherTags) {
    if (tag == known) {
      return TagKind::kOther;
    }
  }
  return TagKind::kUnknown;
}

bool reject_tag(TagKind kind, const EdictNameFilterOptions& options) {
  return (kind == TagKind::kPersonalName && options.reject_personal_names) ||
         (kind == TagKind::kPlaceName && options.reject_place_names);
}

bool logical_tag_start(std::u32string_view definition, std::size_t offset) {
  if (offset == 0) {
    return true;
  }
  const char32_t previous = definition[offset - 1];
  return previous == U'/' || previous == U' ' || previous == U')' ||
         previous == U',';
}

std::vector<std::u32string_view> split_tags(std::u32string_view group) {
  std::vector<std::u32string_view> tags;
  std::size_t begin = 0;
  while (begin <= group.size()) {
    const std::size_t comma = group.find(U',', begin);
    const std::size_t end =
        comma == std::u32string_view::npos ? group.size() : comma;
    tags.push_back(group.substr(begin, end - begin));
    if (comma == std::u32string_view::npos) {
      break;
    }
    begin = comma + 1;
  }
  return tags;
}

struct FilteredGroup {
  bool changed = false;
  bool reject_definition = false;
  std::u32string text;
};

FilteredGroup filter_group(std::u32string_view group,
                           const EdictNameFilterOptions& options) {
  const std::vector<std::u32string_view> tags = split_tags(group);
  std::vector<std::u32string_view> kept;
  kept.reserve(tags.size());
  bool changed = false;
  bool stopped = false;
  for (const std::u32string_view tag : tags) {
    if (stopped) {
      kept.push_back(tag);
      continue;
    }
    const TagKind kind = classify_tag(tag);
    if (kind == TagKind::kUnknown) {
      stopped = true;
      kept.push_back(tag);
      continue;
    }
    if (reject_tag(kind, options)) {
      changed = true;
    } else {
      kept.push_back(tag);
    }
  }

  if (!changed) {
    return {};
  }
  if (kept.empty()) {
    return FilteredGroup{true, true, {}};
  }

  std::u32string text;
  for (std::size_t i = 0; i < kept.size(); ++i) {
    if (i != 0) {
      text.push_back(U',');
    }
    text.append(kept[i]);
  }
  return FilteredGroup{true, false, std::move(text)};
}

std::optional<std::u32string> filter_definition(
    std::u32string_view definition, const EdictNameFilterOptions& options) {
  std::u32string filtered;
  std::size_t copied = 0;
  std::size_t cursor = 0;
  while (cursor < definition.size()) {
    const std::size_t open = definition.find(U'(', cursor);
    if (open == std::u32string_view::npos) {
      break;
    }
    if (!logical_tag_start(definition, open)) {
      cursor = open + 1;
      continue;
    }
    const std::size_t close = definition.find(U')', open + 1);
    if (close == std::u32string_view::npos) {
      break;
    }
    const FilteredGroup group =
        filter_group(definition.substr(open + 1, close - open - 1), options);
    if (group.reject_definition) {
      return std::nullopt;
    }
    if (group.changed) {
      filtered.append(definition.substr(copied, open - copied + 1));
      filtered.append(group.text);
      copied = close;
    }
    cursor = close + 1;
  }
  if (copied == 0) {
    return std::u32string(definition);
  }
  filtered.append(definition.substr(copied));
  return filtered;
}

}  // namespace

std::optional<EdictRecord> filter_edict_name_types(
    const EdictRecord& record, const EdictNameFilterOptions& options) {
  if (!options.reject_personal_names && !options.reject_place_names) {
    return record;
  }

  EdictRecord result = record;
  result.definitions.clear();
  result.definitions.reserve(record.definitions.size());
  for (const std::u32string& definition : record.definitions) {
    std::optional<std::u32string> filtered =
        filter_definition(definition, options);
    if (filtered.has_value()) {
      result.definitions.push_back(std::move(*filtered));
    }
  }
  if (result.definitions.empty()) {
    return std::nullopt;
  }
  return result;
}

}  // namespace jwpqt::core
