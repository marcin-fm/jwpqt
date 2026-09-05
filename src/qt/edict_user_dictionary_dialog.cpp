// SPDX-License-Identifier: GPL-2.0-or-later

#include "edict_user_dictionary_dialog.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include "file_io.h"
#include "jwpqt/core/jwp_text_codec.h"
#include "text_bridge.h"

namespace jwpqt::qt {
namespace {

QString display_entry(const core::EdictUserEntry& entry) {
  return to_qstring(core::render_edict_user_entry(entry));
}

QString display_jwp(const core::JwpText& text,
                    core::LegacyCodePage code_page) {
  return to_qstring(core::decode_jwp_text(text, code_page));
}

void validate_entry_encoding(const core::EdictUserEntry& entry,
                             core::LegacyCodePage code_page) {
  const core::EdictUserDictionary dictionary =
      core::EdictUserDictionary::from_entries({entry});
  (void)dictionary.serialize(code_page);
}

}  // namespace

EdictUserDictionaryDialog::EdictUserDictionaryDialog(
    const core::EdictUserDictionary& dictionary,
    core::LegacyCodePage code_page, SaveHandler save_handler,
    InsertHandler insert_handler, QWidget* parent)
    : QDialog(parent),
      editor_model_(dictionary, code_page),
      code_page_(code_page),
      save_handler_(std::move(save_handler)),
      insert_handler_(std::move(insert_handler)),
      entries_list_(new QListWidget(this)),
      status_label_(new QLabel(this)),
      edit_button_(new QPushButton(tr("&Edit..."), this)),
      delete_button_(new QPushButton(tr("&Delete"), this)),
      up_button_(new QPushButton(tr("Move &Up"), this)),
      down_button_(new QPushButton(tr("Move &Down"), this)),
      insert_button_(new QPushButton(tr("&Insert in Document"), this)) {
  if (dictionary.entries().size() > kMaximumVisibleEntries) {
    throw core::EdictUserDictionaryError(
        "User dictionary is too large for interactive editing");
  }

  setObjectName(QStringLiteral("edictUserDictionaryDialog"));
  setWindowTitle(tr("User Dictionary[*]"));
  setWindowModified(false);
  setModal(false);
  resize(780, 500);

  auto* outer = new QVBoxLayout(this);
  auto* content = new QHBoxLayout();
  entries_list_->setObjectName(QStringLiteral("edictUserEntries"));
  entries_list_->setAlternatingRowColors(true);
  content->addWidget(entries_list_, 1);

  auto* actions = new QVBoxLayout();
  auto* add_button = new QPushButton(tr("&Add..."), this);
  add_button->setObjectName(QStringLiteral("edictUserAdd"));
  edit_button_->setObjectName(QStringLiteral("edictUserEdit"));
  delete_button_->setObjectName(QStringLiteral("edictUserDelete"));
  up_button_->setObjectName(QStringLiteral("edictUserMoveUp"));
  down_button_->setObjectName(QStringLiteral("edictUserMoveDown"));
  auto* sort_button = new QPushButton(tr("&Sort"), this);
  sort_button->setObjectName(QStringLiteral("edictUserSort"));
  auto* import_button = new QPushButton(tr("&Import..."), this);
  import_button->setObjectName(QStringLiteral("edictUserImport"));
  insert_button_->setObjectName(QStringLiteral("edictUserInsert"));
  for (QPushButton* button : {add_button, edit_button_, delete_button_,
                              up_button_, down_button_, sort_button,
                              import_button, insert_button_}) {
    actions->addWidget(button);
  }
  actions->addStretch();
  content->addLayout(actions);
  outer->addLayout(content, 1);

  status_label_->setObjectName(QStringLiteral("edictUserStatus"));
  status_label_->setWordWrap(true);
  outer->addWidget(status_label_);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save |
                                           QDialogButtonBox::Cancel,
                                       this);
  buttons->setObjectName(QStringLiteral("edictUserButtons"));
  outer->addWidget(buttons);

  connect(add_button, &QPushButton::clicked, this,
          [this] { add_from_prompt(); });
  connect(edit_button_, &QPushButton::clicked, this,
          [this] { edit_from_prompt(); });
  connect(delete_button_, &QPushButton::clicked, this, [this] {
    if (const auto index = selected_index()) {
      try {
        erase_entry(*index);
      } catch (const std::exception& error) {
        show_operation_error(QString::fromUtf8(error.what()));
      } catch (...) {
        show_unknown_error();
      }
    }
  });
  connect(up_button_, &QPushButton::clicked, this, [this] {
    if (const auto index = selected_index()) {
      try {
        move_entry_up(*index);
      } catch (const std::exception& error) {
        show_operation_error(QString::fromUtf8(error.what()));
      } catch (...) {
        show_unknown_error();
      }
    }
  });
  connect(down_button_, &QPushButton::clicked, this, [this] {
    if (const auto index = selected_index()) {
      try {
        move_entry_down(*index);
      } catch (const std::exception& error) {
        show_operation_error(QString::fromUtf8(error.what()));
      } catch (...) {
        show_unknown_error();
      }
    }
  });
  connect(sort_button, &QPushButton::clicked, this, [this] {
    try {
      sort_entries();
    } catch (const std::exception& error) {
      show_operation_error(QString::fromUtf8(error.what()));
    } catch (...) {
      show_unknown_error();
    }
  });
  connect(import_button, &QPushButton::clicked, this,
          [this] { import_from_prompt(); });
  connect(insert_button_, &QPushButton::clicked, this,
          [this] { insert_selected(); });
  connect(entries_list_, &QListWidget::itemSelectionChanged, this,
          [this] { update_actions(); });
  connect(entries_list_, &QListWidget::itemDoubleClicked, this,
          [this] { edit_from_prompt(); });
  connect(buttons, &QDialogButtonBox::accepted, this, [this] {
    if (save_changes()) {
      accept();
    }
  });
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  refresh();
}

