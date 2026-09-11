// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_QT_KANJI_CODE_LOOKUP_DIALOG_H
#define JWPQT_QT_KANJI_CODE_LOOKUP_DIALOG_H

#include <functional>
#include <utility>
#include <vector>

#include <QDialog>
#include <QPixmap>

#include "jwpqt/core/kanji_code_search.h"
#include "jwpqt/core/kanji_lookup.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QTimer;

namespace jwpqt::qt {

class KanjiLookupDialog;

class KanjiCodeLookupDialog : public QDialog {
 public:
  using InsertHandler = std::function<void(const std::vector<core::JisCode>&)>;
  using InfoHandler = std::function<void(core::JisCode)>;

  KanjiCodeLookupDialog(const core::KanjiInfoDatabase& information,
                         InsertHandler insert_handler,
                          InfoHandler info_handler,
                          QWidget* parent = nullptr,
                          QPixmap radical_sheet = QPixmap{},
                          const core::KanjiLookupLists* radical_lists = nullptr,
                          const core::KanjiLookupLists* stroke_lists = nullptr);

  void set_skip_query(const core::KanjiSkipQuery& query);
  void set_four_corner_query(const core::KanjiFourCornerQuery& query);
  void set_bushu_query(const core::KanjiBushuQuery& query);
  void set_spahn_query(const core::KanjiSpahnQuery& query);
  void set_index_query(const core::KanjiIndexQuery& query);
  void set_automatic_search(bool automatic);
  void set_auto_search_handler(std::function<void(bool)> handler);
  void set_search_preferences(bool nelson, bool classical, bool miscodes, int index_type);
  void set_radical_preferences(bool reduce, bool deemphasize);
  void set_radical_lookup_options(bool automatic, bool rare_last);
  void set_variants_handler(std::function<void(bool)> handler) { variants_handler_ = std::move(handler); }
  void set_preferences_handler(std::function<void(bool, bool, bool, int)> handler) {
    preferences_handler_ = std::move(handler);
  }
  void select_skip_mode();
  void select_four_corner_mode();
  void select_bushu_mode();
  void select_spahn_mode();
  void select_stroke_bushu_mode();
  void select_index_mode();
  void select_radical_mode();
  void select_stroke_mode();
  bool select_radical_character(core::JisCode code);
  bool search_skip();
  bool search_four_corner();
  bool search_bushu();
  bool search_spahn();
  bool search_stroke_bushu();
  bool search_index();
  bool search_radical();
  bool search_stroke();
  std::vector<core::KanjiCodeMatch> results() const;

 protected:
  void changeEvent(QEvent* event) override;

 private:
  void update_artwork();
  bool publish(core::KanjiCodeSearchReport report);
  std::vector<core::JisCode> selected_codes() const;
  void update_actions();
  void copy_results();
  void insert_results();
  void show_information();
  void populate_stroke_bushu_choices();
  void populate_spahn_choices();
  bool search_current();
  void clear_current();

  const core::KanjiInfoDatabase& information_;
  InsertHandler insert_handler_;
  InfoHandler info_handler_;
  std::function<void(bool)> auto_search_handler_;
  std::function<void(bool, bool, bool, int)> preferences_handler_;
  int preferred_index_ = 0;
  bool deemphasize_radicals_ = false;
  bool rare_last_ = true;
  std::function<void(bool)> variants_handler_;
  QCheckBox* automatic_;
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
  QListWidget* bushu_radicals_;
  QSpinBox* spahn_radical_strokes_;
  QSpinBox* spahn_radical_;
  QSpinBox* spahn_other_strokes_;
  QSpinBox* spahn_index_;
  QCheckBox* spahn_variants_;
  QListWidget* spahn_radicals_;
  QSpinBox* stroke_bushu_radical_strokes_;
  QCheckBox* stroke_bushu_variants_;
  QListWidget* stroke_bushu_radicals_;
  QSpinBox* stroke_bushu_minimum_strokes_;
  QSpinBox* stroke_bushu_maximum_strokes_;
  QCheckBox* stroke_bushu_nelson_;
  QCheckBox* stroke_bushu_classical_;
  QComboBox* index_type_;
  QSpinBox* index_value_;
  QSpinBox* index_volume_;
  QTimer* search_timer_;
  QPixmap radical_sheet_;
  KanjiLookupDialog* radical_lookup_page_ = nullptr;
  KanjiLookupDialog* stroke_lookup_page_ = nullptr;
  QListWidget* results_;
  QLabel* status_;
  QPushButton* copy_button_;
  QPushButton* insert_button_;
  QPushButton* info_button_;
};

}  // namespace jwpqt::qt

#endif
