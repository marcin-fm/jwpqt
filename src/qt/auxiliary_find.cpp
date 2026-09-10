// SPDX-License-Identifier: GPL-2.0-or-later
#include "auxiliary_find.h"
#include "main_window.h"
#include "text_bridge.h"
#include "jwpqt/core/unicode_search.h"
#include <array>
#include <QAction>
#include <QAbstractButton>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QListWidget>
#include <QMenu>
#include <QPersistentModelIndex>
#include <QScopeGuard>
#include <QTextEdit>
#include <QTextDocument>
#include <stdexcept>
#include <utility>

namespace jwpqt::qt {
AuxiliaryFind::AuxiliaryFind(QWidget* target, Ranges ranges, bool repeat_keys)
    : QObject(target), target_(target), ranges_(std::move(ranges)), repeat_keys_(repeat_keys) {
  if (!qobject_cast<QListWidget*>(target) && !(qobject_cast<QTextEdit*>(target) && ranges_))
    throw std::invalid_argument("Auxiliary Find requires a list or logical text ranges");
  for (auto* owner = target; owner; owner = owner->parentWidget())
    if (auto* main = dynamic_cast<MainWindow*>(owner)) { workspace_ = main; break; }
  histories_ = workspace_ ? workspace_->query_histories_ : std::make_shared<core::QueryHistories>();
  auto action = [&](const char* name, const QString& title, QKeySequence shortcut) {
    auto* value = new QAction(title, this);
    value->setObjectName(QString::fromLatin1(name));
    value->setShortcut(shortcut); value->setShortcutContext(Qt::WidgetShortcut);
    value->setProperty("jwpqtAuxiliaryFind", true);
    target->addAction(value); return value;
  };
  open_ = action("auxiliaryFind", tr("Find in Results..."), QKeySequence::Find);
  next_ = action("auxiliaryFindNext", tr("Find Next Entry"), repeat_keys ? QKeySequence(Qt::Key_F3) : QKeySequence{});
  previous_ = action("auxiliaryFindPrevious", tr("Find Previous Entry"), repeat_keys ? QKeySequence(Qt::SHIFT | Qt::Key_F3) : QKeySequence{});
  connect(open_, &QAction::triggered, this, &AuxiliaryFind::open);
  for (const auto& pair : {std::pair<QAction*, bool>{next_, false}, {previous_, true}}) {
    connect(pair.first, &QAction::triggered, this, [this, back = pair.second] {
      auto request = last_;
      if (workspace_) { request.text = workspace_->search_text_; request.options = workspace_->search_options_; }
      if (request.text.isEmpty()) { open(); return; }
      request.options.direction = back ? core::JwpSearchDirection::kBackward : core::JwpSearchDirection::kForward;
      const QPointer<AuxiliaryFind> self(this);
      const auto result = find(request);
      if (self && target_) target_->setToolTip(result.message);
    });
  }
  target->installEventFilter(this);
  if (auto* list = qobject_cast<QListWidget*>(target)) list->viewport()->installEventFilter(this);
  if (auto* text = qobject_cast<QTextEdit*>(target)) text->viewport()->installEventFilter(this);
}

void AuxiliaryFind::set_result_insertion(QAbstractButton* insert_button) {
  if (!insert_button || !workspace_ || !insert_actions_.empty()) return;
  insert_button_ = insert_button;
  struct Definition {
    ResultInsertDestination destination;
    const char* name;
    QString title;
  };
  const std::array<Definition, 5> definitions{{
      {ResultInsertDestination::kCurrent, "resultInsertCurrent",
       tr("Insert to Current File")},
      {ResultInsertDestination::kReplaceCurrent, "resultReplaceCurrent",
       tr("Replace in Current File")},
      {ResultInsertDestination::kNew, "resultInsertNew",
       tr("Insert to New File")},
      {ResultInsertDestination::kAny, "resultInsertAny",
       tr("Insert to Any File...")},
      {ResultInsertDestination::kLast, "resultInsertLast",
       tr("Insert to Last File")},
  }};
  for (const auto& definition : definitions) {
    auto* action = new QAction(definition.title, this);
    action->setObjectName(QString::fromLatin1(definition.name));
    action->setProperty("jwpqtResultInsertion", true);
    target_->addAction(action);
    connect(action, &QAction::triggered, this,
            [this, destination = definition.destination] {
              const QPointer<AuxiliaryFind> self(this);
              const QPointer<MainWindow> workspace(workspace_);
              const QPointer<QAbstractButton> button(insert_button_);
              if (!workspace || !button || !button->isEnabled()) return;
              workspace->insert_result_at_destination(destination, [button] {
                if (button && button->isEnabled()) button->click();
              });
              if (self) self->update_insert_actions();
            });
    insert_actions_.push_back(action);
  }
  update_insert_actions();
}

void AuxiliaryFind::update_insert_actions() {
  if (insert_actions_.empty()) return;
  constexpr std::array destinations{
      ResultInsertDestination::kCurrent,
      ResultInsertDestination::kReplaceCurrent,
      ResultInsertDestination::kNew,
      ResultInsertDestination::kAny,
      ResultInsertDestination::kLast,
  };
  const bool selected = insert_button_ && insert_button_->isEnabled();
  for (std::size_t i = 0; i < destinations.size(); ++i) {
    insert_actions_[i]->setEnabled(
        selected && workspace_ &&
        workspace_->result_insert_destination_available(destinations[i]));
  }
  if (workspace_) {
    insert_actions_.back()->setText(
        workspace_->result_insert_destination_name());
  }
}

void AuxiliaryFind::open() {
  if (busy_ || !target_) return;
  if (dialog_) { dialog_->show(); dialog_->raise(); dialog_->activateWindow(); return; }
  const auto settings = workspace_ ? workspace_->application_settings_ : ApplicationSettings{};
  const QPointer<AuxiliaryFind> self(this);
  dialog_ = new FindReplaceDialog(false, settings, histories_, [self](const FindReplaceRequest& request) {
    return self ? self->find(request) : FindReplaceResult{false, tr("Results closed")};
  }, target_->window());
  dialog_->setObjectName(QStringLiteral("auxiliaryFindDialog"));
  dialog_->set_auxiliary_scope();
  if (workspace_) dialog_->set_overwrite_action(workspace_->overwrite_action_);
  connect(this, &QObject::destroyed, dialog_, &QObject::deleteLater);
  QString query = workspace_ ? workspace_->search_text_ : last_.text;
  if (query.isEmpty() && !histories_->search.entries().empty()) query = to_qstring(histories_->search.entries().front());
  if (query.size() <= 32767) dialog_->set_text(query, {});
  if (self && dialog_) dialog_->show();
}

FindReplaceResult AuxiliaryFind::find(const FindReplaceRequest& request) {
  if (busy_ || !target_) return {false, tr("Results are busy or closed")};
  const QPointer<AuxiliaryFind> self(this);
  busy_ = true;
  const auto idle = qScopeGuard([self] { if (self) self->busy_ = false; });
  try {
    if (request.operation != SearchOperation::kFind || request.all_files || request.text.isEmpty())
      throw std::invalid_argument("Auxiliary Find searches entries in this list only");
    const auto pattern = from_qstring(request.text);
    if (to_qstring(pattern) != request.text) throw std::invalid_argument("Invalid Unicode search text");
    std::size_t work = 100'000'000;
    core::find_unicode_text({}, pattern, request.options, 1, &work);
    struct Entry { QString text; QPersistentModelIndex index; int start = 0, end = 0; };
    std::vector<Entry> entries;
    int current = -1;
    QPointer<QTextDocument> document;
    int revision = 0;
    auto* list = qobject_cast<QListWidget*>(target_.data());
    auto* text = qobject_cast<QTextEdit*>(target_.data());
    std::size_t cells = 0;
    auto add = [&](Entry entry) {
      cells += static_cast<std::size_t>(entry.text.size());
      if (entries.size() >= 100000 || cells > 32 * 1024 * 1024)
        throw std::length_error("Auxiliary results exceed search limits");
      entries.push_back(std::move(entry));
    };
    if (list) {
      if (list->count() > 100000) throw std::length_error("Too many result entries");
      for (int row = 0; row < list->count(); ++row) {
        auto* item = list->item(row);
        if (!(item->flags() & Qt::ItemIsSelectable) || item->isHidden()) continue;
        if (row == list->currentRow()) current = static_cast<int>(entries.size());
        add({item->text(), QPersistentModelIndex(list->model()->index(row, 0)), 0, 0});
      }
    } else {
      document = text->document(); revision = document->revision();
      auto provider = ranges_;
      const auto ranges = provider();
      if (!self || !target_ || !document || text->document() != document || document->revision() != revision)
        return {false, tr("Results changed during search")};
      const int position = text->textCursor().selectionStart();
      if (ranges.size() > 100000) throw std::length_error("Too many logical result entries");
      int previous = -1;
      for (const auto& range : ranges) {
        if (range.first < 0 || range.second < range.first || range.second >= document->characterCount() || range.first < previous)
          throw std::invalid_argument("Invalid logical result range");
        previous = range.second;
        if (static_cast<std::size_t>(range.second - range.first) > 32 * 1024 * 1024 - cells)
          throw std::length_error("Auxiliary results exceed search limits");
        QTextCursor cursor(document); cursor.setPosition(range.first);
        cursor.setPosition(range.second, QTextCursor::KeepAnchor);
        if (position >= range.first && position < range.second) current = static_cast<int>(entries.size());
        add({cursor.selectedText(), {}, range.first, range.second});
      }
    }
    const int count = static_cast<int>(entries.size());
    const int direction = request.options.direction == core::JwpSearchDirection::kBackward ? -1 : 1;
    int row = current < 0 ? (direction > 0 ? -1 : count) : current;
    int match = -1;
    for (int visited = 0; visited < count; ++visited) {
      row += direction;
      if (row < 0 || row >= count) {
        if (!request.options.wrap) break;
        row = row < 0 ? count - 1 : 0;
      }
      if (row == current) break;
      auto value = entries[row].text;
      value.replace(QChar::ParagraphSeparator, QLatin1Char(' '));
      value.replace(QChar::LineSeparator, QLatin1Char(' '));
      const auto scalar = from_qstring(value);
      if (to_qstring(scalar) != value) throw std::invalid_argument("Invalid Unicode result text");
      if (!core::find_unicode_text(scalar, pattern, request.options, 1, &work, false, true).empty()) { match = row; break; }
    }
    auto history = histories_->search;
    history.remember(pattern);
    // Publish logical state before selection signals can invoke another command.
    histories_->search = std::move(history);
    last_ = request;
    if (workspace_) {
      workspace_->search_text_ = request.text;
      workspace_->search_options_ = request.options;
      auto& settings = workspace_->application_settings_;
      settings.search_ignore_case = request.options.ignore_ascii_case;
      settings.search_ignore_width = request.options.jascii_ascii_equivalence;
      settings.search_wrap = request.options.wrap;
      settings.search_keep_open = request.keep_open;
    }
    if (match < 0) return {true, tr("No other matching entry")};
    const auto found = entries[match];
    if (list) {
      if (!found.index.isValid()) return {false, tr("Results changed during search")};
      // Qt emits more selection-model signals after currentRowChanged. Keep
      // that model alive if a callback destroys the owning list in the middle.
      QPointer<QItemSelectionModel> selection = list->selectionModel();
      QPointer<QObject> selection_owner = selection->parent();
      const bool had_owner = !selection_owner.isNull();
      const auto restore_owner = qScopeGuard([selection, selection_owner, had_owner] {
        if (!selection || selection->parent()) return;
        if (selection_owner) selection->setParent(selection_owner);
        else if (had_owner) selection->deleteLater();
      });
      selection->setParent(nullptr);
      if (!self || !target_ || !selection) return {false, tr("Results closed during selection")};
      list->setCurrentRow(found.index.row(), QItemSelectionModel::ClearAndSelect);
      if (!self || !target_ || !found.index.isValid() || list->currentIndex() != found.index)
        return {false, tr("Results closed or changed")};
      list->scrollTo(found.index);
    } else {
      QTextCursor cursor(document); cursor.setPosition(found.start);
      cursor.setPosition(found.end, QTextCursor::KeepAnchor);
      text->setTextCursor(cursor);
      if (!self || !target_ || !document || text->document() != document || document->revision() != revision ||
          text->textCursor().selectionStart() != found.start || text->textCursor().selectionEnd() != found.end)
        return {false, tr("Results changed during selection")};
      text->ensureCursorVisible();
    }
    return {true, tr("Matching entry selected")};
  } catch (const std::exception& error) {
    return {false, QString::fromUtf8(error.what())};
  }
}

bool AuxiliaryFind::eventFilter(QObject* object, QEvent* event) {
  if (!target_) return false;
  if (event->type() == QEvent::ContextMenu) update_insert_actions();
  if (object == target_ && (event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride)) {
    auto* key = static_cast<QKeyEvent*>(event);
    QAction* action = nullptr;
    if ((key->key() == Qt::Key_F || key->key() == Qt::Key_S) &&
        key->modifiers() == Qt::ControlModifier) action = open_;
    if (key->key() == Qt::Key_F8 && key->modifiers() == Qt::NoModifier) action = open_;
    if (repeat_keys_ && key->key() == Qt::Key_F3 && key->modifiers() == Qt::NoModifier) action = next_;
    if (repeat_keys_ && key->key() == Qt::Key_F3 && key->modifiers() == Qt::ShiftModifier) action = previous_;
    if (repeat_keys_ && key->key() == Qt::Key_N &&
        key->modifiers() == Qt::ControlModifier) action = next_;
    if (repeat_keys_ && key->key() == Qt::Key_F9 &&
        key->modifiers() == Qt::NoModifier) action = next_;
    if (action) { event->accept(); if (event->type() == QEvent::KeyPress) action->trigger(); return true; }
  }
  if (event->type() == QEvent::ContextMenu && qobject_cast<QListWidget*>(target_)) {
    auto* context = static_cast<QContextMenuEvent*>(event);
    QPointer<QMenu> menu = new QMenu(target_);
    menu->addActions({open_, next_, previous_});
    update_insert_actions();
    if (!insert_actions_.empty()) {
      menu->addSeparator();
      for (auto* action : insert_actions_) menu->addAction(action);
    }
    menu->exec(context->globalPos());
    delete menu.data(); return true;
  }
  return false;
}
}
