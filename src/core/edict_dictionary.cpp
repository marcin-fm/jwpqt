// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/edict_dictionary.h"

#include <limits>
#include <optional>
#include <sstream>
#include <utility>

#include "jwpqt/core/legacy_code_page.h"
#include "jwpqt/core/legacy_text.h"
#include "jwpqt/core/utf8.h"

namespace jwpqt::core {
namespace {

[[noreturn]] void fail(std::size_t byte_offset, std::string_view reason) {
  std::ostringstream message;
  message << "Invalid EDICT record at byte " << byte_offset << ": " << reason;
  throw EdictDictionaryError(message.str());
}

bool is_line_break(char value) { return value == '\r' || value == '\n'; }

std::optional<std::uint8_t> jwp_euc_0212_byte(std::uint8_t first,
                                               std::uint8_t second) {
  switch ((static_cast<std::uint16_t>(first) << 8U) | second) {
    case 0xa2ed:
      return 0xa9;
    case 0xa2ee:
      return 0xae;
    case 0xa2ef:
      return 0x99;
    case 0xa9a1:
      return 0xc6;
    case 0xa9a2:
      return 0xd0;
    case 0xa9ac:
      return 0xd8;
    case 0xa9ad:
      return 0x8c;
    case 0xa9b0:
      return 0xde;
    case 0xa9c1:
      return 0xe6;
    case 0xa9c2:
      return 0xf0;
    case 0xa9cc:
      return 0xf8;
    case 0xa9cd:
      return 0x9c;
    case 0xa9ce:
      return 0xdf;
    case 0xa9d0:
      return 0xfe;
    case 0xaaa1:
      return 0xc1;
    case 0xaaa2:
      return 0xc0;
    case 0xaaa3:
      return 0xc4;
    case 0xaaa4:
      return 0xc2;
    case 0xaaa9:
      return 0xc5;
    case 0xaaaa:
      return 0xc3;
    case 0xaaae:
      return 0xc7;
    case 0xaab1:
      return 0xc9;
    case 0xaab2:
      return 0xc8;
    case 0xaab3:
      return 0xcb;
    case 0xaab4:
      return 0xca;
    case 0xaabf:
      return 0xcd;
    case 0xaac0:
      return 0xcc;
    case 0xaac1:
      return 0xcf;
    case 0xaac2:
      return 0xce;
    case 0xaad0:
      return 0xd1;
    case 0xaad1:
      return 0xd3;
    case 0xaad2:
      return 0xd2;
    case 0xaad3:
      return 0xd6;
    case 0xaad4:
    case 0xaad7:
      return 0xd4;
    case 0xaad8:
      return 0xd5;
    case 0xaade:
      return 0x8a;
    case 0xaae2:
      return 0xda;
    case 0xaae3:
      return 0xd9;
    case 0xaae4:
      return 0xdc;
    case 0xaae5:
      return 0xdb;
    case 0xaaf2:
      return 0xdd;
    case 0xaaf3:
      return 0x9f;
    case 0xaaf6:
      return 0x8e;
    case 0xaba1:
      return 0xe1;
    case 0xaba2:
      return 0xe0;
    case 0xaba3:
      return 0xe4;
    case 0xaba4:
    case 0xaba7:
      return 0xe2;
    case 0xaba9:
      return 0xe5;
    case 0xabaa:
      return 0xe3;
    case 0xabae:
      return 0xe7;
    case 0xabb1:
      return 0xe9;
    case 0xabb2:
      return 0xe8;
    case 0xabb3:
      return 0xeb;
    case 0xabb4:
    case 0xabb7:
      return 0xea;
    case 0xabbf:
      return 0xed;
    case 0xabc0:
      return 0xec;
    case 0xabc1:
      return 0xef;
    case 0xabc2:
      return 0xee;
    case 0xabd0:
      return 0xf1;
    case 0xabd1:
      return 0xf3;
    case 0xabd2:
      return 0xf2;
    case 0xabd3:
      return 0xf6;
    case 0xabd4:
    case 0xabd7:
      return 0xf4;
    case 0xabd8:
      return 0xf5;
    case 0xabde:
      return 0x9a;
    case 0xabe2:
      return 0xfa;
    case 0xabe3:
      return 0xf9;
    case 0xabe4:
      return 0xfc;
    case 0xabe5:
    case 0xabe9:
      return 0xfb;
    case 0xabf2:
      return 0xfd;
    case 0xabf3:
      return 0xff;
    case 0xabf6:
      return 0x9e;
    default:
      return std::nullopt;
  }
}

std::u32string decode_edict_euc(std::string_view bytes,
                                std::size_t byte_offset) {
  // Legacy dictionary readers mask both JIS bytes, including low-bit trails
  // found in ENAMDICT. Normalize only the decoding copy, never JDX source bytes.
  std::string normalized(bytes);
  for (std::size_t index = 0; index < normalized.size();) {
    const auto first = static_cast<std::uint8_t>(normalized[index]);
    const std::size_t width = first == 0x8fU ? 3 : (first & 0x80U) ? 2 : 1;
    if (width > normalized.size() - index) {
      break;
    }
    if (first >= 0xa1U && first <= 0xfeU) {
      const auto second = static_cast<std::uint8_t>(normalized[index + 1]);
      if (second >= 0x21U && second <= 0x7eU) {
        normalized[index + 1] = static_cast<char>(second | 0x80U);
      }
    }
    index += width;
  }
  bytes = normalized;
  std::u32string result;
  std::size_t segment_start = 0;
  std::size_t cursor = 0;
  while (cursor < bytes.size()) {
    if (static_cast<std::uint8_t>(bytes[cursor]) != 0x8fU) {
      ++cursor;
      continue;
    }
    result += decode_legacy_text(bytes.substr(segment_start,
                                               cursor - segment_start),
                                 LegacyEncoding::kEucJp);
    if (cursor + 2 >= bytes.size()) {
      fail(byte_offset + cursor, "JIS X 0212 sequence is truncated");
    }
    const auto first = static_cast<std::uint8_t>(bytes[cursor + 1]);
    const auto second = static_cast<std::uint8_t>(bytes[cursor + 2]);
    const std::optional<std::uint8_t> mapped =
        jwp_euc_0212_byte(first, second);
    if (!mapped.has_value()) {
      fail(byte_offset + cursor,
           "JIS X 0212 sequence is outside the recovered JWP subset");
    }
    const std::optional<char32_t> code_point =
        legacy_byte_to_unicode(*mapped, LegacyCodePage::k1252);
    if (!code_point.has_value()) {
      fail(byte_offset + cursor,
           "JIS X 0212 sequence has no recovered Unicode mapping");
    }
    result.push_back(*code_point);
    cursor += 3;
    segment_start = cursor;
  }
  result += decode_legacy_text(bytes.substr(segment_start),
                               LegacyEncoding::kEucJp);
  return result;
}

std::u32string decode_mixed_headword(std::string_view bytes,
                                     std::size_t byte_offset) {
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    const auto byte = static_cast<std::uint8_t>(bytes[index]);
    if (byte == 0x8fU) {
      fail(byte_offset + index,
           "mixed dictionary headword contains an unmapped high-bit pair");
    }
    if ((byte & 0x80U) != 0) {
      ++index;
    }
  }
  return decode_legacy_text(bytes, LegacyEncoding::kEucJp);
}

void consume_budget(std::size_t amount, std::size_t& used, std::size_t limit,
                    std::size_t byte_offset, std::string_view name) {
  if (amount > limit - used) {
    std::ostringstream reason;
    reason << "dictionary exceeds the " << name << " limit";
    fail(byte_offset, reason.str());
  }
  used += amount;
}

std::u32string decode_line(std::string_view bytes, EdictEncoding encoding,
                           std::size_t byte_offset) {
  try {
    if (encoding == EdictEncoding::kUtf8) {
      return decode_utf8(bytes);
    }
    if (encoding == EdictEncoding::kEucJp) {
      return decode_edict_euc(bytes, byte_offset);
    }
  } catch (const EdictDictionaryError&) {
    throw;
  } catch (const std::exception& error) {
    fail(byte_offset, error.what());
  }
  fail(byte_offset, "dictionary encoding is unsupported");
}

std::u32string decode_mixed_definitions(std::string_view bytes,
                                        std::size_t byte_offset,
                                        LegacyCodePage code_page) {
  std::u32string result;
  result.reserve(bytes.size());
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    const auto byte = static_cast<std::uint8_t>(bytes[index]);
    const std::optional<char32_t> code_point =
        legacy_byte_to_unicode(byte, code_page);
    if (!code_point.has_value() || *code_point == U'\0') {
      fail(byte_offset + index,
           "mixed dictionary definition byte is unmapped");
    }
    result.push_back(*code_point);
  }
  return result;
}

