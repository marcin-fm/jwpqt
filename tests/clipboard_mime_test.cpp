// SPDX-License-Identifier: GPL-2.0-or-later

#include "clipboard_mime.h"

#include <iostream>
#include <stdexcept>

#include <QCoreApplication>
#include <QMimeData>

namespace {

void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

void test_round_trips() {
  using namespace jwpqt::qt;
  const QString text = QString(QChar(0xfeff)) + QStringLiteral("ABC\n\u3042");
  for (int value = 6; value <= 13; ++value) {
    const auto format = static_cast<ClipboardTextFormat>(value);
    const QString sample = format == ClipboardTextFormat::kUnicode ||
                                   format == ClipboardTextFormat::kUtf7 ||
                                   format == ClipboardTextFormat::kUtf8
                               ? text
                               : QStringLiteral("ABC\n\u3042");
    QMimeData mime;
    mime.setText(sample);
    mime.setHtml(QStringLiteral("<b>ABC</b>"));
    add_clipboard_text_formats(mime, sample, format, 1252, false);
    const auto exact = read_clipboard_text(mime, format, 1252);
    const auto automatic = read_clipboard_text(mime, ClipboardTextFormat::kAutoDetect, 1252);
    if (!(exact && automatic && exact->text == sample && automatic->text == sample &&
          exact->format == format && automatic->format == format))
      throw std::runtime_error("Clipboard format " + std::to_string(value) +
                               " did not round-trip exactly and automatically");
    require(mime.hasText() && mime.hasHtml(), "Unicode export removed public formats");
    QMimeData public_only;
    for (const QString& mime_format : mime.formats()) {
      if (mime_format.startsWith(QStringLiteral("text/plain;charset="),
                                 Qt::CaseInsensitive)) {
        public_only.setData(mime_format, mime.data(mime_format));
      }
    }
    const auto external = read_clipboard_text(
        public_only, ClipboardTextFormat::kAutoDetect, 1252);
    require(external && external->text == sample && external->format == format,
            "Declared public clipboard charset did not round-trip");
  }
}

void test_omission_and_fallback() {
  using namespace jwpqt::qt;
  QMimeData mime;
  mime.setText(QStringLiteral("plain"));
  mime.setHtml(QStringLiteral("<b>plain</b>"));
  mime.setData(QStringLiteral("application/vnd.oasis.opendocument.text"), "rich");
  add_clipboard_text_formats(mime, QStringLiteral("plain"),
                             ClipboardTextFormat::kShiftJis, 1252, true);
  require(!mime.hasFormat(QStringLiteral("text/plain")) && !mime.hasHtml() &&
              !mime.hasFormat(QStringLiteral("application/vnd.oasis.opendocument.text")) &&
              mime.hasFormat(QString::fromLatin1(kEncodedClipboardMime)),
          "Unicode omission retained a public Unicode representation");
  require(read_clipboard_text(mime, ClipboardTextFormat::kAutoDetect, 1252)->text ==
              QStringLiteral("plain"),
          "Automatic import did not use private encoded text");

  QMimeData malformed;
  malformed.setData(QString::fromLatin1(kEncodedClipboardMime), "bad");
  malformed.setText(QStringLiteral("fallback\U0001f600"));
  const auto fallback = read_clipboard_text(
      malformed, ClipboardTextFormat::kAutoDetect, 1252);
  require(fallback && fallback->text == QStringLiteral("fallback\U0001f600") &&
              fallback->format == ClipboardTextFormat::kUnicode,
           "Malformed private text hid the Unicode fallback");

  QMimeData unrepresentable;
  unrepresentable.setText(QString::fromUcs4(U"\U0001f600"));
  unrepresentable.setHtml(QStringLiteral("<b>unicode</b>"));
  bool rejected = false;
  try {
    add_clipboard_text_formats(unrepresentable,
                               QString::fromUcs4(U"\U0001f600"),
                               ClipboardTextFormat::kShiftJis, 1252, true);
  } catch (const std::exception&) {
    rejected = true;
  }
  require(rejected && !unrepresentable.hasText() &&
              !unrepresentable.hasHtml(),
          "Failed encoded export leaked an omitted Unicode representation");

  QMimeData invalid_unicode;
  invalid_unicode.setText(QString(QChar(0xd800)));
  rejected = false;
  try {
    (void)read_clipboard_text(
        invalid_unicode, ClipboardTextFormat::kAutoDetect, 1252);
  } catch (const std::exception&) {
    rejected = true;
  }
  require(rejected, "Malformed clipboard Unicode was accepted");
}

void test_external_formats() {
  using namespace jwpqt::qt;
  QMimeData declared;
  declared.setData(QStringLiteral("text/plain;charset=Shift_JIS"),
                   QByteArray::fromHex("82a00a414243"));
  const auto decoded = read_clipboard_text(
      declared, ClipboardTextFormat::kAutoDetect, 1252);
  if (!(decoded && decoded->text == QStringLiteral("\u3042\nABC") &&
        decoded->format == ClipboardTextFormat::kShiftJis))
    throw std::runtime_error("Declared external charset was not decoded: " +
                             (decoded ? decoded->text.toUtf8().toStdString() : "none"));

  QMimeData detected;
  detected.setData(QStringLiteral("text/plain"), QByteArray::fromHex("e38182"));
  const auto automatic = read_clipboard_text(
      detected, ClipboardTextFormat::kAutoDetect, 1252);
  require(automatic && automatic->text == QStringLiteral("\u3042") &&
              automatic->format == ClipboardTextFormat::kUnicode,
          "Public Unicode text was not preferred");

  QMimeData cp1251;
  cp1251.setData(QStringLiteral("text/plain"), QByteArray::fromHex("80"));
  const auto legacy = read_clipboard_text(
      cp1251, ClipboardTextFormat::kShiftJis, 1251);
  require(legacy && legacy->text == QStringLiteral("\u0402") && legacy->code_page == 1251,
          "Configured legacy code page was not used");
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication application(argc, argv);
  try {
    test_round_trips();
    test_omission_and_fallback();
    test_external_formats();
    std::cout << "All clipboard MIME tests passed\n";
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
