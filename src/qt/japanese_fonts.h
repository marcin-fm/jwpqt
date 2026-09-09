// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "application_settings.h"
#include <QFont>

class QWidget;

namespace jwpqt::qt {

void assign_japanese_font(QWidget& widget, JapaneseFontRole role, bool horizontal_strip = false);
QFont japanese_font(const QWidget& widget, JapaneseFontRole role);
QFont ensure_ascii_font(QFont font);
struct ClipboardBitmapOptions {
  bool enabled = true;
  bool vertical = false;
  bool colors = false;
};
ClipboardBitmapOptions clipboard_bitmap_options(const QWidget& widget);
QFont japanese_print_font(QFont base, const JapaneseFontSetting& setting, const QString& directory);
QStringList set_japanese_fonts(QWidget& owner, const ApplicationSettings& settings,
                              const QString& directory = {});

}  // namespace jwpqt::qt