std::u32string_view trim_trailing_spaces(std::u32string_view text) {
  while (!text.empty() && text.back() == U' ') {
    text.remove_suffix(1);
  }
  return text;
}

void parse_headword_and_readings(std::u32string_view prefix,
                                 EdictRecord& record) {
  prefix = trim_trailing_spaces(prefix);
  if (prefix.empty()) {
    fail(record.byte_offset, "headword is empty");
  }

  const std::size_t readings_start = prefix.find(U" [");
  if (readings_start == std::u32string_view::npos) {
    record.headword.assign(prefix);
    return;
  }
  if (readings_start == 0) {
    fail(record.byte_offset, "headword is empty");
  }
  record.headword.assign(prefix.substr(0, readings_start));

  std::size_t cursor = readings_start;
  while (cursor < prefix.size()) {
    if (cursor + 2 > prefix.size() || prefix[cursor] != U' ' ||
        prefix[cursor + 1] != U'[') {
      fail(record.byte_offset, "reading list is malformed");
    }
    const std::size_t end = prefix.find(U']', cursor + 2);
    if (end == std::u32string_view::npos || end == cursor + 2) {
      fail(record.byte_offset, "reading is empty or unterminated");
    }
    record.readings.emplace_back(prefix.substr(cursor + 2, end - cursor - 2));
    cursor = end + 1;
  }
}

