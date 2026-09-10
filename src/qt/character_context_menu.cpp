// SPDX-License-Identifier: GPL-2.0-or-later

#include "character_context_menu.h"

#include <memory>

#include <QAbstractTextDocumentLayout>
#include <QAction>
#include <QContextMenuEvent>
#include <QList>
#include <QMenu>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextEdit>
#include <QTextLayout>
#include <QVariant>

#include "jwp_editor.h"

namespace jwpqt::qt {

std::optional<CharacterTarget> character_target(
    const QTextEdit& editor, std::optional<QPoint> viewport_position) {
  const QString text = document_plain_text(*editor.document());
  int position;
  if (viewport_position.has_value()) {
    if (!editor.viewport()->rect().contains(*viewport_position)) return {};
    const QTextCursor anchor = editor.cursorForPosition(*viewport_position);
    const QTextBlock block = anchor.block();
    // Preedit is rendered but has no corresponding position in toPlainText().
    if (block.layout() == nullptr || !block.layout()->preeditAreaText().isEmpty()) return {};
    const QTextLine line = block.layout()->lineForTextPosition(
        anchor.position() - block.position());
    if (!line.isValid()) return {};
    auto* layout = editor.document()->documentLayout();
    // Use a rendered caret as the horizontal origin, including RTL and indents.
    const qreal horizontal_offset = layout->blockBoundingRect(block).left() +
        line.cursorToX(anchor.position() - block.position()) -
        editor.cursorRect(anchor).left();
    position = layout->hitTest(
        QPointF(*viewport_position) +
            QPointF(horizontal_offset, editor.verticalScrollBar()->value()),
        Qt::ExactHit);
  } else {
    const QTextCursor cursor = editor.textCursor();
    position = cursor.hasSelection() ? cursor.selectionStart() : cursor.position();
    if (!cursor.hasSelection() && cursor.atBlockEnd() &&
        position > cursor.block().position()) --position;
    const auto* layout = editor.document()->findBlock(position).layout();
    if (layout != nullptr && !layout->preeditAreaText().isEmpty()) return {};
  }
  if (position < 0 || position >= text.size()) return {};
  if (text.at(position).isLowSurrogate() && position > 0 &&
      text.at(position - 1).isHighSurrogate()) --position;
  char32_t character = text.at(position).unicode();
  if (text.at(position).isHighSurrogate()) {
    if (position + 1 >= text.size() || !text.at(position + 1).isLowSurrogate())
      return {};
    character = QChar::surrogateToUcs4(text.at(position), text.at(position + 1));
  }
  if (QChar::category(character) == QChar::Other_Control ||
      QChar::category(character) == QChar::Other_Surrogate ||
      character == 0x2028 || character == 0x2029) return {};
  return CharacterTarget{character, position};
}

void show_character_context_menu(
    QTextEdit& editor, QContextMenuEvent& event,
    const std::function<void(CharacterTarget)>& show_information) {
  const bool keyboard = event.reason() == QContextMenuEvent::Keyboard;
  const auto target = character_target(
      editor, keyboard ? std::nullopt : std::optional<QPoint>(event.pos()));
  if (target && show_information && event.modifiers().testFlag(Qt::ShiftModifier) && !keyboard) {
    show_information(*target);
    return;
  }
  std::unique_ptr<QMenu> menu(editor.createStandardContextMenu());
  menu->setObjectName(QStringLiteral("characterContextMenu"));
  QList<QAction*> standard_actions;
  QList<QAction*> lookup_actions;
  QList<QAction*> mode_actions;
  QList<QAction*> command_actions;
  QList<QAction*> result_actions;
  for (auto* action : editor.actions()) {
    const QString group = action->property("jwpqtEditorContextGroup").toString();
    if (group == QStringLiteral("standard")) standard_actions.append(action);
    else if (group == QStringLiteral("lookup")) lookup_actions.append(action);
    else if (group == QStringLiteral("mode")) mode_actions.append(action);
    else if (group == QStringLiteral("command")) command_actions.append(action);
    else if (action->property("jwpqtAuxiliaryFind").toBool() ||
             action->property("jwpqtResultInsertion").toBool() ||
             action->property("jwpqtResultCommand").toBool())
      result_actions.append(action);
  }
  if (!standard_actions.isEmpty()) {
    menu->clear();
    for (auto* action : standard_actions) {
      menu->addAction(action);
      const QString name = action->objectName();
      if (name == QStringLiteral("redoAction") ||
          name == QStringLiteral("pasteAction")) menu->addSeparator();
    }
  }
  menu->addSeparator();
  QAction* information = menu->addAction(
      QTextEdit::tr("Character &Information"));
  information->setObjectName(QStringLiteral("characterInfoContextAction"));
  information->setEnabled(target.has_value() && static_cast<bool>(show_information));
  for (auto* action : lookup_actions) menu->addAction(action);
  if (!mode_actions.isEmpty() || !command_actions.isEmpty()) menu->addSeparator();
  if (!mode_actions.isEmpty()) {
    auto* input_mode = menu->addMenu(QTextEdit::tr("Input &Mode"));
    input_mode->setObjectName(QStringLiteral("editorInputModeMenu"));
    for (auto* action : mode_actions) input_mode->addAction(action);
  }
  for (auto* action : command_actions) menu->addAction(action);
  for (auto* action : result_actions) menu->addAction(action);
  const QPoint location = keyboard
      ? editor.viewport()->mapToGlobal(editor.cursorRect().center())
      : event.globalPos();
  if (menu->exec(location) == information && target && show_information) show_information(*target);
}

}  // namespace jwpqt::qt
