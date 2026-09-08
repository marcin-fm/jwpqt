// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "application_settings.h"
#include <QFont>

class QWidget;

namespace jwpqt::qt {

void assign_japanese_font(QWidget& widget, JapaneseFontRole role, bool horizontal_strip = false);
QFont japanese_font(const QWidget& widget, JapaneseFontRole role);
bool clipboard_bitmap_enabled(const QWidget& widget);
QStringList set_japanese_fonts(QWidget& owner, const ApplicationSettings& settings);

}  // namespace jwpqt::qt
