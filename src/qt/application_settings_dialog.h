// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

#include "application_settings.h"

class QAction;

namespace jwpqt::qt {

class ApplicationSettingsDialog : public QDialog {
 public:
  explicit ApplicationSettingsDialog(const ApplicationSettings& settings, QWidget* parent = nullptr,
                                     bool dictionary_page = false,
                                     QAction* overwrite_action = nullptr);
  const ApplicationSettings& settings() const noexcept { return settings_; }

 private:
  ApplicationSettings settings_;
};

}  // namespace jwpqt::qt
