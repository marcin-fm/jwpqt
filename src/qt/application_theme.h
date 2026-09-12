// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "application_settings.h"

namespace jwpqt::qt {

void initialize_application_theme();
void set_application_color_scheme(ColorSchemePreference preference);
bool application_theme_is_dark() noexcept;

}  // namespace jwpqt::qt
