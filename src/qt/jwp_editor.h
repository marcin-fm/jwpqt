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
class QMouseEvent;
class QTextDocument;
class QTimer;

namespace jwpqt::qt {

QString document_plain_text(const QTextDocument& document);

class JwpEditor final : public QTextEdit {
 public:
  static constexpr int kPageBreakProperty = QTextFormat::UserProperty + 1;

  explicit JwpEditor(QWidget* parent = nullptr);

  void insert_composed_text(std::u32string_view text, bool allow_overwrite = true);
  int character_page_width() const;
  void apply_jwp_layout(const core::JwpDocument& document);
  void apply_jwp_fonts(const core::JwpDocument& document, core::LegacyCodePage code_page);
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
  void set_selection_autoscroll(bool enabled, int interval_ms);
  bool selection_autoscroll_enabled() const noexcept;
  int selection_autoscroll_interval() const noexcept;

 protected:
  QMimeData* createMimeDataFromSelection() const override;
  void inputMethodEvent(QInputMethodEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void paintEvent(QPaintEvent* event) override;

 private:
  QTextCursor input_cursor(std::u32string_view text, bool allow_overwrite) const;
  qreal indent_unit() const;
  void extend_mouse_selection(int x, int y);
  void scroll_mouse_selection();
  void stop_mouse_autoscroll();
  void update_extra_selections();

  QList<QTextEdit::ExtraSelection> kanji_color_selections_;
  bool kanji_list_coloring_ = false;
  core::LegacyCodePage color_code_page_ = core::kDefaultLegacyCodePage;
  QList<QTextEdit::ExtraSelection> transient_extra_selections_;
  QTimer* selection_scroll_timer_;
  bool selection_autoscroll_ = true;
  bool mouse_selecting_ = false;
  int selection_scroll_interval_ = 100;
  int selection_scroll_direction_ = 0;
  int selection_scroll_x_ = 0;
};

}  // namespace jwpqt::qt
