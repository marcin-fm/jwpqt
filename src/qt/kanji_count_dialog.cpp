// SPDX-License-Identifier: GPL-2.0-or-later

#include "kanji_count_dialog.h"
#include "auxiliary_find.h"
#include "japanese_fonts.h"

#include <algorithm>
#include <exception>
#include <utility>

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStringList>
#include <QVBoxLayout>

#include "jwpqt/core/jwp_text_codec.h"
#include "text_bridge.h"

namespace jwpqt::qt {
namespace {

std::vector<core::JwpDocument> copy_documents(
    const std::vector<const core::JwpDocument*>& documents) {
  if (documents.empty())
    throw core::KanjiInfoError("Kanji count requires a current document");
  std::vector<core::JwpDocument> snapshots;
  snapshots.reserve(documents.size());
  for (const core::JwpDocument* document : documents) {
    if (document == nullptr)
      throw core::KanjiInfoError("Kanji count document is null");
    snapshots.push_back(*document);
  }
  return snapshots;
}

void append_joined(std::u32string& output,
                   const std::vector<std::u32string>& values) {
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index != 0) output.push_back(U' ');
    output.append(values[index]);
  }
}

void append_decimal(std::u32string& output, std::size_t value) {
  for (const char ch : std::to_string(value))
    output.push_back(static_cast<char32_t>(ch));
}

}  // namespace

KanjiCountDialog::KanjiCountDialog(
    std::vector<const core::JwpDocument*> documents,
    const core::KanjiColorList& color_list,
    const core::KanjiInfoDatabase* information, InsertHandler insert_handler,
    InfoHandler info_handler, QWidget* parent)
    : QDialog(parent),
      documents_(copy_documents(documents)),
      color_list_(color_list),
      information_(information),
      insert_handler_(std::move(insert_handler)),
      info_handler_(std::move(info_handler)),
      all_documents_(new QCheckBox(tr("Count &all open documents"), this)),
      filter_(new QComboBox(this)),
      frequency_(new QCheckBox(tr("Show &frequency"), this)),
      on_readings_(new QCheckBox(tr("Show &on readings"), this)),
      kun_readings_(new QCheckBox(tr("Show &kun readings"), this)),
      meanings_(new QCheckBox(tr("Show &meanings"), this)),
      results_(new QListWidget(this)),
      status_(new QLabel(this)),
      copy_button_(new QPushButton(tr("&Copy"), this)),
      insert_button_(new QPushButton(tr("&Insert"), this)),
      info_button_(new QPushButton(tr("&Information"), this)) {
  setObjectName(QStringLiteral("kanjiCountDialog"));
  setWindowTitle(tr("Count Kanji"));
  setModal(false);
  resize(660, 520);

  all_documents_->setObjectName(QStringLiteral("kanjiCountAllDocuments"));
  all_documents_->setEnabled(documents_.size() > 1);
  frequency_->setObjectName(QStringLiteral("kanjiCountFrequency"));
  frequency_->setChecked(true);
  on_readings_->setObjectName(QStringLiteral("kanjiCountOn"));
  kun_readings_->setObjectName(QStringLiteral("kanjiCountKun"));
  meanings_->setObjectName(QStringLiteral("kanjiCountMeanings"));
  filter_->setObjectName(QStringLiteral("kanjiCountFilter"));
  filter_->addItem(tr("All kanji"),
                   static_cast<int>(core::KanjiCountFilter::kAll));
  filter_->addItem(tr("Exclude kanji color list"),
                   static_cast<int>(core::KanjiCountFilter::kExcludeColorList));
  filter_->addItem(tr("Only kanji color list"),
                   static_cast<int>(core::KanjiCountFilter::kColorListOnly));

  auto* outer = new QVBoxLayout(this);
  auto* options = new QFormLayout;
  options->addRow(all_documents_);
  options->addRow(tr("Include"), filter_);
  auto* columns = new QHBoxLayout;
  columns->addWidget(frequency_);
  columns->addWidget(on_readings_);
  columns->addWidget(kun_readings_);
  columns->addWidget(meanings_);
  options->addRow(columns);
  outer->addLayout(options);
  auto* count_button = new QPushButton(tr("&Count"), this);
  count_button->setObjectName(QStringLiteral("kanjiCountSearch"));
  count_button->setDefault(true);
  outer->addWidget(count_button);
  results_->setObjectName(QStringLiteral("kanjiCountResults"));
  assign_japanese_font(*results_, JapaneseFontRole::kList);
  new AuxiliaryFind(results_);
  results_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  outer->addWidget(results_, 1);
  status_->setObjectName(QStringLiteral("kanjiCountStatus"));
  outer->addWidget(status_);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  copy_button_->setObjectName(QStringLiteral("kanjiCountCopy"));
  insert_button_->setObjectName(QStringLiteral("kanjiCountInsert"));
  info_button_->setObjectName(QStringLiteral("kanjiCountInfo"));
  buttons->addButton(copy_button_, QDialogButtonBox::ActionRole);
  buttons->addButton(insert_button_, QDialogButtonBox::ActionRole);
  buttons->addButton(info_button_, QDialogButtonBox::ActionRole);
  outer->addWidget(buttons);

  connect(count_button, &QPushButton::clicked, this, [this] { (void)count(); });
  connect(results_, &QListWidget::itemSelectionChanged, this,
          [this] { update_actions(); });
  connect(results_, &QListWidget::itemDoubleClicked, this,
          [this](QListWidgetItem*) { show_information(); });
  connect(copy_button_, &QPushButton::clicked, this,
          [this] { copy_results(); });
  connect(insert_button_, &QPushButton::clicked, this,
          [this] { insert_results(); });
  connect(info_button_, &QPushButton::clicked, this,
          [this] { show_information(); });
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  update_actions();
}

