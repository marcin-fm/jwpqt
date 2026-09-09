// SPDX-License-Identifier: GPL-2.0-or-later
#include "jwp_text_drawing.h"

#include <cmath>
#include <optional>
#include <QGlyphRun>
#include <QPainter>
#include <QScopeGuard>
#include <QTextBoundaryFinder>
#include <QTextLayout>
#include <QtEndian>
#include "jwpqt/core/jwp_text_codec.h"
#include "jwpqt/core/raster_font.h"
#include "text_bridge.h"

namespace jwpqt::qt {

void draw_jwp_text_layout(QPainter& painter, QTextLayout& layout, const QString& text,
                          const QPointF& origin, bool vertical,
                          core::LegacyCodePage code_page,
                          const std::function<QColor(int)>& foreground) {
  // JWP counter-rotates Japanese glyphs for paper read after a clockwise turn;
  // Latin and these source punctuation exceptions retain their orientation.
  QTextBoundaryFinder boundaries(QTextBoundaryFinder::Grapheme, text);
  const auto jis_code = [&](const QString& cluster) {
    const auto scalars = from_qstring(cluster);
    return scalars.empty() ? std::optional<core::JisCode>{}
                          : core::unicode_to_jwp_code(scalars[0], code_page);
  };
  const auto rotates = [&](const QString& cluster) {
    const auto jis = jis_code(cluster);
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
      const QColor color = foreground ? foreground(at) : painter.pen().color();
      painter.save();
      const auto restore = qScopeGuard([&] { painter.restore(); });
      painter.setPen(color);
      if (vertical && rotates(text.mid(at, next - at))) {
        const auto runs = line.glyphRuns(at, next - at);
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
            const auto code = *jis_code(text.mid(at, next - at));
            for (int i = 0; i < count; ++i) {
              const int x_offset = word(10 + i * 6);
              const auto encoded_y = word(12 + i * 6);
              const int y_offset = encoded_y < 32768 ? encoded_y : static_cast<int>(encoded_y) - 65536;
              if (x_offset > width || y_offset < -height || y_offset > height)
                throw core::RasterFontError("Invalid native raster vertical offset");
              if (word(8 + i * 6) == code) {
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
          if (after <= next || after > end || (vertical && rotates(text.mid(next, after - next))) ||
              (foreground && foreground(next) != color)) break;
          next = after;
        }
      }
      for (const auto& run : line.glyphRuns(at, next - at)) painter.drawGlyphRun(origin, run);
      at = next;
    }
  }
}

}  // namespace jwpqt::qt
