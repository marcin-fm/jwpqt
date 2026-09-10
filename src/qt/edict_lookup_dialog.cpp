// SPDX-License-Identifier: GPL-2.0-or-later
#include "source_highlight.h"
#include <QTextBlock>
#include <QTextFragment>

#include "edict_lookup_dialog.h"
#include "auxiliary_find.h"

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
#include <QHideEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QMimeData>
#include <QPointer>
#include <QPushButton>
#include <QScopeGuard>
#include <QSignalBlocker>
#include <QStringList>
#include <QTextBlockFormat>
#include <QTextDocument>
#include <QToolButton>
#include <QTimer>
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

class NoNamesCheckBox final : public QCheckBox {
 public:
  using QCheckBox::QCheckBox;
 protected:
  void nextCheckState() override {
    setCheckState(checkState() == Qt::Unchecked ? Qt::Checked : Qt::Unchecked);
  }
};

class ResultTextEdit final : public QTextEdit {
 public:
  using QTextEdit::QTextEdit;

 protected:
  QMimeData* createMimeDataFromSelection() const override {
    const QTextCursor cursor = textCursor();
    std::unique_ptr<QMimeData> data(QTextEdit::createMimeDataFromSelection());
    // Materialize Qt's lazy selection, retaining its other clipboard formats.
    (void)data->text();
    // The default plain-text conversion normalizes NBSP into ordinary spaces.
    QString text = cursor.selectedText();
    text.replace(QChar::ParagraphSeparator, QLatin1Char('\n'));
    data->setText(text);
    data->setProperty("jwpqtInternalCopy", true);
    return data.release();
  }
};

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

