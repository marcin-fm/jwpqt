// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QList>
#include <QTextEdit>
#include <QTextFormat>

#include "jwpqt/core/jwp_document.h"
#include "jwpqt/core/kanji_color.h"
#include "jwpqt/core/kanji_color_list.h"
#include "jwpqt/core/legacy_code_page.h"

class QPaintEvent;

namespace jwpqt::qt {

class JwpEditor final : public QTextEdit {
 public:
  static constexpr int kPageBreakProperty = QTextFormat::UserProperty + 1;

  explicit JwpEditor(QWidget* parent = nullptr);

  int character_page_width() const;
  void apply_jwp_layout(const core::JwpDocument& document);
  void clear_jwp_layout();
  void apply_kanji_colors(
      const core::JwpDocument& document,
      const core::KanjiColorList& color_list,
      const core::KanjiColorPolicy& policy,
      core::LegacyCodePage code_page = core::kDefaultLegacyCodePage);
  void clear_kanji_colors();
  void set_transient_extra_selections(
      const QList<QTextEdit::ExtraSelection>& selections);

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  qreal indent_unit() const;
  void update_extra_selections();

  QList<QTextEdit::ExtraSelection> kanji_color_selections_;
  QList<QTextEdit::ExtraSelection> transient_extra_selections_;
};

}  // namespace jwpqt::qt
