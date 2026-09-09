// SPDX-License-Identifier: GPL-2.0-or-later
#include "window_geometry.h"
#include "application_settings.h"

#include <algorithm>
#include <limits>
#include <QApplication>
#include <QEvent>
#include <QMainWindow>
#include <QScreen>
#include <QScopedValueRollback>
#include <QStringList>
#include <QVariant>
#include <QWidget>

namespace jwpqt::qt {
WindowGeometry::WindowGeometry(QWidget& owner, ApplicationSettings& settings)
    : QObject(qApp), owner_(&owner), settings_(settings) {
  qApp->installEventFilter(this);
  connect(&owner, &QObject::destroyed, this, &WindowGeometry::stop);
}
WindowGeometry::~WindowGeometry() { if (qApp) qApp->removeEventFilter(this); }
void WindowGeometry::stop() {
  owner_.clear();
  if (qApp) qApp->removeEventFilter(this);
  deleteLater();
}

bool WindowGeometry::eventFilter(QObject* watched, QEvent* event) {
  if (updating_ || !owner_ || (event->type() != QEvent::Show && event->type() != QEvent::Move &&
      event->type() != QEvent::Resize && event->type() != QEvent::WindowStateChange)) return false;
  auto* window = qobject_cast<QWidget*>(watched);
  if (!window || !window->isWindow()) return false;
  QPointer<QWidget> alive(window);
  auto* ancestor = window;
  while (ancestor && ancestor != owner_) {
    if (qobject_cast<QMainWindow*>(ancestor)) return false;
    ancestor = ancestor->parentWidget();
  }
  if (ancestor != owner_) return false;
  int index = 0;
  if (window != owner_) {
    const QStringList names{QStringLiteral("kanjiInfoDialog"), QStringLiteral("edictLookupDialog"),
        QStringLiteral("kanjiCountDialog"), QStringLiteral("kanjiInfoMoreDialog"),
        QStringLiteral("wnnUserDictionaryDialog"), QStringLiteral("edictUserDictionaryDialog")};
    index = names.indexOf(window->objectName()) + 1;
    if (index == 0) return false;
  }
  QScopedValueRollback<bool> guard(updating_, true);
  constexpr auto initialized = "_jwpqt_geometry_initialized";
  if (event->type() == QEvent::Show && !window->property(initialized).toBool()) {
    window->setProperty(initialized, true);
    if (!alive || !owner_) return !alive;
    const auto stored = settings_.window_geometry[static_cast<std::size_t>(index)];
    const bool restore = index != 0 || settings_.restore_window;
    if (restore && stored[2] > 0 && stored[3] > 0) {
      QScreen* screen = nullptr;
      const bool positioned = stored[0] != std::numeric_limits<std::int32_t>::min() &&
          stored[1] != std::numeric_limits<std::int32_t>::min();
      if (positioned) screen = QGuiApplication::screenAt(QPoint(stored[0], stored[1]));
      if (!screen) screen = owner_->screen();
      if (!screen) screen = QGuiApplication::primaryScreen();
      if (screen) {
        const QRect available = screen->availableGeometry();
        const QSize size = QSize(stored[2], stored[3]).expandedTo(window->minimumSizeHint())
            .expandedTo(window->minimumSize()).boundedTo(available.size());
        const QPoint requested = positioned ? QPoint(stored[0], stored[1]) :
            available.center() - QPoint(size.width() / 2, size.height() / 2);
        const int x = std::clamp(requested.x(), available.left(),
            std::max(available.left(), available.right() - size.width() + 1));
        const int y = std::clamp(requested.y(), available.top(),
            std::max(available.top(), available.bottom() - size.height() + 1));
        window->setGeometry(QRect(QPoint(x, y), size));
        if (!alive || !owner_) return !alive;
      }
    }
    if (index == 0 && restore && settings_.maximize_window)
      window->setWindowState(window->windowState() | Qt::WindowMaximized);
    if (!alive || !owner_) return !alive;
  }
  if (!window->isVisible() || window->isMinimized() || !window->property(initialized).toBool()) return false;
  const QRect normal = window->isMaximized() ? window->normalGeometry() : window->geometry();
  if (normal.isValid()) settings_.window_geometry[static_cast<std::size_t>(index)] =
      {normal.x(), normal.y(), normal.width(), normal.height()};
  if (index == 0) settings_.maximize_window = window->isMaximized();
  return false;
}
}  // namespace jwpqt::qt
