// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_QT_EDICT_LOOKUP_DIALOG_H
#define JWPQT_QT_EDICT_LOOKUP_DIALOG_H

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <QDialog>

#include "edict_resource_search.h"

class QAction;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTextEdit;

namespace jwpqt::qt {

class KanaInputField;

struct EdictLookupOptions {
  bool personal_names = false;
  bool place_names = false;
  bool classical = false;
};

class EdictLookupDialog : public QDialog {
 public:
  using SearchHandler = std::function<EdictResourceSearchReport(
      const core::JwpText&, const EdictLookupOptions&)>;
  using InsertHandler = std::function<bool(const std::u32string&)>;
  using InfoHandler = std::function<void(char32_t)>;

  explicit EdictLookupDialog(SearchHandler search_handler,
                             InsertHandler insert_handler = {},
                             QWidget* parent = nullptr,
                             InfoHandler info_handler = {});

  void set_query(std::u32string_view query);
  void set_overwrite_action(QAction* action);
  bool search();
  bool insert_selected();
  void copy_selected();

  const EdictResourceSearchReport& report() const noexcept;

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  static constexpr std::size_t kMaximumVisibleResults = 100'000;

  std::u32string selected_rows() const;
  void update_actions();
  void show_status();

  SearchHandler search_handler_;
  InsertHandler insert_handler_;
  InfoHandler info_handler_;
  KanaInputField* query_field_;
  QLineEdit* query_edit_;
  QCheckBox* personal_names_;
  QCheckBox* place_names_;
  QCheckBox* classical_;
  QTextEdit* results_;
  QLabel* status_;
  QPushButton* insert_button_;
  EdictResourceSearchReport report_;
  std::vector<std::u32string> rendered_rows_;
  std::vector<std::pair<int, int>> row_ranges_;
};

}  // namespace jwpqt::qt

#endif
