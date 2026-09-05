// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_QT_KANJI_INFO_DIALOG_H
#define JWPQT_QT_KANJI_INFO_DIALOG_H

#include <cstdint>

#include <QDialog>

#include "jwpqt/core/kanji_info.h"

class QLabel;
class QListWidget;
class QTableWidget;

namespace jwpqt::qt {

class KanjiInfoDialog : public QDialog {
 public:
  explicit KanjiInfoDialog(const core::KanjiInfoDatabase& database,
                           QWidget* parent = nullptr);

  bool set_code(core::JisCode code);
  core::JisCode code() const noexcept;

 private:
  void populate_fields(const core::KanjiInfoRecord& record,
                       std::uint32_t unicode);
  void populate_readings(const core::KanjiInfoRecord& record);

  const core::KanjiInfoDatabase& database_;
  core::JisCode code_ = 0;
  QLabel* character_;
  QLabel* status_;
  QTableWidget* fields_;
  QListWidget* readings_;
  QListWidget* meanings_;
};

}  // namespace jwpqt::qt

#endif
