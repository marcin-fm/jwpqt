#pragma once

#include <QColor>
#include <QIcon>
#include <QPalette>
#include <QPixmap>
#include <QSize>
#include <QString>

namespace jwpqt::qt {

QPixmap render_svg_artwork(const QString& resource, QSize size,
                           const QColor& tint = {});
QIcon svg_icon(const QString& resource);
QIcon themed_svg_icon(const QString& resource, const QPalette& palette);

}  // namespace jwpqt::qt
