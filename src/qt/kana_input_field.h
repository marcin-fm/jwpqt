// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QPointer>
#include <QWidget>

#include "input_mode.h"
#include "jwpqt/core/kana_input.h"

class QAction;
class QLineEdit;
class QToolButton;

namespace jwpqt::qt {

class KanaInputField : public QWidget {
 public:
  explicit KanaInputField(const QString& name, QWidget* parent = nullptr);
  QLineEdit* edit() const noexcept;
  InputMode input_mode() const noexcept;
  void set_input_mode(InputMode mode);
  bool overwrite_mode() const noexcept;
  void set_overwrite_action(QAction* action);
  void finish_input();
  void clear_input();

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  void insert_events(const std::vector<core::KanaInputEvent>& events);
  void insert_text(const QString& text);
  int replacement_length(const QString& text) const;
  void update_mode_hint();

  QLineEdit* edit_;
  QToolButton* mode_button_;
  QPointer<QAction> overwrite_action_;
  QMetaObject::Connection overwrite_connection_;
  QMetaObject::Connection overwrite_destroyed_connection_;
  core::KanaInputComposer composer_;
  InputMode mode_ = InputMode::kKanji;
  bool local_overwrite_ = false;
  bool inserting_ = false;
};

}  // namespace jwpqt::qt
