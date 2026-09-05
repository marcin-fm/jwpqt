// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/kanji_info.h"

#include <limits>
#include <utility>

#include "jwpqt/core/jwp_text_codec.h"
#include "jwpqt/core/legacy_code_page.h"

namespace jwpqt::core {
namespace {

constexpr std::size_t kHeaderSize = 12;
constexpr std::size_t kFixedSize = 16;
constexpr std::size_t kExtendedSize = 10;
constexpr std::uint32_t kKnownFlags = 0x3fU;

std::uint16_t read_u16(std::string_view bytes, std::size_t offset) {
  if (offset > bytes.size() || bytes.size() - offset < 2) {
    throw KanjiInfoError("Kanji information data is truncated");
  }
  return static_cast<std::uint16_t>(
      static_cast<unsigned char>(bytes[offset]) |
      (static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[offset + 1]))
       << 8U));
}

std::uint32_t read_u32(std::string_view bytes, std::size_t offset) {
  if (offset > bytes.size() || bytes.size() - offset < 4) {
    throw KanjiInfoError("Kanji information data is truncated");
  }
  return static_cast<std::uint32_t>(
      static_cast<unsigned char>(bytes[offset]) |
      (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset + 1]))
       << 8U) |
      (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset + 2]))
       << 16U) |
      (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset + 3]))
       << 24U));
}

void validate_limits_impl(const KanjiInfoLimits& limits) {
  if (limits.encoded_bytes == 0 || limits.records == 0 ||
      limits.strings == 0 || limits.string_bytes == 0 || limits.codes == 0) {
    throw KanjiInfoError("Kanji information limits must be positive");
  }
}

std::size_t checked_fixed_end(std::uint16_t count) {
  const std::size_t records = static_cast<std::size_t>(count);
  if (records > (std::numeric_limits<std::size_t>::max() - kHeaderSize) /
                    kFixedSize) {
    throw KanjiInfoError("Kanji information fixed table is too large");
  }
  return kHeaderSize + records * kFixedSize;
}

std::size_t index_for_code(JisCode code) {
  const std::uint16_t normalized = static_cast<std::uint16_t>(code & 0x7f7fU);
  const std::uint8_t row = static_cast<std::uint8_t>(normalized >> 8U);
  const std::uint8_t cell = static_cast<std::uint8_t>(normalized & 0xffU);
  if (row < 0x30U || row > 0x7eU || cell < 0x21U || cell > 0x7eU) {
    throw KanjiInfoError("Kanji information code is outside the table");
  }
  return static_cast<std::size_t>(row - 0x30U) * 94U +
         static_cast<std::size_t>(cell - 0x21U);
}

std::string_view read_string(std::string_view bytes, std::size_t& cursor,
                             std::size_t& string_count,
                             std::size_t& string_bytes,
                             const KanjiInfoLimits& limits) {
  if (string_count >= limits.strings || cursor >= bytes.size()) {
    throw KanjiInfoError("Kanji information string budget is exceeded");
  }
  const std::size_t end = bytes.find('\0', cursor);
  if (end == std::string_view::npos) {
    throw KanjiInfoError("Kanji information string is unterminated");
  }
  const std::size_t length = end - cursor;
  if (length > limits.string_bytes - string_bytes) {
    throw KanjiInfoError("Kanji information string bytes exceed their budget");
  }
  const std::string_view value = bytes.substr(cursor, length);
  cursor = end + 1;
  ++string_count;
  string_bytes += length;
  return value;
}

std::u32string decode_single_byte(std::string_view bytes) {
  std::u32string decoded;
  decoded.reserve(bytes.size());
  for (const char raw : bytes) {
    const auto code_point = legacy_byte_to_unicode(
        static_cast<std::uint8_t>(raw), kDefaultLegacyCodePage);
    if (!code_point.has_value()) {
      throw KanjiInfoError(
          "Kanji information string contains an undefined byte");
    }
    decoded.push_back(*code_point);
  }
  return decoded;
}

