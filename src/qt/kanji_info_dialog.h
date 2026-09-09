// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_QT_KANJI_INFO_DIALOG_H
#define JWPQT_QT_KANJI_INFO_DIALOG_H

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include <QDialog>
#include <QColor>

#include "kanji_info_options.h"
#include "jwpqt/core/kanji_info.h"
#include "jwpqt/core/legacy_code_page.h"

class QLabel;
class QPushButton;
class QTableWidget;
class QTextEdit;

namespace jwpqt::qt {

class KanjiInfoDialog : public QDialog {
 public:
  explicit KanjiInfoDialog(
      const core::KanjiInfoDatabase* database,
      std::function<void(char32_t)> show_information, QWidget* parent = nullptr,
      std::function<bool(std::u32string)> insert = {});

  bool set_code(core::JisCode code,
                core::LegacyCodePage code_page = core::kDefaultLegacyCodePage);
  bool set_character(
      char32_t character,
      core::LegacyCodePage code_page = core::kDefaultLegacyCodePage);
  core::JisCode code() const noexcept;
  char32_t character() const noexcept;
  void set_options(const KanjiInfoOptions& options);
  void set_heading_color(const QColor& color);
  static QString field_name(std::uint8_t field);

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  bool populate(char32_t character, std::optional<core::JisCode> code,
                core::LegacyCodePage code_page);
  void populate_fields(const core::KanjiInfoRecord* record);
  void populate_readings(const core::KanjiInfoRecord* record);
  void update_reading_colors();
  void fit_character();
  void insert_text(const QString& text);
  void show_more_info();

  const core::KanjiInfoDatabase* database_;
  std::function<void(char32_t)> show_information_;
  std::function<bool(std::u32string)> insert_;
  core::LegacyCodePage code_page_ = core::kDefaultLegacyCodePage;
  KanjiInfoOptions options_;
  QColor heading_color_;
  std::optional<core::KanjiInfoRecord> record_;
  core::JisCode code_ = 0;
  char32_t unicode_ = 0;
  QLabel* character_;
  QLabel* status_;
  QTableWidget* fields_;
  QTextEdit* readings_;
  QPushButton* insert_button_;
  QPushButton* more_button_;
  QString more_info_;
  QDialog* more_dialog_ = nullptr;
  QTextEdit* more_text_ = nullptr;
};

}  // namespace jwpqt::qt

#endif
