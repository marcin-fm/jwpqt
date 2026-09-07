// SPDX-License-Identifier: GPL-2.0-or-later

#include "kana_input_field.h"

#include <algorithm>
#include <stdexcept>

#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QScopedValueRollback>
#include <QToolButton>
#include <QValidator>

#include "jwpqt/core/jwp_text_codec.h"
#include "text_bridge.h"
#include "japanese_fonts.h"

namespace jwpqt::qt {

KanaInputField::KanaInputField(const QString& name, QWidget* parent)
    : QWidget(parent), edit_(new QLineEdit(this)),
      mode_button_(new QToolButton(this)) {
  setObjectName(name + QStringLiteral("InputField"));
  edit_->setObjectName(name);
  mode_button_->setObjectName(name + QStringLiteral("Mode"));
  mode_button_->setFocusPolicy(Qt::NoFocus);
  mode_button_->setAccessibleName(tr("Input mode"));
  QFont content_font = edit_->font();
  content_font.setPixelSize(16);
  edit_->setFont(content_font);
  assign_japanese_font(*edit_, JapaneseFontRole::kEdit);
  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);
  layout->addWidget(edit_, 1);
  layout->addWidget(mode_button_);
  setFocusProxy(edit_);
  edit_->installEventFilter(this);
  connect(edit_, &QLineEdit::textChanged, this, [this] {
    if (!inserting_) composer_.discard();
  });
  connect(edit_, &QLineEdit::cursorPositionChanged, this, [this] {
    if (!inserting_) composer_.discard();
  });
  connect(edit_, &QLineEdit::selectionChanged, this, [this] {
    if (!inserting_) composer_.discard();
  });
  connect(mode_button_, &QToolButton::clicked, this, [this] {
    switch (mode_) {
      case InputMode::kKanji: set_input_mode(InputMode::kAscii); break;
      case InputMode::kAscii: set_input_mode(InputMode::kJascii); break;
      case InputMode::kJascii: set_input_mode(InputMode::kKanji); break;
    }
    edit_->setFocus();
  });
  set_input_mode(mode_);
  update_mode_hint();
}

QLineEdit* KanaInputField::edit() const noexcept { return edit_; }
InputMode KanaInputField::input_mode() const noexcept { return mode_; }

bool KanaInputField::overwrite_mode() const noexcept {
  return overwrite_action_ ? overwrite_action_->isChecked() : local_overwrite_;
}

void KanaInputField::set_overwrite_action(QAction* action) {
  if (action && !action->isCheckable())
    throw std::invalid_argument("Query overwrite action must be checkable");
  disconnect(overwrite_connection_);
  disconnect(overwrite_destroyed_connection_);
  overwrite_action_ = action;
  if (action) {
    overwrite_connection_ = connect(action, &QAction::toggled, this,
                                    [this] { update_mode_hint(); });
    overwrite_destroyed_connection_ = connect(action, &QObject::destroyed, this,
        [this] { set_overwrite_action(nullptr); });
  }
  update_mode_hint();
}

void KanaInputField::update_mode_hint() {
  const QString state = overwrite_mode() ? tr("Overwrite mode") : tr("Insert mode");
  const QString hint = tr("%1. Insert toggles. Selections keep following text; paste inserts.")
                           .arg(state);
  edit_->setToolTip(hint);
  edit_->setAccessibleDescription(hint);
  mode_button_->setToolTip(
      tr("Click to cycle Kanji, ASCII and JASCII. F4 switches Kanji/ASCII.\n%1").arg(hint));
}

void KanaInputField::set_input_mode(InputMode mode) {
  if (edit_->isReadOnly()) return;
  QString label;
  switch (mode) {
    case InputMode::kKanji: label = QStringLiteral("K"); break;
    case InputMode::kAscii: label = QStringLiteral("A"); break;
    case InputMode::kJascii: label = QStringLiteral("J"); break;
    default: throw core::KanaInputError("Unknown input mode");
  }
  finish_input();
  mode_ = mode;
  mode_button_->setText(label);
}

void KanaInputField::insert_events(const std::vector<core::KanaInputEvent>& events) {
  QString text;
  for (const auto& event : events)
    text += to_qstring(core::decode_jwp_text(event.text));
  insert_text(text);
}

int KanaInputField::replacement_length(const QString& text) const {
  const auto characters = from_qstring(text);
  if (to_qstring(characters) != text)
    throw std::invalid_argument("Invalid Unicode query input");
  const QString before = edit_->text();
  const int start = edit_->hasSelectedText() ? edit_->selectionStart() : edit_->cursorPosition();
  int end = start + edit_->selectedText().size();
  const auto splits_scalar = [&](int position) {
    return position > 0 && position < before.size() &&
           before[position - 1].isHighSurrogate() && before[position].isLowSurrogate();
  };
  if (start < 0 || end > before.size() || splits_scalar(start) || splits_scalar(end))
    throw std::invalid_argument("Query selection splits a Unicode scalar");
  if (overwrite_mode() && !edit_->hasSelectedText()) {
    for (const char32_t character : characters) {
      if (end == before.size() || character == U'\n' || character == U'\r' ||
          character == U'\u2028' || character == U'\u2029') break;
      end += before[end].isHighSurrogate() && end + 1 < before.size() &&
                     before[end + 1].isLowSurrogate() ? 2 : 1;
    }
  }
  if (text.size() > edit_->maxLength() - (before.size() - (end - start)))
    throw std::length_error("Query input exceeds the field limit");
  if (const auto* validator = edit_->validator()) {
    QString candidate = before.left(start) + text + before.mid(end);
    int position = start + text.size();
    if (validator->validate(candidate, position) == QValidator::Invalid)
      throw std::invalid_argument("Query input is rejected by the validator");
  }
  return end - start;
}

