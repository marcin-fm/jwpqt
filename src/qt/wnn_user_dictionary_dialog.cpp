// SPDX-License-Identifier: GPL-2.0-or-later

#include "wnn_user_dictionary_dialog.h"
#include "auxiliary_find.h"
#include "japanese_fonts.h"
#include "kana_input_field.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

#include <QAction>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMimeData>
#include <QPushButton>
#include <QStringList>
#include <QVBoxLayout>

#include "file_io.h"
#include "jwpqt/core/jwp_text_codec.h"
#include "text_bridge.h"

namespace jwpqt::qt {
namespace {

QString display_text(const core::JwpText& text) {
  try {
    return to_qstring(core::decode_jwp_text(text));
  } catch (const std::exception&) {
    QString result;
    for (const core::JisCode character : text) {
      result += QStringLiteral("<%1>").arg(character, 4, 16, QLatin1Char('0'));
    }
    return result;
  }
}

core::WnnUserInflection inflection_for(const core::WnnUserEntry& entry) {
  if (entry.ending == '1') {
    return core::WnnUserInflection::kIchidan;
  }
  if (entry.ending == 'i') {
    return core::WnnUserInflection::kIAdjective;
  }
  if (entry.ending != '*') {
    return core::WnnUserInflection::kGodan;
  }
  return core::WnnUserInflection::kUninflected;
}

QString display_entry(const core::WnnUserEntry& entry) {
  QString reading = display_text(entry.reading);
  switch (inflection_for(entry)) {
    case core::WnnUserInflection::kGodan:
      reading += QStringLiteral(" [godan]");
      break;
    case core::WnnUserInflection::kIchidan:
      reading += QStringLiteral(" (ichidan)");
      break;
    case core::WnnUserInflection::kIAdjective:
      reading += QStringLiteral(" {i-adjective}");
      break;
    case core::WnnUserInflection::kUninflected:
      break;
  }

  QStringList candidates;
  for (const core::JwpText& candidate : entry.candidates) {
    candidates.push_back(display_text(candidate));
  }
  return QStringLiteral("%1  ->  %2")
      .arg(reading, candidates.join(QStringLiteral(" / ")));
}

std::vector<core::JwpText> parse_candidates(const QString& text) {
  std::vector<core::JwpText> candidates;
  for (const QString& component : text.split(QLatin1Char('/'))) {
    const QString trimmed = component.trimmed();
    if (trimmed.isEmpty()) {
      throw core::WnnUserDictionaryError("Candidates cannot be empty");
    }
    candidates.push_back(core::encode_jwp_text(from_qstring(trimmed)));
  }
  if (candidates.empty()) {
    throw core::WnnUserDictionaryError("At least one candidate is required");
  }
  return candidates;
}

core::WnnUserEntry entry_from_fields(
    core::JwpText reading, std::vector<core::JwpText> candidates,
    core::WnnUserInflection inflection,
    const std::optional<core::WnnUserEntry>& initial) {
  if (initial.has_value() && inflection == inflection_for(*initial) &&
      (initial->ending == '*' ||
       (!reading.empty() && reading.back() == initial->reading.back()))) {
    core::WnnUserEntry entry{std::move(reading), initial->ending,
                             std::move(candidates)};
    core::WnnUserDictionary::from_entries({entry});
    return entry;
  }
  return core::make_wnn_user_entry(std::move(reading), std::move(candidates),
                                   inflection);
}

}  // namespace

WnnUserDictionaryDialog::WnnUserDictionaryDialog(
    const core::WnnUserDictionary& dictionary, SaveHandler save_handler,
    InsertHandler insert_handler, QWidget* parent)
    : QDialog(parent),
      editor_model_(dictionary),
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
    throw core::WnnUserDictionaryError(
        "User dictionary is too large for interactive editing");
  }

  setObjectName(QStringLiteral("wnnUserDictionaryDialog"));
  setWindowTitle(tr("User Conversion Dictionary[*]"));
  setWindowModified(false);
  setModal(false);
  resize(760, 480);
  setAcceptDrops(true);

  auto* outer = new QVBoxLayout(this);
  auto* content = new QHBoxLayout();
  entries_list_->setObjectName(QStringLiteral("wnnUserEntries"));
  assign_japanese_font(*entries_list_, JapaneseFontRole::kList);
  new AuxiliaryFind(entries_list_);
  entries_list_->setAlternatingRowColors(true);
  content->addWidget(entries_list_, 1);

  auto* actions = new QVBoxLayout();
  auto* add_button = new QPushButton(tr("&Add..."), this);
  add_button->setObjectName(QStringLiteral("wnnUserAdd"));
  edit_button_->setObjectName(QStringLiteral("wnnUserEdit"));
  delete_button_->setObjectName(QStringLiteral("wnnUserDelete"));
  up_button_->setObjectName(QStringLiteral("wnnUserMoveUp"));
  down_button_->setObjectName(QStringLiteral("wnnUserMoveDown"));
  auto* sort_button = new QPushButton(tr("&Sort"), this);
  sort_button->setObjectName(QStringLiteral("wnnUserSort"));
  auto* import_button = new QPushButton(tr("&Import..."), this);
  import_button->setObjectName(QStringLiteral("wnnUserImport"));
  insert_button_->setObjectName(QStringLiteral("wnnUserInsert"));
  for (QPushButton* button : {add_button, edit_button_, delete_button_,
                              up_button_, down_button_, sort_button,
                              import_button, insert_button_}) {
    actions->addWidget(button);
  }
  actions->addStretch();
  content->addLayout(actions);
  outer->addLayout(content, 1);

  status_label_->setObjectName(QStringLiteral("wnnUserStatus"));
  status_label_->setWordWrap(true);
  outer->addWidget(status_label_);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save |
                                           QDialogButtonBox::Cancel,
                                       this);
  buttons->setObjectName(QStringLiteral("wnnUserButtons"));
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
      }
    }
  });
  connect(up_button_, &QPushButton::clicked, this, [this] {
    if (const auto index = selected_index()) {
      try {
        move_entry_up(*index);
      } catch (const std::exception& error) {
        show_operation_error(QString::fromUtf8(error.what()));
      }
    }
  });
  connect(down_button_, &QPushButton::clicked, this, [this] {
    if (const auto index = selected_index()) {
      try {
        move_entry_down(*index);
      } catch (const std::exception& error) {
        show_operation_error(QString::fromUtf8(error.what()));
      }
    }
  });
  connect(sort_button, &QPushButton::clicked, this, [this] {
    try {
      sort_entries();
    } catch (const std::exception& error) {
      show_operation_error(QString::fromUtf8(error.what()));
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

void WnnUserDictionaryDialog::dragEnterEvent(QDragEnterEvent* event) {
  if (!event->mimeData()->hasUrls() || event->mimeData()->urls().isEmpty()) return;
  for (const auto& url : event->mimeData()->urls())
    if (!url.isLocalFile() || !QFileInfo(url.toLocalFile()).isFile()) return;
  event->acceptProposedAction();
}

void WnnUserDictionaryDialog::dropEvent(QDropEvent* event) {
  QStringList paths;
  for (const auto& url : event->mimeData()->urls()) {
    if (!url.isLocalFile() || !QFileInfo(url.toLocalFile()).isFile()) return;
    paths.push_back(url.toLocalFile());
  }
  if (paths.isEmpty()) return;
  try {
    import_paths(paths);
  } catch (const std::exception& error) {
    show_operation_error(QString::fromUtf8(error.what()));
  }
  event->acceptProposedAction();
}

const std::vector<core::WnnUserEntry>& WnnUserDictionaryDialog::entries()
    const noexcept {
  return editor_model_.entries();
}

std::size_t WnnUserDictionaryDialog::add_entry(core::WnnUserEntry entry) {
  const std::size_t index = editor_model_.add(std::move(entry));
  setWindowModified(true);
  refresh(index);
  return index;
}

void WnnUserDictionaryDialog::replace_entry(
    std::size_t index, core::WnnUserEntry entry) {
  editor_model_.replace(index, std::move(entry));
  setWindowModified(true);
  refresh(index);
}

void WnnUserDictionaryDialog::erase_entry(std::size_t index) {
  editor_model_.erase(index);
  setWindowModified(true);
  const std::size_t next =
      std::min(index, editor_model_.entries().empty()
                          ? std::size_t{0}
                          : editor_model_.entries().size() - 1);
  refresh(editor_model_.entries().empty()
              ? std::nullopt
              : std::optional<std::size_t>{next});
}

bool WnnUserDictionaryDialog::move_entry_up(std::size_t index) {
  if (!editor_model_.move_up(index)) {
    return false;
  }
  setWindowModified(true);
  refresh(index - 1);
  return true;
}

bool WnnUserDictionaryDialog::move_entry_down(std::size_t index) {
  if (!editor_model_.move_down(index)) {
    return false;
  }
  setWindowModified(true);
  refresh(index + 1);
  return true;
}

void WnnUserDictionaryDialog::sort_entries() {
  const std::vector<core::WnnUserEntry> before = editor_model_.entries();
  editor_model_.sort();
  if (editor_model_.entries() != before) {
    setWindowModified(true);
  }
  refresh();
}

void WnnUserDictionaryDialog::append_dictionary(
    const core::WnnUserDictionary& dictionary) {
  if (dictionary.entries().size() >
      kMaximumVisibleEntries - editor_model_.entries().size()) {
    throw core::WnnUserDictionaryError(
        "Imported dictionary is too large for interactive editing");
  }
  std::vector<core::WnnUserEntry> combined = editor_model_.entries();
  combined.insert(combined.end(), dictionary.entries().begin(),
                  dictionary.entries().end());
  core::WnnUserDictionary candidate =
      core::WnnUserDictionary::from_entries(std::move(combined));
  editor_model_ = core::WnnUserDictionaryEditor(candidate);
  if (!dictionary.entries().empty()) {
    setWindowModified(true);
  }
  refresh();
}

bool WnnUserDictionaryDialog::save_changes() {
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
  }
}

bool WnnUserDictionaryDialog::insert_selected() {
  const auto index = selected_index();
  if (!index.has_value() || !insert_handler_) {
    show_operation_error(tr("Select an entry to insert"));
    return false;
  }
  try {
    const core::WnnUserEntry entry = editor_model_.entries()[*index];
    insert_handler_(entry);
    return true;
  } catch (const std::exception& error) {
    show_operation_error(QString::fromUtf8(error.what()));
    return false;
  }
}

void WnnUserDictionaryDialog::set_overwrite_action(QAction* action) {
  if (action && !action->isCheckable()) {
    throw std::invalid_argument(
        "User dictionary overwrite action must be checkable");
  }
  overwrite_action_ = action;
}

std::optional<core::WnnUserEntry>
WnnUserDictionaryDialog::prompt_for_entry(
    const std::optional<core::WnnUserEntry>& initial) {
  QDialog dialog(this);
  dialog.setWindowTitle(initial ? tr("Edit User Conversion")
                                : tr("Add User Conversion"));
  auto* layout = new QVBoxLayout(&dialog);
  auto* form = new QFormLayout();
  auto* reading =
      new KanaInputField(QStringLiteral("wnnUserReading"), &dialog);
  reading->set_overwrite_action(overwrite_action_);
  auto* candidates =
      new KanaInputField(QStringLiteral("wnnUserCandidates"), &dialog);
  candidates->set_overwrite_action(overwrite_action_);
  auto* inflection = new QComboBox(&dialog);
  inflection->setObjectName(QStringLiteral("wnnUserInflection"));
  inflection->addItem(tr("Uninflected"),
                      static_cast<int>(core::WnnUserInflection::kUninflected));
  inflection->addItem(tr("Godan verb"),
                      static_cast<int>(core::WnnUserInflection::kGodan));
  inflection->addItem(tr("Ichidan verb"),
                      static_cast<int>(core::WnnUserInflection::kIchidan));
  inflection->addItem(tr("i-adjective"),
                      static_cast<int>(core::WnnUserInflection::kIAdjective));
  if (initial.has_value()) {
    reading->edit()->setText(display_text(initial->reading));
    QStringList values;
    for (const core::JwpText& candidate : initial->candidates) {
      values.push_back(display_text(candidate));
    }
    candidates->edit()->setText(values.join(QStringLiteral(" / ")));
    inflection->setCurrentIndex(
        inflection->findData(static_cast<int>(inflection_for(*initial))));
  }
  form->addRow(tr("Reading:"), reading);
  form->addRow(tr("Candidates (separate with /):"), candidates);
  form->addRow(tr("Inflection:"), inflection);
  layout->addLayout(form);
  auto* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  layout->addWidget(buttons);
  if (dialog.exec() != QDialog::Accepted) {
    return std::nullopt;
  }
  reading->finish_input();
  candidates->finish_input();
  return entry_from_fields(
      core::encode_jwp_text(from_qstring(reading->edit()->text().trimmed())),
      parse_candidates(candidates->edit()->text()),
      static_cast<core::WnnUserInflection>(
          inflection->currentData().toInt()),
      initial);
}

std::optional<core::WnnUserDictionary>
WnnUserDictionaryDialog::prompt_for_import() {
  const QStringList paths = QFileDialog::getOpenFileNames(
      this, tr("Import User Conversion Dictionary"), {},
      tr("JWP user conversion dictionaries (*.cnv);;All files (*)"));
  if (paths.isEmpty()) return std::nullopt;
  return read_imports(paths);
}

core::WnnUserDictionary WnnUserDictionaryDialog::read_imports(
    const QStringList& paths) const {
  std::vector<core::WnnUserEntry> entries;
  for (const auto& path : paths) {
    const auto imported = read_wnn_user_dictionary_file(path);
    if (!imported.has_value())
      throw core::WnnUserDictionaryError(
          "Selected user dictionary no longer exists");
    if (imported->entries().size() > kMaximumVisibleEntries - entries.size())
      throw core::WnnUserDictionaryError(
          "Imported dictionary is too large for interactive editing");
    entries.insert(entries.end(), imported->entries().begin(),
                   imported->entries().end());
  }
  return core::WnnUserDictionary::from_entries(std::move(entries));
}

void WnnUserDictionaryDialog::import_paths(const QStringList& paths) {
  const std::size_t before = entries().size();
  append_dictionary(read_imports(paths));
  status_label_->setText(tr("Imported %1 entries from %2 files.")
                             .arg(entries().size() - before)
                             .arg(paths.size()));
}

void WnnUserDictionaryDialog::refresh(
    std::optional<std::size_t> selected) {
  entries_list_->clear();
  for (const core::WnnUserEntry& entry : editor_model_.entries()) {
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

std::optional<std::size_t> WnnUserDictionaryDialog::selected_index() const {
  const int row = entries_list_->currentRow();
  if (row < 0 || static_cast<std::size_t>(row) >= editor_model_.entries().size()) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(row);
}

void WnnUserDictionaryDialog::add_from_prompt() {
  try {
    if (auto entry = prompt_for_entry(std::nullopt)) {
      add_entry(std::move(*entry));
    }
  } catch (const std::exception& error) {
    show_operation_error(QString::fromUtf8(error.what()));
  }
}

void WnnUserDictionaryDialog::edit_from_prompt() {
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
  }
}

void WnnUserDictionaryDialog::import_from_prompt() {
  try {
    if (auto dictionary = prompt_for_import()) {
      append_dictionary(*dictionary);
    }
  } catch (const std::exception& error) {
    show_operation_error(QString::fromUtf8(error.what()));
  }
}

void WnnUserDictionaryDialog::show_operation_error(const QString& message) {
  status_label_->setText(message);
}

void WnnUserDictionaryDialog::update_actions() {
  const auto index = selected_index();
  const bool selected = index.has_value();
  edit_button_->setEnabled(selected);
  delete_button_->setEnabled(selected);
  up_button_->setEnabled(selected && *index > 0);
  down_button_->setEnabled(selected && *index + 1 < entries().size());
  insert_button_->setEnabled(selected && static_cast<bool>(insert_handler_));
}

}  // namespace jwpqt::qt
