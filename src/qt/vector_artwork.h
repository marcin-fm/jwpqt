#pragma once

#include <QByteArray>
#include <QColor>
#include <QIcon>
#include <QPalette>
#include <QPixmap>
#include <QSize>
#include <QString>
#include <QWidget>

class QPaintEvent;
class QSvgRenderer;

namespace jwpqt::qt {

class SvgArtworkWidget final : public QWidget {
 public:
  SvgArtworkWidget(QString resource, QSize preferred_size,
                   QWidget* parent = nullptr);

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;
  bool hasHeightForWidth() const override;
  int heightForWidth(int width) const override;

 protected:
  void changeEvent(QEvent* event) override;
  void paintEvent(QPaintEvent* event) override;

 private:
  bool reload_renderer();

  QByteArray source_;
  QSize source_size_;
  QSize preferred_size_;
  QSvgRenderer* renderer_;
};

QPixmap render_svg_artwork(const QString& resource, QSize size,
                           const QColor& tint = {});
QIcon svg_icon(const QString& resource);
QIcon themed_svg_icon(const QString& resource, const QPalette& palette);

}  // namespace jwpqt::qt
