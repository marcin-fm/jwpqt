// SPDX-License-Identifier: GPL-2.0-or-later

#include "kana_input_field.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QScopedValueRollback>
#include <QToolButton>

#include "jwpqt/core/jwp_text_codec.h"
#include "text_bridge.h"

namespace jwpqt::qt {

KanaInputField::KanaInputField(const QString& name, QWidget* parent)
    : QWidget(parent), edit_(new QLineEdit(this)),
      mode_button_(new QToolButton(this)) {
  setObjectName(name + QStringLiteral("InputField"));
  edit_->setObjectName(name);
  mode_button_->setObjectName(name + QStringLiteral("Mode"));
  mode_button_->setFocusPolicy(Qt::NoFocus);
  mode_button_->setAccessibleName(tr("Input mode"));
  mode_button_->setToolTip(tr("Click to cycle Kanji, ASCII and JASCII. F4 switches Kanji/ASCII."));
  QFont content_font = edit_->font();
  content_font.setPixelSize(16);
  edit_->setFont(content_font);
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
}

QLineEdit* KanaInputField::edit() const noexcept { return edit_; }
InputMode KanaInputField::input_mode() const noexcept { return mode_; }

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
  if (text.isEmpty()) return;
  const int start = edit_->hasSelectedText() ? edit_->selectionStart()
                                            : edit_->cursorPosition();
  const QString expected = edit_->text().left(start) + text +
      edit_->text().mid(start + edit_->selectedText().size());
  const QScopedValueRollback<bool> guard(inserting_, true);
  edit_->insert(text);
  // Reentrant listeners, validators and maxLength can change the requested edit.
  if (edit_->text() != expected || edit_->cursorPosition() != start + text.size() ||
      edit_->hasSelectedText()) composer_.discard();
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
  if (watched != edit_ || edit_->isReadOnly()) return QWidget::eventFilter(watched, event);
  if (event->type() == QEvent::InputMethod) {
    composer_.discard();
  } else if (event->type() == QEvent::MouseButtonPress) {
    finish_input();
  } else if (event->type() == QEvent::ShortcutOverride) {
    const auto* key = static_cast<QKeyEvent*>(event);
    if (key->key() == Qt::Key_F4 && key->modifiers() == Qt::NoModifier) {
      event->accept();
      return true;
    }
  } else if (event->type() == QEvent::KeyPress) {
    const auto* key = static_cast<QKeyEvent*>(event);
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
        if (code) edit_->insert(to_qstring(core::decode_jwp_text({*code})));
        return true;
      }
    }
    if (key->key() != Qt::Key_Shift && key->key() != Qt::Key_Control &&
        key->key() != Qt::Key_Alt && key->key() != Qt::Key_Meta)
      finish_input();
  }
  return QWidget::eventFilter(watched, event);
}

}  // namespace jwpqt::qt
