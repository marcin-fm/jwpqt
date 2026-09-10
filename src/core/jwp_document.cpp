#include "jwpqt/core/jwp_document.h"

#include "jwpqt/core/byte_io.h"

#include <algorithm>
#include <limits>
#include <optional>
#include <string>

namespace jwpqt::core {
namespace {

constexpr std::uint32_t kJwpMagic = 0x42022667U;
constexpr std::uint8_t kLandscape = 0x01;
constexpr std::uint8_t kSummary = 0x02;
constexpr std::uint8_t kHeaders = 0x04;
constexpr std::uint8_t kSeparateHeaders = 0x08;
constexpr std::uint8_t kNoFirstPage = 0x10;
constexpr std::uint8_t kVertical = 0x20;
constexpr std::size_t kMaxDecodedTextUnits = 16U * 1024U * 1024U;

JwpFormatError format_error(const std::string& message) {
  return JwpFormatError("Invalid JWP document: " + message);
}

std::int8_t byte_to_i8(std::uint8_t value) noexcept {
  if (value <= 0x7fU) {
    return static_cast<std::int8_t>(value);
  }
  return static_cast<std::int8_t>(static_cast<int>(value) - 0x100);
}

std::uint8_t i8_to_byte(std::int8_t value) noexcept {
  if (value >= 0) {
    return static_cast<std::uint8_t>(value);
  }
  return static_cast<std::uint8_t>(static_cast<int>(value) + 0x100);
}

JwpVersion read_version(ByteReader& reader) {
  const std::string_view field = reader.read_bytes(6);
  const std::size_t end = field.find('\0');
  if (end == std::string_view::npos) {
    throw format_error("unterminated version field");
  }

  const std::string_view version = field.substr(0, end);
  if (version == "J1.20") {
    return JwpVersion::kJ120;
  }
  if (version == "B2") {
    return JwpVersion::kB2;
  }
  if (version == "B1") {
    return JwpVersion::kB1;
  }
  throw format_error("unsupported version");
}

void write_current_version(ByteWriter& writer) {
  writer.write_bytes(std::string_view("J1.20\0", 6));
}

void add_decoded_units(std::size_t count, std::size_t& total) {
  if (count > kMaxDecodedTextUnits - total) {
    throw format_error("decoded text exceeds the safety limit");
  }
  total += count;
}

JwpText read_kstring(ByteReader& reader, std::size_t& decoded_units) {
  const std::int16_t length = reader.read_i16_le();
  if (length < 0) {
    throw format_error("negative metadata string length");
  }
  add_decoded_units(static_cast<std::size_t>(length), decoded_units);

  JwpText text;
  text.reserve(static_cast<std::size_t>(length));
  for (std::int16_t index = 0; index < length; ++index) {
    text.push_back(reader.read_u16_le());
  }
  return text;
}

void write_kstring(ByteWriter& writer, const JwpText& text) {
  if (text.size() >
      static_cast<std::size_t>(std::numeric_limits<std::int16_t>::max())) {
    throw JwpFormatError("JWP metadata string is too long");
  }
  if (std::find(text.begin(), text.end(), static_cast<JisCode>(0)) !=
      text.end()) {
    throw JwpFormatError("JWP metadata string contains an embedded NUL");
  }
  writer.write_i16_le(static_cast<std::int16_t>(text.size()));
  for (const JisCode code : text) {
    writer.write_u16_le(code);
  }
}

JwpParagraph read_paragraph(ByteReader& reader, JwpVersion version,
                            std::size_t& decoded_units) {
  JwpParagraph paragraph;
  std::int16_t text_size = 0;

  if (version == JwpVersion::kB1) {
    text_size = reader.read_i16_le();
    paragraph.first_indent =
        byte_to_i8(static_cast<std::uint8_t>(reader.read_i16_le()));
    paragraph.left_indent =
        static_cast<std::uint8_t>(reader.read_i16_le());
    paragraph.right_indent =
        static_cast<std::uint8_t>(reader.read_i16_le());
  } else {
    text_size = reader.read_i16_le();
    paragraph.line_spacing = reader.read_i16_le();
    paragraph.first_indent = byte_to_i8(reader.read_u8());
    paragraph.left_indent = reader.read_u8();
    paragraph.right_indent = reader.read_u8();
    static_cast<void>(reader.read_u8());
    const std::uint8_t flags = reader.read_u8();
    paragraph.page_break = (flags & 0x01U) != 0;
    static_cast<void>(reader.read_bytes(7));
  }

  if (text_size <= 0) {
    throw format_error("paragraph has no terminator");
  }
  add_decoded_units(static_cast<std::size_t>(text_size - 1), decoded_units);

  JwpText stored_text;
  stored_text.reserve(static_cast<std::size_t>(text_size));
  for (std::int16_t index = 0; index < text_size; ++index) {
    const std::uint8_t lead = reader.read_u8();
    if ((lead & 0x80U) != 0) {
      const std::uint8_t trail = reader.read_u8();
      if ((trail & 0x80U) == 0) {
        throw format_error("JIS trail byte lacks its high bit");
      }
      const JisCode code = static_cast<JisCode>(
          (static_cast<JisCode>(lead & 0x7fU) << 8U) |
          static_cast<JisCode>(trail & 0x7fU));
      if (!is_jis_x0208_pair(code)) {
        throw format_error("invalid JIS character pair");
      }
      stored_text.push_back(code);
    } else if (lead == 0) {
      const std::uint8_t escaped = reader.read_u8();
      if ((escaped & 0x80U) != 0) {
        throw format_error("invalid escaped character byte");
      }
      stored_text.push_back(static_cast<JisCode>(escaped | 0x80U));
    } else {
      stored_text.push_back(lead);
    }
  }

  if (stored_text.back() != static_cast<JisCode>('\n')) {
    throw format_error("paragraph is missing its newline terminator");
  }
  stored_text.pop_back();
  paragraph.text = std::move(stored_text);
  return paragraph;
}

struct DocumentPrefix {
  JwpDocument document;
  std::int16_t paragraph_count = 0;
  std::size_t decoded_units = 0;
};

DocumentPrefix read_document_prefix(ByteReader& reader) {
  DocumentPrefix prefix;
  if (reader.read_u32_le() != kJwpMagic) {
    throw format_error("bad magic");
  }
  prefix.document.source_version = read_version(reader);

  prefix.paragraph_count = reader.read_i16_le();
  if (prefix.paragraph_count < 0) {
    throw format_error("negative paragraph count");
  }
  for (float& margin : prefix.document.margins) {
    margin = reader.read_f32_le();
  }

  const std::uint8_t flags = reader.read_u8();
  prefix.document.landscape = (flags & kLandscape) != 0;
  const bool has_summary = (flags & kSummary) != 0;
  const bool has_headers = (flags & kHeaders) != 0;
  prefix.document.separate_left_right_headers =
      (flags & kSeparateHeaders) != 0;
  prefix.document.suppress_first_page_headers =
      (flags & kNoFirstPage) != 0;
  prefix.document.vertical = (flags & kVertical) != 0;

  const std::int16_t undo_count = reader.read_i16_le();
  if (undo_count < 0) {
    throw format_error("negative undo count");
  }
  if (undo_count != 0) {
    throw format_error("embedded native-ABI undo data is unsupported");
  }
  static_cast<void>(reader.read_bytes(97));

  if (has_summary) {
    for (JwpText& text : prefix.document.summary) {
      text = read_kstring(reader, prefix.decoded_units);
    }
  }
  if (has_headers) {
    for (auto& header : prefix.document.headers) {
      for (JwpText& text : header) {
        text = read_kstring(reader, prefix.decoded_units);
      }
    }
  }
  return prefix;
}

struct RecoveredParagraph {
  std::optional<JwpParagraph> paragraph;
  bool complete = false;
};

RecoveredParagraph recover_paragraph(ByteReader& reader, JwpVersion version,
                                     std::size_t& decoded_units) {
  JwpParagraph paragraph;
  std::int16_t text_size = 0;
  try {
    if (version == JwpVersion::kB1) {
      text_size = reader.read_i16_le();
      paragraph.first_indent =
          byte_to_i8(static_cast<std::uint8_t>(reader.read_i16_le()));
      paragraph.left_indent =
          static_cast<std::uint8_t>(reader.read_i16_le());
      paragraph.right_indent =
          static_cast<std::uint8_t>(reader.read_i16_le());
    } else {
      text_size = reader.read_i16_le();
      paragraph.line_spacing = reader.read_i16_le();
      paragraph.first_indent = byte_to_i8(reader.read_u8());
      paragraph.left_indent = reader.read_u8();
      paragraph.right_indent = reader.read_u8();
      static_cast<void>(reader.read_u8());
      paragraph.page_break = (reader.read_u8() & 0x01U) != 0;
      static_cast<void>(reader.read_bytes(7));
    }
  } catch (const BinaryError&) {
    return {};
  }

  if (text_size <= 0) {
    return {};
  }
  add_decoded_units(static_cast<std::size_t>(text_size - 1), decoded_units);
  paragraph.text.reserve(static_cast<std::size_t>(text_size - 1));

  for (std::int16_t index = 0; index < text_size; ++index) {
    if (reader.empty()) {
      return {std::move(paragraph), false};
    }
    const std::uint8_t lead = reader.read_u8();
    JisCode code = 0;
    if ((lead & 0x80U) != 0) {
      if (reader.empty()) {
        return {std::move(paragraph), false};
      }
      const std::uint8_t trail = reader.read_u8();
      if ((trail & 0x80U) == 0) {
        return {std::move(paragraph), false};
      }
      code = static_cast<JisCode>(
          (static_cast<JisCode>(lead & 0x7fU) << 8U) |
          static_cast<JisCode>(trail & 0x7fU));
      if (!is_jis_x0208_pair(code)) {
        return {std::move(paragraph), false};
      }
    } else if (lead == 0) {
      if (reader.empty()) {
        return {std::move(paragraph), false};
      }
      const std::uint8_t escaped = reader.read_u8();
      if ((escaped & 0x80U) != 0) {
        return {std::move(paragraph), false};
      }
      code = static_cast<JisCode>(escaped | 0x80U);
    } else {
      code = lead;
    }

    if (code == static_cast<JisCode>('\n')) {
      return {std::move(paragraph), index == text_size - 1};
    }
    paragraph.text.push_back(code);
  }
  return {std::move(paragraph), false};
}

void write_paragraph(ByteWriter& writer, const JwpParagraph& paragraph) {
  if (paragraph.text.size() >=
      static_cast<std::size_t>(std::numeric_limits<std::int16_t>::max())) {
    throw JwpFormatError("JWP paragraph is too long");
  }

  writer.write_i16_le(
      static_cast<std::int16_t>(paragraph.text.size() + 1U));
  writer.write_i16_le(paragraph.line_spacing);
  writer.write_u8(i8_to_byte(paragraph.first_indent));
  writer.write_u8(paragraph.left_indent);
  writer.write_u8(paragraph.right_indent);
  writer.write_u8(0);
  writer.write_u8(paragraph.page_break ? 0x01U : 0x00U);
  writer.write_bytes(std::string(7, '\0'));

  for (const JisCode code : paragraph.text) {
    if (is_jis_x0208_pair(code)) {
      writer.write_u8(static_cast<std::uint8_t>((code >> 8U) | 0x80U));
      writer.write_u8(static_cast<std::uint8_t>((code & 0xffU) | 0x80U));
    } else if (code >= 0x80U && code <= 0xffU) {
      const std::uint8_t value = static_cast<std::uint8_t>(code);
      writer.write_u8(0);
      writer.write_u8(static_cast<std::uint8_t>(value & 0x7fU));
    } else if (code > 0 && code <= 0x7fU) {
      writer.write_u8(static_cast<std::uint8_t>(code));
    } else {
      throw JwpFormatError("JWP paragraph contains an invalid character");
    }
  }
  writer.write_u8(static_cast<std::uint8_t>('\n'));
}

JwpDocument decode_impl(std::string_view bytes) {
  ByteReader reader(bytes);
  DocumentPrefix prefix = read_document_prefix(reader);
  prefix.document.paragraphs.reserve(
      static_cast<std::size_t>(prefix.paragraph_count));
  for (std::int16_t index = 0; index < prefix.paragraph_count; ++index) {
    prefix.document.paragraphs.push_back(read_paragraph(
        reader, prefix.document.source_version, prefix.decoded_units));
  }

  if (!reader.empty()) {
    throw format_error("trailing bytes");
  }
  return std::move(prefix.document);
}

}  // namespace

bool has_jwp_document_magic(std::string_view bytes) noexcept {
  return bytes.size() >= 4 &&
         static_cast<std::uint8_t>(bytes[0]) == 0x67U &&
         static_cast<std::uint8_t>(bytes[1]) == 0x26U &&
         static_cast<std::uint8_t>(bytes[2]) == 0x02U &&
         static_cast<std::uint8_t>(bytes[3]) == 0x42U;
}

JwpDocument decode_jwp_document(std::string_view bytes) {
  try {
    return decode_impl(bytes);
  } catch (const BinaryError& error) {
    throw format_error(error.what());
  }
}

JwpDocumentRecovery decode_jwp_document_recovering(std::string_view bytes) {
  try {
    JwpDocument document = decode_jwp_document(bytes);
    const std::size_t paragraphs = document.paragraphs.size();
    return {std::move(document), paragraphs, paragraphs, false, 0, {}};
  } catch (const JwpFormatError& strict_error) {
    try {
      ByteReader reader(bytes);
      DocumentPrefix prefix = read_document_prefix(reader);
      JwpDocumentRecovery recovery;
      recovery.document = std::move(prefix.document);
      recovery.declared_paragraphs =
          static_cast<std::size_t>(prefix.paragraph_count);
      recovery.damage = strict_error.what();
      recovery.document.paragraphs.reserve(recovery.declared_paragraphs);

      for (std::int16_t index = 0; index < prefix.paragraph_count; ++index) {
        RecoveredParagraph recovered = recover_paragraph(
            reader, recovery.document.source_version, prefix.decoded_units);
        if (!recovered.paragraph) {
          break;
        }
        recovery.document.paragraphs.push_back(
            std::move(*recovered.paragraph));
        if (!recovered.complete) {
          recovery.partial_paragraph = true;
          break;
        }
        ++recovery.complete_paragraphs;
      }

      if (recovery.complete_paragraphs == recovery.declared_paragraphs) {
        recovery.ignored_trailing_bytes = reader.remaining();
      }
      return recovery;
    } catch (const BinaryError& error) {
      throw format_error(error.what());
    }
  }
}

std::string encode_jwp_document(const JwpDocument& document) {
  if (document.paragraphs.size() >
      static_cast<std::size_t>(std::numeric_limits<std::int16_t>::max())) {
    throw JwpFormatError("JWP document has too many paragraphs");
  }

  const bool has_summary = std::any_of(
      document.summary.begin(), document.summary.end(),
      [](const JwpText& text) { return !text.empty(); });
  const bool has_headers = std::any_of(
      document.headers.begin(), document.headers.end(), [](const auto& row) {
        return std::any_of(row.begin(), row.end(),
                           [](const JwpText& text) { return !text.empty(); });
      });

  ByteWriter writer;
  writer.write_u32_le(kJwpMagic);
  write_current_version(writer);
  writer.write_i16_le(
      static_cast<std::int16_t>(document.paragraphs.size()));
  for (const float margin : document.margins) {
    writer.write_f32_le(margin);
  }

  std::uint8_t flags = 0;
  flags = static_cast<std::uint8_t>(
      flags | (document.landscape ? kLandscape : 0U) |
      (has_summary ? kSummary : 0U) | (has_headers ? kHeaders : 0U) |
      (document.separate_left_right_headers ? kSeparateHeaders : 0U) |
      (document.suppress_first_page_headers ? kNoFirstPage : 0U) |
      (document.vertical ? kVertical : 0U));
  writer.write_u8(flags);
  writer.write_i16_le(0);
  writer.write_bytes(std::string(97, '\0'));

  if (has_summary) {
    for (const JwpText& text : document.summary) {
      write_kstring(writer, text);
    }
  }
  if (has_headers) {
    for (const auto& header : document.headers) {
      for (const JwpText& text : header) {
        write_kstring(writer, text);
      }
    }
  }
  for (const JwpParagraph& paragraph : document.paragraphs) {
    write_paragraph(writer, paragraph);
  }
  return writer.take_bytes();
}

}  // namespace jwpqt::core