void KanaInputField::insert_text(const QString& text) {
  if (text.isEmpty() || edit_->isReadOnly()) return;
  try {
    const int length = replacement_length(text);
    const QString before = edit_->text();
    const int start = edit_->hasSelectedText() ? edit_->selectionStart() : edit_->cursorPosition();
    const QString expected = before.left(start) + text + before.mid(start + length);
    const QScopedValueRollback<bool> guard(inserting_, true);
    if (length && !edit_->hasSelectedText()) edit_->setSelection(start, length);
    // A selection listener may replace the field before the requested insertion.
    if (edit_->isReadOnly() || edit_->text() != before ||
        text.size() > edit_->maxLength() - (before.size() - length) ||
        edit_->selectedText().size() != length ||
        (length ? edit_->selectionStart() : edit_->cursorPosition()) != start) {
      composer_.discard();
      return;
    }
    edit_->insert(text);
    if (edit_->text() != expected || edit_->cursorPosition() != start + text.size() ||
        edit_->hasSelectedText()) composer_.discard();
  } catch (const std::exception&) {
    composer_.discard();
  }
}

void KanaInputField::clear_input() {
  composer_.discard();
  edit_->clear();
}

void KanaInputField::finish_input() {
  if (!composer_.pending()) return;
  try {
    insert_events(composer_.flush());
  } catch (const core::KanaInputError&) {
    composer_.discard();
  }
}

bool KanaInputField::eventFilter(QObject* watched, QEvent* event) {
  if (watched != edit_) return QWidget::eventFilter(watched, event);
  if (event->type() == QEvent::ShortcutOverride) {
    const auto* key = static_cast<QKeyEvent*>(event);
    if ((key->key() == Qt::Key_F4 && key->modifiers() == Qt::NoModifier) ||
        (key->key() == Qt::Key_Insert &&
         !(key->modifiers() & (Qt::AltModifier | Qt::MetaModifier)))) {
      event->accept();
      return true;
    }
  }
  if (edit_->isReadOnly()) return QWidget::eventFilter(watched, event);
  if (event->type() == QEvent::InputMethod) {
    composer_.discard();
    auto* input = static_cast<QInputMethodEvent*>(event);
    const bool selection_attribute = std::any_of(
        input->attributes().begin(), input->attributes().end(),
        [](const auto& attribute) { return attribute.type == QInputMethodEvent::Selection; });
    if (!inserting_ && overwrite_mode() && !input->commitString().isEmpty() &&
        !edit_->hasSelectedText() && input->replacementStart() == 0 &&
        input->replacementLength() == 0 && !selection_attribute) {
      try {
        QInputMethodEvent replacement(input->preeditString(), input->attributes());
        replacement.setCommitString(input->commitString(), 0,
                                    replacement_length(input->commitString()));
        const QScopedValueRollback<bool> guard(inserting_, true);
        QApplication::sendEvent(edit_, &replacement);
        input->setAccepted(replacement.isAccepted());
      } catch (const std::exception&) {
        input->ignore();
      }
      return true;
    }
  } else if (event->type() == QEvent::MouseButtonPress) {
    finish_input();
  } else if (event->type() == QEvent::KeyPress) {
    const auto* key = static_cast<QKeyEvent*>(event);
    if (key->key() == Qt::Key_Insert &&
        !(key->modifiers() & (Qt::AltModifier | Qt::MetaModifier))) {
      if (key->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier)) {
        finish_input();
        if (key->modifiers() & Qt::ShiftModifier) edit_->paste();
        else edit_->copy();
      } else {
        if (overwrite_action_) overwrite_action_->trigger();
        else { local_overwrite_ = !local_overwrite_; update_mode_hint(); }
      }
      return true;
    }
    if (key->key() == Qt::Key_F4 && key->modifiers() == Qt::NoModifier) {
      set_input_mode(mode_ == InputMode::kKanji ? InputMode::kAscii : InputMode::kKanji);
      return true;
    }
    if (composer_.pending() && (key->key() == Qt::Key_Backspace ||
        key->key() == Qt::Key_Delete || key->key() == Qt::Key_Escape)) {
      composer_.discard();
      return true;
    }
    const QString text = key->text();
    if (!(key->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) &&
        text.size() == 1 && text[0].unicode() >= 0x20 && text[0].unicode() <= 0x7e) {
      const char value = static_cast<char>(text[0].unicode());
      if (mode_ == InputMode::kKanji) {
        insert_events(composer_.push_ascii(value));
        return true;
      }
      if (mode_ == InputMode::kJascii) {
        const auto code = core::ascii_to_jascii(value, true);
        if (code) insert_text(to_qstring(core::decode_jwp_text({*code})));
        return true;
      }
    }
    if (overwrite_mode() && !text.isEmpty() &&
        !(key->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) &&
        (text.front().isPrint() || text.front().isSurrogate() ||
         text.front().category() == QChar::Other_Format)) {
      finish_input();
      insert_text(text);
      return true;
    }
    if (key->key() != Qt::Key_Shift && key->key() != Qt::Key_Control &&
        key->key() != Qt::Key_Alt && key->key() != Qt::Key_Meta)
      finish_input();
  }
  return QWidget::eventFilter(watched, event);
}

}  // namespace jwpqt::qt
