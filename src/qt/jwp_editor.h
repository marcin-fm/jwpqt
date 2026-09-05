// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QTextEdit>
#include <QTextFormat>

#include "jwpqt/core/jwp_document.h"

class QPaintEvent;

namespace jwpqt::qt {

class JwpEditor final : public QTextEdit {
 public:
  static constexpr int kPageBreakProperty = QTextFormat::UserProperty + 1;

  explicit JwpEditor(QWidget* parent = nullptr);

  int character_page_width() const;
  void apply_jwp_layout(const core::JwpDocument& document);
  void clear_jwp_layout();

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  qreal indent_unit() const;
};

}  // namespace jwpqt::qt
