// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/edict_user_dictionary.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <sstream>
#include <utility>

#include "jwpqt/core/jwp_text_codec.h"

namespace jwpqt::core {
namespace {

void validate_limits(const EdictUserDictionaryLimits& limits) {
  if (limits.encoded_bytes == 0 || limits.line_bytes == 0 ||
      limits.records == 0 || limits.jwp_cells == 0 ||
      limits.meaning_code_points == 0) {
    throw EdictUserDictionaryError(
        "EDICT user dictionary limits must be positive");
  }
}

void validate_code_page(LegacyCodePage code_page) {
  switch (code_page) {
    case LegacyCodePage::k1250:
    case LegacyCodePage::k1251:
    case LegacyCodePage::k1252:
    case LegacyCodePage::k1253:
    case LegacyCodePage::k1254:
    case LegacyCodePage::k1255:
    case LegacyCodePage::k1256:
    case LegacyCodePage::k1257:
    case LegacyCodePage::k1258:
      return;
  }
  throw EdictUserDictionaryError("EDICT user dictionary code page is invalid");
}

[[noreturn]] void fail(std::size_t record, std::string_view reason) {
  std::ostringstream message;
  message << "Invalid EDICT user record " << record << ": " << reason;
  throw EdictUserDictionaryError(message.str());
}

void checked_add(std::size_t& destination, std::size_t value,
                 std::size_t limit, std::string_view reason) {
  if (value > limit || destination > limit - value) {
    throw EdictUserDictionaryError(std::string(reason));
  }
  destination += value;
}

void validate_jwp_field(const JwpText& text, std::size_t record,
                        std::string_view name, bool required) {
  if (required && text.empty()) {
    fail(record, std::string(name) + " is empty");
  }
  for (const JisCode code : text) {
    const std::uint16_t row = static_cast<std::uint16_t>(code >> 8U);
    const std::uint16_t cell = static_cast<std::uint16_t>(code & 0xffU);
    const bool ascii = row == 0 && cell >= 0x20U && cell <= 0x7eU;
    const bool jis = row >= 0x21U && row <= 0x7eU && cell >= 0x21U &&
                     cell <= 0x7eU;
    if (!ascii && !jis) {
      fail(record, std::string(name) +
                       " contains a non-serializable JWP code");
    }
  }
}

void validate_meaning(std::u32string_view meaning, std::size_t record,
                      bool required) {
  if (required && meaning.empty()) {
    fail(record, "meaning is empty");
  }
  for (const char32_t code_point : meaning) {
    if (code_point == U'\0' || code_point == U'\r' || code_point == U'\n') {
      fail(record, "meaning contains a line or NUL character");
    }
  }
}

void validate_entries(const std::vector<EdictUserEntry>& entries,
                       const EdictUserDictionaryLimits& limits,
                       bool strict_new_entries) {
  validate_limits(limits);
  if (entries.size() > limits.records) {
    throw EdictUserDictionaryError(
        "EDICT user dictionary contains too many records");
  }
  std::size_t cells = 0;
  std::size_t meanings = 0;
  for (std::size_t i = 0; i < entries.size(); ++i) {
    const EdictUserEntry& entry = entries[i];
    validate_jwp_field(entry.headword, i, "headword", false);
    validate_jwp_field(entry.reading, i, "reading", true);
    validate_meaning(entry.meaning, i, strict_new_entries);
    if (strict_new_entries &&
        std::find(entry.reading.begin(), entry.reading.end(), ' ') !=
            entry.reading.end()) {
      fail(i, "reading contains a space");
    }
    checked_add(cells, entry.headword.size(), limits.jwp_cells,
                "EDICT user dictionary exceeds the JWP-cell limit");
    checked_add(cells, entry.reading.size(), limits.jwp_cells,
                "EDICT user dictionary exceeds the JWP-cell limit");
    checked_add(meanings, entry.meaning.size(), limits.meaning_code_points,
                "EDICT user dictionary exceeds the meaning limit");
  }
}

JwpText parse_jwp_field(std::string_view bytes, std::size_t record,
                        std::string_view name, std::size_t& cell_count,
                        const EdictUserDictionaryLimits& limits) {
  if (bytes.empty()) {
    fail(record, std::string(name) + " is empty");
  }
  if ((bytes.size() % 2U) != 0U) {
    fail(record, std::string(name) + " has an incomplete EUC pair");
  }
  const std::size_t pairs = bytes.size() / 2U;
  checked_add(cell_count, pairs, limits.jwp_cells,
              "EDICT user dictionary exceeds the JWP-cell limit");
  JwpText text;
  text.reserve(pairs);
  for (std::size_t i = 0; i < bytes.size(); i += 2U) {
    const std::uint8_t row = static_cast<std::uint8_t>(bytes[i]);
    const std::uint8_t cell = static_cast<std::uint8_t>(bytes[i + 1U]);
    if ((row & 0x80U) == 0 || (cell & 0x80U) == 0) {
      fail(record, std::string(name) + " contains an invalid JWP pair");
    }
    const JisCode code = static_cast<JisCode>(
        (static_cast<std::uint16_t>(row & 0x7fU) << 8U) |
        static_cast<std::uint16_t>(cell & 0x7fU));
    const std::uint16_t decoded_row = static_cast<std::uint16_t>(code >> 8U);
    const std::uint16_t decoded_cell =
        static_cast<std::uint16_t>(code & 0xffU);
    const bool ascii = decoded_row == 0 && decoded_cell >= 0x20U &&
                       decoded_cell <= 0x7eU;
    const bool jis = decoded_row >= 0x21U && decoded_row <= 0x7eU &&
                     decoded_cell >= 0x21U && decoded_cell <= 0x7eU;
    if (!ascii && !jis) {
      fail(record, std::string(name) + " contains an invalid JWP pair");
    }
    text.push_back(code);
  }
  return text;
}

std::u32string decode_meaning(
    std::string_view bytes, LegacyCodePage code_page, std::size_t record,
    std::size_t& meaning_count, const EdictUserDictionaryLimits& limits) {
  checked_add(meaning_count, bytes.size(), limits.meaning_code_points,
              "EDICT user dictionary exceeds the meaning limit");
  std::u32string meaning;
  meaning.reserve(bytes.size());
  for (const char byte_value : bytes) {
    if (byte_value == '\0') {
      fail(record, "meaning contains a NUL byte");
    }
    const auto decoded = legacy_byte_to_unicode(
        static_cast<std::uint8_t>(byte_value), code_page);
    if (!decoded.has_value()) {
      fail(record, "meaning contains an undefined code-page byte");
    }
    meaning.push_back(*decoded);
  }
  return meaning;
}

EdictUserEntry parse_line(std::string_view line, LegacyCodePage code_page,
                          std::size_t record, std::size_t& cell_count,
                          std::size_t& meaning_count,
                          const EdictUserDictionaryLimits& limits) {
  const std::size_t delimiter = line.find(" /");
  if (delimiter == std::string_view::npos || line.back() != '/' ||
      delimiter + 2U > line.size() - 1U) {
    fail(record, "record framing is invalid");
  }
  std::string_view first = line.substr(0, delimiter);
  const std::string_view meaning_bytes =
      line.substr(delimiter + 2U, line.size() - delimiter - 3U);

  EdictUserEntry entry;
  if (!first.empty() && first.back() == ']') {
    const std::size_t bracket = first.rfind(" [");
    if (bracket == std::string_view::npos || bracket + 2U >= first.size()) {
      fail(record, "reading brackets are invalid");
    }
    entry.headword = parse_jwp_field(first.substr(0, bracket), record,
                                     "headword", cell_count, limits);
    entry.reading = parse_jwp_field(
        first.substr(bracket + 2U, first.size() - bracket - 3U), record,
        "reading", cell_count, limits);
  } else {
    entry.reading = parse_jwp_field(first, record, "reading", cell_count,
                                    limits);
  }
  entry.meaning = decode_meaning(meaning_bytes, code_page, record,
                                 meaning_count, limits);
  return entry;
}

void append_bytes(std::string& output, std::string_view bytes,
                  const EdictUserDictionaryLimits& limits) {
  if (bytes.size() > limits.encoded_bytes ||
      output.size() > limits.encoded_bytes - bytes.size()) {
    throw EdictUserDictionaryError(
        "EDICT user dictionary exceeds its encoded-byte limit");
  }
  output.append(bytes);
}

void append_jwp_field(std::string& output, const JwpText& text,
                      const EdictUserDictionaryLimits& limits) {
  for (const JisCode code : text) {
    const char pair[2] = {
        static_cast<char>((code >> 8U) | 0x80U),
        static_cast<char>((code & 0xffU) | 0x80U),
    };
    append_bytes(output, std::string_view(pair, 2), limits);
  }
}

std::string encode_meaning(std::u32string_view meaning,
                           LegacyCodePage code_page, std::size_t record) {
  std::string bytes;
  bytes.reserve(meaning.size());
  for (const char32_t code_point : meaning) {
    const auto encoded = unicode_to_legacy_byte(code_point, code_page);
    if (!encoded.has_value()) {
      fail(record, "meaning is not representable in the selected code page");
    }
    bytes.push_back(static_cast<char>(*encoded));
  }
  return bytes;
}

std::u32string render_entry_unchecked(const EdictUserEntry& entry) {
  std::u32string display;
  const std::u32string headword = decode_jwp_text(entry.headword);
  const std::u32string reading = decode_jwp_text(entry.reading);
  if (!headword.empty()) {
    display.append(headword);
    display.append(U" [");
  }
  display.append(reading);
  if (!headword.empty()) {
    display.push_back(U']');
  }
  display.push_back(U'\t');
  display.append(entry.meaning);
  return display;
}

void consume_sort_work(std::size_t& remaining) {
  if (remaining == 0) {
    throw EdictUserDictionaryError(
        "EDICT user dictionary exceeds the interactive sort work limit");
  }
  --remaining;
}

struct SortableUserEntry {
  EdictUserEntry entry;
  JwpText display;
};

JwpText render_sort_key(const EdictUserEntry& entry,
                        LegacyCodePage code_page) {
  JwpText display;
  display.reserve(entry.headword.size() + entry.reading.size() +
                  entry.meaning.size() + 4U);
  if (!entry.headword.empty()) {
    display.insert(display.end(), entry.headword.begin(), entry.headword.end());
    display.push_back(' ');
    display.push_back('[');
  }
  display.insert(display.end(), entry.reading.begin(), entry.reading.end());
  if (!entry.headword.empty()) {
    display.push_back(']');
  }
  display.push_back('\t');
  const JwpText meaning = encode_jwp_text(entry.meaning, code_page);
  display.insert(display.end(), meaning.begin(), meaning.end());
  return display;
}

std::optional<bool> reading_precedes(const JwpText& first,
                                      const JwpText& second,
                                      std::size_t& remaining) {
  const std::size_t common = std::min(first.size(), second.size());
  std::size_t index = 0;
  for (; index < common; ++index) {
    consume_sort_work(remaining);
    const JisCode first_cell = first[index] & 0xffU;
    const JisCode second_cell = second[index] & 0xffU;
    if (first_cell != second_cell) {
      return second_cell < first_cell;
    }
  }
  if (index != first.size() || index != second.size()) {
    return second.size() < first.size();
  }
  for (index = 0; index < first.size(); ++index) {
    consume_sort_work(remaining);
    if (first[index] != second[index]) {
      return second[index] < first[index];
    }
  }
  return std::nullopt;
}

bool second_entry_precedes_first(const SortableUserEntry& first,
                                  const SortableUserEntry& second,
                                  std::size_t& remaining) {
  const std::optional<bool> reading =
      reading_precedes(first.entry.reading, second.entry.reading, remaining);
  if (reading.has_value()) {
    return *reading;
  }

  const std::size_t common = std::min(first.display.size(), second.display.size());
  for (std::size_t index = 0; index < common; ++index) {
    consume_sort_work(remaining);
    if (first.display[index] != second.display[index]) {
      return second.display[index] < first.display[index];
    }
  }
  consume_sort_work(remaining);
  return second.display.size() < first.display.size();
}

}  // namespace

bool EdictUserEntry::operator==(const EdictUserEntry& other) const noexcept {
  return headword == other.headword && reading == other.reading &&
         meaning == other.meaning;
}

EdictUserEntry make_edict_user_entry(JwpText reading, JwpText headword,
                                     std::u32string meaning) {
  EdictUserEntry entry{std::move(headword), std::move(reading),
                       std::move(meaning)};
  EdictUserDictionaryLimits limits;
  validate_jwp_field(entry.headword, 0, "headword", false);
  validate_jwp_field(entry.reading, 0, "reading", true);
  validate_meaning(entry.meaning, 0, true);
  if (std::find(entry.reading.begin(), entry.reading.end(), ' ') !=
      entry.reading.end()) {
    fail(0, "reading contains a space");
  }
  if (entry.reading.size() > limits.jwp_cells ||
      entry.headword.size() > limits.jwp_cells - entry.reading.size() ||
      entry.meaning.size() > limits.meaning_code_points) {
    throw EdictUserDictionaryError("EDICT user entry exceeds its size limit");
  }
  return entry;
}

std::u32string render_edict_user_entry(const EdictUserEntry& entry) {
  validate_entries({entry}, EdictUserDictionaryLimits{}, false);
  return render_entry_unchecked(entry);
}

std::vector<EdictUserEntry> sort_edict_user_entries(
    std::vector<EdictUserEntry> entries, LegacyCodePage code_page) {
  validate_entries(entries, EdictUserDictionaryLimits{}, false);
  validate_code_page(code_page);
  std::vector<SortableUserEntry> sortable;
  sortable.reserve(entries.size());
  for (EdictUserEntry& entry : entries) {
    JwpText display = render_sort_key(entry, code_page);
    sortable.push_back({std::move(entry), std::move(display)});
  }
  std::size_t remaining = kEdictUserMaximumSortComparisonSteps;
  for (std::size_t first = 0; first < sortable.size(); ++first) {
    std::size_t selected = first;
    for (std::size_t candidate = first + 1; candidate < sortable.size();
         ++candidate) {
      if (second_entry_precedes_first(sortable[selected], sortable[candidate],
                                      remaining)) {
        selected = candidate;
      }
    }
    if (selected != first) {
      SortableUserEntry value = std::move(sortable[selected]);
      sortable.erase(sortable.begin() + static_cast<std::ptrdiff_t>(selected));
      sortable.insert(sortable.begin() + static_cast<std::ptrdiff_t>(first),
                      std::move(value));
    }
  }
  entries.clear();
  entries.reserve(sortable.size());
  for (SortableUserEntry& value : sortable) {
    entries.push_back(std::move(value.entry));
  }
  return entries;
}

EdictUserDictionary EdictUserDictionary::parse(
    std::string_view bytes, LegacyCodePage code_page,
    const EdictUserDictionaryLimits& limits) {
  validate_limits(limits);
  validate_code_page(code_page);
  if (bytes.size() > limits.encoded_bytes) {
    throw EdictUserDictionaryError(
        "EDICT user dictionary exceeds its encoded-byte limit");
  }
  if (!bytes.empty() && bytes.front() == '\n') {
    bytes.remove_prefix(1);
  }

  EdictUserDictionary dictionary;
  std::size_t cells = 0;
  std::size_t meanings = 0;
  std::size_t cursor = 0;
  while (cursor < bytes.size()) {
    if (dictionary.entries_.size() >= limits.records) {
      throw EdictUserDictionaryError(
          "EDICT user dictionary contains too many records");
    }
    const std::size_t separator = bytes.find_first_of("\r\n", cursor);
    const std::size_t end = separator == std::string_view::npos
                                ? bytes.size()
                                : separator;
    const std::size_t line_size = end - cursor;
    if (line_size == 0 || line_size > limits.line_bytes) {
      fail(dictionary.entries_.size(),
           line_size == 0 ? "record is empty" : "record is too long");
    }
    dictionary.entries_.push_back(parse_line(
        bytes.substr(cursor, line_size), code_page, dictionary.entries_.size(),
        cells, meanings, limits));
    if (separator == std::string_view::npos) {
      break;
    }
    cursor = separator + 1U;
    if (cursor < bytes.size() &&
        ((bytes[separator] == '\r' && bytes[cursor] == '\n') ||
         (bytes[separator] == '\n' && bytes[cursor] == '\r'))) {
      ++cursor;
    }
  }
  return dictionary;
}

EdictUserDictionary EdictUserDictionary::from_entries(
    std::vector<EdictUserEntry> entries,
    const EdictUserDictionaryLimits& limits) {
  validate_entries(entries, limits, false);
  EdictUserDictionary dictionary;
  dictionary.entries_ = std::move(entries);
  return dictionary;
}

std::string EdictUserDictionary::serialize(
    LegacyCodePage code_page, const EdictUserDictionaryLimits& limits) const {
  validate_code_page(code_page);
  validate_entries(entries_, limits, false);
  std::string output;
  for (std::size_t i = 0; i < entries_.size(); ++i) {
    const EdictUserEntry& entry = entries_[i];
    const std::string meaning = encode_meaning(entry.meaning, code_page, i);
    std::size_t line_size = entry.headword.empty() ? 4U : 7U;
    const auto add_to_line_size = [&](std::size_t value) {
      if (value > limits.line_bytes ||
          line_size > limits.line_bytes - value) {
        fail(i, "record is too long");
      }
      line_size += value;
    };
    if (entry.headword.size() > std::numeric_limits<std::size_t>::max() / 2U ||
        entry.reading.size() > std::numeric_limits<std::size_t>::max() / 2U) {
      fail(i, "record is too long");
    }
    add_to_line_size(entry.headword.size() * 2U);
    add_to_line_size(entry.reading.size() * 2U);
    add_to_line_size(meaning.size());
    if (!entry.headword.empty()) {
      append_jwp_field(output, entry.headword, limits);
      append_bytes(output, " [", limits);
    }
    append_jwp_field(output, entry.reading, limits);
    if (!entry.headword.empty()) {
      append_bytes(output, "]", limits);
    }
    append_bytes(output, " /", limits);
    append_bytes(output, meaning, limits);
    append_bytes(output, "/\n", limits);
  }
  return output;
}

const std::vector<EdictUserEntry>& EdictUserDictionary::entries() const
    noexcept {
  return entries_;
}

EdictUserDictionaryEditor::EdictUserDictionaryEditor(
    const EdictUserDictionary& dictionary, LegacyCodePage code_page)
    : entries_(dictionary.entries()), code_page_(code_page) {
  validate_code_page(code_page_);
}

const std::vector<EdictUserEntry>& EdictUserDictionaryEditor::entries() const
    noexcept {
  return entries_;
}

void EdictUserDictionaryEditor::publish(
    std::vector<EdictUserEntry> candidate) {
  EdictUserDictionary validated =
      EdictUserDictionary::from_entries(std::move(candidate));
  entries_ = validated.entries();
}

std::size_t EdictUserDictionaryEditor::add(EdictUserEntry entry) {
  std::vector<EdictUserEntry> candidate = entries_;
  candidate.push_back(std::move(entry));
  publish(std::move(candidate));
  return entries_.size() - 1;
}

void EdictUserDictionaryEditor::replace(std::size_t index,
                                        EdictUserEntry entry) {
  if (index >= entries_.size()) {
    throw EdictUserDictionaryError(
        "EDICT user dictionary entry index is out of bounds");
  }
  std::vector<EdictUserEntry> candidate = entries_;
  candidate[index] = std::move(entry);
  publish(std::move(candidate));
}

void EdictUserDictionaryEditor::erase(std::size_t index) {
  if (index >= entries_.size()) {
    throw EdictUserDictionaryError(
        "EDICT user dictionary entry index is out of bounds");
  }
  std::vector<EdictUserEntry> candidate = entries_;
  candidate.erase(candidate.begin() + static_cast<std::ptrdiff_t>(index));
  publish(std::move(candidate));
}

bool EdictUserDictionaryEditor::move_up(std::size_t index) {
  if (index >= entries_.size()) {
    throw EdictUserDictionaryError(
        "EDICT user dictionary entry index is out of bounds");
  }
  if (index == 0) {
    return false;
  }
  std::vector<EdictUserEntry> candidate = entries_;
  std::swap(candidate[index], candidate[index - 1]);
  publish(std::move(candidate));
  return true;
}

bool EdictUserDictionaryEditor::move_down(std::size_t index) {
  if (index >= entries_.size()) {
    throw EdictUserDictionaryError(
        "EDICT user dictionary entry index is out of bounds");
  }
  if (index + 1 == entries_.size()) {
    return false;
  }
  std::vector<EdictUserEntry> candidate = entries_;
  std::swap(candidate[index], candidate[index + 1]);
  publish(std::move(candidate));
  return true;
}

void EdictUserDictionaryEditor::sort() {
  publish(sort_edict_user_entries(entries_, code_page_));
}

EdictUserDictionary EdictUserDictionaryEditor::dictionary() const {
  return EdictUserDictionary::from_entries(entries_);
}

}  // namespace jwpqt::core
