// SPDX-License-Identifier: GPL-2.0-or-later
#include "find_replace_dialog.h"
#include "kana_input_field.h"
#include "japanese_fonts.h"
#include "text_bridge.h"
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPointer>
#include <QPushButton>
#include <QScopeGuard>
#include <QVBoxLayout>

namespace jwpqt::qt {
FindReplaceDialog::FindReplaceDialog(bool replacing, const ApplicationSettings& settings,
    std::shared_ptr<core::QueryHistories> histories, Handler handler, QWidget* parent)
    : QDialog(parent), replacing_(replacing), histories_(std::move(histories)), handler_(std::move(handler)) {
  if (!histories_ || !handler_) throw std::invalid_argument("Search dialog requires history and a handler");
  setObjectName(replacing ? QStringLiteral("replaceDialog") : QStringLiteral("findDialog"));
  setWindowTitle(replacing ? tr("Replace") : tr("Find"));
  setAttribute(Qt::WA_DeleteOnClose);
  auto* layout = new QVBoxLayout(this);
  auto* form = new QFormLayout;
  for (int i = 0; i < (replacing ? 2 : 1); ++i) {
    fields_[i] = new KanaInputField(i ? QStringLiteral("replacementText") :
        replacing ? QStringLiteral("replaceFindText") : QStringLiteral("findText"), this);
    fields_[i]->edit()->installEventFilter(this);
    auto* row = new QHBoxLayout;
    row->addWidget(fields_[i]);
    auto* button = new QPushButton(tr("History..."), this);
    button->setObjectName(i ? QStringLiteral("replacementHistory") : QStringLiteral("searchHistory"));
    button->setAutoDefault(false);
    row->addWidget(button);
    connect(button, &QPushButton::clicked, this, [this, i] { history(i, true); });
    form->addRow(i ? tr("Replace with:") : tr("Find:"), row);
  }
  layout->addLayout(form);
  auto add = [&](const char* name, const QString& label, bool checked) {
    auto* box = new QCheckBox(label, this);
    box->setObjectName(QString::fromLatin1(name)); box->setChecked(checked);
    layout->addWidget(box); return box;
  };
  case_ = add("searchIgnoreCase", tr("Ignore ASCII case"), settings.search_ignore_case);
  width_ = add("searchIgnoreWidth", tr("Treat full-width letters and digits as ASCII"), settings.search_ignore_width);
  all_ = add("searchAllFiles", tr("All open documents"), settings.search_all_files);
  wrap_ = add("searchWrap", tr("Wrap around"), settings.search_wrap);
  back_ = add("searchBackward", tr("Backward"), false);
  keep_ = add("searchKeepOpen", tr("Keep this window open"), settings.search_keep_open);
  wrap_->setEnabled(!all_->isChecked());
  connect(all_, &QCheckBox::toggled, wrap_, [this](bool all) { wrap_->setEnabled(!all); });
  status_ = new QLabel(this); status_->setObjectName(QStringLiteral("searchStatus"));
  status_->setWordWrap(true); layout->addWidget(status_);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  auto command = [&](const char* name, const QString& label, SearchOperation operation) {
    auto* button = buttons->addButton(label, QDialogButtonBox::ActionRole);
    button->setObjectName(QString::fromLatin1(name)); button->setAutoDefault(false);
    connect(button, &QPushButton::clicked, this, [this, operation] { submit(operation); });
    return button;
  };
  command("searchFind", tr("Find Next"), SearchOperation::kFind)->setDefault(!replacing);
  if (replacing) {
    command("searchReplace", tr("Replace Next"), SearchOperation::kReplace)->setDefault(true);
    command("searchReview", tr("Review Matches..."), SearchOperation::kReview);
    command("searchReplaceAll", tr("Replace All"), SearchOperation::kAll);
  }
  connect(buttons, &QDialogButtonBox::rejected, this, &QWidget::close);
  layout->addWidget(buttons);
  resize(replacing ? 640 : 520, sizeHint().height());
}

void FindReplaceDialog::set_text(const QString& text, const QString& replacement) {
  const QPointer<FindReplaceDialog> self(this);
  fields_[0]->edit()->setText(text);
  if (!self) return;
  if (fields_[1]) fields_[1]->edit()->setText(replacement);
  if (!self) return;
  fields_[0]->edit()->selectAll(); fields_[0]->edit()->setFocus();
}
void FindReplaceDialog::set_overwrite_action(QAction* action) {
  for (auto* field : fields_) if (field) field->set_overwrite_action(action);
}
void FindReplaceDialog::submit(SearchOperation operation) {
  if (busy_) return;
  QPointer<FindReplaceDialog> self(this);
  busy_ = true;
  const auto idle = qScopeGuard([self] { if (self) self->busy_ = false; });
  try {
    fields_[0]->finish_input();
    if (!self) return;
    if (operation != SearchOperation::kFind) fields_[1]->finish_input();
    if (!self) return;
    FindReplaceRequest request{fields_[0]->edit()->text(), fields_[1] ? fields_[1]->edit()->text() : QString{},
        {back_->isChecked() ? core::JwpSearchDirection::kBackward : core::JwpSearchDirection::kForward,
         case_->isChecked(), width_->isChecked(), wrap_->isChecked()},
        all_->isChecked(), keep_->isChecked(), operation};
    if (request.text.isEmpty()) { status_->setText(tr("Enter text to find")); return; }
    auto handler = handler_;
    const auto result = handler(request);
    if (!self) return;
    status_->setText(result.message);
    if (result.valid && !request.keep_open) close();
  } catch (const std::exception& error) {
    if (self) status_->setText(QString::fromUtf8(error.what()));
  }
}

bool FindReplaceDialog::eventFilter(QObject* object, QEvent* event) {
  for (int i = 0; i < (replacing_ ? 2 : 1); ++i) {
    if (object != fields_[i]->edit() ||
        (event->type() != QEvent::KeyPress && event->type() != QEvent::ShortcutOverride)) continue;
    auto* key = static_cast<QKeyEvent*>(event);
    if (key->modifiers() == Qt::NoModifier && (key->key() == Qt::Key_Up || key->key() == Qt::Key_Down)) {
      event->accept();
      if (event->type() == QEvent::KeyPress && !busy_)
        history(i, false, key->key() == Qt::Key_Up ? 1 : -1);
      return true;
    }
  }
  return QDialog::eventFilter(object, event);
}

void FindReplaceDialog::history(int index, bool chooser, int direction) {
  if (busy_ || fields_[index]->edit()->isReadOnly()) return;
  QPointer<FindReplaceDialog> self(this);
  busy_ = true;
  const auto idle = qScopeGuard([self] { if (self) self->busy_ = false; });
  try {
    fields_[index]->finish_input();
    if (!self) return;
    auto& cache = index ? histories_->replace : histories_->search;
    auto* edit = fields_[index]->edit();
    QString selected;
    const QString original = edit->text();
    if (!chooser && direction < 0 && !cache.find(from_qstring(original))) chooser = true;
    if (!chooser) {
      const auto current = cache.find(from_qstring(original));
      if (!current && !original.isEmpty() && !cache.remember(from_qstring(original))) return;
      const auto position = current ? static_cast<int>(*current) : original.isEmpty() ? -1 : 0;
      const int next = position + direction;
      if (next < 0) selected.clear();
      else if (static_cast<std::size_t>(next) < cache.entries().size()) selected = to_qstring(cache.entries()[next]);
      else return;
    } else {
      QPointer<QDialog> dialog = new QDialog(this);
      dialog->setObjectName(QStringLiteral("searchHistoryDialog"));
      dialog->setWindowTitle(index ? tr("Replacement History") : tr("Search History"));
      auto* layout = new QVBoxLayout(dialog);
      auto* list = new QListWidget(dialog); list->setSelectionMode(QAbstractItemView::ExtendedSelection);
      assign_japanese_font(*list, JapaneseFontRole::kList);
      for (const auto& entry : cache.entries()) list->addItem(to_qstring(entry));
      layout->addWidget(list);
      layout->addWidget(new QLabel(tr("Delete removes selected entries immediately, even if you Cancel."), dialog));
      auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
      auto* copy = buttons->addButton(tr("Copy"), QDialogButtonBox::ActionRole);
      auto* remove = buttons->addButton(tr("Delete"), QDialogButtonBox::ActionRole);
      connect(copy, &QPushButton::clicked, dialog, [list] {
        QStringList text; for (auto* item : list->selectedItems()) text.append(item->text());
        QApplication::clipboard()->setText(text.join(QLatin1Char('\n')));
      });
      connect(remove, &QPushButton::clicked, dialog, [list, owner = histories_, index] {
        auto& history = index ? owner->replace : owner->search;
        for (auto* item : list->selectedItems()) {
          const auto at = history.find(from_qstring(item->text()));
          if (at) history.remove(*at);
          delete item;
        }
      });
      connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
      connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
      layout->addWidget(buttons); dialog->resize(560, 360);
      const int result = dialog->exec();
      if (!self || !dialog) return;
      const bool accepted = result == QDialog::Accepted && list->currentItem();
      if (accepted) selected = list->currentItem()->text();
      delete dialog;
      if (!self || !accepted || edit->text() != original) return;
    }
    if (edit->isReadOnly() || selected.size() > edit->maxLength()) return;
    edit->setText(selected); // Recall intentionally starts a new query-undo history.
    if (self && edit->text() == selected) { edit->selectAll(); edit->setFocus(); }
  } catch (const std::exception& error) {
    if (self) status_->setText(QString::fromUtf8(error.what()));
  }
}
}
