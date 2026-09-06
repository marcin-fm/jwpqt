// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <optional>

#include <QPoint>

class QContextMenuEvent;
class QTextEdit;

namespace jwpqt::qt {

struct CharacterTarget {
  char32_t character;
  int position;
};

std::optional<CharacterTarget> character_target(
    const QTextEdit& editor, std::optional<QPoint> viewport_position = {});
void show_character_context_menu(
    QTextEdit& editor, QContextMenuEvent& event,
    const std::function<void(CharacterTarget)>& show_information);

}  // namespace jwpqt::qt