void set_internal_clipboard_text(const QString& text) {
  auto* data = new QMimeData();
  data->setText(text);
  data->setProperty("jwpqtInternalCopy", true);
  QApplication::clipboard()->setMimeData(data);
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
      contingent_(new QCheckBox(tr("Contingent"), this)),
      no_names_(new NoNamesCheckBox(tr("No Names"), this)),
      clipboard_timer_(new QTimer(this)),
      results_(new ResultTextEdit(this)),
      status_(new QLabel(this)),
      insert_button_(new QPushButton(tr("&Insert in Document"), this)),
      sort_button_(new QPushButton(tr("S&ort"), this)),
      options_button_(new QToolButton(this)),
      user_dictionary_button_(new QToolButton(this)),
      registry_button_(new QToolButton(this)) {
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
    clipboard_timer_->stop();
    if (!history_loading_) history_changed_ = true;
  });
  connect(query_edit_, &QLineEdit::selectionChanged, clipboard_timer_, &QTimer::stop);
  auto* history_button = new QPushButton(tr("&History"), this);
  history_button->setObjectName(QStringLiteral("edictHistory"));
  history_button->setToolTip(tr("Recall a query without searching. Up/Down navigate query history."));
  connect(history_button, &QPushButton::clicked, this,
          [this] { history_command(HistoryCommand::kList); });
  auto* search_button = new QPushButton(tr("&Search"), this);
  search_button->setObjectName(QStringLiteral("edictSearch"));
  search_button->setDefault(true);
  search_button->setToolTip(tr("Shift+Search requests a contingent retry without changing the saved policy. Exact Japanese searches require Begin and End With."));
  query_row->addWidget(query_field_, 1);
  query_row->addWidget(history_button);
  query_row->addWidget(search_button);
  auto* names_button = new QPushButton(tr("Names"), this);
  names_button->setObjectName(QStringLiteral("edictNamesSearch"));
  names_button->setToolTip(tr("Search with personal and place names included, without advanced or contingent retries. Saved options are unchanged."));
  query_row->addWidget(names_button);
  connect(names_button, &QPushButton::clicked, this, [this] { search(false, true); });
  outer->addLayout(query_row);

  beginning_->setObjectName(QStringLiteral("edictBeginning"));
  end_->setObjectName(QStringLiteral("edictEnd"));
  advanced_->setObjectName(QStringLiteral("edictAdvanced"));
  always_->setObjectName(QStringLiteral("edictAdvancedAlways"));
  show_all_->setObjectName(QStringLiteral("edictAdvancedShowAll"));
  i_adjectives_->setObjectName(QStringLiteral("edictIAdjectives"));
  full_ascii_->setObjectName(QStringLiteral("edictFullAscii"));
  jascii_to_ascii_->setObjectName(QStringLiteral("edictJasciiToAscii"));
  contingent_->setObjectName(QStringLiteral("edictContingent"));
  contingent_->setToolTip(tr("Retry eligible unsuccessful exact Japanese searches. Requires Begin and End With; does not expand ASCII or wildcard searches."));
  advanced_->setToolTip(tr("Search inflected forms using adaptive deinflection. Wildcard syntax is independent."));
  full_ascii_->setToolTip(tr("Apply Begin/End With to the complete definition, not individual ASCII words."));
  auto* boundaries = new QHBoxLayout();
  boundaries->addWidget(beginning_);
  boundaries->addWidget(end_);
  boundaries->addWidget(advanced_);
  boundaries->addWidget(contingent_);
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
  no_names_->setObjectName(QStringLiteral("edictNoNames"));
  no_names_->setTristate(true);
  no_names_->setToolTip(tr("Checked excludes both name categories; a mixed state preserves the separate personal/place choices."));
  options->addWidget(no_names_);
  connect(no_names_, &QCheckBox::clicked, this, [this] {
    const QPointer<EdictLookupDialog> self(this);
    const bool include = no_names_->checkState() == Qt::Unchecked;
    options_->personal_names = include;
    options_->place_names = include;
    if (include && options_->link_advanced_names) options_->advanced = false;
    set_options(*options_);
    if (self && options_changed_handler_) {
      const auto handler = options_changed_handler_;
      const auto options = *options_;
      handler(options);
    }
  });
  options->addStretch();
  auto* advanced_controls = new QWidget(this);
  auto* advanced_row = new QHBoxLayout(advanced_controls);
  advanced_row->setContentsMargins(0, 0, 0, 0);
  advanced_row->addWidget(always_);
  advanced_row->addWidget(show_all_);
  advanced_row->addWidget(i_adjectives_);
  options->addWidget(advanced_controls);
  outer->addLayout(options);

  auto* presentation_row = new QHBoxLayout();
  auto* priority = new QCheckBox(tr("Priority entries first"), this);
  auto* priority_mark = new QCheckBox(tr("Priority separator"), this);
  auto* advanced_mark = new QCheckBox(tr("Mark advanced results"), this);
  priority->setObjectName(QStringLiteral("edictPriority"));
  priority_mark->setObjectName(QStringLiteral("edictPrioritySeparator"));
  advanced_mark->setObjectName(QStringLiteral("edictAdvancedSeparator"));
  presentation_row->addWidget(priority);
  presentation_row->addWidget(priority_mark);
  presentation_row->addWidget(advanced_mark);
  presentation_row->addStretch();
  outer->addLayout(presentation_row);

  auto* clipboard_row = new QHBoxLayout();
  auto* monitor = new QCheckBox(tr("Monitor Clipboard"), this);
  monitor->setObjectName(QStringLiteral("edictMonitorClipboard"));
  monitor->setToolTip(tr("Search future external copies while this window is visible. Successful queries may enter saved history. Opening or enabling does not read existing clipboard content."));
  auto* from_clipboard = new QPushButton(tr("From Clipboard"), this);
  from_clipboard->setObjectName(QStringLiteral("edictFromClipboard"));
  clipboard_row->addWidget(monitor);
  clipboard_row->addWidget(from_clipboard);
  clipboard_row->addStretch();
  outer->addLayout(clipboard_row);
  connect(from_clipboard, &QPushButton::clicked, this, [this] { search_clipboard(); });
  clipboard_timer_->setSingleShot(true);
  clipboard_timer_->setInterval(150);
  connect(this, &QDialog::finished, clipboard_timer_, &QTimer::stop);
  connect(QApplication::clipboard(), &QClipboard::dataChanged, this, [this] {
    clipboard_timer_->stop();
    if (!options_->monitor_clipboard || !isVisible() || query_busy_ ||
        QApplication::activeModalWidget() || QApplication::clipboard()->ownsClipboard()) return;
    const auto* mime = QApplication::clipboard()->mimeData();
    if (mime && mime->property("jwpqtInternalCopy").toBool()) return;
    const QPointer<EdictLookupDialog> self(this);
    const QString text = QApplication::clipboard()->text().left(201);
    if (self && options_->monitor_clipboard && isVisible() && !query_busy_) {
      clipboard_text_ = text;
      clipboard_timer_->start();
    }
  });
  connect(clipboard_timer_, &QTimer::timeout, this, [this] {
    if (!options_->monitor_clipboard || !isVisible() || query_busy_ ||
        QApplication::activeModalWidget() || QApplication::clipboard()->ownsClipboard()) return;
    search_clipboard_text(clipboard_text_);
  });

  option_bindings_ = {
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
      {jascii_to_ascii_, &EdictLookupOptions::jascii_to_ascii},
      {contingent_, &EdictLookupOptions::contingent},
      {priority, &EdictLookupOptions::priority_first},
      {priority_mark, &EdictLookupOptions::priority_separator},
      {advanced_mark, &EdictLookupOptions::advanced_separator},
      {monitor, &EdictLookupOptions::monitor_clipboard}};
  for (const auto& binding : option_bindings_) {
    auto* checkbox = binding.first;
    checkbox->setChecked((*options_).*binding.second);
    connect(checkbox, &QCheckBox::toggled, this,
            [this, member = binding.second](bool checked) {
              const QPointer<EdictLookupDialog> self(this);
              (*options_).*member = checked;
              if (options_->link_advanced_names && checked) {
                if (member == &EdictLookupOptions::advanced) {
                  options_->personal_names = false;
                  options_->place_names = false;
                } else if (member == &EdictLookupOptions::personal_names ||
                           member == &EdictLookupOptions::place_names) {
                  options_->advanced = false;
                }
              }
              set_options(*options_);
              if (self && options_changed_handler_) {
                auto handler = options_changed_handler_;
                const auto options = *options_;
                handler(options);
              }
            });
  }
  advanced_controls->setEnabled(advanced_->isChecked());
  set_options(*options_);

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
  status_->setTextFormat(Qt::PlainText);
  status_->setWordWrap(true);
  outer->addWidget(status_);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  insert_button_->setObjectName(QStringLiteral("edictInsert"));
  sort_button_->setObjectName(QStringLiteral("edictSort"));
  sort_button_->setToolTip(tr("Cycle Reading, Length, Entry and Definition. Shift cycles backward; Ctrl reverses the current order."));
  buttons->addButton(sort_button_, QDialogButtonBox::ActionRole);
  buttons->addButton(insert_button_, QDialogButtonBox::ActionRole);
  options_button_->setObjectName(QStringLiteral("edictOptions"));
  user_dictionary_button_->setObjectName(QStringLiteral("edictUserDictionary"));
  registry_button_->setObjectName(QStringLiteral("edictRegistry"));
  for (auto* button : {options_button_, user_dictionary_button_, registry_button_}) {
    button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    buttons->addButton(button, QDialogButtonBox::ActionRole);
  }
  set_management_actions(nullptr, nullptr);
  outer->addWidget(buttons);

  auto* copy_action = new QAction(tr("&Copy"), this);
  copy_action->setObjectName(QStringLiteral("edictCopy"));
  copy_action->setShortcut(QKeySequence::Copy);
  copy_action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  copy_action->setProperty("jwpqtResultCommand", true);
  results_->addAction(copy_action);

  connect(search_button, &QPushButton::clicked, this,
          [this] { search(QApplication::keyboardModifiers().testFlag(Qt::ShiftModifier)); });
  connect(insert_button_, &QPushButton::clicked, this,
          [this] { insert_selected(); });
  connect(sort_button_, &QPushButton::clicked, this,
          [this] { sort_results(QApplication::keyboardModifiers()); });
  connect(results_, &QTextEdit::selectionChanged, this,
          [this] {
            if (!updating_result_selection_ &&
                !selected_result_rows_.empty() &&
                results_->textCursor().hasSelection()) {
              clear_result_row_selection();
              update_highlights();
            }
            update_actions();
          });
  connect(copy_action, &QAction::triggered, this,
          [this] { copy_selected(); });
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  status_->setText(tr("Enter a search term."));
  auto* find = new AuxiliaryFind(results_, [this] {
    std::vector<std::pair<int, int>> ranges;
    ranges.reserve(display_order_.size());
    for (const auto row : display_order_) ranges.push_back(row_ranges_.at(row));
    return ranges;
  });
  find->set_result_insertion(insert_button_);
  update_actions();
}

