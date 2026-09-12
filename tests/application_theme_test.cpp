// SPDX-License-Identifier: GPL-2.0-or-later

#include <iostream>
#include <stdexcept>

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPalette>
#include <QPushButton>

#include "application_settings_dialog.h"
#include "application_theme.h"

namespace {

void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

int lightness(const QColor& color) { return qGray(color.rgb()); }

void test_palettes() {
  using namespace jwpqt::qt;
  initialize_application_theme();
  set_application_color_scheme(ColorSchemePreference::kDark);
  const auto dark = qApp->palette();
  require(application_theme_is_dark() &&
              lightness(dark.color(QPalette::Window)) < 64 &&
              lightness(dark.color(QPalette::WindowText)) > 192 &&
              lightness(dark.color(QPalette::Base)) < 64 &&
              lightness(dark.color(QPalette::Text)) > 192,
          "Dark application palette has insufficient contrast");

  set_application_color_scheme(ColorSchemePreference::kLight);
  const auto light = qApp->palette();
  require(!application_theme_is_dark() &&
              lightness(light.color(QPalette::Window)) > 192 &&
              lightness(light.color(QPalette::WindowText)) < 64 &&
              lightness(light.color(QPalette::Base)) > 192 &&
              lightness(light.color(QPalette::Text)) < 64,
          "Light application palette has insufficient contrast");
}

void test_options_control() {
  using namespace jwpqt::qt;
  ApplicationSettings settings;
  ApplicationSettingsDialog accepted(settings);
  auto* combo = accepted.findChild<QComboBox*>(QStringLiteral("settingsColorScheme"));
  auto* buttons = accepted.findChild<QDialogButtonBox*>();
  require(combo && buttons && combo->count() == 3,
          "Options does not expose all color schemes");
  combo->setCurrentIndex(combo->findData(
      static_cast<int>(ColorSchemePreference::kDark)));
  buttons->button(QDialogButtonBox::Ok)->click();
  require(accepted.settings().color_scheme == ColorSchemePreference::kDark,
          "Options did not accept Dark color scheme");

  ApplicationSettingsDialog cancelled(accepted.settings());
  combo = cancelled.findChild<QComboBox*>(QStringLiteral("settingsColorScheme"));
  buttons = cancelled.findChild<QDialogButtonBox*>();
  combo->setCurrentIndex(combo->findData(
      static_cast<int>(ColorSchemePreference::kLight)));
  buttons->button(QDialogButtonBox::Cancel)->click();
  require(cancelled.settings().color_scheme == ColorSchemePreference::kDark,
          "Cancelling Options changed the color scheme");
}

}  // namespace

int main(int argc, char** argv) {
  QApplication application(argc, argv);
  try {
    test_palettes();
    test_options_control();
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
