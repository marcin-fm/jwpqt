// SPDX-License-Identifier: GPL-2.0-or-later
#include "jwp_text_drawing.h"

#include <cmath>
#include <optional>
#include <stdexcept>
#include <vector>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFontDatabase>
#include <QGlyphRun>
#include <QPainter>
#include <QScopeGuard>
#include <QTextBoundaryFinder>
#include <QTextLayout>
#include <QtEndian>
#include <QVariant>
#include "jwpqt/core/character_font.h"
#include "jwpqt/core/jis_unicode.h"
#include "jwpqt/core/jwp_text_codec.h"
#include "jwpqt/core/raster_font.h"
#include "text_bridge.h"

namespace jwpqt::qt {

QRawFont jwp_vertical_font(const QRawFont& original) {
  if (!original.isValid() || !original.fontTable("JWPV").isEmpty()) return {};
  auto* app = QCoreApplication::instance();
  if (!app) throw std::runtime_error("Vertical fonts require a native application");
  auto cache = app->property("_jwpqt_vertical_faces").toList();
  qlonglong total = 0;
  for (const auto& value : cache) {
    const auto entry = value.toList();
    bool matches = false;
    for (const auto& identity : entry[0].toList()) if (identity.value<QRawFont>() == original) matches = true;
    if (matches) {
      auto font = entry[1].value<QRawFont>();
      if (font.isValid()) font.setPixelSize(original.pixelSize());
      return font;
    }
    total += entry[2].toLongLong();
  }
  const auto maxp = original.fontTable("maxp"), gsub = original.fontTable("GSUB");
  if (maxp.size() < 6) throw std::runtime_error("Invalid vertical font glyph table");
  const auto substitutions = core::vertical_glyph_substitutions(gsub.toStdString(), qFromBigEndian<quint16>(maxp.constData() + 4));
  QRawFont font;
  qlonglong size = 0;
  QByteArray digest;
  if (substitutions) {
    std::map<std::string, std::string> tables;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    for (const char* tag : {"head", "hhea", "maxp", "hmtx", "cmap", "loca", "glyf", "CFF ", "name", "OS/2", "post",
                            "fpgm", "prep", "cvt ", "gasp", "GDEF", "GPOS", "GSUB", "kern", "BASE", "VORG",
                            "fvar", "gvar", "avar", "cvar", "HVAR", "VVAR", "MVAR", "STAT", "CFF2"}) {
      const auto data = original.fontTable(tag);
      if (data.isEmpty()) continue;
      hash.addData(QByteArray(tag, 4)); hash.addData(data);
      tables.emplace(tag, data.toStdString());
    }
    hash.addData(QByteArray::number(original.weight()) + ':' + QByteArray::number(original.style()) + ':' +
                 QByteArray::number(original.hintingPreference()));
    digest = hash.result().toHex();
    for (auto& value : cache) {
      auto entry = value.toList();
      if (entry[3].toByteArray() != digest) continue;
      auto identities = entry[0].toList();
      if (identities.size() < 64) identities.push_back(QVariant::fromValue(original));
      entry[0] = identities; value = entry;
      app->setProperty("_jwpqt_vertical_faces", cache);
      font = entry[1].value<QRawFont>(); font.setPixelSize(original.pixelSize());
      return font;
    }
    if (cache.size() >= 16) throw std::runtime_error("Private vertical font count exceeded");
    std::map<char32_t, std::uint32_t> mappings;
    const auto add = [&](char32_t scalar) {
      const auto glyphs = original.glyphIndexesForString(to_qstring(std::u32string(1, scalar)));
      if (glyphs.empty() || !glyphs.front()) return;
      const auto found = substitutions->find(glyphs.front());
      const auto target = found == substitutions->end() ? glyphs.front() : found->second;
      if (!target) throw std::runtime_error("Vertical font replaces a character with a missing glyph");
      mappings.emplace(scalar, target);
    };
    for (unsigned code = 0x2121; code <= 0x747e; ++code)
      if (const auto scalar = core::jis_x0208_to_unicode(code)) add(*scalar);
    // Preserve combining marks in unrestricted Unicode clusters as well.
    for (char32_t scalar = 0x300; scalar <= 0x36f; ++scalar) add(scalar);
    add(0x3099); add(0x309a);
    std::string metadata;
    const auto word = [&](std::uint16_t value) { metadata += static_cast<char>(value >> 8); metadata += static_cast<char>(value); };
    word(1); word(static_cast<std::uint16_t>(substitutions->size()));
    for (const auto& mapping : *substitutions) { word(mapping.first); word(mapping.second); }
    tables["JWPX"] = std::move(metadata);
    const auto family = QStringLiteral("JwpqtVertical-") + QString::fromLatin1(digest.left(32));
    const auto bytes = core::make_character_font(std::move(tables), mappings, family.toStdString(), true);
    size = static_cast<qlonglong>(bytes.size());
    if (total + size > 64 * 1024 * 1024) throw std::runtime_error("Private vertical font memory exceeded");
    const int id = QFontDatabase::addApplicationFontFromData(QByteArray::fromStdString(bytes));
    if (id < 0) throw std::runtime_error("Native font engine rejected vertical face");
    QFont native(family); native.setPixelSize(16); native.setHintingPreference(original.hintingPreference());
    native.setWeight(static_cast<QFont::Weight>(original.weight())); native.setStyle(original.style());
    font = QRawFont::fromFont(native);
    if (!font.isValid() || font.familyName() != family) {
      QFontDatabase::removeApplicationFont(id);
      throw std::runtime_error("Native font engine rejected vertical face");
    }
  }
  if (cache.size() >= 16) throw std::runtime_error("Private vertical font count exceeded");
  cache.push_back(QVariantList{QVariantList{QVariant::fromValue(original)}, QVariant::fromValue(font), size, digest});
  app->setProperty("_jwpqt_vertical_faces", cache);
  if (font.isValid()) font.setPixelSize(original.pixelSize());
  return font;
}

void draw_jwp_text_layout(QPainter& painter, QTextLayout& layout, const QString& text,
                          const QPointF& origin, bool vertical,
                          core::LegacyCodePage code_page,
                          const std::function<QColor(int)>& foreground,
                          const std::function<int(int)>& representation) {
  // TrueType vert faces rotate all Japanese glyphs. Raster/fallback faces use
  // the source exception list and (for raster fonts) ink-position corrections.
  std::vector<std::pair<QRawFont, QRawFont>> prepared;
  const auto alternate = [&](const QRawFont& raw) {
    for (const auto& entry : prepared) if (entry.first == raw) return entry.second;
    auto font = jwp_vertical_font(raw);
    prepared.emplace_back(raw, font);
    return font;
  };
  QTextBoundaryFinder boundaries(QTextBoundaryFinder::Grapheme, text);
  const auto jis_code = [&](const QString& cluster, int at) {
    const auto scalars = from_qstring(cluster);
    const int kind = representation ? representation(at) : 0;
    if (kind == 1) return std::optional<core::JisCode>{0};
    if (kind == 2) return scalars.empty() ? std::optional<core::JisCode>{} : core::unicode_to_jis_x0208(scalars[0]);
    if (kind) throw std::invalid_argument("Invalid JWP glyph representation");
    return scalars.empty() ? std::optional<core::JisCode>{}
                          : core::unicode_to_jwp_code(scalars[0], code_page);
  };
  const auto rotates = [&](const QString& cluster, int at) {
    const auto jis = jis_code(cluster, at);
    return jis && core::jwp_glyph_rotates(*jis);
  };
  for (int number = 0; number < layout.lineCount(); ++number) {
    const auto line = layout.lineAt(number);
    if (painter.hasClipping()) {
      const qreal middle = line.rect().translated(origin).center().y();
      if (middle < painter.clipBoundingRect().top() || middle >= painter.clipBoundingRect().bottom()) continue;
    }
    if (!vertical && !foreground) { line.draw(&painter, origin); continue; }
    const int end = line.textStart() + line.textLength();
    for (int at = line.textStart(); at < end;) {
      boundaries.setPosition(at);
      int next = boundaries.toNextBoundary();
      if (next <= at || next > end) next = end;
      if (representation) for (int p = at + 1; p < next; ++p)
        if (representation(p) != representation(at)) { next = p; break; }
      const QColor color = foreground ? foreground(at) : painter.pen().color();
      painter.save();
      const auto restore = qScopeGuard([&] { painter.restore(); });
      painter.setPen(color);
      auto runs = line.glyphRuns(at, next - at);
      bool true_type = false;
      const auto code = jis_code(text.mid(at, next - at), at);
      if (vertical && code && *code >= 0x2100) for (auto& run : runs) {
        const auto font = alternate(run.rawFont());
        if (!font.isValid()) continue;
        const auto metadata = font.fontTable("JWPX");
        if (metadata.size() < 4 || qFromBigEndian<quint16>(metadata.constData()) != 1 ||
            metadata.size() != 4 + 4 * qFromBigEndian<quint16>(metadata.constData() + 2))
          throw std::runtime_error("Invalid private vertical substitutions");
        auto glyphs = run.glyphIndexes();
        for (auto& glyph : glyphs) {
          int low = 0, high = (metadata.size() - 4) / 4;
          while (low < high) {
            const int mid = low + (high - low) / 2;
            if (qFromBigEndian<quint16>(metadata.constData() + 4 + 4 * mid) < glyph) low = mid + 1;
            else high = mid;
          }
          if (4 + 4 * low < metadata.size() && qFromBigEndian<quint16>(metadata.constData() + 4 + 4 * low) == glyph)
            glyph = qFromBigEndian<quint16>(metadata.constData() + 6 + 4 * low);
        }
        run.setRawFont(font); run.setGlyphIndexes(glyphs); true_type = true;
      }
      if (vertical && (true_type || rotates(text.mid(at, next - at), at))) {
        if (!runs.empty()) {
          const auto raw = runs.front().rawFont();
          const auto metadata = raw.fontTable("JWPV");
          if (!metadata.isEmpty()) {
            const auto word = [&](int offset) {
              if (offset < 0 || offset + 2 > metadata.size())
                throw core::RasterFontError("Invalid native raster vertical metadata");
              return qFromBigEndian<quint16>(metadata.constData() + offset);
            };
            const int width = word(2), height = word(4), count = word(6);
            if (word(0) != 1 || width < 8 || width > 64 || width != height ||
                count > 256 || metadata.size() != 8 + count * 6)
              throw core::RasterFontError("Unsupported native raster vertical geometry");
            for (int i = 0; i < count; ++i) {
              const int x_offset = word(10 + i * 6);
              const auto encoded_y = word(12 + i * 6);
              const int y_offset = encoded_y < 32768 ? encoded_y : static_cast<int>(encoded_y) - 65536;
              if (x_offset > width || y_offset < -height || y_offset > height)
                throw core::RasterFontError("Invalid native raster vertical offset");
              if (word(8 + i * 6) == *code) {
                const qreal scale = raw.pixelSize() / height;
                painter.translate(-x_offset * scale, -y_offset * scale);
              }
            }
          }
        }
        const qreal x = line.cursorToX(at);
        const qreal width = std::abs(line.cursorToX(next) - x);
        const QPointF center = origin + QPointF(x + width / 2, line.y() + line.height() / 2);
        painter.translate(center); painter.rotate(-90); painter.translate(-center);
      } else {
        while (next < end) {
          boundaries.setPosition(next);
          const int after = boundaries.toNextBoundary();
          if (after <= next || after > end) break;
          const auto following = vertical ? jis_code(text.mid(next, after - next), next) : std::optional<core::JisCode>{};
          if ((following && *following >= 0x2100) ||
              (foreground && foreground(next) != color)) break;
          next = after;
        }
        runs = line.glyphRuns(at, next - at);
      }
      for (const auto& run : runs) painter.drawGlyphRun(origin, run);
      at = next;
    }
  }
}

}  // namespace jwpqt::qt
