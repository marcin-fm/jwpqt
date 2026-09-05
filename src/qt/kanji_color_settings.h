// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "jwpqt/core/kanji_color.h"

class QSettings;

namespace jwpqt::qt {

core::KanjiColorPolicy read_kanji_color_policy(QSettings& settings);
void write_kanji_color_policy(QSettings& settings,
                              const core::KanjiColorPolicy& policy);

}  // namespace jwpqt::qt
