// SPDX-License-Identifier: GPL-2.0-or-later

#include "edict_lookup_dialog.h"

#include <algorithm>
#include <exception>
#include <memory>
#include <utility>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QScopeGuard>
#include <QSignalBlocker>
#include <QStringList>
#include <QTextBlockFormat>
#include <QTextDocument>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QValidator>

#include "jwpqt/core/jwp_text_codec.h"
#include "character_context_menu.h"
#include "kana_input_field.h"
#include "text_bridge.h"
#include "japanese_fonts.h"

namespace jwpqt::qt {
namespace {

std::u32string render_row(const core::EdictRecord& record) {
  std::u32string row = record.headword;
  if (!record.readings.empty()) {
    row.append(U" [");
    for (std::size_t i = 0; i < record.readings.size(); ++i) {
      if (i != 0) {
        row.append(U"; ");
      }
      row.append(record.readings[i]);
    }
    row.push_back(U']');
  }
  row.append(U" /");
  for (const std::u32string& definition : record.definitions) {
    row.append(definition);
    row.push_back(U'/');
  }
  return row;
}

QString pluralized(std::size_t value, const QString& singular,
                   const QString& plural) {
  return QStringLiteral("%1 %2")
      .arg(static_cast<qulonglong>(value))
      .arg(value == 1 ? singular : plural);
}

}  // namespace

EdictLookupDialog::EdictLookupDialog(SearchHandler search_handler,
                                      InsertHandler insert_handler,
                                      QWidget* parent, InfoHandler info_handler,
                                      std::shared_ptr<EdictLookupOptions> shared_options,
                                      std::shared_ptr<core::QueryHistory> shared_history)
    : QDialog(parent),
      search_handler_(std::move(search_handler)),
      insert_handler_(std::move(insert_handler)),
      info_handler_(std::move(info_handler)),
      options_(shared_options ? std::move(shared_options)
                              : std::make_shared<EdictLookupOptions>()),
      history_(shared_history ? std::move(shared_history)
                              : std::make_shared<core::QueryHistory>()),
      query_field_(new KanaInputField(QStringLiteral("edictQuery"), this)),
      query_edit_(query_field_->edit()),
      personal_names_(new QCheckBox(tr("Personal &names"), this)),
      place_names_(new QCheckBox(tr("Place na&mes"), this)),
      classical_(new QCheckBox(tr("&Classical"), this)),
      beginning_(new QCheckBox(tr("&Begin With"), this)),
      end_(new QCheckBox(tr("&End With"), this)),
      advanced_(new QCheckBox(tr("&Advanced"), this)),
      always_(new QCheckBox(tr("Always Search"), this)),
      show_all_(new QCheckBox(tr("Show All"), this)),
      i_adjectives_(new QCheckBox(tr("I-adjectives"), this)),
      full_ascii_(new QCheckBox(tr("&Full ASCII"), this)),
      jascii_to_ascii_(new QCheckBox(tr("JASCII to ASCII"), this)),
      results_(new QTextEdit(this)),
      status_(new QLabel(this)),
      insert_button_(new QPushButton(tr("&Insert in Document"), this)),
      sort_button_(new QPushButton(tr("S&ort"), this)) {
  setObjectName(QStringLiteral("edictLookupDialog"));
  setWindowTitle(tr("Dictionary Lookup"));
  setModal(false);
  resize(780, 560);

  auto* outer = new QVBoxLayout(this);
  auto* query_row = new QHBoxLayout();
  query_edit_->setObjectName(QStringLiteral("edictQuery"));
  query_edit_->setClearButtonEnabled(true);
  query_edit_->installEventFilter(this);
  connect(query_edit_, &QLineEdit::textChanged, this, [this] {
    if (!history_loading_) history_changed_ = true;
  });
  auto* history_button = new QPushButton(tr("&History"), this);
  history_button->setObjectName(QStringLiteral("edictHistory"));
  history_button->setToolTip(tr("Recall a query without searching. Up/Down navigate query history."));
  connect(history_button, &QPushButton::clicked, this,
          [this] { history_command(HistoryCommand::kList); });
  auto* search_button = new QPushButton(tr("&Search"), this);
  search_button->setObjectName(QStringLiteral("edictSearch"));
  search_button->setDefault(true);
  query_row->addWidget(query_field_, 1);
  query_row->addWidget(history_button);
  query_row->addWidget(search_button);
  outer->addLayout(query_row);

  beginning_->setObjectName(QStringLiteral("edictBeginning"));
  end_->setObjectName(QStringLiteral("edictEnd"));
  advanced_->setObjectName(QStringLiteral("edictAdvanced"));
  always_->setObjectName(QStringLiteral("edictAdvancedAlways"));
  show_all_->setObjectName(QStringLiteral("edictAdvancedShowAll"));
  i_adjectives_->setObjectName(QStringLiteral("edictIAdjectives"));
  full_ascii_->setObjectName(QStringLiteral("edictFullAscii"));
  jascii_to_ascii_->setObjectName(QStringLiteral("edictJasciiToAscii"));
  advanced_->setToolTip(tr("Search inflected forms using adaptive deinflection. Wildcard syntax is independent."));
  full_ascii_->setToolTip(tr("Apply Begin/End With to the complete definition, not individual ASCII words."));
  auto* boundaries = new QHBoxLayout();
  boundaries->addWidget(beginning_);
  boundaries->addWidget(end_);
  boundaries->addWidget(advanced_);
  boundaries->addStretch();
  boundaries->addWidget(full_ascii_);
  boundaries->addWidget(jascii_to_ascii_);
  outer->addLayout(boundaries);

  auto* options = new QHBoxLayout();
  personal_names_->setObjectName(QStringLiteral("edictPersonalNames"));
  place_names_->setObjectName(QStringLiteral("edictPlaceNames"));
  classical_->setObjectName(QStringLiteral("edictClassical"));
  options->addWidget(personal_names_);
  options->addWidget(place_names_);
  options->addWidget(classical_);
  options->addStretch();
  auto* advanced_controls = new QWidget(this);
  auto* advanced_row = new QHBoxLayout(advanced_controls);
  advanced_row->setContentsMargins(0, 0, 0, 0);
  advanced_row->addWidget(always_);
  advanced_row->addWidget(show_all_);
  advanced_row->addWidget(i_adjectives_);
  options->addWidget(advanced_controls);
  outer->addLayout(options);

  const std::pair<QCheckBox*, bool EdictLookupOptions::*> bindings[] = {
      {personal_names_, &EdictLookupOptions::personal_names},
      {place_names_, &EdictLookupOptions::place_names},
      {classical_, &EdictLookupOptions::classical},
      {beginning_, &EdictLookupOptions::require_beginning},
      {end_, &EdictLookupOptions::require_end},
      {advanced_, &EdictLookupOptions::advanced},
      {always_, &EdictLookupOptions::advanced_always},
      {show_all_, &EdictLookupOptions::advanced_show_all},
      {i_adjectives_, &EdictLookupOptions::i_adjectives},
      {full_ascii_, &EdictLookupOptions::full_ascii},
      {jascii_to_ascii_, &EdictLookupOptions::jascii_to_ascii}};
  for (const auto& binding : bindings) {
    auto* checkbox = binding.first;
    checkbox->setChecked((*options_).*binding.second);
    connect(checkbox, &QCheckBox::toggled, this,
            [this, member = binding.second](bool checked) { (*options_).*member = checked; });
  }
  advanced_controls->setEnabled(advanced_->isChecked());
  connect(advanced_, &QCheckBox::toggled, advanced_controls, &QWidget::setEnabled);

  results_->setObjectName(QStringLiteral("edictResults"));
  results_->setReadOnly(true);
  results_->setUndoRedoEnabled(false);
  QFont content_font = results_->font();
  content_font.setPixelSize(16);
  results_->setFont(content_font);
  assign_japanese_font(*results_, JapaneseFontRole::kList);
  results_->installEventFilter(this);
  results_->viewport()->installEventFilter(this);
  outer->addWidget(results_, 1);

  status_->setObjectName(QStringLiteral("edictStatus"));
  status_->setWordWrap(true);
  outer->addWidget(status_);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  insert_button_->setObjectName(QStringLiteral("edictInsert"));
  sort_button_->setObjectName(QStringLiteral("edictSort"));
  sort_button_->setToolTip(tr("Cycle Reading, Length, Entry and Definition. Shift cycles backward; Ctrl reverses the current order."));
  buttons->addButton(sort_button_, QDialogButtonBox::ActionRole);
  buttons->addButton(insert_button_, QDialogButtonBox::ActionRole);
  outer->addWidget(buttons);

  auto* copy_action = new QAction(tr("&Copy"), this);
  copy_action->setObjectName(QStringLiteral("edictCopy"));
  copy_action->setShortcut(QKeySequence::Copy);
  copy_action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  results_->addAction(copy_action);

  connect(search_button, &QPushButton::clicked, this,
          [this] { search(); });
  connect(insert_button_, &QPushButton::clicked, this,
          [this] { insert_selected(); });
  connect(sort_button_, &QPushButton::clicked, this,
          [this] { sort_results(QApplication::keyboardModifiers()); });
  connect(results_, &QTextEdit::selectionChanged, this,
          [this] { update_actions(); });
  connect(copy_action, &QAction::triggered, this,
          [this] { copy_selected(); });
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  status_->setText(tr("Enter a search term."));
  update_actions();
}

void EdictLookupDialog::set_overwrite_action(QAction* action) {
  query_field_->set_overwrite_action(action);
}

void EdictLookupDialog::set_query(std::u32string_view query) {
  query_edit_->setText(to_qstring(query));
  query_edit_->selectAll();
}

bool EdictLookupDialog::search() {
  if (query_busy_) return false;
  const QPointer<EdictLookupDialog> self(this);
  query_busy_ = true;
  const auto idle = qScopeGuard([self] { if (self) self->query_busy_ = false; });
  query_field_->finish_input();
  if (!self) return false;
  if (!search_handler_) {
    status_->setText(tr("Dictionary resources are unavailable."));
    return false;
  }
  if (query_edit_->text().isEmpty()) {
    status_->setText(tr("Enter a search term."));
    query_edit_->setFocus();
    return false;
  }

  try {
    const QString original_query = query_edit_->text();
    const std::u32string history_text = from_qstring(original_query);
    if (to_qstring(history_text) != original_query) {
      throw core::QueryHistoryError("The query contains invalid Unicode");
    }
    const core::JwpText query = core::encode_jwp_text(history_text);
    const EdictLookupOptions options = *options_;
    const auto handler = search_handler_;
    EdictResourceSearchReport candidate = handler(query, options);
    if (!self) return false;
    if (query_edit_->text() != original_query) {
      throw core::EdictSearchError("The query changed during the search");
    }
    if (candidate.results.size() > kMaximumVisibleResults) {
      throw core::EdictSearchError(
          "Dictionary search returned too many interactive results");
    }

    core::QueryHistory history = *history_;
    history.remember(history_text);
    const bool had_kanji = std::any_of(query.begin(), query.end(),
        [](core::JisCode code) { return code >= 0x3000U; });
    if (!publish_results(std::move(candidate), -1, false, had_kanji, &history)) return false;
    show_status();
    if (!history_->find(history_text)) {
      status_->setText(status_->text() + tr("; query was not retained in bounded history"));
    }
    update_actions();
    return true;
  } catch (const std::exception& error) {
    if (!self) return false;
    status_->setText(tr("Search failed: %1").arg(QString::fromUtf8(error.what())));
    query_edit_->setFocus();
    return false;
  } catch (...) {
    if (!self) return false;
    status_->setText(tr("Search failed with an unknown error."));
    query_edit_->setFocus();
    return false;
  }
}

bool EdictLookupDialog::sort_results(Qt::KeyboardModifiers modifiers,
                                    const core::EdictSortLimits& limits) {
  if (query_busy_ || report_.results.empty()) return false;
  const QPointer<EdictLookupDialog> self(this);
  query_busy_ = true;
  const auto idle = qScopeGuard([self] { if (self) self->query_busy_ = false; });
  try {
    int state = sort_state_;
    bool reverse = sort_reverse_;
    if (modifiers.testFlag(Qt::ControlModifier)) {
      if (state < 0) state = 0;
      reverse = !reverse;
    } else {
      state += modifiers.testFlag(Qt::ShiftModifier) ? -1 : 1;
      if (state < 0) state = 3;
      if (state > 3) state = 0;
    }
    core::EdictSortOptions options;
    options.mode = static_cast<core::EdictSortMode>(state);
    options.reverse = reverse;
    options.headword_length = query_had_kanji_;
    std::vector<std::reference_wrapper<const core::EdictRecord>> records;
    records.reserve(report_.results.size());
    for (const auto& result : report_.results) records.emplace_back(result.result.record);
    const auto order = core::sort_edict_records(records, options, limits);
    EdictResourceSearchReport candidate = report_;
    candidate.results.clear();
    candidate.results.reserve(order.size());
    for (std::size_t index : order) candidate.results.push_back(report_.results[index]);
    if (!publish_results(std::move(candidate), state, reverse, query_had_kanji_)) return false;
    show_status();
    update_actions();
    return true;
  } catch (const std::exception& error) {
    if (self) status_->setText(tr("Sort failed: %1").arg(QString::fromUtf8(error.what())));
    return false;
  } catch (...) {
    if (self) status_->setText(tr("Sort failed with an unknown error."));
    return false;
  }
}

bool EdictLookupDialog::publish_results(EdictResourceSearchReport candidate,
                                       int sort_state, bool reverse,
                                       bool query_had_kanji,
                                       core::QueryHistory* history) {
  std::vector<std::u32string> rows;
  rows.reserve(candidate.results.size());
  for (const auto& result : candidate.results) rows.push_back(render_row(result.result.record));

  auto document = std::make_unique<QTextDocument>();
  document->setDefaultFont(results_->font());
  document->setUndoRedoEnabled(false);
  QTextCursor cursor(document.get());
  std::vector<std::pair<int, int>> ranges;
  ranges.reserve(rows.size());
  for (const auto& result : candidate.results) {
    if (!ranges.empty()) cursor.insertBlock();
    const int start = cursor.position();
    cursor.setBlockFormat(QTextBlockFormat{});
    QTextCharFormat format;
    format.setToolTip(result.label);
    const auto& record = result.result.record;
    QString headword = to_qstring(record.headword);
    if (!record.readings.empty()) {
      QStringList readings;
      for (const auto& reading : record.readings) readings.push_back(to_qstring(reading));
      headword += QStringLiteral(" [%1]").arg(readings.join(QStringLiteral("; ")));
    }
    cursor.insertText(headword, format);
    cursor.insertBlock();
    QTextBlockFormat definition;
    definition.setLeftMargin(16);
    cursor.setBlockFormat(definition);
    QStringList meanings;
    for (const auto& meaning : record.definitions) meanings.push_back(to_qstring(meaning));
    cursor.insertText(meanings.join(QStringLiteral("; ")), format);
    ranges.emplace_back(start, cursor.position());
  }

  // Publish all logical state before widget signals can invoke external handlers.
  report_ = std::move(candidate);
  rendered_rows_ = std::move(rows);
  row_ranges_ = std::move(ranges);
  sort_state_ = sort_state;
  sort_reverse_ = reverse;
  query_had_kanji_ = query_had_kanji;
  if (history) {
    *history_ = std::move(*history);
    history_index_ = -1;
    history_changed_ = true;
  }
  const QPointer<EdictLookupDialog> self(this);
  const QPointer<QTextDocument> previous = results_->document();
  document->setParent(results_);
  results_->setDocument(document.release());
  if (!self) return false;
  if (previous && previous->parent() == results_) delete previous.data();
  if (!self) return false;
  if (!rendered_rows_.empty()) {
    QTextCursor selected(results_->document());
    selected.setPosition(row_ranges_.front().second, QTextCursor::KeepAnchor);
    results_->setTextCursor(selected);
    if (!self) return false;
    results_->setFocus();
  } else {
    query_edit_->setFocus();
  }
  return self != nullptr;
}

bool EdictLookupDialog::recall_history(std::u32string_view text, int index,
                                     bool changed) {
  if (query_edit_->isReadOnly()) return false;
  const QPointer<EdictLookupDialog> self(this);
  const QString original = query_edit_->text();
  const int original_cursor = query_edit_->cursorPosition();
  const int original_selection = query_edit_->selectionStart();
  const int original_length = static_cast<int>(query_edit_->selectedText().size());
  const QString recalled = to_qstring(text);
  if (recalled.size() > query_edit_->maxLength()) {
    throw core::QueryHistoryError("The history query exceeds this field's length limit");
  }
  if (const auto* validator = query_edit_->validator()) {
    QString checked = recalled;
    int position = static_cast<int>(checked.size());
    if (validator->validate(checked, position) == QValidator::Invalid) {
      throw core::QueryHistoryError("The history query is not valid for this field");
    }
  }
  if (!self) return false;
  if (query_edit_->isReadOnly() || query_edit_->text() != original ||
      query_edit_->cursorPosition() != original_cursor ||
      query_edit_->selectionStart() != original_selection ||
      query_edit_->selectedText().size() != original_length ||
      recalled.size() > query_edit_->maxLength()) {
    history_index_ = -1;
    history_changed_ = true;
    status_->setText(tr("The input field changed during history validation."));
    return false;
  }
  history_loading_ = true;
  const auto loaded = qScopeGuard([self] { if (self) self->history_loading_ = false; });
  query_edit_->setText(recalled);
  if (!self) return false;
  if (query_edit_->text() != recalled || query_edit_->isReadOnly()) {
    history_changed_ = true;
    history_index_ = -1;
    status_->setText(tr("History recall was changed by the input field."));
    return false;
  }
  query_edit_->setCursorPosition(static_cast<int>(recalled.size()));
  if (!self) return false;
  if (query_edit_->text() != recalled || query_edit_->isReadOnly()) {
    history_changed_ = true;
    history_index_ = -1;
    return false;
  }
  history_index_ = index;
  history_changed_ = changed;
  query_edit_->setFocus();
  return self != nullptr;
}

void EdictLookupDialog::history_command(HistoryCommand command) {
  if (query_busy_ || query_edit_->isReadOnly()) return;
  const QPointer<EdictLookupDialog> self(this);
  query_busy_ = true;
  const auto idle = qScopeGuard([self] { if (self) self->query_busy_ = false; });
  query_field_->finish_input();
  if (!self) return;
  try {
    if (history_->entries().empty()) {
      status_->setText(tr("Query history is empty."));
      return;
    }
    if (command == HistoryCommand::kOlder) {
      const core::QueryHistory before = *history_;
      core::QueryHistory candidate = before;
      int index = history_index_ + 1;
      if (history_changed_) {
        const QString current = query_edit_->text();
        const std::u32string draft = from_qstring(current);
        if (to_qstring(draft) != current) {
          throw core::QueryHistoryError("The edited query contains invalid Unicode");
        }
        if (!draft.empty()) {
          if (draft.size() > candidate.maximum_text_cells()) {
            throw core::QueryHistoryError("The edited query is too long to retain; it was not replaced");
          }
          candidate.remember(draft);
          index = 1;
        } else {
          index = 0;
        }
      }
      index = std::clamp(index, 0, static_cast<int>(candidate.entries().size()) - 1);
      if (!recall_history(candidate.entries()[static_cast<std::size_t>(index)], index, false)) return;
      if (history_->entries() == before.entries() &&
          history_->storage_cells() == before.storage_cells()) {
        *history_ = std::move(candidate);
      } else {
        history_index_ = -1;
        history_changed_ = true;
      }
      return;
    }
    if (command == HistoryCommand::kNewer && !history_changed_ && history_index_ >= 0) {
      const int index = std::min(history_index_, static_cast<int>(history_->entries().size())) - 1;
      const std::u32string text = index < 0 ? std::u32string{} :
          history_->entries()[static_cast<std::size_t>(index)];
      recall_history(text, index, index < 0);
      return;
    }

    // The resource owner may close this dialog while the chooser's event loop runs.
    QPointer<QDialog> chooser = new QDialog(this);
    const auto dispose = qScopeGuard([chooser] { delete chooser.data(); });
    chooser->setObjectName(QStringLiteral("edictHistoryDialog"));
    chooser->setWindowTitle(tr("Dictionary Query History"));
    chooser->resize(620, 420);
    auto* layout = new QVBoxLayout(chooser);
    auto* list = new QListWidget(chooser);
    list->setObjectName(QStringLiteral("edictHistoryList"));
    list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    QFont font = list->font();
    font.setPixelSize(16);
    list->setFont(font);
    assign_japanese_font(*list, JapaneseFontRole::kList);
    layout->addWidget(list, 1);
    auto* note = new QLabel(tr("Choose a query without searching. Deletions take effect immediately, even on Cancel."), chooser);
    note->setWordWrap(true);
    layout->addWidget(note);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, chooser);
    auto* remove = buttons->addButton(tr("&Delete"), QDialogButtonBox::ActionRole);
    remove->setObjectName(QStringLiteral("edictHistoryDelete"));
    layout->addWidget(buttons);
    const auto history = history_;
    const auto populate = [history, list, buttons, remove] {
      const int row = std::max(0, list->currentRow());
      const QSignalBlocker blocked(list);
      list->clear();
      for (const auto& entry : history->entries()) list->addItem(to_qstring(entry));
      if (list->count() != 0) list->setCurrentRow(std::min(row, list->count() - 1));
      buttons->button(QDialogButtonBox::Ok)->setEnabled(list->count() != 0);
      remove->setEnabled(list->count() != 0);
    };
    populate();
    auto* copy = new QAction(tr("&Copy"), chooser);
    copy->setObjectName(QStringLiteral("edictHistoryCopy"));
    copy->setShortcut(QKeySequence::Copy);
    copy->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    list->addAction(copy);
    list->setContextMenuPolicy(Qt::ActionsContextMenu);
    connect(copy, &QAction::triggered, list, [list] {
      QStringList text;
      for (int row = 0; row < list->count(); ++row) {
        if (list->item(row)->isSelected()) text.push_back(list->item(row)->text());
      }
      if (!text.empty()) QApplication::clipboard()->setText(text.join(QLatin1Char('\n')));
    });
    auto* erase = new QAction(tr("&Delete"), chooser);
    erase->setShortcut(QKeySequence(Qt::Key_Delete));
    erase->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    list->addAction(erase);
    connect(remove, &QPushButton::clicked, erase, &QAction::trigger);
    connect(erase, &QAction::triggered, list, [history, list, note, populate] {
      try {
        core::QueryHistory candidate = *history;
        auto selected = list->selectedItems();
        if (selected.empty() && list->currentItem()) selected.push_back(list->currentItem());
        for (auto* item : selected) {
          if (const auto index = candidate.find(from_qstring(item->text()))) candidate.remove(*index);
        }
        *history = std::move(candidate);
        populate();
      } catch (const std::exception& error) {
        note->setText(QString::fromUtf8(error.what()));
      }
    });
    connect(buttons, &QDialogButtonBox::accepted, chooser, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, chooser, &QDialog::reject);
    connect(list, &QListWidget::itemDoubleClicked, chooser, &QDialog::accept);
    const int result = chooser->exec();
    if (!self || !chooser) return;
    const QString selected = list->currentItem() ? list->currentItem()->text() : QString{};
    delete chooser.data();
    history_index_ = -1;
    history_changed_ = true;
    if (result == QDialog::Accepted && !selected.isEmpty()) {
      recall_history(from_qstring(selected), -1, true);
    }
  } catch (const std::exception& error) {
    if (self) status_->setText(tr("History: %1").arg(QString::fromUtf8(error.what())));
  }
}

