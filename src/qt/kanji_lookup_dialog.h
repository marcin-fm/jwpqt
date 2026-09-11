// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_QT_KANJI_LOOKUP_DIALOG_H
#define JWPQT_QT_KANJI_LOOKUP_DIALOG_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include <QDialog>
#include <QPixmap>

#include "jwpqt/core/kanji_lookup.h"

class QLabel;
class QComboBox;
class QCheckBox;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTimer;
class QToolButton;

namespace jwpqt::qt {

enum class KanjiLookupPageMode { kRadical, kStrokeCount };

class KanjiLookupDialog : public QDialog {
 public:
  using InsertHandler = std::function<void(const std::vector<core::JisCode>&)>;
  using InfoHandler = std::function<void(core::JisCode)>;

  KanjiLookupDialog(const core::KanjiLookupLists& radical_lists,
                    const core::KanjiLookupLists& stroke_lists,
                    const core::KanjiInfoDatabase& information,
                    QPixmap radical_sheet, InsertHandler insert_handler,
                    InfoHandler info_handler, QWidget* parent = nullptr,
                    KanjiLookupPageMode page_mode =
                        KanjiLookupPageMode::kRadical,
                    bool embedded = false);

  void set_selected_radicals(const std::vector<std::size_t>& radicals);
  std::vector<std::size_t> selected_radicals() const;
  void set_stroke_range(std::uint8_t minimum, std::uint8_t maximum);
  bool select_kanji(core::JisCode code);
  void set_lookup_options(bool automatic, bool rare_last);
  void set_deemphasize_radicals(bool enabled) {
    if (deemphasize_radicals_ == enabled) return;
    deemphasize_radicals_ = enabled;
    update_artwork();
  }
  void set_auto_search_handler(std::function<void(bool)> handler);
  void clear();
  bool search();
  std::vector<core::JisCode> result_codes() const;

 protected:
  void changeEvent(QEvent* event) override;

 private:
  void update_artwork();
  bool deemphasize_radicals_ = false;
  void update_stroke_estimate();
  void update_quick_strokes();
  void schedule_search();
  std::vector<core::JisCode> selected_result_codes() const;
  void update_result_actions();
  void copy_results();
  void insert_results();
  void show_information();

  const core::KanjiLookupLists& radical_lists_;
  const core::KanjiLookupLists& stroke_lists_;
  const core::KanjiInfoDatabase& information_;
  KanjiLookupPageMode page_mode_;
  bool embedded_;
  InsertHandler insert_handler_;
  InfoHandler info_handler_;
  std::function<void(bool)> auto_search_handler_;
  bool rare_last_ = true;
  QPixmap radical_sheet_;
  std::vector<QToolButton*> radical_buttons_;
  std::vector<QLabel*> stroke_headings_;
  QCheckBox* automatic_;
  QSpinBox* stroke_count_;
  QComboBox* tolerance_;
  QLabel* stroke_estimate_;
  QSpinBox* minimum_strokes_;
  QSpinBox* maximum_strokes_;
  QTimer* search_timer_;
  QListWidget* results_;
  QLabel* status_;
  QPushButton* copy_button_;
  QPushButton* insert_button_;
  QPushButton* info_button_;
};

}  // namespace jwpqt::qt

#endif
