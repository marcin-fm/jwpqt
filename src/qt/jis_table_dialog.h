// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_QT_JIS_TABLE_DIALOG_H
#define JWPQT_QT_JIS_TABLE_DIALOG_H

#include <functional>
#include <optional>

#include <QDialog>

#include "jwpqt/core/jis_table.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

namespace jwpqt::qt {

class JisTableDialog : public QDialog {
 public:
  using InsertHandler = std::function<void(const core::JisTableEntry&)>;
  using InfoHandler = std::function<void(core::JisCode)>;

  explicit JisTableDialog(InsertHandler insert_handler,
                          InfoHandler info_handler,
                          QWidget* parent = nullptr);

  bool set_jis(core::JisCode code);
  bool set_unicode(char32_t code_point);
  std::optional<core::JisTableEntry> current() const noexcept;

 private:
  void populate_page(std::uint8_t page);
  void select_entry(const core::JisTableEntry& entry);
  void parse_code_field(QLineEdit& field, int kind);
  void update_actions();
  void copy_character();
  void insert_character();
  void show_information();

  InsertHandler insert_handler_;
  InfoHandler info_handler_;
  QSpinBox* page_;
  QTableWidget* table_;
  QLineEdit* jis_;
  QLineEdit* euc_;
  QLineEdit* shift_jis_;
  QLineEdit* unicode_;
  QLabel* status_;
  QPushButton* copy_button_;
  QPushButton* insert_button_;
  QPushButton* info_button_;
  std::optional<core::JisTableEntry> current_;
  bool synchronizing_ = false;
};

}  // namespace jwpqt::qt

#endif
