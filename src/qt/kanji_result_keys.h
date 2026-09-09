// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <vector>

#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QKeyEvent>
#include <QListWidget>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>

#include "jwpqt/core/jwp_text_codec.h"
#include "text_bridge.h"
#include "rare_kanji_delegate.h"

namespace jwpqt::qt {

// Shared by the three kanji lookup result strips, not their query fields.
class KanjiResultKeys final : public QObject {
 public:
  KanjiResultKeys(QListWidget* list, QPushButton* insert, QPushButton* info,
                  QPushButton* copy, QDialog* dialog)
      : QObject(list), list_(list), insert_(insert), info_(info), copy_(copy), dialog_(dialog) {
    install_rare_kanji_marks(list);
    list->installEventFilter(this);
    copy->installEventFilter(this);
  }

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override {
    if (watched == copy_ && (event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride)) {
      const auto* key = static_cast<QKeyEvent*>(event);
      if ((key->modifiers() & ~Qt::KeypadModifier) == Qt::ShiftModifier &&
          (key->key() == Qt::Key_Space || key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)) {
        event->accept();
        if (event->type() == QEvent::KeyPress && copy_ && copy_->isEnabled()) copy_all();
        return true;
      }
    }
    if (watched == copy_ && event->type() == QEvent::MouseButtonRelease) {
      const auto* mouse = static_cast<QMouseEvent*>(event);
      if (mouse->button() == Qt::LeftButton && mouse->modifiers() == Qt::ShiftModifier &&
          copy_ && copy_->isEnabled() && copy_->rect().contains(mouse->position().toPoint())) {
        copy_->setDown(false);
        copy_all();
        return true;
      }
    }
    if (watched != list_ || (event->type() != QEvent::KeyPress &&
                             event->type() != QEvent::ShortcutOverride)) return false;
    const auto* key = static_cast<QKeyEvent*>(event);
    const auto modifiers = key->modifiers() & ~Qt::KeypadModifier;
    if (modifiers & (Qt::AltModifier | Qt::MetaModifier)) return false;
    const bool control = modifiers & Qt::ControlModifier;
    const bool shift = modifiers & Qt::ShiftModifier;
    const int code = key->key();
    const bool move = code == Qt::Key_F2 || code == Qt::Key_F3 ||
        code == Qt::Key_Greater || code == Qt::Key_Less || code == Qt::Key_Period || code == Qt::Key_Comma ||
        (control && !shift && (code == Qt::Key_Left || code == Qt::Key_Right));
    const bool command = code == Qt::Key_C || code == Qt::Key_I || code == Qt::Key_F23 ||
        code == Qt::Key_F4 || code == Qt::Key_Return || code == Qt::Key_Enter;
    if (!move && !command) return false;
    event->accept();
    if (event->type() == QEvent::ShortcutOverride) return true;
    if (move && list_ && list_->count()) {
      const bool backwards = code == Qt::Key_F3 || code == Qt::Key_Less ||
          code == Qt::Key_Comma || code == Qt::Key_Left;
      const int row = qBound(0, list_->currentRow() + (backwards ? -1 : 1) * (control ? 5 : 1),
                             list_->count() - 1);
      const QPointer<QListWidget> list = list_;
      list->setCurrentRow(row, QItemSelectionModel::ClearAndSelect);
      if (list && list->currentItem()) list->scrollToItem(list->currentItem());
    } else if (code == Qt::Key_C) {
      if (shift) copy_all();
      else if (copy_ && copy_->isEnabled()) copy_->click();
    } else if (code == Qt::Key_I || code == Qt::Key_F23) {
      if (info_ && info_->isEnabled()) info_->click();
    } else if (code == Qt::Key_F4) {
      if (dialog_) dialog_->close();
    } else if (code == Qt::Key_Return || code == Qt::Key_Enter) {
      if (insert_ && insert_->isEnabled()) insert_->click();
    }
    return true;
  }

 private:
  void copy_all() {
    if (!list_ || !list_->count()) return;
    std::vector<core::JisCode> codes;
    codes.reserve(static_cast<std::size_t>(list_->count()));
    for (int row = 0; row < list_->count(); ++row)
      codes.push_back(static_cast<core::JisCode>(list_->item(row)->data(Qt::UserRole).toUInt()));
    const auto text = to_qstring(core::decode_jwp_text(codes));
    QApplication::clipboard()->setText(text);
  }
  QPointer<QListWidget> list_;
  QPointer<QPushButton> insert_, info_, copy_;
  QPointer<QDialog> dialog_;
};

}  // namespace jwpqt::qt
