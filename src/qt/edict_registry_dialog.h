// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDialog>
#include <QList>
#include <QUrl>
#include "jwpqt/core/edict_registry.h"
#include "jwpqt/core/legacy_code_page.h"

class QListWidget;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QLabel;
class QPushButton;
class QDragEnterEvent;
class QDropEvent;

namespace jwpqt::qt {

class EdictRegistryDialog : public QDialog {
 public:
  EdictRegistryDialog(core::EdictRegistry registry, QString directory,
                      core::LegacyCodePage code_page, QWidget* parent = nullptr);
  const core::EdictRegistry& registry() const noexcept { return registry_; }
  bool allow_unavailable() const;
  void accept() override;

 protected:
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dropEvent(QDropEvent* event) override;

 private:
  void refresh(int row);
  void select_entry();
  void update_entry();
  void inspect_entry();
  bool detect_entry(int row, const QString& filename, bool replace_name);
  void add_dictionary_files(const QList<QUrl>& urls);
  void defaults();
  core::EdictRegistry registry_;
  QString directory_;
  core::LegacyCodePage code_page_;
  bool updating_ = false;
  QListWidget* list_;
  QLineEdit* name_;
  QLineEdit* path_;
  QComboBox* encoding_;
  QComboBox* names_;
  QComboBox* special_;
  QCheckBox* searched_;
  QCheckBox* indexed_;
  QCheckBox* buffered_;
  QCheckBox* keep_;
  QCheckBox* quiet_;
  QCheckBox* allow_;
  QLabel* status_;
  QPushButton* add_;
  QPushButton* remove_;
  QPushButton* up_;
  QPushButton* down_;
};

}  // namespace jwpqt::qt
