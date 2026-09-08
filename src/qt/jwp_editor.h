// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string_view>

#include <QList>
#include <QTextEdit>
#include <QTextFormat>

#include "jwpqt/core/jwp_document.h"
#include "jwpqt/core/kanji_color.h"
#include "jwpqt/core/kanji_color_list.h"
#include "jwpqt/core/legacy_code_page.h"

class QKeyEvent;
class QInputMethodEvent;
class QPaintEvent;
class QTextDocument;

namespace jwpqt::qt {

QString document_plain_text(const QTextDocument& document);

class JwpEditor final : public QTextEdit {
 public:
  static constexpr int kPageBreakProperty = QTextFormat::UserProperty + 1;

  explicit JwpEditor(QWidget* parent = nullptr);

  void insert_composed_text(std::u32string_view text, bool allow_overwrite = true);
  int character_page_width() const;
  void apply_jwp_layout(const core::JwpDocument& document);
  void clear_jwp_layout();
  void apply_kanji_colors(
      const core::JwpDocument& document,
      const core::KanjiColorList& color_list,
      const core::KanjiColorPolicy& policy,
      core::LegacyCodePage code_page = core::kDefaultLegacyCodePage);
  QList<QTextEdit::ExtraSelection> prepare_kanji_colors(
      const core::JwpDocument& document,
      const core::KanjiColorList& color_list,
      const core::KanjiColorPolicy& policy,
      core::LegacyCodePage code_page = core::kDefaultLegacyCodePage) const;
  void set_kanji_color_selections(
      QList<QTextEdit::ExtraSelection> selections);
  void clear_kanji_colors();
  void set_transient_extra_selections(
      const QList<QTextEdit::ExtraSelection>& selections);

 protected:
  QMimeData* createMimeDataFromSelection() const override;
  void inputMethodEvent(QInputMethodEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void paintEvent(QPaintEvent* event) override;

 private:
  QTextCursor input_cursor(std::u32string_view text, bool allow_overwrite) const;
  qreal indent_unit() const;
  void update_extra_selections();

  QList<QTextEdit::ExtraSelection> kanji_color_selections_;
  QList<QTextEdit::ExtraSelection> transient_extra_selections_;
};

}  // namespace jwpqt::qt
