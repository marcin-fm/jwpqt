// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_QT_KANJI_CODE_LOOKUP_DIALOG_H
#define JWPQT_QT_KANJI_CODE_LOOKUP_DIALOG_H

#include <functional>
#include <vector>

#include <QDialog>

#include "jwpqt/core/kanji_code_search.h"

class QCheckBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTabWidget;

namespace jwpqt::qt {

class KanjiCodeLookupDialog : public QDialog {
 public:
  using InsertHandler = std::function<void(const std::vector<core::JisCode>&)>;
  using InfoHandler = std::function<void(core::JisCode)>;

  KanjiCodeLookupDialog(const core::KanjiInfoDatabase& information,
                        InsertHandler insert_handler,
                        InfoHandler info_handler,
                        QWidget* parent = nullptr);

  void set_skip_query(const core::KanjiSkipQuery& query);
  void set_four_corner_query(const core::KanjiFourCornerQuery& query);
  void set_bushu_query(const core::KanjiBushuQuery& query);
  void set_spahn_query(const core::KanjiSpahnQuery& query);
  void select_skip_mode();
  void select_four_corner_mode();
  void select_bushu_mode();
  void select_spahn_mode();
  bool search_skip();
  bool search_four_corner();
  bool search_bushu();
  bool search_spahn();
  std::vector<core::KanjiCodeMatch> results() const;

 private:
  bool publish(core::KanjiCodeSearchReport report);
  std::vector<core::JisCode> selected_codes() const;
  void update_actions();
  void copy_results();
  void insert_results();
  void show_information();

  const core::KanjiInfoDatabase& information_;
  InsertHandler insert_handler_;
  InfoHandler info_handler_;
  QTabWidget* tabs_;
  QSpinBox* skip_type_;
  QSpinBox* skip_first_;
  QSpinBox* skip_second_;
  QCheckBox* skip_misclassifications_;
  std::vector<QSpinBox*> corner_digits_;
  QSpinBox* bushu_radical_;
  QSpinBox* bushu_strokes_;
  QCheckBox* bushu_nelson_;
  QCheckBox* bushu_classical_;
  QSpinBox* spahn_radical_strokes_;
  QSpinBox* spahn_radical_;
  QSpinBox* spahn_other_strokes_;
  QSpinBox* spahn_index_;
  QListWidget* results_;
  QLabel* status_;
  QPushButton* copy_button_;
  QPushButton* insert_button_;
  QPushButton* info_button_;
};

}  // namespace jwpqt::qt

#endif
