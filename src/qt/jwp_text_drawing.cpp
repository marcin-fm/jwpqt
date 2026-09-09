// SPDX-License-Identifier: GPL-2.0-or-later
#include "jwp_text_drawing.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <optional>
#include <QGlyphRun>
#include <QPainter>
#include <QTextBoundaryFinder>
#include <QTextLayout>
#include "jwpqt/core/jwp_text_codec.h"
#include "text_bridge.h"

namespace jwpqt::qt {

void draw_jwp_text_layout(QPainter& painter, QTextLayout& layout, const QString& text,
                          const QPointF& origin, bool vertical,
                          core::LegacyCodePage code_page,
                          const std::function<QColor(int)>& foreground) {
  // JWP counter-rotates Japanese glyphs for paper read after a clockwise turn;
  // Latin and these source punctuation exceptions retain their orientation.
  static constexpr core::JisCode no_rotate[] = {
    0x213b,0x213c,0x2141,0x2142,0x2143,0x2144,0x2145,0x214a,0x214b,
    0x214c,0x214d,0x214e,0x214f,0x2150,0x2151,0x2152,0x2153,0x2154,0x2155,
    0x2156,0x2157,0x2158,0x2159,0x215a,0x215b,0x2161,0x2162,0x2163,0x2164,
    0x2165,0x2166,0x2167,0x222a,0x222b,0x222e,0x2127};
  QTextBoundaryFinder boundaries(QTextBoundaryFinder::Grapheme, text);
  const auto rotates = [&](const QString& cluster) {
    const auto scalars = from_qstring(cluster);
    const auto jis = scalars.empty() ? std::optional<core::JisCode>{}
                                    : core::unicode_to_jwp_code(scalars[0], code_page);
    return jis && *jis >= 0x2100 &&
        std::find(std::begin(no_rotate), std::end(no_rotate), *jis) == std::end(no_rotate);
  };
  for (int number = 0; number < layout.lineCount(); ++number) {
    const auto line = layout.lineAt(number);
    if (painter.hasClipping()) {
      const qreal middle = line.rect().translated(origin).center().y();
      if (middle < painter.clipBoundingRect().top() || middle >= painter.clipBoundingRect().bottom()) continue;
    }
    if (!vertical) { line.draw(&painter, origin); continue; }
    const int end = line.textStart() + line.textLength();
    for (int at = line.textStart(); at < end;) {
      boundaries.setPosition(at);
      int next = boundaries.toNextBoundary();
      if (next <= at || next > end) next = end;
      const QColor color = foreground ? foreground(at) : painter.pen().color();
      painter.save();
      painter.setPen(color);
      if (rotates(text.mid(at, next - at))) {
        const qreal x = line.cursorToX(at);
        const qreal width = std::abs(line.cursorToX(next) - x);
        const QPointF center = origin + QPointF(x + width / 2, line.y() + line.height() / 2);
        painter.translate(center); painter.rotate(-90); painter.translate(-center);
      } else {
        while (next < end) {
          boundaries.setPosition(next);
          const int after = boundaries.toNextBoundary();
          if (after <= next || after > end || rotates(text.mid(next, after - next)) ||
              (foreground && foreground(next) != color)) break;
          next = after;
        }
      }
      for (const auto& run : line.glyphRuns(at, next - at)) painter.drawGlyphRun(origin, run);
      painter.restore();
      at = next;
    }
  }
}

}  // namespace jwpqt::qt