bool EdictLookupDialog::insert_selected() {
  const std::u32string rows = selected_rows();
  if (!insert_handler_ || rows.empty()) {
    return false;
  }
  try {
    if (!insert_handler_(rows)) {
      status_->setText(tr("The selected entry could not be inserted."));
      return false;
    }
    return true;
  } catch (const std::exception& error) {
    status_->setText(
        tr("Insert failed: %1").arg(QString::fromUtf8(error.what())));
    return false;
  } catch (...) {
    status_->setText(tr("Insert failed with an unknown error."));
    return false;
  }
}

void EdictLookupDialog::copy_selected() {
  results_->copy();
}

const EdictResourceSearchReport& EdictLookupDialog::report() const noexcept {
  return report_;
}

std::u32string EdictLookupDialog::selected_rows() const {
  std::u32string rows;
  const QTextCursor cursor = results_->textCursor();
  if (!cursor.hasSelection()) return rows;
  for (std::size_t row = 0; row < row_ranges_.size(); ++row) {
    if (cursor.selectionEnd() <= row_ranges_[row].first ||
        cursor.selectionStart() >= row_ranges_[row].second) {
      continue;
    }
    if (!rows.empty()) {
      rows.push_back(U'\n');
    }
    rows.append(rendered_rows_[static_cast<std::size_t>(row)]);
  }
  return rows;
}