void EdictLookupDialog::set_overwrite_action(QAction* action) {
  query_field_->set_overwrite_action(action);
}

void EdictLookupDialog::set_management_actions(QAction* options, QAction* user_dictionary, QAction* registry) {
  for (const auto& binding : {std::make_pair(options_button_, options),
                             std::make_pair(user_dictionary_button_, user_dictionary),
                             std::make_pair(registry_button_, registry)}) {
    auto* button = binding.first;
    const QPointer<QAction> source(binding.second);
    delete button->defaultAction();
    auto* proxy = new QAction(button == options_button_ ? tr("Options...") :
        button == registry_button_ ? tr("Dictionaries...") : tr("User Dictionary..."), button);
    proxy->setEnabled(source && source->isEnabled());
    button->setDefaultAction(proxy);
    if (!source) continue;
    connect(source, &QAction::changed, proxy, [source, proxy] {
      proxy->setEnabled(source && source->isEnabled());
    });
    connect(source, &QObject::destroyed, proxy, [proxy] { proxy->setEnabled(false); });
    connect(proxy, &QAction::triggered, this, [this, source] {
      if (!query_busy_ && source && source->isEnabled()) source->trigger();
    });
  }
}

void EdictLookupDialog::set_options(const EdictLookupOptions& options) {
  *options_ = options;
  for (const auto& binding : option_bindings_) {
    const QSignalBlocker blocked(binding.first);
    binding.first->setChecked((*options_).*binding.second);
  }
  {
    const QSignalBlocker blocked(no_names_);
    no_names_->setCheckState(options_->personal_names != options_->place_names
        ? Qt::PartiallyChecked : options_->personal_names ? Qt::Unchecked : Qt::Checked);
  }
  if (!options_->monitor_clipboard) clipboard_timer_->stop();
  always_->parentWidget()->setEnabled(options_->advanced);
}

void EdictLookupDialog::set_query(std::u32string_view query) {
  const QPointer<EdictLookupDialog> self(this);
  const QString text = to_qstring(query);
  query_edit_->setText(text);
  if (self && query_edit_->text() == text) query_edit_->selectAll();
}

std::u32string EdictLookupDialog::query() const {
  return from_qstring(query_edit_->text());
}

bool EdictLookupDialog::search_clipboard() {
  const QPointer<EdictLookupDialog> self(this);
  const QString text = QApplication::clipboard()->text().left(201);
  return self && search_clipboard_text(text);
}

