// SPDX-License-Identifier: GPL-2.0-or-later

#include "clipboard_mime.h"

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

#include <QMimeData>

#include "jwpqt/core/text_detection.h"
#include "jwpqt/core/text_file.h"
#include "jwpqt/core/jis_encoding.h"
#include "jwpqt/core/jwp_text_codec.h"
#include "jwpqt/core/legacy_code_page.h"
#include "text_bridge.h"

namespace jwpqt::qt {
namespace {

constexpr std::array<char, 4> kMagic{'J', 'Q', 'T', 'E'};
constexpr std::size_t kHeaderSize = 12;
constexpr std::size_t kMaximumBytes = 64U * 1024U * 1024U;

core::LegacyCodePage resolved_code_page(int value) {
  if (value == 0) return core::kDefaultLegacyCodePage;
  if (value < 1250 || value > 1258)
    throw std::invalid_argument("Clipboard code page is unsupported");
  return static_cast<core::LegacyCodePage>(value);
}

core::TextEncoding text_encoding(ClipboardTextFormat format) {
  switch (format) {
    case ClipboardTextFormat::kEucJp: return core::TextEncoding::kEucJp;
    case ClipboardTextFormat::kShiftJis: return core::TextEncoding::kShiftJis;
    case ClipboardTextFormat::kNewJis: return core::TextEncoding::kNewJis;
    case ClipboardTextFormat::kOldJis: return core::TextEncoding::kOldJis;
    case ClipboardTextFormat::kNecJis: return core::TextEncoding::kNecJis;
    case ClipboardTextFormat::kUnicode: return core::TextEncoding::kUtf16Le;
    case ClipboardTextFormat::kUtf7: return core::TextEncoding::kUtf7;
    case ClipboardTextFormat::kUtf8: return core::TextEncoding::kUtf8;
    case ClipboardTextFormat::kAutoDetect: break;
  }
  throw std::invalid_argument("Automatic clipboard format has no encoder");
}

ClipboardTextFormat clipboard_format(core::TextEncoding encoding) {
  switch (encoding) {
    case core::TextEncoding::kEucJp: return ClipboardTextFormat::kEucJp;
    case core::TextEncoding::kShiftJis: return ClipboardTextFormat::kShiftJis;
    case core::TextEncoding::kNewJis: return ClipboardTextFormat::kNewJis;
    case core::TextEncoding::kOldJis: return ClipboardTextFormat::kOldJis;
    case core::TextEncoding::kNecJis: return ClipboardTextFormat::kNecJis;
    case core::TextEncoding::kUtf16Le:
    case core::TextEncoding::kUtf16Be: return ClipboardTextFormat::kUnicode;
    case core::TextEncoding::kUtf7: return ClipboardTextFormat::kUtf7;
    case core::TextEncoding::kUtf8:
    case core::TextEncoding::kJfc: return ClipboardTextFormat::kUtf8;
  }
  throw std::invalid_argument("Detected clipboard format is unsupported");
}

QString charset(ClipboardTextFormat format) {
  switch (format) {
    case ClipboardTextFormat::kEucJp: return QStringLiteral("EUC-JP");
    case ClipboardTextFormat::kShiftJis: return QStringLiteral("Shift_JIS");
    case ClipboardTextFormat::kNewJis: return QStringLiteral("ISO-2022-JP");
    case ClipboardTextFormat::kOldJis: return QStringLiteral("x-jwpqt-old-jis");
    case ClipboardTextFormat::kNecJis: return QStringLiteral("x-jwpqt-nec-jis");
    case ClipboardTextFormat::kUnicode: return QStringLiteral("UTF-16LE");
    case ClipboardTextFormat::kUtf7: return QStringLiteral("UTF-7");
    case ClipboardTextFormat::kUtf8: return QStringLiteral("UTF-8");
    case ClipboardTextFormat::kAutoDetect: break;
  }
  throw std::invalid_argument("Automatic clipboard format has no charset");
}

std::optional<ClipboardTextFormat> parse_charset(QString value) {
  value = value.trimmed();
  if (value.compare(QStringLiteral("EUC-JP"), Qt::CaseInsensitive) == 0)
    return ClipboardTextFormat::kEucJp;
  if (value.compare(QStringLiteral("Shift_JIS"), Qt::CaseInsensitive) == 0 ||
      value.compare(QStringLiteral("SJIS"), Qt::CaseInsensitive) == 0 ||
      value.compare(QStringLiteral("CP932"), Qt::CaseInsensitive) == 0)
    return ClipboardTextFormat::kShiftJis;
  if (value.compare(QStringLiteral("ISO-2022-JP"), Qt::CaseInsensitive) == 0)
    return ClipboardTextFormat::kNewJis;
  if (value.compare(QStringLiteral("x-jwpqt-old-jis"), Qt::CaseInsensitive) == 0)
    return ClipboardTextFormat::kOldJis;
  if (value.compare(QStringLiteral("x-jwpqt-nec-jis"), Qt::CaseInsensitive) == 0)
    return ClipboardTextFormat::kNecJis;
  if (value.compare(QStringLiteral("UTF-16LE"), Qt::CaseInsensitive) == 0 ||
      value.compare(QStringLiteral("UTF-16"), Qt::CaseInsensitive) == 0)
    return ClipboardTextFormat::kUnicode;
  if (value.compare(QStringLiteral("UTF-7"), Qt::CaseInsensitive) == 0)
    return ClipboardTextFormat::kUtf7;
  if (value.compare(QStringLiteral("UTF-8"), Qt::CaseInsensitive) == 0)
    return ClipboardTextFormat::kUtf8;
  return std::nullopt;
}

std::string encode_text(const QString& text, ClipboardTextFormat format,
                        core::LegacyCodePage code_page) {
  const auto unicode = from_qstring(text);
  if (to_qstring(unicode) != text)
    throw std::invalid_argument("Clipboard text is not valid Unicode");
  if (format == ClipboardTextFormat::kUnicode ||
      format == ClipboardTextFormat::kUtf7 ||
      format == ClipboardTextFormat::kUtf8) {
    const bool preserve_leading_signature =
        (format == ClipboardTextFormat::kUnicode ||
         format == ClipboardTextFormat::kUtf8) &&
        !unicode.empty() && unicode.front() == 0xfeffU;
    return core::encode_text_file(
        {unicode, text_encoding(format), preserve_leading_signature});
  }

  const core::JwpText tokens = core::encode_jwp_text(unicode, code_page);
  std::string bytes;
  bytes.reserve(tokens.size() * 2U);
  bool in_jis = false;
  const auto leave_jis = [&] {
    if (!in_jis) return;
    bytes.append(format == ClipboardTextFormat::kNecJis ? "\x1bH" : "\x1b(J");
    in_jis = false;
  };
  for (const core::JisCode token : tokens) {
    if ((token >> 8U) == 0) {
      leave_jis();
      bytes.push_back(static_cast<char>(token));
      continue;
    }
    if (format == ClipboardTextFormat::kEucJp) {
      const auto pair = core::encode_euc_jp_pair(token);
      bytes.push_back(static_cast<char>(pair.lead));
      bytes.push_back(static_cast<char>(pair.trail));
    } else if (format == ClipboardTextFormat::kShiftJis) {
      const auto pair = core::encode_shift_jis_pair(token);
      bytes.push_back(static_cast<char>(pair.lead));
      bytes.push_back(static_cast<char>(pair.trail));
    } else {
      if (!in_jis) {
        if (format == ClipboardTextFormat::kNewJis) bytes.append("\x1b$B");
        else if (format == ClipboardTextFormat::kOldJis) bytes.append("\x1b$@");
        else bytes.append("\x1bK");
        in_jis = true;
      }
      bytes.push_back(static_cast<char>(token >> 8U));
      bytes.push_back(static_cast<char>(token));
    }
  }
  leave_jis();
  return bytes;
}

QString decode_text(std::string_view bytes, ClipboardTextFormat format,
                     core::LegacyCodePage code_page) {
  if (format == ClipboardTextFormat::kUnicode ||
      format == ClipboardTextFormat::kUtf7 ||
      format == ClipboardTextFormat::kUtf8)
    return to_qstring(core::decode_text_file(bytes, text_encoding(format)).text);

  core::JwpText tokens;
  tokens.reserve(bytes.size());
  bool in_jis = false;
  const auto byte_at = [&](std::size_t index) {
    return static_cast<std::uint8_t>(bytes[index]);
  };
  for (std::size_t i = 0; i < bytes.size();) {
    const std::uint8_t lead = byte_at(i);
    if (format == ClipboardTextFormat::kNewJis ||
        format == ClipboardTextFormat::kOldJis ||
        format == ClipboardTextFormat::kNecJis) {
      if (lead == 0x1bU) {
        if (i + 1 >= bytes.size())
          throw std::invalid_argument("Clipboard JIS escape is truncated");
        const std::uint8_t middle = byte_at(i + 1);
        if (middle == 'K' || middle == 'H') {
          in_jis = middle == 'K';
          i += 2;
          continue;
        }
        if (i + 2 >= bytes.size() || (middle != '$' && middle != '('))
          throw std::invalid_argument("Clipboard JIS escape is invalid");
        const std::uint8_t marker = byte_at(i + 2);
        if (middle == '$' && (marker == 'B' || marker == '@')) in_jis = true;
        else if (middle == '(' && (marker == 'B' || marker == 'J')) in_jis = false;
        else throw std::invalid_argument("Clipboard JIS escape is unsupported");
        i += 3;
        continue;
      }
      if (lead == '\r' || lead == '\n') in_jis = false;
      if (!in_jis) {
        tokens.push_back(lead);
        ++i;
        continue;
      }
      if (i + 1 >= bytes.size())
        throw std::invalid_argument("Clipboard JIS pair is truncated");
      const core::JisCode jis = static_cast<core::JisCode>(
          (static_cast<unsigned int>(lead) << 8U) | byte_at(i + 1));
      if (!core::is_jis_x0208_pair(jis))
        throw std::invalid_argument("Clipboard JIS pair is invalid");
      tokens.push_back(jis);
      i += 2;
      continue;
    }

    if (format == ClipboardTextFormat::kEucJp && lead >= 0xa1U && lead <= 0xfeU &&
        i + 1 < bytes.size() && byte_at(i + 1) >= 0xa1U && byte_at(i + 1) <= 0xfeU) {
      tokens.push_back(core::decode_euc_jp_pair({lead, byte_at(i + 1)}));
      i += 2;
    } else if (format == ClipboardTextFormat::kShiftJis &&
               ((lead >= 0x81U && lead <= 0x9fU) ||
                (lead >= 0xe0U && lead <= 0xefU)) &&
               i + 1 < bytes.size() &&
               ((byte_at(i + 1) >= 0x40U && byte_at(i + 1) <= 0x7eU) ||
                (byte_at(i + 1) >= 0x80U && byte_at(i + 1) <= 0xfcU))) {
      tokens.push_back(core::decode_shift_jis_pair({lead, byte_at(i + 1)}));
      i += 2;
    } else {
      tokens.push_back(lead);
      ++i;
    }
  }
  return to_qstring(core::decode_jwp_text(tokens, code_page));
}

QString decode_bytes(const QByteArray& bytes, ClipboardTextFormat format,
                     core::LegacyCodePage code_page) {
  if (bytes.size() > static_cast<qsizetype>(kMaximumBytes))
    throw std::invalid_argument("Clipboard text exceeds its byte limit");
  return decode_text(
      std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size())),
      format, code_page);
}

