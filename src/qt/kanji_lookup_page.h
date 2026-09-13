// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_QT_KANJI_LOOKUP_PAGE_H
#define JWPQT_QT_KANJI_LOOKUP_PAGE_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include <QPixmap>
#include <QWidget>

#include "jwpqt/core/kanji_lookup.h"

class QComboBox;
class QLabel;
class QSpinBox;
class QToolButton;

namespace jwpqt::qt {

enum class KanjiLookupPageMode { kRadical, kStrokeCount };

class KanjiLookupPage final : public QWidget {
 public:
  using ChangeHandler = std::function<void()>;
  using ErrorHandler = std::function<void(const QString&)>;

  KanjiLookupPage(const core::KanjiLookupLists& radical_lists,
                  const core::KanjiLookupLists& stroke_lists,
                  const core::KanjiInfoDatabase& information,
                  QPixmap radical_sheet, KanjiLookupPageMode page_mode,
                  QWidget* parent = nullptr);

  void set_change_handler(ChangeHandler handler);
  void set_error_handler(ErrorHandler handler);
  void set_selected_radicals(const std::vector<std::size_t>& radicals);
  std::vector<std::size_t> selected_radicals() const;
  void set_stroke_range(std::uint8_t minimum, std::uint8_t maximum);
  bool select_kanji(core::JisCode code);
  void set_deemphasize_radicals(bool enabled);
  void clear();
  core::KanjiLookupReport search(bool rare_last) const;

 protected:
  void changeEvent(QEvent* event) override;

 private:
  void notify_change();
  void update_artwork();
  void update_stroke_estimate();
  void update_quick_strokes();

  const core::KanjiLookupLists& radical_lists_;
  const core::KanjiLookupLists& stroke_lists_;
  const core::KanjiInfoDatabase& information_;
  KanjiLookupPageMode page_mode_;
  ChangeHandler change_handler_;
  ErrorHandler error_handler_;
  bool deemphasize_radicals_ = false;
  bool suppress_changes_ = false;
  QPixmap radical_sheet_;
  std::vector<QToolButton*> radical_buttons_;
  std::vector<QLabel*> stroke_headings_;
  QSpinBox* stroke_count_;
  QComboBox* tolerance_;
  QLabel* stroke_estimate_;
  QSpinBox* minimum_strokes_;
  QSpinBox* maximum_strokes_;
};

}  // namespace jwpqt::qt

#endif
