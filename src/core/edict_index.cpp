// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/edict_index.h"

#include <algorithm>
#include <limits>
#include <optional>
#include <utility>

#include "jwpqt/core/byte_io.h"
#include "jwpqt/core/jis_unicode.h"
#include "jwpqt/core/legacy_code_page.h"
#include "jwpqt/core/utf8.h"

namespace jwpqt::core {
namespace {

constexpr std::uint16_t kBadCharacter = 0xffff;
constexpr std::uint16_t kHiraganaBase = 0x2400;
constexpr std::uint16_t kKatakanaBase = 0x2500;

std::uint16_t normalize_token(std::uint16_t token) {
  if (token >= 'A' && token <= 'Z') {
    return static_cast<std::uint16_t>(token + ('a' - 'A'));
  }
  if ((token & 0xff00U) == kKatakanaBase) {
    return static_cast<std::uint16_t>(kHiraganaBase | (token & 0x00ffU));
  }
  return token;
}

std::size_t utf8_width(unsigned char lead) {
  if ((lead & 0x80U) == 0) {
    return 1;
  }
  if ((lead & 0xe0U) == 0xc0U) {
    return 2;
  }
  if ((lead & 0xf0U) == 0xe0U) {
    return 3;
  }
  if ((lead & 0xf8U) == 0xf0U) {
    return 4;
  }
  throw EdictIndexError("EDICT index offset is not at a UTF-8 character");
}

std::uint16_t unicode_token(std::string_view source, std::size_t offset,
                            LegacyCodePage code_page, std::size_t& width) {
  width = utf8_width(static_cast<unsigned char>(source[offset]));
  if (width > source.size() - offset) {
    throw EdictIndexError("EDICT index UTF-8 character is truncated");
  }
  const std::u32string scalar = decode_utf8(source.substr(offset, width));
  const char32_t code_point = scalar.front();
  if (code_point > U'\0' && code_point <= U'\u007e') {
    return static_cast<std::uint16_t>(code_point);
  }
  if (const std::optional<JisCode> jis =
          unicode_to_jis_x0208(code_point)) {
    return *jis;
  }
  if (const std::optional<std::uint8_t> extended =
          unicode_to_legacy_byte(code_point, code_page)) {
    return *extended;
  }
  return kBadCharacter;
}

std::uint16_t euc_runtime_token(std::string_view source, std::size_t offset,
                                std::size_t& width) {
  const auto first = static_cast<unsigned char>(source[offset]);
  if ((first & 0x80U) == 0) {
    width = 1;
    return first;
  }
  if (source.size() - offset < 2) {
    throw EdictIndexError("EDICT index EUC-JP character is truncated");
  }
  if (first == 0x8fU) {
    if (source.size() - offset < 3) {
      throw EdictIndexError(
          "EDICT index JIS X 0212 character is truncated");
    }
    const std::optional<std::uint8_t> mapped = edict_euc_0212_byte(
        static_cast<std::uint8_t>(source[offset + 1]),
        static_cast<std::uint8_t>(source[offset + 2]));
    if (!mapped.has_value()) {
      throw EdictIndexError(
          "EDICT index JIS X 0212 character is outside the recovered subset");
    }
    width = 3;
    return *mapped;
  }
  width = 2;
  const auto second = static_cast<unsigned char>(source[offset + 1]);
  const auto pair = static_cast<std::uint16_t>(
      (static_cast<std::uint16_t>(first) << 8U) |
      static_cast<std::uint16_t>(second));
  return static_cast<std::uint16_t>(pair & 0x7f7fU);
}

std::uint16_t mixed_runtime_token(std::string_view source, std::size_t offset,
                                  std::size_t& width) {
  const auto first = static_cast<unsigned char>(source[offset]);
  if ((first & 0x80U) == 0) {
    width = 1;
    return first;
  }
  if (source.size() - offset < 2) {
    throw EdictIndexError("Mixed EDICT token is truncated");
  }
  width = 2;
  const auto second = static_cast<unsigned char>(source[offset + 1]);
  const auto pair = static_cast<std::uint16_t>(
      (static_cast<std::uint16_t>(first) << 8U) |
      static_cast<std::uint16_t>(second));
  return static_cast<std::uint16_t>(pair & 0x7f7fU);
}

std::uint16_t source_token(std::string_view source, EdictEncoding encoding,
                           LegacyCodePage utf8_code_page, std::size_t offset,
                           std::size_t& width) {
  if (offset >= source.size()) {
    throw EdictIndexError("EDICT index comparison exceeds the dictionary");
  }
  if (encoding == EdictEncoding::kUtf8) {
    return unicode_token(source, offset, utf8_code_page, width);
  }
  if (encoding == EdictEncoding::kMixed) {
    return mixed_runtime_token(source, offset, width);
  }
  return euc_runtime_token(source, offset, width);
}

std::vector<bool> character_starts(const EdictDictionary& dictionary) {
  const std::string_view source = dictionary.source_bytes();
  std::vector<bool> starts(source.size(), false);
  for (const EdictRecord& record : dictionary.records()) {
    const std::size_t end = record.byte_offset + record.byte_length;
    for (std::size_t offset = record.byte_offset; offset < end;) {
      starts[offset] = true;
      std::size_t width = 1;
      const unsigned char lead = static_cast<unsigned char>(source[offset]);
      if (dictionary.encoding() == EdictEncoding::kUtf8) {
        width = utf8_width(lead);
      } else if (dictionary.encoding() == EdictEncoding::kMixed) {
        width = (lead & 0x80U) != 0 ? 2 : 1;
      } else if (lead == 0x8fU) {
        width = 3;
      } else if ((lead & 0x80U) != 0) {
        width = 2;
      }
      if (width > end - offset) {
        throw EdictIndexError(
            "EDICT record ends inside an encoded character");
      }
      offset += width;
    }
  }
  return starts;
}

std::size_t record_for_offset(const std::vector<EdictRecord>& records,
                              std::size_t offset) {
  const auto next = std::upper_bound(
      records.begin(), records.end(), offset,
      [](std::size_t value, const EdictRecord& record) {
        return value < record.byte_offset;
      });
  if (next == records.begin()) {
    throw EdictIndexError("EDICT index offset precedes the first record");
  }
  const auto record = std::prev(next);
  if (offset - record->byte_offset >= record->byte_length) {
    throw EdictIndexError("EDICT index offset is outside a record");
  }
  return static_cast<std::size_t>(record - records.begin());
}

JwpText normalize_key(const JwpText& key) {
  if (key.empty()) {
    throw EdictIndexError("EDICT index search key is empty");
  }
  JwpText normalized = key;
  for (std::uint16_t& token : normalized) {
    token = normalize_token(token);
  }
  return normalized;
}

}  // namespace

bool EdictIndexEntry::operator==(const EdictIndexEntry& other) const noexcept {
  return byte_offset == other.byte_offset &&
         record_index == other.record_index;
}

bool EdictIndexMatch::operator==(const EdictIndexMatch& other) const noexcept {
  return byte_offset == other.byte_offset && byte_length == other.byte_length &&
         record_index == other.record_index;
}

EdictIndex& EdictIndex::operator=(const EdictIndex& other) {
  if (this != &other) {
    EdictIndex copy(other);
    swap(copy);
  }
  return *this;
}

void EdictIndex::swap(EdictIndex& other) noexcept {
  using std::swap;
  swap(encoding_, other.encoding_);
  swap(utf8_code_page_, other.utf8_code_page_);
  source_bytes_.swap(other.source_bytes_);
  swap(source_extent_, other.source_extent_);
  swap(lookup_steps_, other.lookup_steps_);
  swap(matches_, other.matches_);
  entries_.swap(other.entries_);
  record_ends_.swap(other.record_ends_);
}

EdictIndex EdictIndex::parse(std::string_view bytes,
                             const EdictDictionary& dictionary,
                             const EdictIndexOptions& options) {
  if (bytes.size() > options.encoded_bytes) {
    throw EdictIndexError("EDICT index exceeds its encoded-size limit");
  }
  if (bytes.size() < 4 || bytes.size() % 4 != 0) {
    throw EdictIndexError(
        "EDICT index must contain an aligned header");
  }
  const std::size_t entry_count = bytes.size() / 4 - 1;
  if (entry_count > options.entries) {
    throw EdictIndexError("EDICT index exceeds its entry limit");
  }
  if (dictionary.source_bytes().empty() || dictionary.records().empty()) {
    throw EdictIndexError("EDICT index requires a nonempty dictionary");
  }
  if (dictionary.source_bytes().size() >
      std::numeric_limits<std::uint32_t>::max() - 15U) {
    throw EdictIndexError("EDICT dictionary is too large for a JDX index");
  }

  ByteReader reader(bytes);
  EdictIndex result;
  result.encoding_ = dictionary.encoding();
  result.utf8_code_page_ = options.utf8_code_page;
  result.source_bytes_ = std::string(dictionary.source_bytes());
  result.lookup_steps_ = options.lookup_steps;
  result.matches_ = options.matches;
  result.source_extent_ = reader.read_u32_le();
  const std::uint32_t source_size =
      static_cast<std::uint32_t>(dictionary.source_bytes().size());
  if (result.source_extent_ < source_size ||
      result.source_extent_ > source_size + 15U) {
    throw EdictIndexError("EDICT index header does not match its dictionary");
  }

  const std::vector<bool> starts = character_starts(dictionary);
  result.entries_.reserve(entry_count);
  while (!reader.empty()) {
    const std::uint32_t stored_offset = reader.read_u32_le();
    if (stored_offset == 0 || stored_offset > source_size) {
      throw EdictIndexError("EDICT index contains an out-of-range offset");
    }
    const std::size_t byte_offset =
        static_cast<std::size_t>(stored_offset - 1U);
    if (!starts[byte_offset]) {
      throw EdictIndexError(
          "EDICT index offset is not at a dictionary character");
    }
    const std::size_t record_index =
        record_for_offset(dictionary.records(), byte_offset);
    const EdictRecord& record = dictionary.records()[record_index];
    result.entries_.push_back({byte_offset, record_index});
    result.record_ends_.push_back(record.byte_offset + record.byte_length);
  }
  return result;
}

std::string EdictIndex::serialize() const {
  ByteWriter writer;
  writer.write_u32_le(source_extent_);
  for (const EdictIndexEntry& entry : entries_) {
    if (entry.byte_offset >= std::numeric_limits<std::uint32_t>::max()) {
      throw EdictIndexError("EDICT index offset cannot be serialized");
    }
    writer.write_u32_le(static_cast<std::uint32_t>(entry.byte_offset + 1U));
  }
  return writer.take_bytes();
}

std::uint32_t EdictIndex::source_extent() const noexcept {
  return source_extent_;
}

std::string_view EdictIndex::source_bytes() const noexcept {
  return source_bytes_;
}

LegacyCodePage EdictIndex::utf8_code_page() const noexcept {
  return utf8_code_page_;
}

const std::vector<EdictIndexEntry>& EdictIndex::entries() const noexcept {
  return entries_;
}

int EdictIndex::compare_with_key(std::size_t byte_offset,
                                 std::size_t record_end,
                                 const JwpText& normalized_key,
                                 std::size_t& steps,
                                 std::size_t work_limit,
                                 std::size_t* matched_bytes) const {
  std::size_t source_offset = byte_offset;
  for (const std::uint16_t expected : normalized_key) {
    if (steps >= work_limit) {
      throw EdictIndexError("EDICT index lookup exceeds its work limit");
    }
    ++steps;
    if (source_offset >= record_end) {
      return -1;
    }
    std::size_t width = 0;
    const std::uint16_t actual = normalize_token(
        source_token(source_bytes_, encoding_, utf8_code_page_, source_offset,
                     width));
    if (width > record_end - source_offset) {
      return -1;
    }
    if (actual != expected) {
      return actual < expected ? -1 : 1;
    }
    source_offset += width;
  }
  if (matched_bytes != nullptr) {
    *matched_bytes = source_offset - byte_offset;
  }
  return 0;
}

std::vector<EdictIndexMatch> EdictIndex::find_matches(
    const JwpText& key) const {
  return find_matches_bounded(key, lookup_steps_, matches_).matches;
}

EdictIndexLookup EdictIndex::find_matches_bounded(
    const JwpText& key, std::size_t work_steps,
    std::size_t matches_limit) const {
  const JwpText normalized_key = normalize_key(key);
  EdictIndexLookup lookup;
  const std::size_t work_limit = std::min(work_steps, lookup_steps_);
  const std::size_t result_limit = std::min(matches_limit, matches_);
  for (std::size_t i = 0; i < entries_.size(); ++i) {
    const EdictIndexEntry& entry = entries_[i];
    std::size_t byte_length = 0;
    if (compare_with_key(entry.byte_offset, record_ends_[i], normalized_key,
                          lookup.work_steps, work_limit,
                          &byte_length) != 0) {
      continue;
    }
    if (lookup.matches.size() >= result_limit) {
      throw EdictIndexError("EDICT index lookup exceeds its result limit");
    }
    lookup.matches.push_back(
        {entry.byte_offset, byte_length, entry.record_index});
  }
  return lookup;
}

std::vector<EdictIndexEntry> EdictIndex::find(const JwpText& key) const {
  const std::vector<EdictIndexMatch> matches = find_matches(key);
  std::vector<EdictIndexEntry> entries;
  entries.reserve(matches.size());
  for (const EdictIndexMatch& match : matches) {
    entries.push_back({match.byte_offset, match.record_index});
  }
  return entries;
}

}  // namespace jwpqt::core