void KanjiCountDialog::set_display_options(
    const KanjiCountDisplayOptions& options) {
  all_documents_->setChecked(options.all_documents && documents_.size() > 1);
  const int filter_index = filter_->findData(static_cast<int>(options.filter));
  if (filter_index < 0)
    throw core::KanjiInfoError("Kanji count filter is invalid");
  filter_->setCurrentIndex(filter_index);
  frequency_->setChecked(options.frequency);
  on_readings_->setChecked(options.on_readings);
  kun_readings_->setChecked(options.kun_readings);
  meanings_->setChecked(options.meanings);
}

void KanjiCountDialog::set_document_provider(DocumentProvider provider) {
  document_provider_ = std::move(provider);
}

KanjiCountDisplayOptions KanjiCountDialog::display_options() const {
  KanjiCountDisplayOptions options;
  options.all_documents = all_documents_->isChecked();
  options.filter = static_cast<core::KanjiCountFilter>(
      filter_->currentData().toInt());
  options.frequency = frequency_->isChecked();
  options.on_readings = on_readings_->isChecked();
  options.kun_readings = kun_readings_->isChecked();
  options.meanings = meanings_->isChecked();
  return options;
}

bool KanjiCountDialog::count() {
  try {
    if (document_provider_) {
      auto documents = document_provider_();
      if (documents.empty())
        throw core::KanjiCountError("Kanji count requires a current document");
      documents_ = std::move(documents);
      all_documents_->setEnabled(documents_.size() > 1);
      if (documents_.size() == 1) all_documents_->setChecked(false);
    }
    const KanjiCountDisplayOptions options = display_options();
    std::vector<const core::JwpDocument*> selected;
    if (options.all_documents) {
      selected.reserve(documents_.size());
      for (const core::JwpDocument& document : documents_)
        selected.push_back(&document);
    } else {
      selected.push_back(&documents_.front());
    }
    return publish(core::count_kanji(selected, color_list_, options.filter),
                   options);
  } catch (const std::exception& error) {
    status_->setText(QString::fromUtf8(error.what()));
  } catch (...) {
    status_->setText(tr("Kanji count failed"));
  }
  return false;
}