QString unicode_text(const QMimeData& mime) {
  const QString text = mime.text();
  if (text.size() > static_cast<qsizetype>(kMaximumBytes / 2U))
    throw std::invalid_argument("Clipboard text exceeds its character limit");
  if (to_qstring(from_qstring(text)) != text)
    throw std::invalid_argument("Clipboard text is not valid Unicode");
  return text;
}

QByteArray envelope(std::string_view payload, ClipboardTextFormat format,
                    core::LegacyCodePage code_page) {
  if (payload.size() > kMaximumBytes - kHeaderSize)
    throw std::invalid_argument("Clipboard text exceeds its byte limit");
  QByteArray result;
  result.reserve(static_cast<qsizetype>(kHeaderSize + payload.size()));
  result.append(kMagic.data(), static_cast<qsizetype>(kMagic.size()));
  result.append(char(1));
  result.append(static_cast<char>(format));
  const auto page = static_cast<std::uint16_t>(code_page);
  result.append(static_cast<char>(page & 0xffU));
  result.append(static_cast<char>(page >> 8U));
  const auto size = static_cast<std::uint32_t>(payload.size());
  for (unsigned shift = 0; shift < 32; shift += 8)
    result.append(static_cast<char>((size >> shift) & 0xffU));
  result.append(payload.data(), static_cast<qsizetype>(payload.size()));
  return result;
}

