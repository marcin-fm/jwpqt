// SPDX-License-Identifier: GPL-2.0-or-later

#include "text_bridge.h"

#include <stdexcept>

#include <QList>

namespace jwpqt::qt {

QString to_qstring(std::u32string_view text) {
  QString result;
  result.reserve(static_cast<qsizetype>(text.size()));
  // QString::fromUcs4 treats a leading U+FEFF as a file signature, not text.
  for (const char32_t value : text) {
    if (value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) {
      throw std::invalid_argument("Invalid Unicode scalar in Qt text");
    }
    result.append(QChar::fromUcs4(value));
  }
  return result;
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
