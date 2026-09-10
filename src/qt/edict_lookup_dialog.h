// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_QT_EDICT_LOOKUP_DIALOG_H
#define JWPQT_QT_EDICT_LOOKUP_DIALOG_H

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <QDialog>
#include <QColor>

#include "edict_resource_search.h"
#include "edict_lookup_options.h"
#include "jwpqt/core/edict_sort.h"
#include "jwpqt/core/edict_presentation.h"
#include "jwpqt/core/query_history.h"

class QAction;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTextEdit;
class QToolButton;
class QTimer;

namespace jwpqt::qt {

class KanaInputField;

class EdictLookupDialog : public QDialog {
 public:
  using SearchHandler = std::function<EdictResourceSearchReport(
      const core::JwpText&, const EdictLookupOptions&, bool force_contingent)>;
  using InsertHandler = std::function<bool(const std::u32string&)>;
  using InfoHandler = std::function<void(char32_t)>;
  using OptionsHandler = std::function<void(const EdictLookupOptions&)>;

  explicit EdictLookupDialog(SearchHandler search_handler,
                             InsertHandler insert_handler = {},
                             QWidget* parent = nullptr,
                             InfoHandler info_handler = {},
                             std::shared_ptr<EdictLookupOptions> shared_options = {},
                             std::shared_ptr<core::QueryHistory> shared_history = {});

  void set_query(std::u32string_view query);
  void set_highlight_color(const QColor& color);
  std::u32string query() const;
  void set_overwrite_action(QAction* action);
  void set_management_actions(QAction* options, QAction* user_dictionary, QAction* registry = nullptr);
  void set_radical_handler(InfoHandler handler) {
    radical_handler_ = std::move(handler);
  }
  void set_options(const EdictLookupOptions& options);
  void reset_history_navigation() noexcept {
    history_index_ = -1;
    history_changed_ = true;
  }
  void set_options_changed_handler(OptionsHandler handler) {
    options_changed_handler_ = std::move(handler);
  }
  bool search(bool force_contingent = false, bool names_request = false);
  bool search_clipboard();
  bool sort_results(Qt::KeyboardModifiers modifiers = Qt::NoModifier,
                    const core::EdictSortLimits& limits = {});
  bool insert_selected();
  void copy_selected();

  const EdictResourceSearchReport& report() const noexcept;

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;
  void hideEvent(QHideEvent* event) override;

 private:
  static constexpr std::size_t kMaximumVisibleResults = 100'000;
  enum class HistoryCommand { kOlder, kNewer, kList };

  std::u32string selected_rows() const;
  std::optional<std::size_t> current_result_row() const;
  std::optional<std::size_t> result_row_at(int position) const;
  void select_result_row(std::size_t row, bool toggle);
  void clear_result_row_selection();
  void move_result_row_selection(std::size_t row, bool extend, bool preserve);
  void navigate_result_rows(int key, Qt::KeyboardModifiers modifiers);
  void copy_current_result_field(bool reading);
  std::optional<char32_t> current_result_character() const;
  void show_current_information();
  void show_current_radical_lookup();
  void select_current_result(bool whole_row);
  void history_command(HistoryCommand command);
  bool recall_history(std::u32string_view text, int index, bool changed);
  bool publish_results(EdictResourceSearchReport report, int sort_state,
                       bool reverse, bool query_had_kanji, bool compact,
                       core::QueryHistory* history = nullptr,
                       const core::EdictPresentationOptions* presentation = nullptr);
  void update_actions();
  void show_status();
  void update_highlights();
  QColor highlight_color_;
  bool search_clipboard_text(const QString& text);

  SearchHandler search_handler_;
  InsertHandler insert_handler_;
  InfoHandler info_handler_;
  InfoHandler radical_handler_;
  OptionsHandler options_changed_handler_;
  std::shared_ptr<EdictLookupOptions> options_;
  std::shared_ptr<core::QueryHistory> history_;
  int history_index_ = -1;
  bool history_changed_ = false;
  bool history_loading_ = false;
  bool query_busy_ = false;
  int sort_state_ = -1;
  bool sort_reverse_ = false;
  bool query_had_kanji_ = false;
  bool compact_results_ = false;
  KanaInputField* query_field_;
  QLineEdit* query_edit_;
  QCheckBox* personal_names_;
  QCheckBox* place_names_;
  QCheckBox* classical_;
  QCheckBox* beginning_;
  QCheckBox* end_;
  QCheckBox* advanced_;
  QCheckBox* always_;
  QCheckBox* show_all_;
  QCheckBox* i_adjectives_;
  QCheckBox* full_ascii_;
  QCheckBox* jascii_to_ascii_;
  QCheckBox* contingent_;
  QCheckBox* no_names_;
  QTimer* clipboard_timer_;
  QString clipboard_text_;
  std::vector<std::pair<QCheckBox*, bool EdictLookupOptions::*>> option_bindings_;
  QTextEdit* results_;
  QLabel* status_;
  QPushButton* insert_button_;
  QPushButton* sort_button_;
  QToolButton* options_button_;
  QToolButton* user_dictionary_button_;
  QToolButton* registry_button_;
  EdictResourceSearchReport report_;
  std::vector<std::u32string> rendered_rows_;
  std::vector<std::pair<int, int>> row_ranges_;
  std::vector<std::size_t> display_order_;
  std::vector<std::size_t> selected_result_rows_;
  std::optional<std::size_t> result_selection_anchor_;
  bool updating_result_selection_ = false;
};

}  // namespace jwpqt::qt

#endif
