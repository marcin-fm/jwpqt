// SPDX-License-Identifier: GPL-2.0-or-later

#include "application_theme.h"

#include <QApplication>
#include <QGuiApplication>
#include <QPalette>
#include <QStyleHints>

#ifdef Q_OS_WASM
#include <emscripten.h>
#endif

namespace jwpqt::qt {
namespace {

ColorSchemePreference preference = ColorSchemePreference::kSystem;
bool initialized = false;
bool system_dark = false;
bool effective_dark = false;

void set_all_groups(QPalette& palette, QPalette::ColorRole role,
                    const QColor& color) {
  palette.setColor(QPalette::Active, role, color);
  palette.setColor(QPalette::Inactive, role, color);
  palette.setColor(QPalette::Disabled, role, color);
}

QPalette light_palette() {
  QPalette palette;
  set_all_groups(palette, QPalette::Window, QColor(243, 243, 243));
  set_all_groups(palette, QPalette::WindowText, QColor(32, 32, 32));
  set_all_groups(palette, QPalette::Base, QColor(255, 255, 255));
  set_all_groups(palette, QPalette::AlternateBase, QColor(238, 238, 238));
  set_all_groups(palette, QPalette::ToolTipBase, QColor(255, 255, 220));
  set_all_groups(palette, QPalette::ToolTipText, QColor(32, 32, 32));
  set_all_groups(palette, QPalette::Text, QColor(32, 32, 32));
  set_all_groups(palette, QPalette::Button, QColor(243, 243, 243));
  set_all_groups(palette, QPalette::ButtonText, QColor(32, 32, 32));
  set_all_groups(palette, QPalette::BrightText, QColor(255, 255, 255));
  set_all_groups(palette, QPalette::Light, QColor(255, 255, 255));
  set_all_groups(palette, QPalette::Midlight, QColor(229, 229, 229));
  set_all_groups(palette, QPalette::Dark, QColor(154, 154, 154));
  set_all_groups(palette, QPalette::Mid, QColor(198, 198, 198));
  set_all_groups(palette, QPalette::Shadow, QColor(90, 90, 90));
  set_all_groups(palette, QPalette::Highlight, QColor(11, 101, 194));
  set_all_groups(palette, QPalette::HighlightedText, QColor(255, 255, 255));
  set_all_groups(palette, QPalette::Link, QColor(0, 95, 184));
  set_all_groups(palette, QPalette::LinkVisited, QColor(85, 26, 139));
  set_all_groups(palette, QPalette::PlaceholderText, QColor(112, 112, 112));
  for (const auto role : {QPalette::WindowText, QPalette::Text,
                          QPalette::ButtonText})
    palette.setColor(QPalette::Disabled, role, QColor(112, 112, 112));
  return palette;
}

QPalette dark_palette() {
  QPalette palette;
  set_all_groups(palette, QPalette::Window, QColor(35, 38, 41));
  set_all_groups(palette, QPalette::WindowText, QColor(240, 240, 240));
  set_all_groups(palette, QPalette::Base, QColor(24, 26, 27));
  set_all_groups(palette, QPalette::AlternateBase, QColor(44, 47, 51));
  set_all_groups(palette, QPalette::ToolTipBase, QColor(50, 54, 59));
  set_all_groups(palette, QPalette::ToolTipText, QColor(240, 240, 240));
  set_all_groups(palette, QPalette::Text, QColor(240, 240, 240));
  set_all_groups(palette, QPalette::Button, QColor(48, 52, 58));
  set_all_groups(palette, QPalette::ButtonText, QColor(240, 240, 240));
  set_all_groups(palette, QPalette::BrightText, QColor(255, 107, 107));
  set_all_groups(palette, QPalette::Light, QColor(86, 91, 97));
  set_all_groups(palette, QPalette::Midlight, QColor(67, 71, 76));
  set_all_groups(palette, QPalette::Dark, QColor(18, 20, 22));
  set_all_groups(palette, QPalette::Mid, QColor(54, 58, 63));
  set_all_groups(palette, QPalette::Shadow, QColor(8, 9, 10));
  set_all_groups(palette, QPalette::Highlight, QColor(61, 126, 255));
  set_all_groups(palette, QPalette::HighlightedText, QColor(255, 255, 255));
  set_all_groups(palette, QPalette::Link, QColor(122, 162, 255));
  set_all_groups(palette, QPalette::LinkVisited, QColor(192, 153, 255));
  set_all_groups(palette, QPalette::PlaceholderText, QColor(156, 163, 175));
  for (const auto role : {QPalette::WindowText, QPalette::Text,
                          QPalette::ButtonText})
    palette.setColor(QPalette::Disabled, role, QColor(139, 145, 151));
  return palette;
}

#ifdef Q_OS_WASM
EM_JS(int, browser_prefers_dark, (), {
  return window.matchMedia &&
                 window.matchMedia('(prefers-color-scheme: dark)').matches
             ? 1
             : 0;
});

EM_JS(void, publish_effective_scheme, (int dark), {
  document.documentElement.dataset.jwpqtColorScheme = dark ? 'dark' : 'light';
});

EM_JS(void, install_color_scheme_listener, (), {
  if (Module.jwpqtColorSchemeQuery || !window.matchMedia) return;
  const query = window.matchMedia('(prefers-color-scheme: dark)');
  const listener = (event) => {
    Module._jwpqt_browser_color_scheme_changed(event.matches ? 1 : 0);
  };
  if (query.addEventListener) query.addEventListener('change', listener);
  else query.addListener(listener);
  Module.jwpqtColorSchemeQuery = query;
  Module.jwpqtColorSchemeListener = listener;
});
#endif

void apply_effective_scheme() {
  if (!qApp) return;
  effective_dark = preference == ColorSchemePreference::kDark ||
                   (preference == ColorSchemePreference::kSystem && system_dark);
  qApp->setPalette(effective_dark ? dark_palette() : light_palette());
#ifdef Q_OS_WASM
  publish_effective_scheme(effective_dark ? 1 : 0);
#endif
}

void update_system_scheme(bool dark) {
  system_dark = dark;
  if (preference == ColorSchemePreference::kSystem) apply_effective_scheme();
}

}  // namespace

#ifdef Q_OS_WASM
extern "C" EMSCRIPTEN_KEEPALIVE void jwpqt_browser_color_scheme_changed(
    int dark) {
  update_system_scheme(dark != 0);
}
#endif

void initialize_application_theme() {
  if (initialized || !qApp) return;
  initialized = true;
#ifdef Q_OS_WASM
  system_dark = browser_prefers_dark() != 0;
  install_color_scheme_listener();
#else
  const auto scheme = QGuiApplication::styleHints()->colorScheme();
  system_dark = scheme == Qt::ColorScheme::Dark ||
                (scheme == Qt::ColorScheme::Unknown &&
                 qGray(qApp->palette().color(QPalette::Window).rgb()) < 128);
  QObject::connect(QGuiApplication::styleHints(),
                   &QStyleHints::colorSchemeChanged, qApp,
                   [](Qt::ColorScheme next) {
                     if (next != Qt::ColorScheme::Unknown)
                       update_system_scheme(next == Qt::ColorScheme::Dark);
                   });
#endif
  apply_effective_scheme();
}

void set_application_color_scheme(ColorSchemePreference next) {
  preference = next;
  if (!initialized) initialize_application_theme();
  else apply_effective_scheme();
}

bool application_theme_is_dark() noexcept { return effective_dark; }

}  // namespace jwpqt::qt
