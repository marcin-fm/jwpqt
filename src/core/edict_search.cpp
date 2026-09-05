// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/edict_search.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string_view>

namespace jwpqt::core {
namespace {

constexpr std::size_t kMaximumQueryLength = 100;

std::uint16_t normalize_query_token(std::uint16_t token) noexcept {
  if (token >= 'A' && token <= 'Z') {
    return static_cast<std::uint16_t>(token - 'A' + 'a');
  }
  if ((token & 0xff00U) == 0x2500U) {
    return static_cast<std::uint16_t>(0x2400U | (token & 0x00ffU));
  }
  return token;
}

bool ascii_alphanumeric(unsigned char value) noexcept {
  return (value >= '0' && value <= '9') ||
         (value >= 'A' && value <= 'Z') ||
         (value >= 'a' && value <= 'z');
}

bool ascii_beginning_matches(std::string_view source,
                             const EdictRecord& record,
                             std::size_t offset,
                             bool full_ascii_boundaries) {
  if (offset == record.byte_offset) {
    return !full_ascii_boundaries;
  }
  const unsigned char previous =
      static_cast<unsigned char>(source[offset - 1]);
  if (previous == '/') {
    return true;
  }
  if (!full_ascii_boundaries && !ascii_alphanumeric(previous)) {
    return true;
  }
  return previous == ' ' && offset >= record.byte_offset + 2 &&
         source[offset - 2] == ')';
}

bool ascii_end_matches(std::string_view source, const EdictRecord& record,
                       std::size_t offset,
                       bool full_ascii_boundaries) {
  const std::size_t record_end = record.byte_offset + record.byte_length;
  if (offset == record_end) {
    return !full_ascii_boundaries;
  }
  const unsigned char next = static_cast<unsigned char>(source[offset]);
  return next == '/' ||
         (!full_ascii_boundaries && !ascii_alphanumeric(next));
}

bool japanese_beginning_matches(std::string_view source,
                                const EdictRecord& record,
                                std::size_t offset) {
  return offset == record.byte_offset || source[offset - 1] == '[';
}

bool japanese_end_matches(std::string_view source, const EdictRecord& record,
                          std::size_t offset) {
  const std::size_t record_end = record.byte_offset + record.byte_length;
  return offset == record_end || source[offset] == ']' ||
         source[offset] == ' ';
}

}  // namespace

EdictQuery prepare_edict_query(const JwpText& input) {
  if (input.empty()) {
    throw EdictSearchError("EDICT query is empty");
  }

  EdictQuery query;
  const std::size_t length = std::min(input.size(), kMaximumQueryLength);
  query.truncated = input.size() > length;
  query.key.reserve(length);

  bool has_ascii = false;
  bool has_japanese = false;
  for (std::size_t i = 0; i < length; ++i) {
    const std::uint16_t token = normalize_query_token(input[i]);
    const bool is_ascii = (token & 0x7f00U) == 0;
    has_ascii = has_ascii || is_ascii;
    has_japanese = has_japanese || !is_ascii;
    query.key.push_back(token);
  }
  if (has_ascii && has_japanese) {
    throw EdictSearchError("EDICT query mixes ASCII and Japanese text");
  }
  if (has_ascii && query.key.size() < 3) {
    throw EdictSearchError(
        "ASCII EDICT query is shorter than three characters");
  }
  query.kind = has_ascii ? EdictQueryKind::kAscii
                         : EdictQueryKind::kJapanese;
  return query;
}

std::vector<EdictIndexMatch> search_edict_direct(
    const EdictDictionary& dictionary, const EdictIndex& index,
    const EdictQuery& query, const EdictDirectSearchOptions& options) {
  const EdictQuery validated_query = prepare_edict_query(query.key);
  if (index.source_bytes() != dictionary.source_bytes()) {
    throw EdictSearchError("EDICT index belongs to a different dictionary");
  }

  const std::vector<EdictIndexMatch> candidates =
      index.find_matches(validated_query.key);
  std::vector<EdictIndexMatch> results;
  results.reserve(std::min(options.results, candidates.size()));
  const std::string_view source = dictionary.source_bytes();
  const std::vector<EdictRecord>& records = dictionary.records();

  for (const EdictIndexMatch& candidate : candidates) {
    if (candidate.record_index >= records.size()) {
      throw EdictSearchError("EDICT index record mapping is invalid");
    }
    const EdictRecord& record = records[candidate.record_index];
    if (candidate.byte_offset >
            std::numeric_limits<std::size_t>::max() - candidate.byte_length ||
        candidate.byte_offset < record.byte_offset ||
        candidate.byte_offset + candidate.byte_length >
            record.byte_offset + record.byte_length) {
      throw EdictSearchError("EDICT index match span is invalid");
    }
    const std::size_t end = candidate.byte_offset + candidate.byte_length;
    const bool beginning_matches =
        validated_query.kind == EdictQueryKind::kAscii
            ? ascii_beginning_matches(source, record, candidate.byte_offset,
                                      options.full_ascii_boundaries)
            : japanese_beginning_matches(source, record,
                                         candidate.byte_offset);
    const bool end_matches =
        validated_query.kind == EdictQueryKind::kAscii
            ? ascii_end_matches(source, record, end,
                                options.full_ascii_boundaries)
            : japanese_end_matches(source, record, end);
    if ((options.require_beginning && !beginning_matches) ||
        (options.require_end && !end_matches)) {
      continue;
    }
    if (results.size() >= options.results) {
      throw EdictSearchError("EDICT search exceeds its result limit");
    }
    results.push_back(candidate);
  }
  return results;
}

}  // namespace jwpqt::core
