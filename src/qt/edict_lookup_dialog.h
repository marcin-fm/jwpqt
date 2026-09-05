// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_QT_EDICT_LOOKUP_DIALOG_H
#define JWPQT_QT_EDICT_LOOKUP_DIALOG_H

#include <functional>
#include <string>
#include <vector>

#include <QDialog>

#include "edict_resource_search.h"

class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

namespace jwpqt::qt {

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

  explicit EdictLookupDialog(SearchHandler search_handler,
                             InsertHandler insert_handler = {},
                             QWidget* parent = nullptr);

  void set_query(std::u32string_view query);
  bool search();
  bool insert_selected();
  void copy_selected();

  const EdictResourceSearchReport& report() const noexcept;

 private:
  static constexpr std::size_t kMaximumVisibleResults = 100'000;

  std::u32string selected_rows() const;
  void update_actions();
  void show_status();

  SearchHandler search_handler_;
  InsertHandler insert_handler_;
  QLineEdit* query_edit_;
  QCheckBox* personal_names_;
  QCheckBox* place_names_;
  QCheckBox* classical_;
  QListWidget* results_;
  QLabel* status_;
  QPushButton* insert_button_;
  EdictResourceSearchReport report_;
  std::vector<std::u32string> rendered_rows_;
};

}  // namespace jwpqt::qt

#endif