void parse_definitions(std::u32string_view text, EdictRecord& record,
                       std::size_t& definition_count,
                       const EdictParseLimits& limits,
                       EdictEncoding encoding) {
  if (text.empty() || text.front() != U'/' ||
      (text.back() != U'/' && encoding != EdictEncoding::kEucJp)) {
    fail(record.byte_offset, "definition list is not slash-terminated");
  }
  std::size_t start = 1;
  while (start < text.size()) {
    std::size_t end = text.find(U'/', start);
    if (end == std::u32string_view::npos) {
      // The shipped CLASSICAL dictionary also terminates meanings at CR/LF.
      end = text.size();
    }
    if (end == start) {
      fail(record.byte_offset, "definition is empty");
    }
    consume_budget(1, definition_count, limits.definitions,
                   record.byte_offset, "definition count");
    record.definitions.emplace_back(text.substr(start, end - start));
    start = end + 1;
  }
  if (record.definitions.empty()) {
    fail(record.byte_offset, "definition list is empty");
  }
}

EdictRecord parse_record(std::string_view bytes, EdictEncoding encoding,
                          std::size_t byte_offset,
                          std::size_t& decoded_code_points,
                          std::size_t& definition_count,
                          const EdictParseLimits& limits, bool strip_utf8_bom,
                          LegacyCodePage mixed_code_page) {
  EdictRecord record;
  record.byte_offset = byte_offset;
  record.byte_length = bytes.size();
  const std::size_t nul = bytes.find('\0');
  if (nul != std::string_view::npos) {
    fail(byte_offset + nul, "dictionary record contains an embedded NUL");
  }

  if (strip_utf8_bom && bytes.size() >= 3 &&
      static_cast<unsigned char>(bytes[0]) == 0xefU &&
      static_cast<unsigned char>(bytes[1]) == 0xbbU &&
      static_cast<unsigned char>(bytes[2]) == 0xbfU) {
    bytes.remove_prefix(3);
  }
  std::u32string line;
  bool decoded_budget_consumed = false;
  if (encoding == EdictEncoding::kMixed) {
    const std::size_t definitions_start = bytes.find('/');
    if (definitions_start == std::string_view::npos) {
      fail(byte_offset, "mixed dictionary definition list is missing");
    }
    try {
      std::u32string headword =
          decode_mixed_headword(bytes.substr(0, definitions_start), byte_offset);
      std::u32string definitions = decode_mixed_definitions(
          bytes.substr(definitions_start + 1),
          byte_offset + definitions_start + 1, mixed_code_page);
      consume_budget(headword.size(), decoded_code_points,
                     limits.decoded_code_points, byte_offset,
                     "decoded code-point");
      consume_budget(1, decoded_code_points, limits.decoded_code_points,
                     byte_offset + definitions_start, "decoded code-point");
      consume_budget(definitions.size(), decoded_code_points,
                     limits.decoded_code_points,
                     byte_offset + definitions_start + 1,
                     "decoded code-point");
      decoded_budget_consumed = true;
      line = std::move(headword);
      if (line.empty() || line.back() != U' ') {
        line.push_back(U' ');
      }
      line.push_back(U'/');
      line += definitions;
    } catch (const EdictDictionaryError&) {
      throw;
    } catch (const std::exception& error) {
      fail(byte_offset, error.what());
    }
  } else {
    line = decode_line(bytes, encoding, byte_offset);
  }
  if (!decoded_budget_consumed) {
    consume_budget(line.size(), decoded_code_points,
                   limits.decoded_code_points, byte_offset,
                   "decoded code-point");
  }

  const std::size_t definitions_start = line.find(U" /");
  if (definitions_start == std::u32string::npos) {
    fail(byte_offset, "headword and definitions are not separated by space-slash");
  }
  parse_headword_and_readings(
      std::u32string_view(line).substr(0, definitions_start), record);
  parse_definitions(std::u32string_view(line).substr(definitions_start + 1),
                    record, definition_count, limits, encoding);
  return record;
}

}  // namespace

