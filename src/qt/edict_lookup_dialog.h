// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_QT_EDICT_LOOKUP_DIALOG_H
#define JWPQT_QT_EDICT_LOOKUP_DIALOG_H

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <QDialog>

#include "edict_resource_search.h"
#include "edict_lookup_options.h"
#include "jwpqt/core/edict_sort.h"
#include "jwpqt/core/query_history.h"

class QAction;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTextEdit;

namespace jwpqt::qt {

class KanaInputField;

class EdictLookupDialog : public QDialog {
 public:
  using SearchHandler = std::function<EdictResourceSearchReport(
      const core::JwpText&, const EdictLookupOptions&)>;
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
  void set_overwrite_action(QAction* action);
  void set_options(const EdictLookupOptions& options);
  void reset_history_navigation() noexcept {
    history_index_ = -1;
    history_changed_ = true;
  }
  void set_options_changed_handler(OptionsHandler handler) {
    options_changed_handler_ = std::move(handler);
  }
  bool search();
  bool sort_results(Qt::KeyboardModifiers modifiers = Qt::NoModifier,
                    const core::EdictSortLimits& limits = {});
  bool insert_selected();
  void copy_selected();

  const EdictResourceSearchReport& report() const noexcept;

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  static constexpr std::size_t kMaximumVisibleResults = 100'000;
  enum class HistoryCommand { kOlder, kNewer, kList };

  std::u32string selected_rows() const;
  void history_command(HistoryCommand command);
  bool recall_history(std::u32string_view text, int index, bool changed);
  bool publish_results(EdictResourceSearchReport report, int sort_state,
                       bool reverse, bool query_had_kanji,
                       core::QueryHistory* history = nullptr);
  void update_actions();
  void show_status();

  SearchHandler search_handler_;
  InsertHandler insert_handler_;
  InfoHandler info_handler_;
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
  std::vector<std::pair<QCheckBox*, bool EdictLookupOptions::*>> option_bindings_;
  QTextEdit* results_;
  QLabel* status_;
  QPushButton* insert_button_;
  QPushButton* sort_button_;
  EdictResourceSearchReport report_;
  std::vector<std::u32string> rendered_rows_;
  std::vector<std::pair<int, int>> row_ranges_;
};

}  // namespace jwpqt::qt

#endif
