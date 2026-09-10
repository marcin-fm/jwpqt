// SPDX-License-Identifier: GPL-2.0-or-later

#include "edict_results_window.h"
#include "auxiliary_find.h"
#include "text_bridge.h"
#include "japanese_fonts.h"
#include "source_highlight.h"
#include <QEvent>
#include <QSignalBlocker>

#include <algorithm>
#include <exception>
#include <limits>
#include <stdexcept>
#include <utility>

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QPointer>
#include <QShortcut>
#include <QVBoxLayout>

namespace jwpqt::qt {
namespace {

QString from_utf32(std::u32string_view text) {
  return to_qstring(text);
}

std::u32string render_record(const core::EdictRecord& record) {
  std::u32string output = record.headword;
  if (!record.readings.empty()) {
    output.append(U" [");
    for (std::size_t i = 0; i < record.readings.size(); ++i) {
      if (i != 0) {
        output.append(U"; ");
      }
      output.append(record.readings[i]);
    }
    output.push_back(U']');
  }
  output.append(U" /");
  for (const std::u32string& definition : record.definitions) {
    output.append(definition);
    output.push_back(U'/');
  }
  return output;
}

void checked_add(std::size_t& destination, std::size_t value,
                 const char* message) {
  if (destination > std::numeric_limits<std::size_t>::max() - value) {
    throw std::overflow_error(message);
  }
  destination += value;
}

}  // namespace

EdictResultsWindow::EdictResultsWindow(QWidget* parent)
    : QWidget(parent, Qt::Window),
      results_(new QListWidget(this)),
      status_(new QLabel(this)),
      copy_button_(new QPushButton(tr("&Copy"), this)),
      insert_button_(new QPushButton(tr("&Insert"), this)) {
  setObjectName(QStringLiteral("edictResultsWindow"));
  setWindowTitle(tr("Dictionary Results"));
  setAttribute(Qt::WA_DeleteOnClose);
  resize(760, 420);

  results_->setObjectName(QStringLiteral("edictResultsList"));
  results_->installEventFilter(this);
  assign_japanese_font(*results_, JapaneseFontRole::kList);
  auto* find = new AuxiliaryFind(results_);
  find->set_result_insertion(insert_button_);
  results_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  status_->setObjectName(QStringLiteral("edictResultsStatus"));
  copy_button_->setObjectName(QStringLiteral("edictResultsCopy"));
  insert_button_->setObjectName(QStringLiteral("edictResultsInsert"));
  copy_button_->setEnabled(false);
  insert_button_->setEnabled(false);

  auto* buttons = new QHBoxLayout;
  buttons->addWidget(status_);
  buttons->addStretch();
  buttons->addWidget(copy_button_);
  buttons->addWidget(insert_button_);

  auto* layout = new QVBoxLayout(this);
  layout->addWidget(results_);
  layout->addLayout(buttons);

  connect(results_, &QListWidget::itemSelectionChanged, this, [this] {
    const bool selected = !results_->selectedItems().empty();
    copy_button_->setEnabled(selected);
    insert_button_->setEnabled(selected && static_cast<bool>(insert_handler_));
  });
  connect(copy_button_, &QPushButton::clicked, this,
          [this] { copy_selected(); });
  connect(insert_button_, &QPushButton::clicked, this,
          [this] { insert_selected(); });
  connect(results_, &QListWidget::itemDoubleClicked, this,
          [this](QListWidgetItem*) { insert_selected(); });

  auto* copy_shortcut = new QShortcut(QKeySequence::Copy, this);
  connect(copy_shortcut, &QShortcut::activated, this,
          [this] { copy_selected(); });
  update_status();
}

void EdictResultsWindow::set_insert_handler(InsertHandler handler) {
  insert_handler_ = std::move(handler);
  insert_button_->setEnabled(static_cast<bool>(insert_handler_) &&
                             !results_->selectedItems().empty());
}

void EdictResultsWindow::append_report(EdictResourceSearchReport report) {
  if (stored_results_.size() >
      static_cast<std::size_t>(std::numeric_limits<int>::max()) -
          report.results.size()) {
    throw std::length_error("Too many EDICT results to display");
  }

  std::vector<StoredResult> additions;
  std::vector<QString> rendered;
  additions.reserve(report.results.size());
  for (const EdictResourceSearchResult& result : report.results) {
    const core::EdictRecord& record = result.result.record;
    additions.push_back({render_record(record), record.headword,
                         record.readings.empty() ? record.headword
                                                 : record.readings.front(),
                         result.label});
    rendered.push_back(from_utf32(additions.back().text));
  }
  std::size_t new_rejected = rejected_;
  checked_add(new_rejected, report.rejected, "EDICT rejected count overflows");
  std::size_t visible_failures = 0;
  for (const EdictResourceFailure& failure : report.failures) {
    if (!failure.quiet) {
      checked_add(visible_failures, 1, "EDICT failure count overflows");
    }
  }
  std::size_t new_failures = failures_;
  checked_add(new_failures, visible_failures,
              "EDICT failure count overflows");

  for (std::size_t index = 0; index < additions.size(); ++index) {
    auto& result = additions[index];
    auto* item = new QListWidgetItem(rendered[index]);
    item->setToolTip(result.source);
    item->setData(kSourceHighlight, report.results[index].highlighted);
    if (report.results[index].highlighted)
      item->setForeground(source_highlight_color(results_->palette(), highlight_color_));
    results_->addItem(item);
    stored_results_.push_back(std::move(result));
  }
  rejected_ = new_rejected;
  failures_ = new_failures;
  update_status();
}

void EdictResultsWindow::set_highlight_color(const QColor& color) {
  highlight_color_ = color;
  update_highlights();
}

void EdictResultsWindow::update_highlights() {
  const QSignalBlocker blocker(results_->model());
  const auto color = source_highlight_color(results_->palette(), highlight_color_);
  for (int i = 0; i < results_->count(); ++i)
    if (results_->item(i)->data(kSourceHighlight).toBool()) results_->item(i)->setForeground(color);
  results_->viewport()->update();
}

bool EdictResultsWindow::eventFilter(QObject* watched, QEvent* event) {
  if (watched == results_ && event->type() == QEvent::PaletteChange) update_highlights();
  if (watched == results_ &&
      (event->type() == QEvent::ShortcutOverride ||
       event->type() == QEvent::KeyPress)) {
    const auto* key = static_cast<QKeyEvent*>(event);
    const auto modifiers = key->modifiers() & ~Qt::KeypadModifier;
    const bool copy_field = modifiers == Qt::ControlModifier &&
                            (key->key() == Qt::Key_E ||
                             key->key() == Qt::Key_R);
    const bool select_result = key->key() == Qt::Key_W &&
        (modifiers == Qt::ControlModifier ||
         modifiers == (Qt::ControlModifier | Qt::ShiftModifier));
    if (copy_field || select_result) {
      event->accept();
      if (event->type() == QEvent::KeyPress) {
        if (copy_field) copy_current_field(key->key() == Qt::Key_R);
        else select_current_result();
      }
      return true;
    }
  }
  return QWidget::eventFilter(watched, event);
}

void EdictResultsWindow::copy_current_field(bool reading) {
  const int row = results_->currentRow();
  if (row < 0 || static_cast<std::size_t>(row) >= stored_results_.size()) {
    QApplication::clipboard()->clear();
    return;
  }
  const StoredResult& result = stored_results_[static_cast<std::size_t>(row)];
  QApplication::clipboard()->setText(
      from_utf32(reading ? result.reading : result.headword));
}

void EdictResultsWindow::select_current_result() {
  const int row = results_->currentRow();
  if (row < 0) return;
  results_->setCurrentRow(row, QItemSelectionModel::ClearAndSelect);
}

void EdictResultsWindow::clear_results() {
  results_->clear();
  stored_results_.clear();
  rejected_ = 0;
  failures_ = 0;
  update_status();
}

std::size_t EdictResultsWindow::result_count() const noexcept {
  return stored_results_.size();
}

std::vector<int> EdictResultsWindow::selected_rows() const {
  std::vector<int> rows;
  rows.reserve(results_->selectedItems().size());
  for (QListWidgetItem* item : results_->selectedItems()) {
    const int row = results_->row(item);
    if (row >= 0) {
      rows.push_back(row);
    }
  }
  std::sort(rows.begin(), rows.end());
  return rows;
}

std::u32string EdictResultsWindow::selected_text() const {
  std::u32string output;
  const std::vector<int> rows = selected_rows();
  for (std::size_t i = 0; i < rows.size(); ++i) {
    if (i != 0) {
      output.push_back(U'\n');
    }
    output.append(stored_results_.at(static_cast<std::size_t>(rows[i])).text);
  }
  return output;
}

void EdictResultsWindow::copy_selected() {
  const std::u32string text = selected_text();
  if (!text.empty()) {
    QApplication::clipboard()->setText(from_utf32(text));
  }
}

void EdictResultsWindow::insert_selected() {
  if (!insert_handler_) {
    return;
  }
  const std::u32string text = selected_text();
  if (text.empty()) {
    return;
  }
  const QPointer<EdictResultsWindow> self(this);
  auto handler = insert_handler_;
  try {
    if (!handler(text) && self) {
      status_->setText(tr("Could not insert the selected result"));
    }
  } catch (const std::exception& error) {
    if (self) status_->setText(tr("Insert failed: %1")
                         .arg(QString::fromUtf8(error.what())));
  } catch (...) {
    if (self) status_->setText(tr("Insert failed: unknown error"));
  }
}

void EdictResultsWindow::update_status() {
  QString text = tr("%1 result(s), %2 rejected")
                     .arg(static_cast<qulonglong>(stored_results_.size()))
                     .arg(static_cast<qulonglong>(rejected_));
  if (failures_ != 0) {
    text += tr(", %1 resource error(s)")
                .arg(static_cast<qulonglong>(failures_));
  }
  status_->setText(text);
}

}  // namespace jwpqt::qt