bool EdictLookupDialog::search_clipboard_text(const QString& clipboard) {
  if (query_busy_ || query_edit_->isReadOnly()) return false;
  const QPointer<EdictLookupDialog> self(this);
  query_busy_ = true;
  const auto idle = qScopeGuard([self] { if (self) self->query_busy_ = false; });
  try {
    const QString previous_query = query_edit_->text();
    int end = 0;
    while (end < clipboard.size() && end <= 200 && clipboard[end] != QLatin1Char('\r') &&
           clipboard[end] != QLatin1Char('\n') && clipboard[end] != QChar::LineSeparator &&
           clipboard[end] != QChar::ParagraphSeparator) ++end;
    QString text = clipboard.left(end);
    const auto scalars = from_qstring(text);
    if (text.isEmpty() || text.size() > query_edit_->maxLength() ||
        scalars.size() > 100 || to_qstring(scalars) != text) {
      throw core::EdictSearchError("Clipboard must contain a valid single query of at most 100 characters");
    }
    const auto plan = core::prepare_edict_search_plan(core::encode_jwp_text(scalars),
                                                    {options_->jascii_to_ascii});
    if (plan.input_truncated) throw core::EdictSearchError("Clipboard query would be truncated");
    if (const auto* validator = query_edit_->validator()) {
      int position = text.size();
      QString validated = text;
      if (validator->validate(validated, position) != QValidator::Acceptable || validated != text)
        throw core::EdictSearchError("Clipboard text is not accepted by the query field");
    }
    if (!self || query_edit_->isReadOnly() || query_edit_->text() != previous_query ||
        text.size() > query_edit_->maxLength()) return false;
    set_query(scalars);
    if (!self || query_edit_->text() != text) return false;
    query_busy_ = false;
    return search();
  } catch (const std::exception& error) {
    if (self) status_->setText(tr("Clipboard search failed: %1").arg(QString::fromUtf8(error.what())));
    return false;
  }
}

bool EdictLookupDialog::search(bool force_contingent, bool names_request) {
  if (query_busy_) return false;
  clipboard_timer_->stop();
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
    EdictLookupOptions options = *options_;
    if (names_request) {
      options.personal_names = options.place_names = true;
      options.advanced = options.contingent = false;
      force_contingent = false;
    }
    const auto handler = search_handler_;
    EdictResourceSearchReport candidate = handler(query, options, force_contingent);
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
    const core::EdictPresentationOptions presentation{
        options.priority_first, options.priority_separator, options.advanced_separator};
    if (!publish_results(std::move(candidate), -1, false, had_kanji, options.compact, &history, &presentation)) return false;
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
    for (std::size_t index : display_order_) records.emplace_back(report_.results[index].result.record);
    const auto order = core::sort_edict_records(records, options, limits);
    EdictResourceSearchReport candidate = report_;
    candidate.results.clear();
    candidate.sections.clear();  // Explicit Sort removes search presentation labels.
    candidate.results.reserve(order.size());
    for (std::size_t index : order) candidate.results.push_back(report_.results[display_order_[index]]);
    if (!publish_results(std::move(candidate), state, reverse, query_had_kanji_, compact_results_)) return false;
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
                                       bool query_had_kanji, bool compact,
                                       core::QueryHistory* history,
                                       const core::EdictPresentationOptions* presentation) {
  std::vector<std::u32string> rows;
  rows.reserve(candidate.results.size());
  for (const auto& result : candidate.results) rows.push_back(render_row(result.result.record));

  std::vector<core::EdictPresentationItem> items;
  if (presentation) {
    std::vector<bool> priority;
    priority.reserve(candidate.results.size());
    for (const auto& result : candidate.results) priority.push_back(result.result.priority);
    items = core::prepare_edict_presentation(priority, candidate.sections, *presentation);
  } else {
    for (std::size_t i = 0; i < rows.size(); ++i) items.push_back({core::EdictPresentationKind::kEntry, i});
  }

  auto document = std::make_unique<QTextDocument>();
  document->setDefaultFont(results_->font());
  document->setUndoRedoEnabled(false);
  QTextCursor cursor(document.get());
  std::vector<std::pair<int, int>> ranges;
  ranges.resize(rows.size());
  std::vector<std::size_t> display_order;
  display_order.reserve(rows.size());
  bool first = true;
  for (const auto& item : items) {
    if (!first) cursor.insertBlock();
    first = false;
    cursor.setBlockFormat(QTextBlockFormat{});
    if (item.kind != core::EdictPresentationKind::kEntry) {
      QTextBlockFormat heading;
      heading.setAlignment(Qt::AlignCenter);
      cursor.setBlockFormat(heading);
      QTextCharFormat style;
      style.setFontWeight(QFont::Bold);
      style.setProperty(kSourceHighlight, item.kind == core::EdictPresentationKind::kAdaptive);
      const QString label = item.kind == core::EdictPresentationKind::kPriorityEnd ? tr("End of Priority Entries") :
          item.kind == core::EdictPresentationKind::kContingent ? tr("No Exact Matches") : tr("Advanced");
      cursor.insertText(label, style);
      continue;
    }
    const auto& result = candidate.results[item.index];
    const int start = cursor.position();
    cursor.setBlockFormat(QTextBlockFormat{});
    QTextCharFormat format;
    format.setToolTip(result.label);
    format.setProperty(kSourceHighlight, result.highlighted);
    const auto& record = result.result.record;
    QString headword = to_qstring(record.headword);
    if (!record.readings.empty()) {
      QStringList readings;
      for (const auto& reading : record.readings) readings.push_back(to_qstring(reading));
      headword += QStringLiteral(" [%1]").arg(readings.join(QStringLiteral("; ")));
    }
    cursor.insertText(headword, format);
    if (compact) {
      cursor.insertText(QStringLiteral(" "), format);
    } else {
      cursor.insertBlock();
      QTextBlockFormat definition;
      definition.setLeftMargin(16);
      cursor.setBlockFormat(definition);
    }
    QStringList meanings;
    for (const auto& meaning : record.definitions) meanings.push_back(to_qstring(meaning));
    cursor.insertText(meanings.join(compact ? QStringLiteral(", ") : QStringLiteral("; ")), format);
    ranges[item.index] = {start, cursor.position()};
    display_order.push_back(item.index);
  }

  // Publish all logical state before widget signals can invoke external handlers.
  report_ = std::move(candidate);
  rendered_rows_ = std::move(rows);
  row_ranges_ = std::move(ranges);
  display_order_ = std::move(display_order);
  clear_result_row_selection();
  sort_state_ = sort_state;
  sort_reverse_ = reverse;
  query_had_kanji_ = query_had_kanji;
  compact_results_ = compact;
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
  update_highlights();
  if (!self) return false;
  if (previous && previous->parent() == results_) delete previous.data();
  if (!self) return false;
  if (!rendered_rows_.empty()) {
    QTextCursor selected(results_->document());
    const auto& range = row_ranges_[display_order_.front()];
    selected.setPosition(range.first);
    selected.setPosition(range.second, QTextCursor::KeepAnchor);
    results_->setTextCursor(selected);
    if (!self) return false;
    results_->setFocus();
  } else {
    query_edit_->setFocus();
  }
  return self != nullptr;
}