const std::vector<core::EdictUserEntry>&
EdictUserDictionaryDialog::entries() const noexcept {
  return editor_model_.entries();
}

std::size_t EdictUserDictionaryDialog::add_entry(
    core::EdictUserEntry entry) {
  if (editor_model_.entries().size() >= kMaximumVisibleEntries) {
    throw core::EdictUserDictionaryError(
        "User dictionary is too large for interactive editing");
  }
  validate_entry_encoding(entry, code_page_);
  const std::size_t index = editor_model_.add(std::move(entry));
  setWindowModified(true);
  refresh(index);
  return index;
}

void EdictUserDictionaryDialog::replace_entry(
    std::size_t index, core::EdictUserEntry entry) {
  validate_entry_encoding(entry, code_page_);
  editor_model_.replace(index, std::move(entry));
  setWindowModified(true);
  refresh(index);
}

void EdictUserDictionaryDialog::erase_entry(std::size_t index) {
  editor_model_.erase(index);
  setWindowModified(true);
  const std::size_t next =
      std::min(index, editor_model_.entries().empty()
                          ? std::size_t{0}
                          : editor_model_.entries().size() - 1U);
  refresh(editor_model_.entries().empty()
              ? std::nullopt
              : std::optional<std::size_t>{next});
}

bool EdictUserDictionaryDialog::move_entry_up(std::size_t index) {
  if (!editor_model_.move_up(index)) {
    return false;
  }
  setWindowModified(true);
  refresh(index - 1U);
  return true;
}

bool EdictUserDictionaryDialog::move_entry_down(std::size_t index) {
  if (!editor_model_.move_down(index)) {
    return false;
  }
  setWindowModified(true);
  refresh(index + 1U);
  return true;
}

void EdictUserDictionaryDialog::sort_entries() {
  const std::vector<core::EdictUserEntry> before = editor_model_.entries();
  editor_model_.sort();
  if (editor_model_.entries() != before) {
    setWindowModified(true);
  }
  refresh();
}

void EdictUserDictionaryDialog::append_dictionary(
    const core::EdictUserDictionary& dictionary) {
  if (dictionary.entries().size() >
      kMaximumVisibleEntries - editor_model_.entries().size()) {
    throw core::EdictUserDictionaryError(
        "Imported dictionary is too large for interactive editing");
  }
  std::vector<core::EdictUserEntry> combined = editor_model_.entries();
  combined.insert(combined.end(), dictionary.entries().begin(),
                  dictionary.entries().end());
  core::EdictUserDictionary candidate =
      core::EdictUserDictionary::from_entries(std::move(combined));
  (void)candidate.serialize(code_page_);
  editor_model_ = core::EdictUserDictionaryEditor(candidate, code_page_);
  if (!dictionary.entries().empty()) {
    setWindowModified(true);
  }
  refresh();
}

bool EdictUserDictionaryDialog::save_changes() {
  if (!save_handler_) {
    show_operation_error(tr("No dictionary save destination is available"));
    return false;
  }
  try {
    if (!save_handler_(editor_model_.dictionary())) {
      show_operation_error(tr("Could not save the user dictionary"));
      return false;
    }
    setWindowModified(false);
    status_label_->setText(tr("Saved"));
    return true;
  } catch (const std::exception& error) {
    show_operation_error(QString::fromUtf8(error.what()));
    return false;
  } catch (...) {
    show_unknown_error();
    return false;
  }
}

bool EdictUserDictionaryDialog::insert_selected() {
  const auto index = selected_index();
  if (!index.has_value() || !insert_handler_) {
    show_operation_error(tr("Select an entry to insert"));
    return false;
  }
  try {
    const core::EdictUserEntry entry = editor_model_.entries()[*index];
    insert_handler_(entry);
    return true;
  } catch (const std::exception& error) {
    show_operation_error(QString::fromUtf8(error.what()));
    return false;
  } catch (...) {
    show_unknown_error();
    return false;
  }
}

