#include "vector_artwork.h"

#include <QCache>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>

#include <stdexcept>

namespace jwpqt::qt {

QPixmap render_svg_artwork(const QString& resource, QSize size,
                           const QColor& tint) {
  QSvgRenderer renderer(resource);
  if (!renderer.isValid() || !size.isValid()) {
    throw std::runtime_error("Invalid embedded SVG artwork");
  }
  QImage image(size, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);
  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing);
  renderer.render(&painter, QRectF(QPointF(), QSizeF(size)));
  if (tint.isValid()) {
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(image.rect(), tint);
  }
  painter.end();
  return QPixmap::fromImage(image);
}

QIcon svg_icon(const QString& resource) {
  QIcon icon;
  for (const int size : {16, 24, 32, 48, 64, 128, 256}) {
    icon.addPixmap(render_svg_artwork(resource, QSize(size, size)));
  }
  return icon;
}

QIcon themed_svg_icon(const QString& resource, const QPalette& palette) {
  const QString cache_key =
      resource + QLatin1Char('|') +
      palette.color(QPalette::Active, QPalette::ButtonText)
          .name(QColor::HexArgb) +
      QLatin1Char('|') +
      palette.color(QPalette::Disabled, QPalette::ButtonText)
          .name(QColor::HexArgb);
  static QCache<QString, QIcon> cache(256);
  if (const QIcon* cached = cache.object(cache_key); cached != nullptr) {
    return *cached;
  }

  QIcon icon;
  for (const int size : {16, 24, 32, 48, 64}) {
    const QSize dimensions(size, size);
    const QPixmap active = render_svg_artwork(
        resource, dimensions,
        palette.color(QPalette::Active, QPalette::ButtonText));
    const QPixmap disabled = render_svg_artwork(
        resource, dimensions,
        palette.color(QPalette::Disabled, QPalette::ButtonText));
    for (const auto mode : {QIcon::Normal, QIcon::Active, QIcon::Selected}) {
      icon.addPixmap(active, mode, QIcon::Off);
      icon.addPixmap(active, mode, QIcon::On);
    }
    icon.addPixmap(disabled, QIcon::Disabled, QIcon::Off);
    icon.addPixmap(disabled, QIcon::Disabled, QIcon::On);
  }
  cache.insert(cache_key, new QIcon(icon));
  return icon;
}

}  // namespace jwpqt::qt
