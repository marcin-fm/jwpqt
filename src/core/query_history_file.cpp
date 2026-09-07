// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/query_history_file.h"

#include <vector>

#include "jwpqt/core/byte_io.h"
#include "jwpqt/core/jwp_text_codec.h"
#include "jwpqt/core/utf8.h"

namespace jwpqt::core {
namespace {

void check_size(std::string_view bytes) {
  if (bytes.size() > kMaximumQueryHistoryFileBytes)
    throw QueryHistoryError("History file exceeds the byte limit");
}

void fill_history(QueryHistory& history, const std::vector<std::u32string>& entries) {
  if (entries.size() > history.maximum_entries())
    throw QueryHistoryError("History entry count exceeds its capacity");
  std::size_t remaining = history.maximum_text_cells();
  for (const auto& entry : entries) {
    if (entry.empty() || entry.size() > remaining)
      throw QueryHistoryError("History text exceeds its capacity or is empty");
    remaining -= entry.size();
  }
  // Populate oldest first, but reject rather than silently prune or coalesce.
  for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
    if (history.find(*it) || !history.remember(*it))
      throw QueryHistoryError("History contains duplicate or invalid entries");
  }
}

}  // namespace

QueryHistories parse_query_history_file(std::string_view bytes) {
  try {
    check_size(bytes);
    ByteReader reader(bytes);
    if (reader.read_u32_le() != kQueryHistoryMagic)
      throw QueryHistoryError("Unknown native history format");
    QueryHistories result(reader.read_u32_le());
    QueryHistory* histories[] = {&result.dictionary, &result.search, &result.replace};
    for (auto* history : histories) {
      const auto count = reader.read_u32_le();
      if (count > history->maximum_entries())
        throw QueryHistoryError("History entry count exceeds its capacity");
      std::vector<std::u32string> entries;
      entries.reserve(count);
      for (std::uint32_t i = 0; i < count; ++i) {
        const auto length = reader.read_u32_le();
        entries.push_back(decode_utf8(reader.read_bytes(length)));
      }
      fill_history(*history, entries);
    }
    if (!reader.empty()) throw QueryHistoryError("Unexpected data after native histories");
    return result;
  } catch (const std::runtime_error& error) {
    throw QueryHistoryError(std::string("Invalid native history: ") + error.what());
  }
}

std::string encode_query_history_file(const QueryHistories& histories) {
  try {
    const auto capacity = histories.dictionary.storage_cells();
    if (histories.search.storage_cells() != capacity || histories.replace.storage_cells() != capacity)
      throw QueryHistoryError("History capacities must match");
    ByteWriter writer;
    writer.write_u32_le(kQueryHistoryMagic);
    writer.write_u32_le(static_cast<std::uint32_t>(capacity));
    const QueryHistory* lists[] = {&histories.dictionary, &histories.search, &histories.replace};
    for (const auto* history : lists) {
      writer.write_u32_le(static_cast<std::uint32_t>(history->entries().size()));
      for (const auto& entry : history->entries()) {
        const auto text = encode_utf8(entry);
        writer.write_u32_le(static_cast<std::uint32_t>(text.size()));
        writer.write_bytes(text);
      }
    }
    check_size(writer.bytes());
    return writer.take_bytes();
  } catch (const std::runtime_error& error) {
    throw QueryHistoryError(std::string("Could not encode histories: ") + error.what());
  }
}

QueryHistories parse_legacy_query_history_file(std::string_view bytes,
                                              std::size_t storage_cells,
                                              LegacyCodePage code_page) {
  try {
    check_size(bytes);
    const int page = static_cast<int>(code_page);
    if (page < 1250 || page > 1258) throw QueryHistoryError("Unknown history code page");
    QueryHistories result(storage_cells);
    ByteReader reader(bytes);
    if (reader.read_u32_le() != kLegacyQueryHistoryMagic)
      throw QueryHistoryError("Unknown legacy history format");
    const auto pointer_cells = storage_cells / 10 + 2;
    QueryHistory* histories[] = {&result.dictionary, &result.search, &result.replace};
    for (auto* history : histories) {
      const auto count = reader.read_i32_le();
      const auto raw = reader.read_bytes(storage_cells * 2);
      if (count < 0 || static_cast<std::size_t>(count) > history->maximum_entries())
        throw QueryHistoryError("Invalid legacy history entry count");
      if (count == 0) continue;  // Unused pointer/data cells may contain stale bytes.
      ByteReader offsets(raw);
      if (offsets.read_i16_le() != 0)
        throw QueryHistoryError("Legacy history must start at offset zero");
      std::vector<std::u32string> entries;
      entries.reserve(static_cast<std::size_t>(count));
      std::size_t begin = 0;
      for (std::int32_t i = 0; i < count; ++i) {
        const auto end = offsets.read_i16_le();
        if (end < 0 || static_cast<std::size_t>(end) <= begin ||
            static_cast<std::size_t>(end) > history->maximum_text_cells())
          throw QueryHistoryError("Invalid legacy history text offset");
        const auto length = static_cast<std::size_t>(end) - begin;
        ByteReader text(raw.substr((pointer_cells + begin) * 2, length * 2));
        JwpText tokens;
        tokens.reserve(length);
        for (std::size_t j = 0; j < length; ++j) tokens.push_back(text.read_u16_le());
        entries.push_back(decode_jwp_text(tokens, code_page));
        begin = static_cast<std::size_t>(end);
      }
      fill_history(*history, entries);
    }
    return result;
  } catch (const std::runtime_error& error) {
    throw QueryHistoryError(std::string("Invalid legacy history: ") + error.what());
  }
}

}  // namespace jwpqt::core