void EdictLookupDialog::set_highlight_color(const QColor& color) {
  highlight_color_ = color;
  update_highlights();
}

void EdictLookupDialog::update_highlights() {
  QList<QTextEdit::ExtraSelection> selections;
  const auto color = source_highlight_color(results_->palette(), highlight_color_);
  for (auto block = results_->document()->begin(); block.isValid(); block = block.next()) {
    for (auto it = block.begin(); !it.atEnd(); ++it) {
      const auto fragment = it.fragment();
      if (!fragment.isValid() || !fragment.charFormat().property(kSourceHighlight).toBool()) continue;
      QTextEdit::ExtraSelection selection;
      selection.cursor = QTextCursor(results_->document());
      selection.cursor.setPosition(fragment.position());
      selection.cursor.setPosition(fragment.position() + fragment.length(), QTextCursor::KeepAnchor);
      selection.format.setForeground(color);
      selections.push_back(selection);
    }
  }
  for (const std::size_t row : selected_result_rows_) {
    if (row >= row_ranges_.size()) continue;
    const auto range = row_ranges_[row];
    QTextEdit::ExtraSelection selection;
    selection.cursor = QTextCursor(results_->document());
    selection.cursor.setPosition(range.first);
    selection.cursor.setPosition(range.second, QTextCursor::KeepAnchor);
    selection.format.setBackground(results_->palette().highlight());
    selection.format.setForeground(results_->palette().highlightedText());
    selection.format.setProperty(QTextFormat::FullWidthSelection, true);
    selections.push_back(selection);
  }
  results_->setExtraSelections(selections);
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
  if (query_busy_) return false;
  const std::u32string rows = selected_rows();
  if (!insert_handler_ || rows.empty()) {
    return false;
  }
  const QPointer<EdictLookupDialog> self(this);
  const auto handler = insert_handler_;
  try {
    if (!handler(rows)) {
      if (self) status_->setText(tr("The selected entry could not be inserted."));
      return false;
    }
    return true;
  } catch (const std::exception& error) {
    if (self) status_->setText(
        tr("Insert failed: %1").arg(QString::fromUtf8(error.what())));
    return false;
  } catch (...) {
    if (self) status_->setText(tr("Insert failed with an unknown error."));
    return false;
  }
}

void EdictLookupDialog::copy_selected() {
  if (!selected_result_rows_.empty()) {
    set_internal_clipboard_text(to_qstring(selected_rows()));
    return;
  }
  if (results_->textCursor().hasSelection()) {
    results_->copy();
  } else if (const auto row = current_result_row()) {
    set_internal_clipboard_text(to_qstring(rendered_rows_.at(*row)));
  }
}

const EdictResourceSearchReport& EdictLookupDialog::report() const noexcept {
  return report_;
}

