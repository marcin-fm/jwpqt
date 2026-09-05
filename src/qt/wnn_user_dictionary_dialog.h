// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

#include <QDialog>

#include "jwpqt/core/wnn_user_dictionary.h"

class QLabel;
class QListWidget;
class QPushButton;

namespace jwpqt::qt {

class WnnUserDictionaryDialog : public QDialog {
 public:
  using SaveHandler = std::function<bool(core::WnnUserDictionary)>;
  using InsertHandler = std::function<void(const core::WnnUserEntry&)>;

  explicit WnnUserDictionaryDialog(
      const core::WnnUserDictionary& dictionary, SaveHandler save_handler,
      InsertHandler insert_handler = {}, QWidget* parent = nullptr);

  const std::vector<core::WnnUserEntry>& entries() const noexcept;
  std::size_t add_entry(core::WnnUserEntry entry);
  void replace_entry(std::size_t index, core::WnnUserEntry entry);
  void erase_entry(std::size_t index);
  bool move_entry_up(std::size_t index);
  bool move_entry_down(std::size_t index);
  void sort_entries();
  void append_dictionary(const core::WnnUserDictionary& dictionary);
  bool save_changes();
  bool insert_selected();

 protected:
  virtual std::optional<core::WnnUserEntry> prompt_for_entry(
      const std::optional<core::WnnUserEntry>& initial);
  virtual std::optional<core::WnnUserDictionary> prompt_for_import();

 private:
  static constexpr std::size_t kMaximumVisibleEntries = 100'000;

  void refresh(std::optional<std::size_t> selected = std::nullopt);
  std::optional<std::size_t> selected_index() const;
  void add_from_prompt();
  void edit_from_prompt();
  void import_from_prompt();
  void show_operation_error(const QString& message);
  void update_actions();

  core::WnnUserDictionaryEditor editor_model_;
  SaveHandler save_handler_;
  InsertHandler insert_handler_;
  QListWidget* entries_list_;
  QLabel* status_label_;
  QPushButton* edit_button_;
  QPushButton* delete_button_;
  QPushButton* up_button_;
  QPushButton* down_button_;
  QPushButton* insert_button_;
};

}  // namespace jwpqt::qt