std::optional<std::uint8_t> edict_euc_0212_byte(
    std::uint8_t first, std::uint8_t second) noexcept {
  return jwp_euc_0212_byte(first, second);
}

bool EdictRecord::operator==(const EdictRecord& other) const noexcept {
  return byte_offset == other.byte_offset && byte_length == other.byte_length &&
         headword == other.headword && readings == other.readings &&
         definitions == other.definitions;
}

EdictDictionary EdictDictionary::parse(std::string_view bytes,
                                       EdictEncoding encoding,
                                       const EdictParseLimits& limits) {
  return parse(bytes, encoding, limits, kDefaultLegacyCodePage);
}

EdictDictionary EdictDictionary::parse(std::string_view bytes,
                                       EdictEncoding encoding,
                                       const EdictParseLimits& limits,
                                       LegacyCodePage mixed_code_page) {
  if (bytes.size() > limits.encoded_bytes) {
    throw EdictDictionaryError("EDICT dictionary exceeds the encoded size limit");
  }

  switch (encoding) {
    case EdictEncoding::kEucJp:
    case EdictEncoding::kUtf8:
    case EdictEncoding::kMixed:
      break;
    default:
      throw EdictDictionaryError("EDICT dictionary encoding is invalid");
  }
  if (encoding == EdictEncoding::kMixed &&
      legacy_code_page_name(mixed_code_page) == "Unknown") {
    mixed_code_page = kDefaultLegacyCodePage;
  }

  EdictDictionary dictionary;
  dictionary.encoding_ = encoding;
  dictionary.mixed_code_page_ = mixed_code_page;
  dictionary.source_bytes_.assign(bytes);
  std::size_t decoded_code_points = 0;
  std::size_t definition_count = 0;
  std::size_t line_start = 0;
  std::optional<char> previous_line_break;
  while (line_start < bytes.size()) {
    if (previous_line_break.has_value() && is_line_break(bytes[line_start]) &&
        bytes[line_start] != *previous_line_break) {
      ++line_start;
      previous_line_break.reset();
      if (line_start == bytes.size()) {
        break;
      }
    } else {
      previous_line_break.reset();
    }
    if (dictionary.records_.size() == limits.records) {
      fail(line_start, "dictionary exceeds the record count limit");
    }
    std::size_t line_end = line_start;
    while (line_end < bytes.size() && !is_line_break(bytes[line_end])) {
      ++line_end;
    }
    if (line_end == bytes.size()) {
      fail(line_start, "record is not line-terminated");
    }
    if (line_end - line_start > limits.line_bytes) {
      fail(line_start, "record exceeds the line size limit");
    }
    if (line_end == line_start) {
      fail(line_start, "record is empty");
    }

    dictionary.records_.push_back(parse_record(
        bytes.substr(line_start, line_end - line_start), encoding, line_start,
        decoded_code_points, definition_count, limits,
        line_start == 0 && encoding == EdictEncoding::kUtf8,
        mixed_code_page));

    previous_line_break = bytes[line_end];
    ++line_end;
    line_start = line_end;
  }
  dictionary.definition_count_ = definition_count;
  dictionary.decoded_code_points_ = decoded_code_points;
  return dictionary;
}

EdictEncoding EdictDictionary::encoding() const noexcept { return encoding_; }

LegacyCodePage EdictDictionary::mixed_code_page() const noexcept {
  return mixed_code_page_;
}

std::string_view EdictDictionary::source_bytes() const noexcept {
  return source_bytes_;
}

const std::vector<EdictRecord>& EdictDictionary::records() const noexcept {
  return records_;
}

std::size_t EdictDictionary::definition_count() const noexcept {
  return definition_count_;
}

std::size_t EdictDictionary::decoded_code_points() const noexcept {
  return decoded_code_points_;
}

}  // namespace jwpqt::core
