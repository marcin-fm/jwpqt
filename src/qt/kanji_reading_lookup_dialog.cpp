// SPDX-License-Identifier: GPL-2.0-or-later

#include "kanji_reading_lookup_dialog.h"

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
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "jwpqt/core/jwp_text_codec.h"
#include "kana_input_field.h"
#include "text_bridge.h"

namespace jwpqt::qt {
namespace {

core::KanjiReadingKind selected_kind(const QComboBox& combo) {
  return static_cast<core::KanjiReadingKind>(combo.currentData().toInt());
}

core::JisCode item_code(const QListWidgetItem& item) {
  return static_cast<core::JisCode>(item.data(Qt::UserRole).toUInt());
}

}  // namespace

KanjiReadingLookupDialog::KanjiReadingLookupDialog(
    const core::KanjiInfoDatabase& information, InsertHandler insert_handler,
    InfoHandler info_handler, QWidget* parent)
    : QDialog(parent),
      information_(information),
      insert_handler_(std::move(insert_handler)),
      info_handler_(std::move(info_handler)),
      kind_(new QComboBox(this)),
      query_field_(new KanaInputField(QStringLiteral("kanjiReadingQuery"), this)),
      query_(query_field_->edit()),
      minimum_strokes_(new QSpinBox(this)),
      maximum_strokes_(new QSpinBox(this)),
      flexible_kun_(new QCheckBox(tr("Flexible &kun-yomi matching"), this)),
      partial_words_(new QCheckBox(tr("Allow partial-&word matches"), this)),
      results_(new QListWidget(this)),
      status_(new QLabel(this)),
      copy_button_(new QPushButton(tr("&Copy"), this)),
      insert_button_(new QPushButton(tr("&Insert"), this)),
      info_button_(new QPushButton(tr("&Information"), this)) {
  setObjectName(QStringLiteral("kanjiReadingLookupDialog"));
  setWindowTitle(tr("Reading Lookup"));
  setModal(false);
  resize(580, 520);

  kind_->setObjectName(QStringLiteral("kanjiReadingKind"));
  const auto add_kind = [this](const QString& label,
                               core::KanjiReadingKind kind) {
    kind_->addItem(label, static_cast<int>(kind));
  };
  add_kind(tr("On-yomi"), core::KanjiReadingKind::kOn);
  add_kind(tr("Kun-yomi"), core::KanjiReadingKind::kKun);
  add_kind(tr("On-yomi or kun-yomi"), core::KanjiReadingKind::kOnOrKun);
  add_kind(tr("Meaning"), core::KanjiReadingKind::kMeaning);
  add_kind(tr("Nanori"), core::KanjiReadingKind::kNanori);
  add_kind(tr("Pinyin"), core::KanjiReadingKind::kPinyin);
  add_kind(tr("Korean"), core::KanjiReadingKind::kKorean);
  query_->setObjectName(QStringLiteral("kanjiReadingQuery"));
  minimum_strokes_->setObjectName(QStringLiteral("kanjiReadingMinimumStrokes"));
  maximum_strokes_->setObjectName(QStringLiteral("kanjiReadingMaximumStrokes"));
  minimum_strokes_->setRange(0, 30);
  maximum_strokes_->setRange(0, 30);
  maximum_strokes_->setValue(30);
  flexible_kun_->setObjectName(QStringLiteral("kanjiReadingFlexibleKun"));
  partial_words_->setObjectName(QStringLiteral("kanjiReadingPartialWords"));

  auto* outer = new QVBoxLayout(this);
  auto* form = new QFormLayout;
  form->addRow(tr("Type"), kind_);
  form->addRow(tr("Reading or text"), query_field_);
  auto* strokes = new QHBoxLayout;
  strokes->addWidget(minimum_strokes_);
  strokes->addWidget(new QLabel(tr("to"), this));
  strokes->addWidget(maximum_strokes_);
  form->addRow(tr("Strokes"), strokes);
  form->addRow(flexible_kun_);
  form->addRow(partial_words_);
  outer->addLayout(form);

  auto* search_button = new QPushButton(tr("&Search"), this);
  search_button->setObjectName(QStringLiteral("kanjiReadingSearch"));
  search_button->setDefault(true);
  outer->addWidget(search_button);
  results_->setObjectName(QStringLiteral("kanjiReadingResults"));
  results_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  outer->addWidget(results_, 1);
  status_->setObjectName(QStringLiteral("kanjiReadingStatus"));
  outer->addWidget(status_);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  copy_button_->setObjectName(QStringLiteral("kanjiReadingCopy"));
  insert_button_->setObjectName(QStringLiteral("kanjiReadingInsert"));
  info_button_->setObjectName(QStringLiteral("kanjiReadingInfo"));
  buttons->addButton(copy_button_, QDialogButtonBox::ActionRole);
  buttons->addButton(insert_button_, QDialogButtonBox::ActionRole);
  buttons->addButton(info_button_, QDialogButtonBox::ActionRole);
  outer->addWidget(buttons);

  connect(kind_, &QComboBox::currentIndexChanged, this,
          [this] { update_mode(); });
  connect(search_button, &QPushButton::clicked, this,
          [this] { (void)search(); });
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
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
  kind_->setCurrentIndex(2);
  update_mode();
  update_actions();
}

void KanjiReadingLookupDialog::set_query(
    const core::KanjiReadingQuery& query) {
  const int index = kind_->findData(static_cast<int>(query.kind));
  if (index < 0 || query.strokes.minimum > query.strokes.maximum ||
      query.strokes.maximum > 30) {
    throw core::KanjiInfoError("Native reading query is invalid");
  }
  kind_->setCurrentIndex(index);
  query_->setText(to_qstring(query.text));
  minimum_strokes_->setValue(query.strokes.minimum);
  maximum_strokes_->setValue(query.strokes.maximum);
  flexible_kun_->setChecked(query.flexible_kun);
  partial_words_->setChecked(query.partial_words);
}

void KanjiReadingLookupDialog::set_query_text(std::u32string_view text) {
  query_->setText(to_qstring(text));
}

bool KanjiReadingLookupDialog::search() {
  query_field_->finish_input();
  core::KanjiReadingQuery query;
  query.kind = selected_kind(*kind_);
  query.text = from_qstring(query_->text());
  query.strokes = {static_cast<std::uint8_t>(minimum_strokes_->value()),
                   static_cast<std::uint8_t>(maximum_strokes_->value())};
  query.flexible_kun = flexible_kun_->isChecked();
  query.partial_words = partial_words_->isChecked();
  try {
    return publish(core::search_kanji_readings(information_, query));
  } catch (const std::exception& error) {
    status_->setText(QString::fromUtf8(error.what()));
  } catch (...) {
    status_->setText(tr("Reading lookup failed"));
  }
  return false;
}

bool KanjiReadingLookupDialog::publish(core::KanjiCodeSearchReport report) {
  std::vector<QString> rendered;
  rendered.reserve(report.matches.size());
  for (const core::KanjiCodeMatch& match : report.matches) {
    const std::u32string decoded = core::decode_jwp_text({match.code});
    if (decoded.size() != 1)
      throw core::KanjiInfoError("Reading result cannot be displayed");
    rendered.push_back(to_qstring(decoded));
  }
  results_->clear();
  for (std::size_t index = 0; index < rendered.size(); ++index) {
    auto* item = new QListWidgetItem(rendered[index], results_);
    item->setData(Qt::UserRole, report.matches[index].code);
  }
  status_->setText(report.truncated
                       ? tr("%1 matches shown (result limit reached)")
                             .arg(results_->count())
                       : tr("%1 matches").arg(results_->count()));
  update_actions();
  return true;
}

std::vector<core::JisCode> KanjiReadingLookupDialog::results() const {
  std::vector<core::JisCode> values;
  values.reserve(static_cast<std::size_t>(results_->count()));
  for (int row = 0; row < results_->count(); ++row)
    values.push_back(item_code(*results_->item(row)));
  return values;
}

std::vector<core::JisCode> KanjiReadingLookupDialog::selected_codes() const {
  std::vector<core::JisCode> values;
  for (int row = 0; row < results_->count(); ++row) {
    if (results_->item(row)->isSelected())
      values.push_back(item_code(*results_->item(row)));
  }
  return values;
}

void KanjiReadingLookupDialog::update_mode() {
  const core::KanjiReadingKind kind = selected_kind(*kind_);
  query_field_->set_input_mode(
      kind == core::KanjiReadingKind::kMeaning || kind == core::KanjiReadingKind::kPinyin ||
              kind == core::KanjiReadingKind::kKorean
          ? InputMode::kAscii : InputMode::kKanji);
  flexible_kun_->setEnabled(kind == core::KanjiReadingKind::kKun ||
                            kind == core::KanjiReadingKind::kOnOrKun);
  partial_words_->setEnabled(kind == core::KanjiReadingKind::kMeaning);
}

void KanjiReadingLookupDialog::update_actions() {
  const bool selected = !results_->selectedItems().isEmpty();
  copy_button_->setEnabled(selected);
  insert_button_->setEnabled(selected && static_cast<bool>(insert_handler_));
  info_button_->setEnabled(selected && static_cast<bool>(info_handler_));
}

void KanjiReadingLookupDialog::copy_results() {
  try {
    core::JwpText text;
    for (const core::JisCode code : selected_codes()) text.push_back(code);
    QApplication::clipboard()->setText(to_qstring(core::decode_jwp_text(text)));
    status_->setText(tr("Copied %1 characters").arg(text.size()));
  } catch (const std::exception& error) {
    status_->setText(QString::fromUtf8(error.what()));
  } catch (...) {
    status_->setText(tr("Copy failed"));
  }
}

void KanjiReadingLookupDialog::insert_results() {
  try {
    const std::vector<core::JisCode> codes = selected_codes();
    if (!codes.empty() && insert_handler_) insert_handler_(codes);
  } catch (const std::exception& error) {
    status_->setText(QString::fromUtf8(error.what()));
  } catch (...) {
    status_->setText(tr("Insertion failed"));
  }
}

void KanjiReadingLookupDialog::show_information() {
  try {
    const std::vector<core::JisCode> codes = selected_codes();
    if (!codes.empty() && info_handler_) info_handler_(codes.front());
  } catch (const std::exception& error) {
    status_->setText(QString::fromUtf8(error.what()));
  } catch (...) {
    status_->setText(tr("Information lookup failed"));
  }
}

}  // namespace jwpqt::qt
