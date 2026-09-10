// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

#include <QDialog>
#include <QPointer>
#include <QStringList>

#include "jwpqt/core/edict_user_dictionary.h"

class QAction;
class QCloseEvent;
class QDragEnterEvent;
class QDropEvent;
class QLabel;
class QListWidget;
class QPushButton;

namespace jwpqt::qt {

class EdictUserDictionaryDialog : public QDialog {
 public:
  using SaveHandler = std::function<bool(core::EdictUserDictionary)>;
  using InsertHandler = std::function<void(const core::EdictUserEntry&)>;

  explicit EdictUserDictionaryDialog(
      const core::EdictUserDictionary& dictionary,
      core::LegacyCodePage code_page, SaveHandler save_handler,
      InsertHandler insert_handler = {}, QWidget* parent = nullptr,
      QString dictionary_path = {});

  const std::vector<core::EdictUserEntry>& entries() const noexcept;
  std::size_t add_entry(core::EdictUserEntry entry);
  void replace_entry(std::size_t index, core::EdictUserEntry entry);
  void erase_entry(std::size_t index);
  bool move_entry_up(std::size_t index);
  bool move_entry_down(std::size_t index);
  void sort_entries();
  void append_dictionary(const core::EdictUserDictionary& dictionary);
  bool save_changes();
  bool insert_selected();
  void set_overwrite_action(QAction* action);

 protected:
  void closeEvent(QCloseEvent* event) override;
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dropEvent(QDropEvent* event) override;
  virtual std::optional<core::EdictUserEntry> prompt_for_entry(
      const std::optional<core::EdictUserEntry>& initial);
  virtual std::optional<core::EdictUserDictionary> prompt_for_import();

 private:
  static constexpr std::size_t kMaximumVisibleEntries = 100'000;

  void refresh(std::optional<std::size_t> selected = std::nullopt);
  core::EdictUserDictionary read_imports(const QStringList& paths) const;
  void import_paths(const QStringList& paths);
  std::optional<std::size_t> selected_index() const;
  void add_from_prompt();
  void edit_from_prompt();
  void import_from_prompt();
  void show_operation_error(const QString& message);
  void show_unknown_error();
  void update_actions();

  core::EdictUserDictionaryEditor editor_model_;
  core::LegacyCodePage code_page_;
  QPointer<QAction> overwrite_action_;
  SaveHandler save_handler_;
  InsertHandler insert_handler_;
  QString dictionary_path_;
  QListWidget* entries_list_;
  QLabel* status_label_;
  QPushButton* edit_button_;
  QPushButton* delete_button_;
  QPushButton* up_button_;
  QPushButton* down_button_;
  QPushButton* insert_button_;
};

}  // namespace jwpqt::qt
