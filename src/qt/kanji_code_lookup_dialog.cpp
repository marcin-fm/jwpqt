// SPDX-License-Identifier: GPL-2.0-or-later

#include "kanji_code_lookup_dialog.h"

#include <exception>
#include <utility>

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include "jwpqt/core/jwp_text_codec.h"
#include "text_bridge.h"

namespace jwpqt::qt {
namespace {

QSpinBox* wildcard_spin(int maximum, const QString& object_name,
                        QWidget* parent) {
  auto* spin = new QSpinBox(parent);
  spin->setObjectName(object_name);
  spin->setRange(-1, maximum);
  spin->setSpecialValueText(KanjiCodeLookupDialog::tr("Any"));
  spin->setValue(-1);
  return spin;
}

core::KanjiCodeMatch item_match(const QListWidgetItem& item) {
  return {static_cast<core::JisCode>(item.data(Qt::UserRole).toUInt()),
          item.data(Qt::UserRole + 1).toBool()};
}

bool exact_or_any(const core::KanjiNumericRange& range,
                  std::uint8_t maximum) {
  return range.minimum <= range.maximum && range.maximum <= maximum &&
         (range.minimum == range.maximum ||
          (range.minimum == 0 && range.maximum == maximum));
}

int spin_value(const core::KanjiNumericRange& range) {
  return range.minimum == range.maximum ? static_cast<int>(range.minimum) : -1;
}

core::KanjiNumericRange spin_range(const QSpinBox& spin,
                                   std::uint8_t maximum) {
  return spin.value() < 0
             ? core::KanjiNumericRange{0, maximum}
             : core::KanjiNumericRange{
                   static_cast<std::uint8_t>(spin.value()),
                   static_cast<std::uint8_t>(spin.value())};
}

}  // namespace

KanjiCodeLookupDialog::KanjiCodeLookupDialog(
    const core::KanjiInfoDatabase& information, InsertHandler insert_handler,
    InfoHandler info_handler, QWidget* parent)
    : QDialog(parent),
      information_(information),
      insert_handler_(std::move(insert_handler)),
      info_handler_(std::move(info_handler)),
      tabs_(new QTabWidget(this)),
      skip_type_(wildcard_spin(4, QStringLiteral("skipType"), this)),
      skip_first_(wildcard_spin(20, QStringLiteral("skipFirst"), this)),
      skip_second_(wildcard_spin(24, QStringLiteral("skipSecond"), this)),
      skip_misclassifications_(new QCheckBox(tr("Include &miscodes"), this)),
      bushu_radical_(wildcard_spin(255, QStringLiteral("bushuRadical"), this)),
      bushu_strokes_(wildcard_spin(30, QStringLiteral("bushuStrokes"), this)),
      bushu_nelson_(new QCheckBox(tr("&Nelson radical"), this)),
      bushu_classical_(new QCheckBox(tr("&Classical radical"), this)),
      spahn_radical_strokes_(wildcard_spin(
          11, QStringLiteral("spahnRadicalStrokes"), this)),
      spahn_radical_(wildcard_spin(19, QStringLiteral("spahnRadical"), this)),
      spahn_other_strokes_(wildcard_spin(
          26, QStringLiteral("spahnOtherStrokes"), this)),
      spahn_index_(wildcard_spin(47, QStringLiteral("spahnIndex"), this)),
      results_(new QListWidget(this)),
      status_(new QLabel(this)),
      copy_button_(new QPushButton(tr("&Copy"), this)),
      insert_button_(new QPushButton(tr("&Insert"), this)),
      info_button_(new QPushButton(tr("&Information"), this)) {
  setObjectName(QStringLiteral("kanjiCodeLookupDialog"));
  setWindowTitle(tr("Kanji Code Lookup"));
  setModal(false);
  resize(580, 520);

  auto* outer = new QVBoxLayout(this);
  auto* skip_page = new QWidget(tabs_);
  auto* skip_layout = new QFormLayout(skip_page);
  skip_layout->addRow(tr("Type"), skip_type_);
  skip_layout->addRow(tr("First value"), skip_first_);
  skip_layout->addRow(tr("Second value"), skip_second_);
  skip_misclassifications_->setObjectName(
      QStringLiteral("skipMisclassifications"));
  skip_layout->addRow(skip_misclassifications_);
  tabs_->addTab(skip_page, tr("SKIP"));

  auto* corner_page = new QWidget(tabs_);
  auto* corner_layout = new QFormLayout(corner_page);
  for (std::size_t index = 0; index < 5; ++index) {
    QSpinBox* spin = wildcard_spin(
        9, QStringLiteral("fourCorner%1").arg(index + 1), corner_page);
    corner_digits_.push_back(spin);
    corner_layout->addRow(tr("Digit %1").arg(index + 1), spin);
  }
  tabs_->addTab(corner_page, tr("Four corner"));

  auto* bushu_page = new QWidget(tabs_);
  auto* bushu_layout = new QFormLayout(bushu_page);
  bushu_layout->addRow(tr("Radical number"), bushu_radical_);
  bushu_layout->addRow(tr("Stroke count"), bushu_strokes_);
  bushu_nelson_->setObjectName(QStringLiteral("bushuNelson"));
  bushu_classical_->setObjectName(QStringLiteral("bushuClassical"));
  bushu_nelson_->setChecked(true);
  bushu_classical_->setChecked(true);
  bushu_layout->addRow(bushu_nelson_);
  bushu_layout->addRow(bushu_classical_);
  tabs_->addTab(bushu_page, tr("Bushu"));

  auto* spahn_page = new QWidget(tabs_);
  auto* spahn_layout = new QFormLayout(spahn_page);
  spahn_layout->addRow(tr("Radical strokes"), spahn_radical_strokes_);
  spahn_layout->addRow(tr("Radical"), spahn_radical_);
  spahn_layout->addRow(tr("Other strokes"), spahn_other_strokes_);
  spahn_layout->addRow(tr("Kanji index"), spahn_index_);
  tabs_->addTab(spahn_page, tr("Spahn-Hadamitzky"));
  outer->addWidget(tabs_);

  auto* search_button = new QPushButton(tr("&Search"), this);
  search_button->setObjectName(QStringLiteral("kanjiCodeSearch"));
  outer->addWidget(search_button);
  results_->setObjectName(QStringLiteral("kanjiCodeResults"));
  results_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  outer->addWidget(results_, 1);
  status_->setObjectName(QStringLiteral("kanjiCodeStatus"));
  outer->addWidget(status_);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  copy_button_->setObjectName(QStringLiteral("kanjiCodeCopy"));
  insert_button_->setObjectName(QStringLiteral("kanjiCodeInsert"));
  info_button_->setObjectName(QStringLiteral("kanjiCodeInfo"));
  buttons->addButton(copy_button_, QDialogButtonBox::ActionRole);
  buttons->addButton(insert_button_, QDialogButtonBox::ActionRole);
  buttons->addButton(info_button_, QDialogButtonBox::ActionRole);
  outer->addWidget(buttons);

  connect(search_button, &QPushButton::clicked, this, [this] {
    switch (tabs_->currentIndex()) {
      case 0:
        (void)search_skip();
        break;
      case 1:
        (void)search_four_corner();
        break;
      case 2:
        (void)search_bushu();
        break;
      default:
        (void)search_spahn();
        break;
    }
  });
  connect(bushu_nelson_, &QCheckBox::toggled, this, [this](bool checked) {
    if (!checked && !bushu_classical_->isChecked())
      bushu_classical_->setChecked(true);
  });
  connect(bushu_classical_, &QCheckBox::toggled, this, [this](bool checked) {
    if (!checked && !bushu_nelson_->isChecked())
      bushu_nelson_->setChecked(true);
  });
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
  update_actions();
}

void KanjiCodeLookupDialog::set_skip_query(const core::KanjiSkipQuery& query) {
  if (!exact_or_any(query.type, 4) || !exact_or_any(query.first, 20) ||
      !exact_or_any(query.second, 24)) {
    throw core::KanjiInfoError(
        "Native SKIP query must be exact or completely wildcarded");
  }
  skip_type_->setValue(spin_value(query.type));
  skip_first_->setValue(spin_value(query.first));
  skip_second_->setValue(spin_value(query.second));
  skip_misclassifications_->setChecked(query.include_misclassifications);
  tabs_->setCurrentIndex(0);
}

void KanjiCodeLookupDialog::set_bushu_query(
    const core::KanjiBushuQuery& query) {
  if (!exact_or_any(query.radical, 255) ||
      !exact_or_any(query.strokes, 30) ||
      (!query.nelson && !query.classical)) {
    throw core::KanjiInfoError(
        "Native Bushu query is invalid or is not exact/wildcarded");
  }
  bushu_radical_->setValue(spin_value(query.radical));
  bushu_strokes_->setValue(spin_value(query.strokes));
  bushu_nelson_->setChecked(query.nelson);
  bushu_classical_->setChecked(query.classical);
  tabs_->setCurrentIndex(2);
}

void KanjiCodeLookupDialog::set_spahn_query(
    const core::KanjiSpahnQuery& query) {
  if (!exact_or_any(query.radical_strokes, 11) ||
      !exact_or_any(query.radical, 19) ||
      !exact_or_any(query.other_strokes, 26) ||
      !exact_or_any(query.index, 47)) {
    throw core::KanjiInfoError(
        "Native Spahn query must be exact or completely wildcarded");
  }
  spahn_radical_strokes_->setValue(spin_value(query.radical_strokes));
  spahn_radical_->setValue(spin_value(query.radical));
  spahn_other_strokes_->setValue(spin_value(query.other_strokes));
  spahn_index_->setValue(spin_value(query.index));
  tabs_->setCurrentIndex(3);
}

void KanjiCodeLookupDialog::set_four_corner_query(
    const core::KanjiFourCornerQuery& query) {
  for (const std::int8_t digit : query.digits) {
    if (digit < -1 || digit > 9)
      throw core::KanjiInfoError("Native four-corner digit is invalid");
  }
  for (std::size_t index = 0; index < query.digits.size(); ++index)
    corner_digits_[index]->setValue(query.digits[index]);
  tabs_->setCurrentIndex(1);
}

void KanjiCodeLookupDialog::select_skip_mode() { tabs_->setCurrentIndex(0); }

void KanjiCodeLookupDialog::select_four_corner_mode() {
  tabs_->setCurrentIndex(1);
}

void KanjiCodeLookupDialog::select_bushu_mode() { tabs_->setCurrentIndex(2); }

void KanjiCodeLookupDialog::select_spahn_mode() { tabs_->setCurrentIndex(3); }

bool KanjiCodeLookupDialog::search_skip() {
  core::KanjiSkipQuery query;
  query.type = spin_range(*skip_type_, 4);
  query.first = spin_range(*skip_first_, 20);
  query.second = spin_range(*skip_second_, 24);
  query.include_misclassifications = skip_misclassifications_->isChecked();
  try {
    return publish(core::search_kanji_skip(information_, query));
  } catch (const std::exception& error) {
    status_->setText(QString::fromUtf8(error.what()));
  } catch (...) {
    status_->setText(tr("SKIP lookup failed"));
  }
  return false;
}

bool KanjiCodeLookupDialog::search_bushu() {
  core::KanjiBushuQuery query;
  query.radical = spin_range(*bushu_radical_, 255);
  query.strokes = spin_range(*bushu_strokes_, 30);
  query.nelson = bushu_nelson_->isChecked();
  query.classical = bushu_classical_->isChecked();
  try {
    return publish(core::search_kanji_bushu(information_, query));
  } catch (const std::exception& error) {
    status_->setText(QString::fromUtf8(error.what()));
  } catch (...) {
    status_->setText(tr("Bushu lookup failed"));
  }
  return false;
}

bool KanjiCodeLookupDialog::search_spahn() {
  core::KanjiSpahnQuery query;
  query.radical_strokes = spin_range(*spahn_radical_strokes_, 11);
  query.radical = spin_range(*spahn_radical_, 19);
  query.other_strokes = spin_range(*spahn_other_strokes_, 26);
  query.index = spin_range(*spahn_index_, 47);
  try {
    return publish(core::search_kanji_spahn(information_, query));
  } catch (const std::exception& error) {
    status_->setText(QString::fromUtf8(error.what()));
  } catch (...) {
    status_->setText(tr("Spahn-Hadamitzky lookup failed"));
  }
  return false;
}

bool KanjiCodeLookupDialog::search_four_corner() {
  core::KanjiFourCornerQuery query;
  for (std::size_t index = 0; index < query.digits.size(); ++index)
    query.digits[index] =
        static_cast<std::int8_t>(corner_digits_[index]->value());
  try {
    return publish(core::search_kanji_four_corner(information_, query));
  } catch (const std::exception& error) {
    status_->setText(QString::fromUtf8(error.what()));
  } catch (...) {
    status_->setText(tr("Four-corner lookup failed"));
  }
  return false;
}

bool KanjiCodeLookupDialog::publish(core::KanjiCodeSearchReport report) {
  struct Rendered {
    QString text;
    QString tooltip;
    core::KanjiCodeMatch match;
  };
  std::vector<Rendered> rendered;
  rendered.reserve(report.matches.size());
  for (const core::KanjiCodeMatch& match : report.matches) {
    const std::u32string decoded = core::decode_jwp_text({match.code});
    if (decoded.size() != 1)
      throw core::KanjiInfoError("Kanji code result cannot be displayed");
    rendered.push_back(
        {to_qstring(decoded),
         match.alternate ? tr("Alternate code") : tr("Primary code"), match});
  }
  results_->clear();
  for (const Rendered& value : rendered) {
    auto* item = new QListWidgetItem(value.text, results_);
    item->setToolTip(value.tooltip);
    item->setData(Qt::UserRole, value.match.code);
    item->setData(Qt::UserRole + 1, value.match.alternate);
  }
  status_->setText(report.truncated
                       ? tr("%1 matches shown (result limit reached)")
                             .arg(results_->count())
                       : tr("%1 matches").arg(results_->count()));
  update_actions();
  return true;
}

std::vector<core::KanjiCodeMatch> KanjiCodeLookupDialog::results() const {
  std::vector<core::KanjiCodeMatch> values;
  values.reserve(static_cast<std::size_t>(results_->count()));
  for (int row = 0; row < results_->count(); ++row)
    values.push_back(item_match(*results_->item(row)));
  return values;
}

std::vector<core::JisCode> KanjiCodeLookupDialog::selected_codes() const {
  std::vector<core::JisCode> values;
  for (int row = 0; row < results_->count(); ++row) {
    const QListWidgetItem* item = results_->item(row);
    if (item->isSelected()) values.push_back(item_match(*item).code);
  }
  return values;
}

void KanjiCodeLookupDialog::update_actions() {
  const std::size_t selected = selected_codes().size();
  copy_button_->setEnabled(selected != 0);
  insert_button_->setEnabled(selected != 0 && static_cast<bool>(insert_handler_));
  info_button_->setEnabled(selected == 1 && static_cast<bool>(info_handler_));
}

void KanjiCodeLookupDialog::copy_results() {
  const std::vector<core::JisCode> codes = selected_codes();
  if (!codes.empty())
    QApplication::clipboard()->setText(to_qstring(core::decode_jwp_text(codes)));
}

void KanjiCodeLookupDialog::insert_results() {
  const std::vector<core::JisCode> codes = selected_codes();
  if (codes.empty() || !insert_handler_) return;
  try {
    insert_handler_(codes);
  } catch (const std::exception& error) {
    status_->setText(QString::fromUtf8(error.what()));
  } catch (...) {
    status_->setText(tr("Could not insert kanji results"));
  }
}

void KanjiCodeLookupDialog::show_information() {
  const std::vector<core::JisCode> codes = selected_codes();
  if (codes.size() != 1 || !info_handler_) return;
  try {
    info_handler_(codes.front());
  } catch (const std::exception& error) {
    status_->setText(QString::fromUtf8(error.what()));
  } catch (...) {
    status_->setText(tr("Could not show kanji information"));
  }
}

}  // namespace jwpqt::qt