std::u32string decode_reading(std::string_view bytes, JisCode base) {
  JwpText text;
  text.reserve(bytes.size() + 1);
  bool okurigana = false;
  for (const char raw : bytes) {
    const auto value = static_cast<std::uint8_t>(raw);
    if (value == 0x1fU) {
      text.push_back(0x213cU);
    } else if (value == 0x20U) {
      text.push_back(static_cast<JisCode>('('));
      okurigana = true;
    } else if ((value & 0x80U) != 0) {
      if (!okurigana) {
        text.push_back(static_cast<JisCode>('('));
        okurigana = true;
      }
      text.push_back(static_cast<JisCode>(base | (value & 0x7fU)));
    } else if (value >= 0x21U && value <= 0x7eU) {
      text.push_back(static_cast<JisCode>(base | value));
    } else {
      throw KanjiInfoError("Kanji information reading contains an invalid byte");
    }
  }
  if (okurigana) {
    text.push_back(static_cast<JisCode>(')'));
  }
  return decode_jwp_text(text);
}

}  // namespace

KanjiInfoDatabase KanjiInfoDatabase::parse(std::string_view bytes,
                                           const KanjiInfoLimits& limits) {
  validate_kanji_info_limits(limits);
  if (bytes.size() > limits.encoded_bytes) {
    throw KanjiInfoError("Kanji information data exceeds its byte limit");
  }
  if (bytes.size() < kHeaderSize || read_u32(bytes, 0) != kKanjiInfoMagic) {
    throw KanjiInfoError("Kanji information header is invalid");
  }
  const std::uint32_t flags = read_u32(bytes, 4);
  const std::uint16_t count = read_u16(bytes, 8);
  const JisCode maximum = read_u16(bytes, 10);
  if ((flags & ~kKnownFlags) != 0 || count == 0 || count > limits.records ||
      checked_fixed_end(count) > bytes.size()) {
    throw KanjiInfoError("Kanji information table is invalid");
  }
  const std::size_t maximum_index = index_for_code(maximum);
  if (maximum_index >= count) {
    throw KanjiInfoError("Kanji information maximum code exceeds its table");
  }

  KanjiInfoDatabase database;
  database.bytes_.assign(bytes);
  database.flags_ = flags;
  database.count_ = count;
  database.maximum_code_ = static_cast<JisCode>(maximum & 0x7f7fU);
  database.limits_ = limits;
  return database;
}

void validate_kanji_info_limits(const KanjiInfoLimits& limits) {
  validate_limits_impl(limits);
}

std::uint32_t KanjiInfoDatabase::flags() const noexcept { return flags_; }
std::uint16_t KanjiInfoDatabase::count() const noexcept { return count_; }
JisCode KanjiInfoDatabase::maximum_code() const noexcept { return maximum_code_; }

bool KanjiInfoDatabase::contains(JisCode code) const noexcept {
  try {
    return index_for_code(code) < count_ &&
           static_cast<JisCode>(code & 0x7f7fU) <= maximum_code_;
  } catch (const KanjiInfoError&) {
    return false;
  }
}