bool KanjiCountDialog::publish(core::KanjiCountReport report,
                               const KanjiCountDisplayOptions& options) {
  std::vector<RenderedEntry> candidate;
  candidate.reserve(report.entries.size());
  for (const core::KanjiCountEntry& entry : report.entries) {
    std::u32string row = core::decode_jwp_text({entry.code});
    if (row.size() != 1)
      throw core::KanjiInfoError("Kanji count result cannot be displayed");
    if (options.frequency) {
      row.push_back(U'\t');
      append_decimal(row, entry.count);
    }
    if (information_ != nullptr && information_->contains(entry.code) &&
        (options.on_readings || options.kun_readings || options.meanings)) {
      const core::KanjiInfoRecord information = information_->record(entry.code);
      if (options.on_readings && !information.on_readings.empty()) {
        row.push_back(U'\t');
        append_joined(row, information.on_readings);
      }
      if (options.kun_readings && !information.kun_readings.empty()) {
        row.push_back(U'\t');
        append_joined(row, information.kun_readings);
      }
      if (options.meanings && !information.meanings.empty()) {
        row.push_back(U'\t');
        append_joined(row, information.meanings);
      }
    }
    candidate.push_back({entry, std::move(row)});
  }
  results_->clear();
  rendered_ = std::move(candidate);
  for (const RenderedEntry& entry : rendered_)
    results_->addItem(to_qstring(entry.text));
  status_->setText(
      tr("%1 characters; %2 kanji (%3 unique); %4 on list, %5 off list%6")
          .arg(report.summary.total)
          .arg(report.summary.kanji_on_color_list +
               report.summary.kanji_off_color_list)
          .arg(report.summary.unique_kanji_on_color_list +
               report.summary.unique_kanji_off_color_list)
          .arg(report.summary.kanji_on_color_list)
          .arg(report.summary.kanji_off_color_list)
          .arg(report.truncated ? tr("; result limit reached") : QString()));
  update_actions();
  return true;
}

std::vector<core::KanjiCountEntry> KanjiCountDialog::results() const {
  std::vector<core::KanjiCountEntry> result;
  result.reserve(rendered_.size());
  for (const RenderedEntry& value : rendered_) result.push_back(value.entry);
  return result;
}

std::vector<int> KanjiCountDialog::selected_rows() const {
  std::vector<int> rows;
  for (const QListWidgetItem* item : results_->selectedItems())
    rows.push_back(results_->row(item));
  std::sort(rows.begin(), rows.end());
  return rows;
}

void KanjiCountDialog::update_actions() {
  const bool selected = !results_->selectedItems().empty();
  copy_button_->setEnabled(selected);
  insert_button_->setEnabled(selected && static_cast<bool>(insert_handler_));
  info_button_->setEnabled(selected && information_ != nullptr &&
                           static_cast<bool>(info_handler_));
}

void KanjiCountDialog::copy_results() {
  QStringList lines;
  for (const int row : selected_rows())
    lines.push_back(results_->item(row)->text());
  if (!lines.empty()) QApplication::clipboard()->setText(lines.join('\n'));
}

void KanjiCountDialog::insert_results() {
  if (!insert_handler_) return;
  std::u32string text;
  for (const int row : selected_rows()) {
    if (!text.empty()) text.push_back(U'\n');
    text.append(rendered_.at(static_cast<std::size_t>(row)).text);
  }
  if (text.empty()) return;
  try {
    insert_handler_(std::move(text));
  } catch (const std::exception& error) {
    status_->setText(QString::fromUtf8(error.what()));
  } catch (...) {
    status_->setText(tr("Could not insert kanji count results"));
  }
}

void KanjiCountDialog::show_information() {
  const std::vector<int> rows = selected_rows();
  if (rows.empty() || !info_handler_) return;
  try {
    info_handler_(rendered_.at(static_cast<std::size_t>(rows.front())).entry.code);
  } catch (const std::exception& error) {
    status_->setText(QString::fromUtf8(error.what()));
  } catch (...) {
    status_->setText(tr("Could not show kanji information"));
  }
}

}  // namespace jwpqt::qt
