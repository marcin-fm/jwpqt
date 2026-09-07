// SPDX-License-Identifier: GPL-2.0-or-later

#include "edict_lookup_dialog.h"

#include <algorithm>
#include <exception>
#include <memory>
#include <utility>

#include <QAction>
#include <QCheckBox>
#include <QContextMenuEvent>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QStringList>
#include <QTextBlockFormat>
#include <QTextDocument>
#include <QTextEdit>
#include <QVBoxLayout>

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
                                      QWidget* parent, InfoHandler info_handler)
    : QDialog(parent),
      search_handler_(std::move(search_handler)),
      insert_handler_(std::move(insert_handler)),
      info_handler_(std::move(info_handler)),
      query_field_(new KanaInputField(QStringLiteral("edictQuery"), this)),
      query_edit_(query_field_->edit()),
      personal_names_(new QCheckBox(tr("Personal &names"), this)),
      place_names_(new QCheckBox(tr("Place na&mes"), this)),
      classical_(new QCheckBox(tr("&Classical"), this)),
      results_(new QTextEdit(this)),
      status_(new QLabel(this)),
      insert_button_(new QPushButton(tr("&Insert in Document"), this)) {
  setObjectName(QStringLiteral("edictLookupDialog"));
  setWindowTitle(tr("Dictionary Lookup"));
  setModal(false);
  resize(780, 520);

  auto* outer = new QVBoxLayout(this);
  auto* query_row = new QHBoxLayout();
  query_edit_->setObjectName(QStringLiteral("edictQuery"));
  query_edit_->setClearButtonEnabled(true);
  auto* search_button = new QPushButton(tr("&Search"), this);
  search_button->setObjectName(QStringLiteral("edictSearch"));
  search_button->setDefault(true);
  query_row->addWidget(query_field_, 1);
  query_row->addWidget(search_button);
  outer->addLayout(query_row);

  auto* options = new QHBoxLayout();
  personal_names_->setObjectName(QStringLiteral("edictPersonalNames"));
  place_names_->setObjectName(QStringLiteral("edictPlaceNames"));
  classical_->setObjectName(QStringLiteral("edictClassical"));
  options->addWidget(personal_names_);
  options->addWidget(place_names_);
  options->addWidget(classical_);
  options->addStretch();
  outer->addLayout(options);

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
  connect(results_, &QTextEdit::selectionChanged, this,
          [this] { update_actions(); });
  connect(copy_action, &QAction::triggered, this,
          [this] { copy_selected(); });
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  status_->setText(tr("Enter a search term."));
  update_actions();
}

void EdictLookupDialog::set_query(std::u32string_view query) {
  query_edit_->setText(to_qstring(query));
  query_edit_->selectAll();
}

bool EdictLookupDialog::search() {
  query_field_->finish_input();
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
    const core::JwpText query =
        core::encode_jwp_text(from_qstring(query_edit_->text()));
    const EdictLookupOptions options{personal_names_->isChecked(),
                                     place_names_->isChecked(),
                                     classical_->isChecked()};
    EdictResourceSearchReport candidate = search_handler_(query, options);
    if (candidate.results.size() > kMaximumVisibleResults) {
      throw core::EdictSearchError(
          "Dictionary search returned too many interactive results");
    }

    std::vector<std::u32string> rows;
    rows.reserve(candidate.results.size());
    for (const EdictResourceSearchResult& result : candidate.results) {
      rows.push_back(render_row(result.result.record));
    }

    auto document = std::make_unique<QTextDocument>();
    document->setDefaultFont(results_->font());
    document->setUndoRedoEnabled(false);
    QTextCursor cursor(document.get());
    std::vector<std::pair<int, int>> ranges;
    ranges.reserve(rows.size());
    for (const auto& result : candidate.results) {
      if (!ranges.empty()) cursor.insertBlock();
      const int start = cursor.position();
      QTextBlockFormat heading;
      cursor.setBlockFormat(heading);
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
    report_ = std::move(candidate);
    rendered_rows_ = std::move(rows);
    row_ranges_ = std::move(ranges);
    const QPointer<QTextDocument> previous = results_->document();
    document->setParent(results_);
    results_->setDocument(document.release());
    if (previous && previous->parent() == results_) delete previous.data();
    if (!rendered_rows_.empty()) {
      QTextCursor selected(results_->document());
      selected.setPosition(row_ranges_.front().second, QTextCursor::KeepAnchor);
      results_->setTextCursor(selected);
      results_->setFocus();
    } else {
      query_edit_->setFocus();
    }
    show_status();
    update_actions();
    return true;
  } catch (const std::exception& error) {
    status_->setText(tr("Search failed: %1").arg(QString::fromUtf8(error.what())));
    query_edit_->setFocus();
    return false;
  } catch (...) {
    status_->setText(tr("Search failed with an unknown error."));
    query_edit_->setFocus();
    return false;
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
}

bool EdictLookupDialog::eventFilter(QObject* watched, QEvent* event) {
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