struct EncodedText {
  ClipboardTextFormat format;
  core::LegacyCodePage code_page;
  QByteArray payload;
};

std::optional<EncodedText> read_envelope(const QMimeData& mime) {
  if (!mime.hasFormat(QString::fromLatin1(kEncodedClipboardMime))) return std::nullopt;
  const QByteArray bytes = mime.data(QString::fromLatin1(kEncodedClipboardMime));
  if (bytes.size() < static_cast<qsizetype>(kHeaderSize) ||
      bytes.size() > static_cast<qsizetype>(kMaximumBytes) ||
      std::string_view(bytes.constData(), 4) != std::string_view(kMagic.data(), 4) ||
      static_cast<unsigned char>(bytes[4]) != 1)
    throw std::invalid_argument("Encoded clipboard text has an invalid header");
  const auto format = static_cast<ClipboardTextFormat>(
      static_cast<unsigned char>(bytes[5]));
  (void)text_encoding(format);
  const auto page = static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[6])) |
                    (static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[7])) << 8U);
  const auto code_page = resolved_code_page(page);
  std::uint32_t size = 0;
  for (unsigned index = 0; index < 4; ++index)
    size |= static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[8 + index]))
            << (index * 8U);
  if (size != static_cast<std::uint32_t>(bytes.size() - static_cast<qsizetype>(kHeaderSize)))
    throw std::invalid_argument("Encoded clipboard text has an invalid size");
  return EncodedText{format, code_page, bytes.mid(static_cast<qsizetype>(kHeaderSize))};
}

