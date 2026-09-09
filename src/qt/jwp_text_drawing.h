// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <functional>
#include <QColor>
#include <QPointF>
#include <QString>
#include "jwpqt/core/legacy_code_page.h"

class QPainter;
class QRawFont;
class QTextLayout;

namespace jwpqt::qt {

// Invalid means no vert feature (or a raster face). Private cmap retains the
// original characters for alternate outlines, including PDF text extraction.
QRawFont jwp_vertical_font(const QRawFont& original);

void draw_jwp_text_layout(QPainter& painter, QTextLayout& layout, const QString& text,
                          const QPointF& origin, bool vertical,
                          core::LegacyCodePage code_page,
                          const std::function<QColor(int)>& foreground = {},
                          const std::function<int(int)>& representation = {});

}  // namespace jwpqt::qt
