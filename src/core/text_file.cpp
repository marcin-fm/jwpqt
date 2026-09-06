// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/text_file.h"

#include <utility>

#include "jwpqt/core/jfc_text.h"
#include "jwpqt/core/jis_text.h"
#include "jwpqt/core/legacy_text.h"
#include "jwpqt/core/utf7.h"
#include "jwpqt/core/utf8.h"
#include "jwpqt/core/utf16.h"

namespace jwpqt::core {
namespace {

LegacyEncoding legacy_encoding(TextEncoding encoding) {
  switch (encoding) {
    case TextEncoding::kEucJp:
      return LegacyEncoding::kEucJp;
    case TextEncoding::kShiftJis:
      return LegacyEncoding::kShiftJis;
    case TextEncoding::kUtf8:
    case TextEncoding::kUtf7:
    case TextEncoding::kUtf16Le:
    case TextEncoding::kUtf16Be:
    case TextEncoding::kJfc:
    case TextEncoding::kNewJis:
    case TextEncoding::kOldJis:
    case TextEncoding::kNecJis:
      throw TextFileError("UTF-8 is not a legacy encoding");
  }
  throw TextFileError("Unknown text encoding");
}

JisTextEncoding jis_text_encoding(TextEncoding encoding) {
  switch (encoding) {
    case TextEncoding::kNewJis:
      return JisTextEncoding::kNewJis;
    case TextEncoding::kOldJis:
      return JisTextEncoding::kOldJis;
    case TextEncoding::kNecJis:
      return JisTextEncoding::kNecJis;
    case TextEncoding::kUtf8:
    case TextEncoding::kUtf7:
    case TextEncoding::kUtf16Le:
    case TextEncoding::kUtf16Be:
    case TextEncoding::kJfc:
    case TextEncoding::kEucJp:
    case TextEncoding::kShiftJis:
      throw TextFileError("Encoding is not a JIS escape format");
  }
  throw TextFileError("Unknown text encoding");
}

}  // namespace

std::string_view text_encoding_name(TextEncoding encoding) {
  switch (encoding) {
    case TextEncoding::kUtf8:
      return "UTF-8";
    case TextEncoding::kUtf7:
      return "UTF-7";
    case TextEncoding::kUtf16Le:
      return "UTF-16LE";
    case TextEncoding::kUtf16Be:
      return "UTF-16BE";
    case TextEncoding::kJfc:
      return "JFC";
    case TextEncoding::kEucJp:
      return "EUC-JP";
    case TextEncoding::kShiftJis:
      return "Shift-JIS";
    case TextEncoding::kNewJis:
      return "New JIS";
    case TextEncoding::kOldJis:
      return "Old JIS";
    case TextEncoding::kNecJis:
      return "NEC JIS";
  }
  throw TextFileError("Unknown text encoding");
}

std::optional<TextEncoding> parse_text_encoding(std::string_view name) noexcept {
  if (name == "utf-8") {
    return TextEncoding::kUtf8;
  }
  if (name == "utf-7") {
    return TextEncoding::kUtf7;
  }
  if (name == "utf-16le") return TextEncoding::kUtf16Le;
  if (name == "utf-16be") return TextEncoding::kUtf16Be;
  if (name == "jfc") {
    return TextEncoding::kJfc;
  }
  if (name == "euc-jp") {
    return TextEncoding::kEucJp;
  }
  if (name == "shift-jis") {
    return TextEncoding::kShiftJis;
  }
  if (name == "new-jis") {
    return TextEncoding::kNewJis;
  }
  if (name == "old-jis") {
    return TextEncoding::kOldJis;
  }
  if (name == "nec-jis") {
    return TextEncoding::kNecJis;
  }
  return std::nullopt;
}

TextFile decode_text_file(std::string_view bytes, TextEncoding encoding) {
  switch (encoding) {
    case TextEncoding::kUtf8: {
      Utf8File file = decode_utf8_file(bytes);
      return {std::move(file.text), encoding, file.has_byte_order_mark};
    }
    case TextEncoding::kUtf7:
      return {decode_utf7(bytes), encoding, false};
    case TextEncoding::kUtf16Le:
    case TextEncoding::kUtf16Be: {
      auto file = decode_utf16_file(
          bytes, encoding == TextEncoding::kUtf16Le
                     ? Utf16ByteOrder::kLittleEndian : Utf16ByteOrder::kBigEndian);
      return {std::move(file.text), encoding, file.has_byte_order_mark};
    }
    case TextEncoding::kJfc:
      return {decode_jfc_text(bytes), encoding, false};
    case TextEncoding::kEucJp:
    case TextEncoding::kShiftJis:
      return {decode_legacy_text(bytes, legacy_encoding(encoding)), encoding,
              false};
    case TextEncoding::kNewJis:
    case TextEncoding::kOldJis:
    case TextEncoding::kNecJis:
      return {decode_jis_text(bytes, jis_text_encoding(encoding)), encoding,
              false};
  }
  throw TextFileError("Unknown text encoding");
}

std::string encode_text_file(const TextFile& file) {
  switch (file.encoding) {
    case TextEncoding::kUtf8:
      return encode_utf8_file({file.text, file.has_byte_order_mark});
    case TextEncoding::kUtf16Le:
    case TextEncoding::kUtf16Be:
      return encode_utf16_file({file.text, file.has_byte_order_mark},
                               file.encoding == TextEncoding::kUtf16Le
                                   ? Utf16ByteOrder::kLittleEndian
                                   : Utf16ByteOrder::kBigEndian);
    case TextEncoding::kUtf7:
      if (file.has_byte_order_mark) {
        throw TextFileError(
            "A byte-order mark is only valid for UTF-8 or UTF-16 documents");
      }
      return encode_utf7(file.text);
    case TextEncoding::kJfc:
      if (file.has_byte_order_mark) {
        throw TextFileError(
            "A byte-order mark is only valid for UTF-8 or UTF-16 documents");
      }
      return encode_jfc_text(file.text);
    case TextEncoding::kEucJp:
    case TextEncoding::kShiftJis:
      if (file.has_byte_order_mark) {
        throw TextFileError(
            "A byte-order mark is only valid for UTF-8 or UTF-16 documents");
      }
      return encode_legacy_text(file.text, legacy_encoding(file.encoding));
    case TextEncoding::kNewJis:
    case TextEncoding::kOldJis:
    case TextEncoding::kNecJis:
      if (file.has_byte_order_mark) {
        throw TextFileError(
            "A byte-order mark is only valid for UTF-8 or UTF-16 documents");
      }
      return encode_jis_text(file.text, jis_text_encoding(file.encoding));
  }
  throw TextFileError("Unknown text encoding");
}

}  // namespace jwpqt::core
