// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <string_view>
#include <vector>

#include <QDialog>

#include "jwpqt/core/kanji_reading_search.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;

namespace jwpqt::qt {

class KanjiReadingLookupDialog : public QDialog {
 public:
  using InsertHandler = std::function<void(const std::vector<core::JisCode>&)>;
  using InfoHandler = std::function<void(core::JisCode)>;

  KanjiReadingLookupDialog(const core::KanjiInfoDatabase& information,
                           InsertHandler insert_handler,
                           InfoHandler info_handler,
                           QWidget* parent = nullptr);

  void set_query(const core::KanjiReadingQuery& query);
  void set_query_text(std::u32string_view text);
  bool search();
  std::vector<core::JisCode> results() const;

 private:
  bool publish(core::KanjiCodeSearchReport report);
  std::vector<core::JisCode> selected_codes() const;
  void update_mode();
  void update_actions();
  void copy_results();
  void insert_results();
  void show_information();

  const core::KanjiInfoDatabase& information_;
  InsertHandler insert_handler_;
  InfoHandler info_handler_;
  QComboBox* kind_;
  QLineEdit* query_;
  QSpinBox* minimum_strokes_;
  QSpinBox* maximum_strokes_;
  QCheckBox* flexible_kun_;
  QCheckBox* partial_words_;
  QListWidget* results_;
  QLabel* status_;
  QPushButton* copy_button_;
  QPushButton* insert_button_;
  QPushButton* info_button_;
};

}  // namespace jwpqt::qt
