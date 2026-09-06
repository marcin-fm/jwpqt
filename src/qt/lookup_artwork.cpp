// SPDX-License-Identifier: GPL-2.0-or-later

#include "lookup_artwork.h"

#include <QImage>
#include <QPainter>

namespace jwpqt::qt {

QPixmap themed_lookup_artwork(const QPixmap& source, const QPalette& palette) {
  const QColor paper = palette.color(QPalette::Window);
  if (source.isNull() || paper.lightness() >= 128) return source;
  QColor ink = palette.color(QPalette::Text);
  if (qGray(ink.rgb()) - qGray(paper.rgb()) < 96) ink = Qt::white;
  QImage image = source.toImage().convertToFormat(QImage::Format_ARGB32);
  for (int y = 0; y < image.height(); ++y) {
    auto* pixels = reinterpret_cast<QRgb*>(image.scanLine(y));
    for (int x = 0; x < image.width(); ++x) {
      const QRgb pixel = pixels[x];
      if (qAlpha(pixel) == 0 || qRed(pixel) != qGreen(pixel) || qGreen(pixel) != qBlue(pixel))
        continue;
      const int gray = qRed(pixel);
      auto channel = [gray](int foreground, int background) {
        return (foreground * (255 - gray) + background * gray + 127) / 255;
      };
      pixels[x] = qRgba(channel(ink.red(), paper.red()), channel(ink.green(), paper.green()),
                       channel(ink.blue(), paper.blue()), qAlpha(pixel));
    }
  }
  return QPixmap::fromImage(image);
}

QIcon themed_lookup_icon(const QPixmap& source, const QPalette& palette) {
  if (source.isNull() || palette.color(QPalette::Window).lightness() >= 128)
    return QIcon(source);
  const QPixmap normal = themed_lookup_artwork(source, palette);
  QPixmap disabled = normal;
  QPainter painter(&disabled);
  painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
  painter.fillRect(disabled.rect(), QColor(0, 0, 0, 140));
  painter.end();
  QIcon icon;
  // Qt's light-style generated icon modes can otherwise restore light paper.
  for (const auto mode : {QIcon::Normal, QIcon::Active, QIcon::Selected, QIcon::Disabled})
    for (const auto state : {QIcon::Off, QIcon::On})
      icon.addPixmap(mode == QIcon::Disabled ? disabled : normal, mode, state);
  return icon;
}

}  // namespace jwpqt::qt
