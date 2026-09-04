// SPDX-License-Identifier: GPL-2.0-or-later

#include "text_bridge.h"

#include <QList>

namespace jwpqt::qt {

QString to_qstring(std::u32string_view text) {
  return QString::fromUcs4(text.data(), static_cast<qsizetype>(text.size()));
}

std::u32string from_qstring(const QString& text) {
  const QList<uint> code_points = text.toUcs4();
  std::u32string result;
  result.reserve(static_cast<std::size_t>(code_points.size()));
  for (const uint code_point : code_points) {
    result.push_back(static_cast<char32_t>(code_point));
  }
  return result;
}

}  // namespace jwpqt::qt
