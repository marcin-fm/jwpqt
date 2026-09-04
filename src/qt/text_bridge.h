// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_QT_TEXT_BRIDGE_H
#define JWPQT_QT_TEXT_BRIDGE_H

#include <string>
#include <string_view>

#include <QString>

namespace jwpqt::qt {

QString to_qstring(std::u32string_view text);
std::u32string from_qstring(const QString& text);

}  // namespace jwpqt::qt

#endif
