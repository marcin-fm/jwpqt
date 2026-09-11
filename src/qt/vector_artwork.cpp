#include "vector_artwork.h"

#include <QCache>
#include <QEvent>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QPaintEvent>
#include <QSizePolicy>
#include <QSvgRenderer>

#include <cmath>
#include <stdexcept>

namespace jwpqt::qt {
namespace {

QByteArray read_svg(const QString& resource) {
  QFile file(resource);
  if (!file.open(QIODevice::ReadOnly)) {
    throw std::runtime_error("Could not read embedded SVG artwork");
  }
  return file.readAll();
}

}  // namespace

SvgArtworkWidget::SvgArtworkWidget(QString resource, QSize preferred_size,
                                   QWidget* parent)
    : QWidget(parent),
      source_(read_svg(resource)),
      preferred_size_(preferred_size),
      renderer_(new QSvgRenderer(this)) {
  if (preferred_size_.isEmpty() || !reload_renderer()) {
    throw std::runtime_error("Invalid embedded SVG artwork");
  }
  source_size_ = renderer_->defaultSize();
  if (source_size_.isEmpty()) {
    throw std::runtime_error("Embedded SVG artwork has no natural size");
  }
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  setMinimumSize(source_size_);
  setAttribute(Qt::WA_OpaquePaintEvent, false);
}

QSize SvgArtworkWidget::sizeHint() const { return preferred_size_; }

QSize SvgArtworkWidget::minimumSizeHint() const { return source_size_; }

bool SvgArtworkWidget::hasHeightForWidth() const { return true; }

int SvgArtworkWidget::heightForWidth(int width) const {
  if (source_size_.width() <= 0) return QWidget::heightForWidth(width);
  return static_cast<int>(std::lround(
      static_cast<double>(width) * source_size_.height() /
      source_size_.width()));
}

void SvgArtworkWidget::changeEvent(QEvent* event) {
  QWidget::changeEvent(event);
  if (event->type() == QEvent::PaletteChange ||
      event->type() == QEvent::ApplicationPaletteChange ||
      event->type() == QEvent::StyleChange ||
      event->type() == QEvent::EnabledChange) {
    reload_renderer();
    update();
  }
}

void SvgArtworkWidget::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  const QRectF available = contentsRect();
  QSizeF size = source_size_;
  size.scale(available.size(), Qt::KeepAspectRatio);
  const QRectF target(
      available.center() - QPointF(size.width() / 2.0, size.height() / 2.0),
      size);
  renderer_->render(&painter, target);
}

bool SvgArtworkWidget::reload_renderer() {
  QByteArray themed = source_;
  const QPalette::ColorGroup group =
      isEnabled() ? QPalette::Active : QPalette::Disabled;
  QColor ink = palette().color(group, QPalette::Text);
  const QColor paper = palette().color(group, QPalette::Window);
  if (std::abs(qGray(ink.rgb()) - qGray(paper.rgb())) < 96) {
    ink = qGray(paper.rgb()) < 128 ? QColor(Qt::white) : QColor(Qt::black);
  }
  const QByteArray marker("#000\"");
  const QByteArray replacement = ink.name(QColor::HexRgb).toLatin1() + '"';
  if (!themed.contains(marker)) return false;
  themed.replace(marker, replacement);
  return renderer_->load(themed);
}

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
