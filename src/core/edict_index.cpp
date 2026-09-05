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
    result.entries_.push_back(
        {byte_offset,
         record_for_offset(dictionary.records(), byte_offset)});
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

const std::vector<EdictIndexEntry>& EdictIndex::entries() const noexcept {
  return entries_;
}

int EdictIndex::compare_with_key(std::size_t byte_offset,
                                 const JwpText& normalized_key,
                                 std::size_t& steps) const {
  std::size_t source_offset = byte_offset;
  for (const std::uint16_t expected : normalized_key) {
    if (steps >= lookup_steps_) {
      throw EdictIndexError("EDICT index lookup exceeds its work limit");
    }
    ++steps;
    if (source_offset >= source_bytes_.size()) {
      return -1;
    }
    std::size_t width = 0;
    const std::uint16_t actual = normalize_token(
        source_token(source_bytes_, encoding_, utf8_code_page_, source_offset,
                     width));
    if (actual != expected) {
      return actual < expected ? -1 : 1;
    }
    source_offset += width;
  }
  return 0;
}

std::vector<EdictIndexEntry> EdictIndex::find(const JwpText& key) const {
  const JwpText normalized_key = normalize_key(key);
  std::vector<EdictIndexEntry> matches;
  std::size_t steps = 0;
  for (const EdictIndexEntry& entry : entries_) {
    if (compare_with_key(entry.byte_offset, normalized_key, steps) != 0) {
      continue;
    }
    if (matches.size() >= matches_) {
      throw EdictIndexError("EDICT index lookup exceeds its result limit");
    }
    matches.push_back(entry);
  }
  return matches;
}

}  // namespace jwpqt::core