void EdictLookupDialog::update_actions() {
  insert_button_->setEnabled(insert_handler_ && results_->textCursor().hasSelection());
  sort_button_->setEnabled(!report_.results.empty());
}

bool EdictLookupDialog::eventFilter(QObject* watched, QEvent* event) {
  if (watched == query_edit_ && (event->type() == QEvent::ShortcutOverride ||
                                event->type() == QEvent::KeyPress)) {
    auto* key = static_cast<QKeyEvent*>(event);
    if (key->modifiers() == Qt::NoModifier &&
        (key->key() == Qt::Key_Up || key->key() == Qt::Key_Down)) {
      if (event->type() == QEvent::KeyPress) {
        history_command(key->key() == Qt::Key_Up ? HistoryCommand::kOlder : HistoryCommand::kNewer);
      }
      event->accept();
      return true;
    }
  }
  if (watched == results_ || watched == results_->viewport()) {
    if (event->type() == QEvent::MouseButtonPress &&
        static_cast<QMouseEvent*>(event)->button() == Qt::RightButton) return true;
    if (event->type() == QEvent::ContextMenu && info_handler_) {
      show_character_context_menu(*results_, *static_cast<QContextMenuEvent*>(event),
          [this](CharacterTarget target) { info_handler_(target.character); });
      return true;
    }
  }
  return QDialog::eventFilter(watched, event);
}

void EdictLookupDialog::show_status() {
  QString text;
  if (report_.results.empty()) {
    text = tr("No matches");
  } else {
    text = pluralized(report_.results.size(), tr("match"), tr("matches"));
  }
  if (report_.rejected != 0) {
    text += tr("; %1 rejected").arg(
        static_cast<qulonglong>(report_.rejected));
  }
  if (sort_state_ >= 0) {
    const QString modes[] = {tr("Reading"), tr("Length"), tr("Entry"), tr("Definition")};
    text += tr("; %1 order").arg(modes[sort_state_]);
    if (sort_reverse_) text += tr(" (reversed)");
  }

  QStringList failures;
  for (const EdictResourceFailure& failure : report_.failures) {
    if (!failure.quiet) {
      failures.push_back(tr("%1: %2").arg(failure.source_path, failure.message));
    }
  }
  if (!failures.empty()) {
    text += tr("; %1").arg(failures.join(QStringLiteral(" | ")));
  }
  status_->setText(text);
}

}  // namespace jwpqt::qt
