// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

#include "application_settings.h"

namespace jwpqt::qt {

class ApplicationSettingsDialog : public QDialog {
 public:
  explicit ApplicationSettingsDialog(const ApplicationSettings& settings, QWidget* parent = nullptr);
  const ApplicationSettings& settings() const noexcept { return settings_; }

 private:
  ApplicationSettings settings_;
};

}  // namespace jwpqt::qt