std::optional<std::pair<ClipboardTextFormat, QByteArray>> declared_plain_text(
    const QMimeData& mime) {
  for (const QString& mime_format : mime.formats()) {
    const QString prefix = QStringLiteral("text/plain;charset=");
    if (!mime_format.startsWith(prefix, Qt::CaseInsensitive)) continue;
    const auto format = parse_charset(mime_format.mid(prefix.size()));
    if (format) return std::pair(*format, mime.data(mime_format));
  }
  return std::nullopt;
}

bool has_unicode_text(const QMimeData& mime) {
  return mime.formats().contains(QStringLiteral("text/plain"));
}

}  // namespace

void add_clipboard_text_formats(QMimeData& mime, const QString& text,
                                ClipboardTextFormat format, int code_page,
                                bool omit_unicode) {
  const auto page = resolved_code_page(code_page);
  if (omit_unicode) {
    for (const QString& mime_format : mime.formats())
      if (mime_format.compare(QStringLiteral("text/plain"), Qt::CaseInsensitive) == 0 ||
          mime_format.compare(QStringLiteral("text/html"), Qt::CaseInsensitive) == 0 ||
          mime_format == QStringLiteral("application/vnd.oasis.opendocument.text"))
        mime.removeFormat(mime_format);
  }
  const std::string bytes = encode_text(text, format, page);
  mime.setData(QString::fromLatin1(kEncodedClipboardMime), envelope(bytes, format, page));
  mime.setData(QStringLiteral("text/plain;charset=%1").arg(charset(format)),
               QByteArray(bytes.data(), static_cast<qsizetype>(bytes.size())));
}

std::optional<ClipboardText> read_clipboard_text(const QMimeData& mime,
                                                 ClipboardTextFormat format,
                                                 int code_page) {
  const auto configured_page = resolved_code_page(code_page);
  std::optional<EncodedText> encoded;
  try { encoded = read_envelope(mime); }
  catch (const std::exception&) {
    // A malformed private representation does not hide a valid public format.
  }

  if (format != ClipboardTextFormat::kAutoDetect) {
    if (format == ClipboardTextFormat::kUnicode && has_unicode_text(mime))
      return ClipboardText{unicode_text(mime), format,
                           static_cast<int>(configured_page)};
    QByteArray bytes;
    if (encoded) bytes = encoded->payload;
    else if (const auto declared = declared_plain_text(mime)) bytes = declared->second;
    else if (mime.hasFormat(QStringLiteral("text/plain")))
      bytes = mime.data(QStringLiteral("text/plain"));
    else return std::nullopt;
    return ClipboardText{decode_bytes(bytes, format, configured_page), format,
                         static_cast<int>(configured_page)};
  }

  if (encoded) {
    return ClipboardText{decode_bytes(encoded->payload, encoded->format,
                                      encoded->code_page),
                         encoded->format, static_cast<int>(encoded->code_page)};
  }
  if (has_unicode_text(mime))
    return ClipboardText{unicode_text(mime), ClipboardTextFormat::kUnicode,
                          static_cast<int>(configured_page)};
  if (const auto declared = declared_plain_text(mime)) {
    return ClipboardText{decode_bytes(declared->second, declared->first,
                                      configured_page),
                         declared->first, static_cast<int>(configured_page)};
  }
  if (!mime.hasFormat(QStringLiteral("text/plain"))) return std::nullopt;
  const QByteArray bytes = mime.data(QStringLiteral("text/plain"));
  const auto detection = core::detect_text_encoding(std::string_view(
      bytes.constData(), static_cast<std::size_t>(bytes.size())));
  if (detection.candidates.empty())
    throw std::invalid_argument("Clipboard text encoding could not be detected");
  const auto detected = clipboard_format(detection.candidates.front());
  return ClipboardText{decode_bytes(bytes, detected, configured_page), detected,
                       static_cast<int>(configured_page)};
}

}  // namespace jwpqt::qt
