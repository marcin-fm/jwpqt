// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

#include "input_mode.h"
#include "jwpqt/core/kana_input.h"

class QLineEdit;
class QToolButton;

namespace jwpqt::qt {

class KanaInputField : public QWidget {
 public:
  explicit KanaInputField(const QString& name, QWidget* parent = nullptr);
  QLineEdit* edit() const noexcept;
  InputMode input_mode() const noexcept;
  void set_input_mode(InputMode mode);
  void finish_input();
  void clear_input();

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  void insert_events(const std::vector<core::KanaInputEvent>& events);

  QLineEdit* edit_;
  QToolButton* mode_button_;
  core::KanaInputComposer composer_;
  InputMode mode_ = InputMode::kKanji;
  bool inserting_ = false;
};

}  // namespace jwpqt::qt