std::u32string EdictLookupDialog::selected_rows() const {
  std::u32string rows;
  if (!selected_result_rows_.empty()) {
    for (const std::size_t row : display_order_) {
      if (std::find(selected_result_rows_.begin(), selected_result_rows_.end(), row) ==
          selected_result_rows_.end()) {
        continue;
      }
      if (!rows.empty()) rows.push_back(U'\n');
      rows.append(rendered_rows_.at(row));
    }
    return rows;
  }
  const QTextCursor cursor = results_->textCursor();
  if (!cursor.hasSelection()) return rows;
  for (std::size_t row : display_order_) {
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

std::optional<std::size_t> EdictLookupDialog::current_result_row() const {
  const QTextCursor cursor = results_->textCursor();
  const int position = cursor.hasSelection() ? cursor.selectionStart()
                                             : cursor.position();
  return result_row_at(position);
}

std::optional<std::size_t> EdictLookupDialog::result_row_at(int position) const {
  for (std::size_t row : display_order_) {
    const auto range = row_ranges_.at(row);
    if (position >= range.first &&
        (position < range.second ||
         (position == range.second && row == display_order_.back()))) {
      return row;
    }
  }
  return std::nullopt;
}

void EdictLookupDialog::select_result_row(std::size_t row, bool toggle) {
  if (row >= row_ranges_.size()) return;
  const auto selected = std::find(selected_result_rows_.begin(),
                                  selected_result_rows_.end(), row);
  if (selected != selected_result_rows_.end()) {
    if (toggle) selected_result_rows_.erase(selected);
  } else {
    selected_result_rows_.push_back(row);
  }
  result_selection_anchor_ = row;

  const auto range = row_ranges_.at(row);
  updating_result_selection_ = true;
  QTextCursor cursor(results_->document());
  cursor.setPosition(range.first);
  results_->setTextCursor(cursor);
  updating_result_selection_ = false;
  update_highlights();
  update_actions();
}

void EdictLookupDialog::clear_result_row_selection() {
  selected_result_rows_.clear();
  result_selection_anchor_.reset();
}

void EdictLookupDialog::navigate_result_rows(
    int key, Qt::KeyboardModifiers modifiers) {
  if (display_order_.empty()) return;
  auto current = current_result_row();
  auto current_position = current
      ? std::find(display_order_.begin(), display_order_.end(), *current)
      : display_order_.end();
  std::size_t index = current_position == display_order_.end()
      ? 0U : static_cast<std::size_t>(current_position - display_order_.begin());
  const int visible_lines = std::max(1, results_->viewport()->height() /
                                         std::max(1, results_->fontMetrics().lineSpacing()));
  const std::size_t page = static_cast<std::size_t>(std::max(1, visible_lines - 1));
  switch (key) {
    case Qt::Key_Up:
      if (index > 0) --index;
      break;
    case Qt::Key_Down:
      if (index + 1 < display_order_.size()) ++index;
      break;
    case Qt::Key_Home:
      index = 0;
      break;
    case Qt::Key_End:
      index = display_order_.size() - 1;
      break;
    case Qt::Key_PageUp:
      index = index > page ? index - page : 0;
      break;
    case Qt::Key_PageDown:
      index = std::min(display_order_.size() - 1, index + page);
      break;
    default:
      return;
  }

  const bool vertical = key == Qt::Key_Up || key == Qt::Key_Down;
  const bool extend = modifiers.testFlag(Qt::ShiftModifier) &&
      (vertical || !modifiers.testFlag(Qt::ControlModifier));
  move_result_row_selection(display_order_.at(index), extend,
      modifiers.testFlag(Qt::ControlModifier) && !extend);
}

void EdictLookupDialog::move_result_row_selection(
    std::size_t target, bool extend, bool preserve) {
  if (target >= row_ranges_.size()) return;
  const auto current = current_result_row();
  if ((extend || preserve) && selected_result_rows_.empty()) {
    const QTextCursor cursor = results_->textCursor();
    if (cursor.hasSelection()) {
      for (const std::size_t row : display_order_) {
        const auto range = row_ranges_.at(row);
        if (cursor.selectionEnd() > range.first &&
            cursor.selectionStart() < range.second) {
          selected_result_rows_.push_back(row);
        }
      }
    }
  }
  if (extend) {
    const std::size_t anchor = result_selection_anchor_.value_or(
        current.value_or(target));
    auto anchor_position = std::find(
        display_order_.begin(), display_order_.end(), anchor);
    const auto target_position = std::find(
        display_order_.begin(), display_order_.end(), target);
    if (target_position == display_order_.end()) return;
    if (anchor_position == display_order_.end()) anchor_position = target_position;
    const std::size_t anchor_index = static_cast<std::size_t>(
        anchor_position - display_order_.begin());
    const std::size_t target_index = static_cast<std::size_t>(
        target_position - display_order_.begin());
    clear_result_row_selection();
    const std::size_t first = std::min(anchor_index, target_index);
    const std::size_t last = std::max(anchor_index, target_index);
    selected_result_rows_.insert(selected_result_rows_.end(),
        display_order_.begin() + static_cast<std::ptrdiff_t>(first),
        display_order_.begin() + static_cast<std::ptrdiff_t>(last + 1));
    result_selection_anchor_ = anchor;
  } else if (!preserve) {
    selected_result_rows_.assign(1, target);
    result_selection_anchor_ = target;
  }

  updating_result_selection_ = true;
  QTextCursor cursor(results_->document());
  cursor.setPosition(row_ranges_.at(target).first);
  results_->setTextCursor(cursor);
  results_->ensureCursorVisible();
  updating_result_selection_ = false;
  update_highlights();
  update_actions();
}

void EdictLookupDialog::copy_current_result_field(bool reading) {
  const auto row = current_result_row();
  if (!row) {
    set_internal_clipboard_text({});
    return;
  }
  const core::EdictRecord& record = report_.results.at(*row).result.record;
  const std::u32string& field = reading && !record.readings.empty()
                                    ? record.readings.front()
                                    : record.headword;
  set_internal_clipboard_text(to_qstring(field));
}

std::optional<char32_t> EdictLookupDialog::current_result_character() const {
  const auto row = current_result_row();
  if (!row || *row >= report_.results.size()) return {};
  const auto& record = report_.results[*row].result.record;
  if (!record.headword.empty()) return record.headword.front();
  if (!record.readings.empty() && !record.readings.front().empty())
    return record.readings.front().front();
  return {};
}

void EdictLookupDialog::show_current_information() {
  const auto character = current_result_character();
  const auto handler = info_handler_;
  if (!character || !handler) return;
  const QPointer<EdictLookupDialog> self(this);
  try {
    handler(*character);
  } catch (const std::exception& error) {
    if (self) status_->setText(QString::fromLocal8Bit(error.what()));
  } catch (...) {
    if (self) status_->setText(tr("Could not show kanji information"));
  }
}

void EdictLookupDialog::show_current_radical_lookup() {
  const auto character = current_result_character();
  const auto handler = radical_handler_;
  if (!character || !handler) return;
  const QPointer<EdictLookupDialog> self(this);
  try {
    handler(*character);
  } catch (const std::exception& error) {
    if (self) status_->setText(QString::fromLocal8Bit(error.what()));
  } catch (...) {
    if (self) status_->setText(tr("Could not open Radical Lookup"));
  }
}

void EdictLookupDialog::select_current_result(bool whole_row) {
  const auto row = current_result_row();
  if (!row) return;
  const auto range = row_ranges_.at(*row);
  int end = range.second;
  if (!whole_row) {
    end = range.first +
          to_qstring(report_.results.at(*row).result.record.headword).size();
  }
  if (end <= range.first || end > range.second) return;
  QTextCursor cursor(results_->document());
  cursor.setPosition(range.first);
  cursor.setPosition(end, QTextCursor::KeepAnchor);
  results_->setTextCursor(cursor);
}

void EdictLookupDialog::update_actions() {
  const auto cursor = results_->textCursor();
  const bool entry_selected = !selected_result_rows_.empty() ||
      (cursor.hasSelection() && std::any_of(row_ranges_.begin(), row_ranges_.end(),
          [&cursor](const auto& range) {
            return cursor.selectionEnd() > range.first &&
                   cursor.selectionStart() < range.second;
          }));
  insert_button_->setEnabled(insert_handler_ && entry_selected);
  sort_button_->setEnabled(!report_.results.empty());
}

void EdictLookupDialog::hideEvent(QHideEvent* event) {
  clipboard_timer_->stop();
  QDialog::hideEvent(event);
}

bool EdictLookupDialog::eventFilter(QObject* watched, QEvent* event) {
  if (watched == results_ && event->type() == QEvent::PaletteChange) update_highlights();
  if (watched == query_edit_ && (event->type() == QEvent::KeyPress || event->type() == QEvent::InputMethod))
    clipboard_timer_->stop();
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
    if (event->type() == QEvent::MouseButtonPress) {
      auto* mouse = static_cast<QMouseEvent*>(event);
      if (mouse->button() == Qt::RightButton) return true;
      if (mouse->button() == Qt::LeftButton) {
        const QPoint point = watched == results_->viewport()
            ? mouse->position().toPoint()
            : results_->viewport()->mapFrom(results_, mouse->position().toPoint());
        const auto row = result_row_at(results_->cursorForPosition(point).position());
        const auto modifiers = mouse->modifiers() & ~Qt::KeypadModifier;
        if (modifiers.testFlag(Qt::ControlModifier) &&
            !(modifiers & (Qt::AltModifier | Qt::MetaModifier))) {
          event->accept();
          if (!query_busy_ && row) select_result_row(*row, true);
          return true;
        }
        if (modifiers == Qt::ShiftModifier) {
          event->accept();
          if (!query_busy_ && row) move_result_row_selection(*row, true, false);
          return true;
        }
        if (modifiers == Qt::NoModifier) {
          clear_result_row_selection();
          if (!query_busy_ && row) {
            select_result_row(*row, false);
          } else {
            update_highlights();
            update_actions();
          }
        }
      }
    }
    if (event->type() == QEvent::MouseButtonDblClick) {
      auto* mouse = static_cast<QMouseEvent*>(event);
      if (mouse->button() == Qt::LeftButton && mouse->modifiers() == Qt::NoModifier) {
        event->accept();
        if (query_busy_ || !insert_handler_) return true;
        const QPoint point = watched == results_->viewport() ? mouse->position().toPoint() :
            results_->viewport()->mapFrom(results_, mouse->position().toPoint());
        const int position = results_->cursorForPosition(point).position();
        for (std::size_t row : display_order_) {
          const auto range = row_ranges_[row];
          if (position < range.first || position >= range.second) continue;
          const QPointer<EdictLookupDialog> self(this);
          const QPointer<QTextDocument> document(results_->document());
          QTextCursor selected(document);
          selected.setPosition(range.first);
          selected.setPosition(range.second, QTextCursor::KeepAnchor);
          results_->setTextCursor(selected);
          if (self && document && results_->document() == document &&
              results_->textCursor().selectionStart() == range.first &&
              results_->textCursor().selectionEnd() == range.second) insert_selected();
          return true;
        }
        return true;
      }
    }
    if (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress) {
      auto* key = static_cast<QKeyEvent*>(event);
      const auto modifiers = key->modifiers() & ~Qt::KeypadModifier;
      const bool copy_field = modifiers == Qt::ControlModifier &&
                              (key->key() == Qt::Key_E ||
                               key->key() == Qt::Key_R);
      const bool select_field = key->key() == Qt::Key_W &&
          (modifiers == Qt::ControlModifier ||
           modifiers == (Qt::ControlModifier | Qt::ShiftModifier));
      const bool copy_rows = modifiers == Qt::ControlModifier &&
          (key->key() == Qt::Key_C || key->key() == Qt::Key_Insert);
      const bool select_row = key->key() == Qt::Key_Space &&
          (modifiers == Qt::NoModifier || modifiers == Qt::ControlModifier);
      const bool select_all = modifiers == Qt::ControlModifier &&
                              key->key() == Qt::Key_A;
      const bool input_mode = modifiers == Qt::ControlModifier &&
          (key->key() == Qt::Key_J || key->key() == Qt::Key_K ||
            key->key() == Qt::Key_6);
      const bool toggle_input_mode = key->key() == Qt::Key_F4 &&
                                     modifiers == Qt::NoModifier;
      const bool information = key->key() == Qt::Key_I &&
                               modifiers == Qt::ControlModifier;
      const bool radical_lookup =
          (key->key() == Qt::Key_F5 && modifiers == Qt::NoModifier) ||
          (key->key() == Qt::Key_L && modifiers == Qt::ControlModifier);
      const bool navigate_rows =
          (key->key() == Qt::Key_Up || key->key() == Qt::Key_Down ||
           key->key() == Qt::Key_Home || key->key() == Qt::Key_End ||
           key->key() == Qt::Key_PageUp || key->key() == Qt::Key_PageDown) &&
          !(modifiers & (Qt::AltModifier | Qt::MetaModifier));
      const bool handoff_query =
          (key->key() == Qt::Key_Left || key->key() == Qt::Key_Right) &&
          !(modifiers & (Qt::AltModifier | Qt::MetaModifier));
      const bool focus_query =
          (key->key() == Qt::Key_F6 && modifiers == Qt::NoModifier) ||
          (key->key() == Qt::Key_D && modifiers == Qt::ControlModifier);
      if (copy_field || copy_rows || select_field || select_row || select_all || input_mode ||
          toggle_input_mode || information || radical_lookup ||
          navigate_rows || handoff_query || focus_query) {
        event->accept();
        if (event->type() == QEvent::KeyPress && !query_busy_) {
          if (navigate_rows) {
            navigate_result_rows(key->key(), modifiers);
          } else if (handoff_query) {
            const QPointer<EdictLookupDialog> self(this);
            const QPointer<QLineEdit> query(query_edit_);
            const int cursor_position = query->cursorPosition();
            const int selection_start = query->selectionStart();
            const int selection_length = query->selectedText().size();
            query->setFocus();
            if (self && query) {
              if (selection_start < 0) {
                query->deselect();
                query->setCursorPosition(cursor_position);
              } else if (cursor_position == selection_start) {
                query->setSelection(selection_start + selection_length, -selection_length);
              } else {
                query->setSelection(selection_start, selection_length);
              }
              QKeyEvent forwarded(key->type(), key->key(), key->modifiers(), key->text(),
                                  key->isAutoRepeat(), static_cast<ushort>(key->count()));
              QApplication::sendEvent(query, &forwarded);
            }
          } else if (focus_query) {
            query_edit_->setFocus();
          } else if (copy_field) {
            copy_current_result_field(key->key() == Qt::Key_R);
          } else if (copy_rows) {
            copy_selected();
          } else if (toggle_input_mode) {
            query_field_->set_input_mode(
                query_field_->input_mode() == InputMode::kKanji
                    ? InputMode::kAscii : InputMode::kKanji);
          } else if (information) {
            show_current_information();
          } else if (radical_lookup) {
            show_current_radical_lookup();
          } else if (input_mode) {
            if (key->key() == Qt::Key_J) {
              query_field_->set_input_mode(InputMode::kJascii);
            } else if (key->key() == Qt::Key_K) {
              query_field_->set_input_mode(InputMode::kKanji);
            } else {
              query_field_->set_input_mode(
                  query_field_->input_mode() == InputMode::kKanji
                      ? InputMode::kAscii : InputMode::kKanji);
            }
          } else if (select_all) {
            if (display_order_.empty()) {
              query_field_->set_input_mode(InputMode::kAscii);
            } else {
              selected_result_rows_ = display_order_;
              result_selection_anchor_ = current_result_row().value_or(display_order_.front());
              QTextCursor cursor = results_->textCursor();
              cursor.clearSelection();
              results_->setTextCursor(cursor);
              update_highlights();
              update_actions();
            }
          } else if (select_row) {
            if (const auto row = current_result_row()) {
              select_result_row(*row, modifiers == Qt::ControlModifier);
            }
          } else {
            select_current_result(modifiers.testFlag(Qt::ShiftModifier));
          }
        }
        return true;
      }
      const bool plain = !(key->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
      const bool enter = key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter;
      const bool typing = !key->text().isEmpty() && key->text().front().unicode() >= 0x20U &&
                          key->text().front().unicode() != 0x7fU;
      if (plain && (enter || typing)) {
        event->accept();
        if (event->type() == QEvent::ShortcutOverride || query_busy_) return true;
        if (enter) {
          if (insert_handler_ &&
              (results_->textCursor().hasSelection() || !selected_result_rows_.empty())) {
            insert_selected();
          }
          else query_edit_->setFocus();
        } else {
          const QPointer<EdictLookupDialog> self(this);
          const QPointer<QLineEdit> query(query_edit_);
          QKeyEvent forwarded(key->type(), key->key(), key->modifiers(), key->text(),
                              key->isAutoRepeat(), static_cast<ushort>(key->count()));
          query->setFocus();
          if (self && query) QApplication::sendEvent(query, &forwarded);
        }
        return true;
      }
    }
    if (event->type() == QEvent::ContextMenu) {
      std::function<void(CharacterTarget)> callback;
      if (auto handler = info_handler_) callback = [handler](CharacterTarget target) { handler(target.character); };
      show_character_context_menu(*results_, *static_cast<QContextMenuEvent*>(event),
          callback);
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
