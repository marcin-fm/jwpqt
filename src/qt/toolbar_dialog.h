// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "toolbar_settings.h"
#include <QDialog>
#include <QList>
class QAction;
namespace jwpqt::qt {
class ToolbarDialog final : public QDialog {
 public:
  ToolbarDialog(const ToolbarSettings& settings, const QList<QAction*>& catalog, QWidget* parent);
  const ToolbarSettings& settings() const noexcept { return settings_; }
 private:
  ToolbarSettings settings_;
};
}
