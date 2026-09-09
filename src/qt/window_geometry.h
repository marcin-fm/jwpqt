// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QObject>
#include <QPointer>

class QWidget;
namespace jwpqt::qt {
struct ApplicationSettings;

// Per-workspace placement. No disk I/O and no effect on an already initialized window.
class WindowGeometry final : public QObject {
 public:
  WindowGeometry(QWidget& owner, ApplicationSettings& settings);
  ~WindowGeometry() override;
  void stop();
 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;
 private:
  QPointer<QWidget> owner_;
  ApplicationSettings& settings_;
  bool updating_ = false;
};
}  // namespace jwpqt::qt