KanjiInfoRecord KanjiInfoDatabase::record(JisCode code) const {
  if (!contains(code)) {
    throw KanjiInfoError("Kanji information code is unavailable");
  }
  const std::string_view bytes(bytes_);
  const std::size_t fixed_offset =
      kHeaderSize + index_for_code(code) * kFixedSize;
  const std::uint16_t word0 = read_u16(bytes, fixed_offset);
  const std::uint16_t word1 = read_u16(bytes, fixed_offset + 2);
  const std::uint16_t word2 = read_u16(bytes, fixed_offset + 4);
  const std::uint16_t word3 = read_u16(bytes, fixed_offset + 6);
  const std::uint16_t word4 = read_u16(bytes, fixed_offset + 8);
  const std::uint16_t haig = read_u16(bytes, fixed_offset + 10);
  const std::uint32_t final = read_u32(bytes, fixed_offset + 12);

  const std::uint8_t on_count = static_cast<std::uint8_t>((word0 >> 13U) & 7U);
  const std::uint8_t meaning_count =
      static_cast<std::uint8_t>((word1 >> 4U) & 15U);
  const bool pinyin_present = ((word2 >> 5U) & 1U) != 0;
  const std::uint8_t kun_count = static_cast<std::uint8_t>((word2 >> 6U) & 31U);
  const std::uint8_t nanori_count =
      static_cast<std::uint8_t>((word2 >> 11U) & 31U);
  const bool extra_present = (word3 & 1U) != 0;
  const bool korean_present = (word4 & 1U) != 0;
  std::size_t cursor = static_cast<std::size_t>((final >> 8U) & 0x00ffffffU);
  if (cursor < checked_fixed_end(count_) || cursor > bytes.size()) {
    throw KanjiInfoError("Kanji information variable offset is invalid");
  }

  KanjiInfoRecord result;
  result.code = static_cast<JisCode>(code & 0x7f7fU);
  result.fixed.bushu = static_cast<std::uint8_t>(word0 & 0xffU);
  result.fixed.strokes = static_cast<std::uint8_t>((word0 >> 8U) & 31U);
  result.fixed.grade = static_cast<std::uint8_t>(word1 & 15U);
  result.fixed.skip.type = static_cast<std::uint8_t>((word1 >> 8U) & 7U);
  result.fixed.skip.first = static_cast<std::uint8_t>((word1 >> 11U) & 31U);
  result.fixed.skip.second = static_cast<std::uint8_t>(word2 & 31U);
  result.fixed.halpern = static_cast<std::uint16_t>(word3 >> 1U);
  result.fixed.nelson = static_cast<std::uint16_t>(word4 >> 1U);
  result.fixed.haig = haig;
  result.fixed.classical_bushu = static_cast<std::uint8_t>(final & 0xffU);

  std::size_t strings = 0;
  std::size_t string_bytes = 0;
  auto next = [&] {
    return read_string(bytes, cursor, strings, string_bytes, limits_);
  };
  if (korean_present) result.korean = decode_single_byte(next());
  if (pinyin_present) result.pinyin = decode_single_byte(next());
  for (std::uint8_t i = 0; i < meaning_count; ++i) {
    result.meanings.push_back(decode_single_byte(next()));
  }
  for (std::uint8_t i = 0; i < on_count; ++i) {
    result.on_readings.push_back(decode_reading(next(), 0x2500U));
  }
  for (std::uint8_t i = 0; i < kun_count; ++i) {
    result.kun_readings.push_back(decode_reading(next(), 0x2400U));
  }
  for (std::uint8_t i = 0; i < nanori_count; ++i) {
    result.nanori.push_back(decode_reading(next(), 0x2400U));
  }

  if (!extra_present) return result;
  if (cursor > bytes.size() || bytes.size() - cursor < kExtendedSize) {
    throw KanjiInfoError("Kanji information extended record is truncated");
  }
  result.has_extended = true;
  result.extended.morohashi_long = read_u16(bytes, cursor);
  const std::uint32_t first = read_u32(bytes, cursor + 2);
  const std::uint32_t second = read_u32(bytes, cursor + 6);
  result.extended.morohashi_volume = static_cast<std::uint8_t>(first & 15U);
  result.extended.morohashi_index =
      static_cast<std::uint16_t>((first >> 4U) & 0x1fffU);
  result.extended.spahn_radical_strokes =
      static_cast<std::uint8_t>((first >> 17U) & 31U);
  result.extended.spahn_radical =
      static_cast<std::uint8_t>((first >> 22U) & 31U);
  result.extended.spahn_other_strokes =
      static_cast<std::uint8_t>((first >> 27U) & 31U);
  result.extended.spahn_index = static_cast<std::uint8_t>(second & 63U);
  result.extended.four_corner =
      static_cast<std::uint16_t>((second >> 6U) & 0x3fffU);
  result.extended.four_corner_index =
      static_cast<std::uint8_t>((second >> 20U) & 15U);
  result.extended.four_corner_second_index =
      static_cast<std::uint8_t>((second >> 24U) & 15U);
  result.extended.morohashi_page = ((second >> 28U) & 1U) != 0;
  result.extended.morohashi_cross = ((second >> 29U) & 1U) != 0;
  cursor += kExtendedSize;

  while (true) {
    if (cursor >= bytes.size()) {
      throw KanjiInfoError("Kanji information reference list is unterminated");
    }
    const char kind = bytes[cursor++];
    if (kind == '\0') break;
    if (result.references.size() >= limits_.codes ||
        cursor > bytes.size() || bytes.size() - cursor < 2) {
      throw KanjiInfoError("Kanji information reference budget is exceeded");
    }
    result.references.push_back({kind, read_u16(bytes, cursor)});
    cursor += 2;
  }
  return result;
}

}  // namespace jwpqt::core
