// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <algorithm>
#include <cmath>
#include <QColor>
#include <QPalette>
#include <QTextFormat>

namespace jwpqt::qt {
inline constexpr int kSourceHighlight = QTextFormat::UserProperty + 0x4a02;

inline QColor source_highlight_color(const QPalette& palette, const QColor& requested) {
  const auto luminance = [](const QColor& color) {
    const auto linear = [](qreal value) {
      return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) + 0.0722 * linear(color.blueF());
  };
  const auto background = luminance(palette.color(QPalette::Base));
  for (const QColor& candidate : {requested, QColor(QStringLiteral("#b00020")),
       QColor(QStringLiteral("#ff8080")), palette.color(QPalette::Text), QColor(Qt::black), QColor(Qt::white)}) {
    if (!candidate.isValid()) continue;
    const auto foreground = luminance(candidate);
    if ((std::max(background, foreground) + 0.05) / (std::min(background, foreground) + 0.05) >= 4.5)
      return candidate;
  }
  return palette.color(QPalette::Text);
}
}  // namespace jwpqt::qt
