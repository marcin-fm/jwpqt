// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/wnn_dictionary.h"

#include <algorithm>
#include <limits>
#include <sstream>
#include <string>

#include "jwpqt/core/byte_io.h"

namespace jwpqt::core {
namespace {

constexpr std::size_t kIndexRecordSize = 8;
constexpr std::uint8_t kIndexPadding = 0x80;
constexpr std::uint8_t kIndexMarker = 0x77;
constexpr std::size_t kMaximumIndexRecords = 1'000'000;
constexpr std::size_t kMaximumDataSize = 64U * 1024U * 1024U;

[[noreturn]] void fail_at(std::string_view field, std::size_t offset,
                          std::string_view reason) {
  std::ostringstream message;
  message << "Invalid WNN " << field << " at byte " << offset << ": "
          << reason;
  throw WnnDictionaryError(message.str());
}

bool is_high_bit_byte(std::uint8_t value) { return value >= 0x80U; }

std::array<std::uint8_t, kWnnIndexKeySize> prefix_for(
    const std::vector<std::uint8_t>& key) {
  std::array<std::uint8_t, kWnnIndexKeySize> result{};
  result.fill(kIndexPadding);
  const std::size_t count = std::min(key.size(), result.size());
  std::copy_n(key.begin(), count, result.begin());
  return result;
}

std::vector<WnnRecord> parse_records(std::string_view bytes) {
  if (bytes.empty()) {
    throw WnnDictionaryError("WNN data is empty");
  }
  if (bytes.size() > kMaximumDataSize ||
      bytes.size() > std::numeric_limits<std::uint32_t>::max()) {
    throw WnnDictionaryError("WNN data exceeds the supported size");
  }

  std::vector<WnnRecord> records;
  std::size_t candidate_count = 0;
  std::size_t candidate_cells = 0;
  std::size_t line_start = 0;
  while (line_start < bytes.size()) {
    const std::size_t line_end = bytes.find('\n', line_start);
    if (line_end == std::string_view::npos) {
      fail_at("data", line_start, "record is not LF-terminated");
    }
    if (line_end == line_start) {
      fail_at("data", line_start, "record is empty");
    }

    WnnRecord record;
    record.data_offset = static_cast<std::uint32_t>(line_start);
    std::size_t cursor = line_start;
    while (cursor < line_end) {
      const auto value = static_cast<std::uint8_t>(
          static_cast<unsigned char>(bytes[cursor]));
      if (value <= 0x7fU) {
        break;
      }
      if (!is_high_bit_byte(value)) {
        fail_at("data key", cursor, "byte does not have its high bit set");
      }
      record.key.push_back(value);
      ++cursor;
    }
    if (record.key.empty()) {
      fail_at("data key", line_start, "key is empty");
    }
    if (record.key.size() > kWnnMaximumKeySize) {
      fail_at("data key", line_start, "key exceeds the legacy limit");
    }
    if (cursor >= line_end) {
      fail_at("data", cursor, "ending and candidate list are missing");
    }

    const auto ending = static_cast<std::uint8_t>(
        static_cast<unsigned char>(bytes[cursor]));
    if (ending < 0x21U || ending > 0x7eU) {
      fail_at("data ending", cursor, "ending is not printable ASCII");
    }
    record.ending = static_cast<char>(ending);
    ++cursor;
    if (cursor < line_end) {
      if (candidate_count == kWnnMaximumCandidateCount) {
        fail_at("data candidate", cursor,
                "dictionary exceeds the candidate limit");
      }
      record.candidates.emplace_back();
      ++candidate_count;
    }
    while (cursor < line_end) {
      const auto first = static_cast<std::uint8_t>(
          static_cast<unsigned char>(bytes[cursor]));
      if (first == '/') {
        ++cursor;
        if (cursor < line_end) {
          if (candidate_count == kWnnMaximumCandidateCount) {
            fail_at("data candidate", cursor,
                    "dictionary exceeds the candidate limit");
          }
          record.candidates.emplace_back();
          ++candidate_count;
        }
        continue;
      }
      if (!is_high_bit_byte(first)) {
        fail_at("data candidate", cursor,
                "candidate contains an invalid first high-bit byte");
      }
      if (cursor + 1 >= line_end) {
        fail_at("data candidate", cursor, "candidate ends in a partial pair");
      }
      const auto second = static_cast<std::uint8_t>(
          static_cast<unsigned char>(bytes[cursor + 1]));
      if (!is_high_bit_byte(second)) {
        fail_at("data candidate", cursor + 1,
                "candidate contains an invalid second high-bit byte");
      }
      const JisCode code = static_cast<JisCode>(
          (static_cast<std::uint16_t>(first & 0x7fU) << 8U) |
          static_cast<std::uint16_t>(second & 0x7fU));
      if (candidate_cells == kWnnMaximumCandidateCells) {
        fail_at("data candidate", cursor,
                "dictionary exceeds the candidate-cell limit");
      }
      record.candidates.back().push_back(code);
      ++candidate_cells;
      cursor += 2;
    }

    if (!records.empty() && record.key < records.back().key) {
      fail_at("data key", line_start, "records are not sorted");
    }
    if (records.size() == kWnnMaximumRecordCount) {
      fail_at("data", line_start, "dictionary exceeds the record limit");
    }
    records.push_back(std::move(record));
    line_start = line_end + 1;
  }
  return records;
}

std::vector<WnnIndexEntry> parse_index(
    std::string_view bytes, const std::vector<WnnRecord>& records) {
  if (bytes.empty()) {
    throw WnnDictionaryError("WNN index is empty");
  }
  if (bytes.size() % kIndexRecordSize != 0) {
    throw WnnDictionaryError("WNN index size is not a multiple of 8");
  }
  const std::size_t count = bytes.size() / kIndexRecordSize;
  if (count > kMaximumIndexRecords) {
    throw WnnDictionaryError("WNN index exceeds the supported entry count");
  }

  ByteReader reader(bytes);
  std::vector<WnnIndexEntry> entries;
  entries.reserve(count);
  for (std::size_t entry_index = 0; entry_index < count; ++entry_index) {
    const std::size_t offset = reader.position();
    WnnIndexEntry entry;
    bool padding = false;
    for (std::uint8_t& key_byte : entry.key) {
      key_byte = reader.read_u8();
      if (key_byte == kIndexPadding) {
        padding = true;
      } else if (padding || !is_high_bit_byte(key_byte)) {
        fail_at("index key", offset, "key has invalid padding or bytes");
      }
    }
    if (entry.key.front() == kIndexPadding) {
      fail_at("index key", offset, "key is empty");
    }
    if (reader.read_u8() != kIndexMarker) {
      fail_at("index", offset + 3, "record marker is not 0x77");
    }
    entry.data_offset = reader.read_u32_le();

    const auto record = std::lower_bound(
        records.begin(), records.end(), entry.data_offset,
        [](const WnnRecord& candidate, std::uint32_t data_offset) {
          return candidate.data_offset < data_offset;
        });
    if (record == records.end() || record->data_offset != entry.data_offset) {
      fail_at("index offset", offset + 4,
              "offset does not point to a record boundary");
    }
    entry.record_index =
        static_cast<std::size_t>(record - records.begin());
    if (entry.key != prefix_for(record->key)) {
      fail_at("index key", offset,
              "key does not match its referenced data record");
    }
    if (!entries.empty()) {
      if (!(entries.back().key < entry.key)) {
        fail_at("index key", offset, "keys are not strictly increasing");
      }
      if (entries.back().data_offset >= entry.data_offset) {
        fail_at("index offset", offset + 4,
                "offsets are not strictly increasing");
      }
    }
    entries.push_back(entry);
  }
  if (entries.front().data_offset != 0) {
    throw WnnDictionaryError("WNN index does not begin at data offset 0");
  }
  return entries;
}

}  // namespace

WnnDictionary WnnDictionary::parse(std::string_view index_bytes,
                                   std::string_view data_bytes) {
  WnnDictionary dictionary;
  dictionary.records_ = parse_records(data_bytes);
  dictionary.index_ = parse_index(index_bytes, dictionary.records_);
  return dictionary;
}

const std::vector<WnnIndexEntry>& WnnDictionary::index() const noexcept {
  return index_;
}

const std::vector<WnnRecord>& WnnDictionary::records() const noexcept {
  return records_;
}

}  // namespace jwpqt::core