std::optional<core::EdictUserEntry>
EdictUserDictionaryDialog::prompt_for_entry(
    const std::optional<core::EdictUserEntry>& initial) {
  QDialog dialog(this);
  dialog.setWindowTitle(initial ? tr("Edit User Dictionary Entry")
                                : tr("Add User Dictionary Entry"));
  auto* layout = new QVBoxLayout(&dialog);
  auto* form = new QFormLayout();
  auto* headword = new QLineEdit(&dialog);
  headword->setObjectName(QStringLiteral("edictUserHeadword"));
  auto* reading = new QLineEdit(&dialog);
  reading->setObjectName(QStringLiteral("edictUserReading"));
  auto* meaning = new QLineEdit(&dialog);
  meaning->setObjectName(QStringLiteral("edictUserMeaning"));
  if (initial.has_value()) {
    headword->setText(display_jwp(initial->headword, code_page_));
    reading->setText(display_jwp(initial->reading, code_page_));
    meaning->setText(to_qstring(initial->meaning));
  }
  form->addRow(tr("Headword (optional):"), headword);
  form->addRow(tr("Reading:"), reading);
  form->addRow(tr("Meaning:"), meaning);
  layout->addLayout(form);
  auto* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  layout->addWidget(buttons);
  if (dialog.exec() != QDialog::Accepted) {
    return std::nullopt;
  }

  core::EdictUserEntry entry{
      core::encode_jwp_text(from_qstring(headword->text()), code_page_),
      core::encode_jwp_text(from_qstring(reading->text()), code_page_),
      from_qstring(meaning->text())};
  if (initial.has_value() && entry == *initial) {
    return initial;
  }
  return core::make_edict_user_entry(std::move(entry.reading),
                                     std::move(entry.headword),
                                     std::move(entry.meaning));
}

std::optional<core::EdictUserDictionary>
EdictUserDictionaryDialog::prompt_for_import() {
  const QString path = QFileDialog::getOpenFileName(
      this, tr("Import User Dictionary"), {},
      tr("JWP user dictionaries (*.dct);;All files (*)"));
  if (path.isEmpty()) {
    return std::nullopt;
  }
  std::optional<core::EdictUserDictionary> imported =
      read_edict_user_dictionary_file(path, code_page_);
  if (!imported.has_value()) {
    throw core::EdictUserDictionaryError(
        "Selected user dictionary no longer exists");
  }
  return imported;
}

void EdictUserDictionaryDialog::refresh(
    std::optional<std::size_t> selected) {
  entries_list_->clear();
  for (const core::EdictUserEntry& entry : editor_model_.entries()) {
    entries_list_->addItem(display_entry(entry));
  }
  if (selected.has_value() &&
      *selected <= static_cast<std::size_t>(std::numeric_limits<int>::max()) &&
      *selected < editor_model_.entries().size()) {
    entries_list_->setCurrentRow(static_cast<int>(*selected));
  }
  status_label_->clear();
  update_actions();
}

std::optional<std::size_t>
EdictUserDictionaryDialog::selected_index() const {
  const int row = entries_list_->currentRow();
  if (row < 0) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(row);
}

void EdictUserDictionaryDialog::add_from_prompt() {
  try {
    if (auto entry = prompt_for_entry(std::nullopt)) {
      add_entry(std::move(*entry));
    }
  } catch (const std::exception& error) {
    show_operation_error(QString::fromUtf8(error.what()));
  } catch (...) {
    show_unknown_error();
  }
}

void EdictUserDictionaryDialog::edit_from_prompt() {
  const auto index = selected_index();
  if (!index.has_value()) {
    return;
  }
  try {
    if (auto entry = prompt_for_entry(editor_model_.entries()[*index])) {
      replace_entry(*index, std::move(*entry));
    }
  } catch (const std::exception& error) {
    show_operation_error(QString::fromUtf8(error.what()));
  } catch (...) {
    show_unknown_error();
  }
}

void EdictUserDictionaryDialog::import_from_prompt() {
  try {
    if (auto dictionary = prompt_for_import()) {
      append_dictionary(*dictionary);
    }
  } catch (const std::exception& error) {
    show_operation_error(QString::fromUtf8(error.what()));
  } catch (...) {
    show_unknown_error();
  }
}

void EdictUserDictionaryDialog::show_operation_error(const QString& message) {
  status_label_->setText(message);
}

void EdictUserDictionaryDialog::show_unknown_error() {
  show_operation_error(tr("The dictionary operation failed"));
}

void EdictUserDictionaryDialog::update_actions() {
  const auto selected = selected_index();
  edit_button_->setEnabled(selected.has_value());
  delete_button_->setEnabled(selected.has_value());
  up_button_->setEnabled(selected.has_value() && *selected > 0);
  down_button_->setEnabled(
      selected.has_value() && *selected + 1U < editor_model_.entries().size());
  insert_button_->setEnabled(selected.has_value() &&
                             static_cast<bool>(insert_handler_));
}

}  // namespace jwpqt::qt
