// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "application_settings.h"

class QWidget;

namespace jwpqt::qt {

void assign_japanese_font(QWidget& widget, JapaneseFontRole role, bool horizontal_strip = false);
QStringList set_japanese_fonts(QWidget& owner, const ApplicationSettings& settings);

}  // namespace jwpqt::qt
