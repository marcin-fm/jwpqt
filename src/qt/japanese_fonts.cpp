// SPDX-License-Identifier: GPL-2.0-or-later

#include "japanese_fonts.h"
#include "jwpqt/core/raster_font.h"
#include "text_bridge.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QStyle>
#include <QVariant>
#include <QWidget>

namespace jwpqt::qt {
namespace {

constexpr char kRole[] = "_jwpqt_font_role";
constexpr char kOriginal[] = "_jwpqt_font_original";
constexpr char kStrip[] = "_jwpqt_font_strip";
constexpr char kSettings[] = "_jwpqt_font_settings";
constexpr char kBitmap[] = "_jwpqt_clipboard_bitmap";

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
    break;
  }
  return font;
}

bool clipboard_bitmap_enabled(const QWidget& widget) {
  for (const QWidget* owner = &widget; owner; owner = owner->parentWidget())
    if (owner->property(kBitmap).isValid()) return owner->property(kBitmap).toBool();
  return true;
}

QFont japanese_print_font(QFont base, const JapaneseFontSetting& setting, const QString& directory) {
  if (!setting.automatic && !setting.family.isEmpty()) {
    if (setting.family.endsWith(QStringLiteral(".f00"), Qt::CaseInsensitive)) {
      const auto root = directory.isEmpty() ? QDir::currentPath() : directory;
      const auto path = QDir::isAbsolutePath(setting.family) ? setting.family : root + QLatin1Char('/') + setting.family;
      base.setFamily(raster_face(path)[0].toString());
      base.setHintingPreference(QFont::PreferNoHinting);
    } else base.setFamily(setting.family);
  }
  base.setPointSizeF(setting.size / 10.0);
  return base;
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
      bool unavailable = !family.isEmpty() && !families.contains(family, Qt::CaseInsensitive);
      raster_height = 0;
      if (family.endsWith(QStringLiteral(".f00"), Qt::CaseInsensitive) &&
          (base == system || !settings.fonts[base].automatic)) {
        try {
          const auto root = directory.isEmpty() ? QDir::currentPath() : directory;
          const auto face = raster_face(QDir::isAbsolutePath(family) ? family : root + QLatin1Char('/') + family);
          family = face[0].toString(); raster_height = face[1].toInt(); unavailable = false;
        } catch (const std::exception& error) {
          const auto warning = QObject::tr("Raster font '%1': %2").arg(family, QString::fromUtf8(error.what()));
          if (!warnings.contains(warning)) warnings.push_back(warning);
        }
      }
      if (unavailable && (base == system || !settings.fonts[base].automatic)) {
        const auto warning = QObject::tr("Font '%1' is unavailable; an inherited native fallback is used.").arg(family);
        if (!warnings.contains(warning)) warnings.push_back(warning);
      }
      if (base == system || (!settings.fonts[base].automatic && !unavailable)) {
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
  owner.setProperty(kBitmap, !settings.omit_clipboard_bitmap);
  for (auto* widget : owner.findChildren<QWidget*>()) {
    if (!widget->property(kRole).isValid()) continue;
    assign_japanese_font(*widget, static_cast<JapaneseFontRole>(widget->property(kRole).toInt()),
                         widget->property(kStrip).toBool());
  }
  return warnings;
}

}  // namespace jwpqt::qt
