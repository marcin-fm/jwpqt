// SPDX-License-Identifier: GPL-2.0-or-later

#include "japanese_fonts.h"
#include "jwpqt/core/character_font.h"
#include "jwpqt/core/jis_unicode.h"
#include "jwpqt/core/legacy_code_page.h"
#include "jwpqt/core/raster_font.h"
#include "jwpqt/core/jwp_text_codec.h"
#include "text_bridge.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QRawFont>
#include <QStyle>
#include <QVariant>
#include <QWidget>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <map>
#include <stdexcept>
#include <utility>

namespace jwpqt::qt {
namespace {

constexpr char kRole[] = "_jwpqt_font_role";
constexpr char kOriginal[] = "_jwpqt_font_original";
constexpr char kStrip[] = "_jwpqt_font_strip";
constexpr char kSettings[] = "_jwpqt_font_settings";
constexpr char kBitmap[] = "_jwpqt_clipboard_bitmap";
constexpr char kAscii[] = "_jwpqt_ascii_face";

QString ascii_face(const QFont& font, bool raw_bytes = false) {
  const auto raw = QRawFont::fromFont(font);
  if (!raw.isValid()) throw std::runtime_error("Could not open native ASCII font");
  auto* app = QCoreApplication::instance();
  auto cache = app->property("_jwpqt_ascii_faces").toMap();
  for (const auto& entry : cache) {
    const auto values = entry.toList();
    if (values[3].value<QRawFont>() == raw &&
        values[0].toString().startsWith(QStringLiteral("JwpqtAscii-Bytes-")) == raw_bytes)
      return values[0].toString();
  }
  std::map<std::string, std::string> tables;
  QCryptographicHash hash(QCryptographicHash::Sha256);
  if (raw_bytes) hash.addData(QByteArrayLiteral("raw-bytes"));
  for (const char* tag : {"head", "hhea", "maxp", "hmtx", "cmap", "loca", "glyf", "CFF ", "name", "OS/2", "post",
                          "fpgm", "prep", "cvt ", "gasp", "GDEF", "GPOS", "GSUB", "kern", "BASE", "VORG",
                          "fvar", "gvar", "avar", "cvar", "HVAR", "VVAR", "MVAR", "STAT", "CFF2"}) {
    const auto data = raw.fontTable(tag);
    if (data.isEmpty()) continue;
    hash.addData(QByteArray(tag, 4)); hash.addData(data);
    tables.emplace(tag, data.toStdString());
  }
  const auto digest = QString::fromLatin1(hash.result().toHex());
  if (cache.contains(digest)) return cache[digest].toList()[0].toString();
  qlonglong total = 0;
  for (const auto& value : cache) total += value.toList()[1].toLongLong();
  if (cache.size() >= 16) throw std::runtime_error("Private ASCII font count exceeded");
  std::map<char32_t, std::uint32_t> mappings;
  const auto add = [&](char32_t scalar) {
    const auto ids = raw.glyphIndexesForString(to_qstring(std::u32string(1, scalar)));
    if (!ids.empty() && ids.front()) mappings.emplace(scalar, ids.front());
  };
  for (char32_t scalar = 0x20; scalar < 0x7f; ++scalar) add(scalar);
  for (int page = 1250; page <= 1258; ++page) for (int byte = 128; byte <= 255; ++byte) {
    const auto scalar = core::legacy_byte_to_unicode(static_cast<std::uint8_t>(byte), static_cast<core::LegacyCodePage>(page));
    if (scalar && (raw_bytes || !core::unicode_to_jis_x0208(*scalar))) add(*scalar);
  }
  const auto family = QStringLiteral("JwpqtAscii-") + (raw_bytes ? QStringLiteral("Bytes-") : QString{}) + digest.left(32);
  const auto face = core::make_character_font(std::move(tables), mappings, family.toStdString());
  if (total + static_cast<qlonglong>(face.size()) > 64 * 1024 * 1024) throw std::runtime_error("Private ASCII font memory exceeded");
  const int id = QFontDatabase::addApplicationFontFromData(QByteArray::fromStdString(face));
  if (id < 0) throw std::runtime_error("Native font engine rejected ASCII face");
  if (!QFontDatabase::applicationFontFamilies(id).contains(family)) {
    QFontDatabase::removeApplicationFont(id);
    throw std::runtime_error("Private ASCII font family mismatch");
  }
  cache.insert(digest, QVariantList{family, static_cast<qlonglong>(face.size()), QVariant::fromValue(font), QVariant::fromValue(raw)});
  app->setProperty("_jwpqt_ascii_faces", cache);
  return family;
}

QVariantList raster_face(const QString& path) {
  if (path.contains(QChar(0)) || to_qstring(from_qstring(path)) != path || !QFileInfo(path).isFile())
    throw core::RasterFontError("Raster font is not a regular file");
  QFile input(path);
  if (!input.open(QIODevice::ReadOnly)) throw core::RasterFontError(input.errorString().toStdString());
  const auto data = input.read(8 * 1024 * 1024 + 1);
  if (input.error() != QFileDevice::NoError || !input.atEnd() || data.size() > 8 * 1024 * 1024)
    throw core::RasterFontError("Could not read bounded raster font");
  const auto digest = QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
  auto* application = QCoreApplication::instance();
  if (!application) throw core::RasterFontError("Raster fonts require a native application");
  auto cache = application->property("_jwpqt_raster_faces").toMap();
  if (cache.contains(digest)) return cache[digest].toList();
  qlonglong total = 0;
  for (const auto& value : cache) total += value.toList()[2].toLongLong();
  if (cache.size() >= 16) throw core::RasterFontError("Private raster font count exceeded");
  const core::RasterFont source(std::string_view(data.constData(), static_cast<std::size_t>(data.size())));
  const auto family = QStringLiteral("JwpqtRaster-") + digest.left(32);
  // Qt owns these application-local faces and releases them with its font
  // database. Keeping their identities stable avoids invalidating live views.
  const auto face = source.native_face(family.toStdString());
  if (total + static_cast<qlonglong>(face.size()) > 64 * 1024 * 1024)
    throw core::RasterFontError("Private raster font memory budget exceeded");
  const int id = QFontDatabase::addApplicationFontFromData(QByteArray::fromStdString(face));
  if (id < 0) throw core::RasterFontError("Native font engine rejected raster font");
  const auto families = QFontDatabase::applicationFontFamilies(id);
  if (!families.contains(family)) {
    QFontDatabase::removeApplicationFont(id);
    throw core::RasterFontError("Native raster font family mismatch");
  }
  const QVariantList result{family, source.height(), static_cast<qlonglong>(face.size())};
  cache.insert(digest, result);
  application->setProperty("_jwpqt_raster_faces", cache);
  return result;
}

}  // namespace

void assign_japanese_font(QWidget& widget, JapaneseFontRole role, bool horizontal_strip) {
  const int index = static_cast<int>(role);
  if (index < 0 || index >= static_cast<int>(JapaneseFontRole::kCount)) return;
  if (!widget.property(kOriginal).isValid()) widget.setProperty(kOriginal, QVariant::fromValue(widget.font()));
  widget.setProperty(kRole, index);
  widget.setProperty(kStrip, horizontal_strip);
  widget.setFont(japanese_font(widget, role));
  if (horizontal_strip) {
    widget.setFixedHeight(widget.fontMetrics().height() + 12 +
                          widget.style()->pixelMetric(QStyle::PM_ScrollBarExtent));
  }
}

QFont japanese_font(const QWidget& widget, JapaneseFontRole role) {
  const int index = static_cast<int>(role);
  QFont font = widget.property(kOriginal).isValid() ? widget.property(kOriginal).value<QFont>() : widget.font();
  if (index < 0 || index >= static_cast<int>(JapaneseFontRole::kCount)) return font;
  for (const QWidget* owner = &widget; owner; owner = owner->parentWidget()) {
    const auto settings = owner->property(kSettings).toList();
    if (settings.size() != static_cast<int>(JapaneseFontRole::kCount)) continue;
    const auto setting = settings[index].toList();
    if (setting.size() != 2) continue;
    if (!setting[0].toString().isEmpty()) font.setFamily(setting[0].toString());
    if (font.family().startsWith(QStringLiteral("JwpqtRaster-"))) font.setHintingPreference(QFont::PreferNoHinting);
    if (role != JapaneseFontRole::kBig) font.setPixelSize(setting[1].toInt());
    const auto ascii = owner->property(kAscii).toString();
    if (!ascii.isEmpty()) font.setFamilies({ascii, font.family()});
    break;
  }
  return font;
}

ClipboardBitmapOptions clipboard_bitmap_options(const QWidget& widget) {
  for (const QWidget* owner = &widget; owner; owner = owner->parentWidget())
    if (owner->property(kBitmap).isValid()) {
      const auto values = owner->property(kBitmap).toList();
      if (values.size() == 3) return {values[0].toBool(), values[1].toBool(), values[2].toBool()};
    }
  return {};
}

QFont ensure_ascii_font(QFont font) {
  if (font.families().isEmpty() || !font.families().front().startsWith(QStringLiteral("JwpqtAscii-")))
    font.setFamilies({ascii_face(QFontDatabase::systemFont(QFontDatabase::GeneralFont)), font.family()});
  return font;
}

QFont jwp_representation_font(QFont font, int kind) {
  if (kind == 0) return font;
  auto families = ensure_ascii_font(font).families();
  const QString ascii = families.takeFirst();
  if (kind == 1) {
    const auto cache = QCoreApplication::instance()->property("_jwpqt_ascii_faces").toMap();
    bool found = false;
    for (const auto& entry : cache) {
      const auto values = entry.toList();
      if (values[0].toString() != ascii) continue;
      families.prepend(ascii_face(values[2].value<QFont>(), true)); found = true; break;
    }
    if (!found) throw std::runtime_error("Missing original ASCII font identity");
  } else if (kind != 2) throw std::invalid_argument("Invalid JWP character representation");
  font.setFamilies(families);
  return font;
}

void apply_jwp_character_fonts(QTextDocument& document, const core::JwpDocument& source,
                               core::LegacyCodePage code_page) {
  if (document.blockCount() != static_cast<int>(source.paragraphs.size()))
    throw std::invalid_argument("JWP font paragraph count mismatch");
  // Validate the entire mapping before publishing any display metadata.
  auto block = document.begin();
  for (const auto& paragraph : source.paragraphs) {
    if (block.text() != to_qstring(core::decode_jwp_text(paragraph.text, code_page)))
      throw std::invalid_argument("JWP font text mismatch");
    block = block.next();
  }
  const auto bytes = jwp_representation_font(document.defaultFont(), 1).families();
  const auto japanese = jwp_representation_font(document.defaultFont(), 2).families();
  block = document.begin();
  for (const auto& paragraph : source.paragraphs) {
    const auto decoded = core::decode_jwp_text(paragraph.text, code_page);
    int position = block.position(), start = position, previous = 0;
    const auto publish = [&](int end) {
      if (start == end) return;
      QTextCursor cursor(&document); cursor.setPosition(start); cursor.setPosition(end, QTextCursor::KeepAnchor);
      QTextCharFormat format;
      format.setProperty(kJwpCharacterKind, previous);
      format.setFontFamilies(previous == 1 ? bytes : japanese);
      cursor.mergeCharFormat(format);
    };
    for (std::size_t i = 0; i < paragraph.text.size(); ++i) {
      const int kind = paragraph.text[i] < 0x100 ? 1 : 2;
      if (kind != previous) { publish(position); start = position; previous = kind; }
      position += decoded[i] > 0xffff ? 2 : 1;
    }
    publish(position); block = block.next();
  }
}

QFont japanese_print_font(QFont base, const JapaneseFontSetting& setting, const QString& directory) {
  const auto ascii = base.families().isEmpty() ? QString{} : base.families().front();
  if (!setting.automatic && !setting.family.isEmpty()) {
    if (setting.family.endsWith(QStringLiteral(".f00"), Qt::CaseInsensitive)) {
      const auto root = directory.isEmpty() ? QDir::currentPath() : directory;
      const auto path = QDir::isAbsolutePath(setting.family) ? setting.family : root + QLatin1Char('/') + setting.family;
      base.setFamily(raster_face(path)[0].toString());
      base.setHintingPreference(QFont::PreferNoHinting);
    } else base.setFamily(setting.family);
  }
  base.setPointSizeF(setting.size / 10.0);
  if (ascii.startsWith(QStringLiteral("JwpqtAscii-"))) {
    auto families = base.families(); families.removeAll(ascii); families.prepend(ascii); base.setFamilies(families);
  }
  return ensure_ascii_font(base);
}

QStringList set_japanese_fonts(QWidget& owner, const ApplicationSettings& settings, const QString& directory) {
  QVariantList resolved;
  QStringList warnings;
  const auto families = QFontDatabase::families();
  constexpr std::size_t system = static_cast<std::size_t>(JapaneseFontRole::kSystem);
  constexpr std::size_t edit = static_cast<std::size_t>(JapaneseFontRole::kEdit);
  constexpr std::size_t file = static_cast<std::size_t>(JapaneseFontRole::kFile);
  for (std::size_t role = 0; role < settings.fonts.size(); ++role) {
    std::size_t base = role;
    QString family;
    int raster_height = 0;
    for (;;) {
      family = settings.fonts[base].family;
      const bool automatic = settings.fonts[base].automatic &&
          !(base == static_cast<std::size_t>(JapaneseFontRole::kBitmap) && settings.vertical_clipboard_bitmap);
      bool unavailable = !family.isEmpty() && !families.contains(family, Qt::CaseInsensitive);
      raster_height = 0;
      if (family.endsWith(QStringLiteral(".f00"), Qt::CaseInsensitive) &&
          (base == system || !automatic)) {
        try {
          const auto root = directory.isEmpty() ? QDir::currentPath() : directory;
          const auto face = raster_face(QDir::isAbsolutePath(family) ? family : root + QLatin1Char('/') + family);
          family = face[0].toString(); raster_height = face[1].toInt(); unavailable = false;
        } catch (const std::exception& error) {
          const auto warning = QObject::tr("Raster font '%1': %2").arg(family, QString::fromUtf8(error.what()));
          if (!warnings.contains(warning)) warnings.push_back(warning);
        }
      }
      if (unavailable && (base == system || !automatic)) {
        const auto warning = QObject::tr("Font '%1' is unavailable; an inherited native fallback is used.").arg(family);
        if (!warnings.contains(warning)) warnings.push_back(warning);
      }
      if (base == system || (!automatic && !unavailable)) {
        if (unavailable) family.clear();
        break;
      }
      if (base == static_cast<std::size_t>(JapaneseFontRole::kList) ||
          base == static_cast<std::size_t>(JapaneseFontRole::kKanjiBar)) base = edit;
      else if (base == static_cast<std::size_t>(JapaneseFontRole::kBig) ||
               base == static_cast<std::size_t>(JapaneseFontRole::kBitmap)) base = file;
      else base = system;
    }
    const int size = role == static_cast<std::size_t>(JapaneseFontRole::kTable)
                         ? 16 : raster_height ? raster_height : settings.fonts[base].size;
    resolved.push_back(QVariantList{family, size});
  }
  owner.setProperty(kSettings, resolved);
  owner.setProperty(kBitmap, QVariantList{!settings.omit_clipboard_bitmap,
                                        settings.vertical_clipboard_bitmap, settings.color_clipboard_bitmap});
  QString ascii;
  try {
    QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    if (!settings.ascii_font.family.isEmpty()) {
      if (families.contains(settings.ascii_font.family, Qt::CaseInsensitive)) font.setFamily(settings.ascii_font.family);
      else warnings.push_back(QObject::tr("ASCII font '%1' is unavailable; the native default is used.").arg(settings.ascii_font.family));
    }
    ascii = ascii_face(font);
  } catch (const std::exception& error) {
    warnings.push_back(QObject::tr("Independent ASCII font unavailable: %1").arg(QString::fromUtf8(error.what())));
  }
  owner.setProperty(kAscii, ascii);
  for (auto* widget : owner.findChildren<QWidget*>()) {
    if (!widget->property(kRole).isValid()) continue;
    assign_japanese_font(*widget, static_cast<JapaneseFontRole>(widget->property(kRole).toInt()),
                         widget->property(kStrip).toBool());
  }
  return warnings;
}

}  // namespace jwpqt::qt
