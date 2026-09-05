// SPDX-License-Identifier: GPL-2.0-or-later

#include "kanji_lookup_dialog.h"

#include <exception>
#include <utility>

#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

#include "jwpqt/core/jwp_text_codec.h"
#include "text_bridge.h"

namespace jwpqt::qt {
namespace {

constexpr int kRadicalColumns = 16;
constexpr int kRadicalSourceSize = 16;

core::JisCode item_code(const QListWidgetItem& item) {
  return static_cast<core::JisCode>(item.data(Qt::UserRole).toUInt());
}

}  // namespace

KanjiLookupDialog::KanjiLookupDialog(
    const core::KanjiLookupLists& radical_lists,
    const core::KanjiLookupLists& stroke_lists,
    const core::KanjiInfoDatabase& information, QPixmap radical_sheet,
    InsertHandler insert_handler, InfoHandler info_handler, QWidget* parent)
    : QDialog(parent),
      radical_lists_(radical_lists),
      stroke_lists_(stroke_lists),
      information_(information),
      insert_handler_(std::move(insert_handler)),
      info_handler_(std::move(info_handler)),
      minimum_strokes_(new QSpinBox(this)),
      maximum_strokes_(new QSpinBox(this)),
      results_(new QListWidget(this)),
      status_(new QLabel(this)),
      copy_button_(new QPushButton(tr("&Copy"), this)),
      insert_button_(new QPushButton(tr("&Insert"), this)),
      info_button_(new QPushButton(tr("&Information"), this)) {
  setObjectName(QStringLiteral("kanjiLookupDialog"));
  setWindowTitle(tr("Radical and Stroke Lookup"));
  setModal(false);
  resize(760, 700);

  auto* outer = new QVBoxLayout(this);
  auto* radical_group = new QGroupBox(tr("Radicals"), this);
  auto* radical_outer = new QVBoxLayout(radical_group);
  auto* scroll = new QScrollArea(radical_group);
  scroll->setWidgetResizable(true);
  auto* radical_widget = new QWidget(scroll);
  auto* radical_grid = new QGridLayout(radical_widget);
  radical_grid->setSpacing(2);
  radical_buttons_.reserve(radical_lists_.group_count());
  const bool has_sheet = !radical_sheet.isNull() &&
                         radical_sheet.width() >= kRadicalSourceSize &&
                         radical_sheet.height() >=
                             static_cast<int>(radical_lists_.group_count()) *
                                 kRadicalSourceSize;
  for (std::size_t index = 0; index < radical_lists_.group_count(); ++index) {
    auto* button = new QToolButton(radical_widget);
    button->setCheckable(true);
    button->setObjectName(
        QStringLiteral("radicalButton%1").arg(index + 1));
    button->setToolTip(tr("Radical group %1").arg(index + 1));
    button->setFixedSize(30, 30);
    if (has_sheet) {
      button->setIcon(QIcon(radical_sheet.copy(
          0, static_cast<int>(index) * kRadicalSourceSize,
          kRadicalSourceSize, kRadicalSourceSize)));
      button->setIconSize(QSize(22, 22));
    } else {
      button->setText(QString::number(index + 1));
    }
    radical_grid->addWidget(button, static_cast<int>(index) / kRadicalColumns,
                            static_cast<int>(index) % kRadicalColumns);
    radical_buttons_.push_back(button);
  }
  scroll->setWidget(radical_widget);
  radical_outer->addWidget(scroll);
  outer->addWidget(radical_group, 2);

  auto* controls = new QHBoxLayout;
  minimum_strokes_->setObjectName(QStringLiteral("minimumStrokes"));
  maximum_strokes_->setObjectName(QStringLiteral("maximumStrokes"));
  minimum_strokes_->setRange(1, 30);
  maximum_strokes_->setRange(1, 30);
  minimum_strokes_->setValue(1);
  maximum_strokes_->setValue(30);
  controls->addWidget(new QLabel(tr("Strokes from"), this));
  controls->addWidget(minimum_strokes_);
  controls->addWidget(new QLabel(tr("to"), this));
  controls->addWidget(maximum_strokes_);
  auto* search_button = new QPushButton(tr("&Search"), this);
  search_button->setObjectName(QStringLiteral("kanjiLookupSearch"));
  controls->addWidget(search_button);
  controls->addStretch();
  outer->addLayout(controls);

  results_->setObjectName(QStringLiteral("kanjiLookupResults"));
  results_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  outer->addWidget(results_, 1);
  status_->setObjectName(QStringLiteral("kanjiLookupStatus"));
  outer->addWidget(status_);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  copy_button_->setObjectName(QStringLiteral("kanjiLookupCopy"));
  insert_button_->setObjectName(QStringLiteral("kanjiLookupInsert"));
  info_button_->setObjectName(QStringLiteral("kanjiLookupInfo"));
  buttons->addButton(copy_button_, QDialogButtonBox::ActionRole);
  buttons->addButton(insert_button_, QDialogButtonBox::ActionRole);
  buttons->addButton(info_button_, QDialogButtonBox::ActionRole);
  outer->addWidget(buttons);

  connect(search_button, &QPushButton::clicked, this,
          [this] { (void)search(); });
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(results_, &QListWidget::itemSelectionChanged, this,
          [this] { update_result_actions(); });
  connect(results_, &QListWidget::itemDoubleClicked, this,
          [this](QListWidgetItem*) { show_information(); });
  connect(copy_button_, &QPushButton::clicked, this,
          [this] { copy_results(); });
  connect(insert_button_, &QPushButton::clicked, this,
          [this] { insert_results(); });
  connect(info_button_, &QPushButton::clicked, this,
          [this] { show_information(); });
  update_result_actions();
}

void KanjiLookupDialog::set_selected_radicals(
    const std::vector<std::size_t>& radicals) {
  for (const std::size_t index : radicals) {
    if (index >= radical_buttons_.size()) {
      throw core::KanjiLookupListError("Selected radical is out of range");
    }
  }
  for (QToolButton* button : radical_buttons_) button->setChecked(false);
  for (const std::size_t index : radicals) {
    radical_buttons_[index]->setChecked(true);
  }
}

std::vector<std::size_t> KanjiLookupDialog::selected_radicals() const {
  std::vector<std::size_t> selected;
  for (std::size_t index = 0; index < radical_buttons_.size(); ++index) {
    if (radical_buttons_[index]->isChecked()) selected.push_back(index);
  }
  return selected;
}

void KanjiLookupDialog::set_stroke_range(std::uint8_t minimum,
                                         std::uint8_t maximum) {
  if (minimum < 1 || maximum > 30 || minimum > maximum) {
    throw core::KanjiLookupListError("Kanji stroke range is invalid");
  }
  minimum_strokes_->setValue(minimum);
  maximum_strokes_->setValue(maximum);
}

bool KanjiLookupDialog::search() {
  try {
    core::KanjiLookupOptions options;
    options.radicals = selected_radicals();
    options.minimum_strokes =
        static_cast<std::uint8_t>(minimum_strokes_->value());
    options.maximum_strokes =
        static_cast<std::uint8_t>(maximum_strokes_->value());
    options.rare_last = true;
    const core::KanjiLookupReport report = core::search_kanji_radicals(
        radical_lists_, stroke_lists_, &information_, options);
    struct RenderedResult {
      QString text;
      QString tooltip;
      core::JisCode code;
    };
    std::vector<RenderedResult> rendered;
    rendered.reserve(report.results.size());
    for (const core::KanjiLookupResult& result : report.results) {
      const std::u32string decoded = core::decode_jwp_text({result.code});
      if (decoded.size() != 1) {
        throw core::KanjiLookupListError(
            "Kanji lookup result cannot be displayed");
      }
      rendered.push_back(
          {to_qstring(decoded),
           tr("JIS %1, %2 strokes")
               .arg(result.code, 4, 16, QLatin1Char('0'))
               .arg(result.strokes),
           result.code});
    }
    results_->clear();
    for (const RenderedResult& result : rendered) {
      auto* item = new QListWidgetItem(result.text, results_);
      item->setData(Qt::UserRole, result.code);
      item->setToolTip(result.tooltip);
    }
    status_->setText(report.truncated
                         ? tr("%1 matches shown (result limit reached)")
                               .arg(results_->count())
                         : tr("%1 matches").arg(results_->count()));
    update_result_actions();
    return true;
  } catch (const std::exception& error) {
    status_->setText(QString::fromUtf8(error.what()));
  } catch (...) {
    status_->setText(tr("Kanji lookup failed"));
  }
  return false;
}

std::vector<core::JisCode> KanjiLookupDialog::result_codes() const {
  std::vector<core::JisCode> codes;
  codes.reserve(static_cast<std::size_t>(results_->count()));
  for (int row = 0; row < results_->count(); ++row)
    codes.push_back(item_code(*results_->item(row)));
  return codes;
}

std::vector<core::JisCode> KanjiLookupDialog::selected_result_codes() const {
  std::vector<core::JisCode> codes;
  for (int row = 0; row < results_->count(); ++row) {
    const QListWidgetItem* item = results_->item(row);
    if (item->isSelected()) codes.push_back(item_code(*item));
  }
  return codes;
}

void KanjiLookupDialog::update_result_actions() {
  const std::size_t selected = selected_result_codes().size();
  copy_button_->setEnabled(selected != 0);
  insert_button_->setEnabled(selected != 0 && static_cast<bool>(insert_handler_));
  info_button_->setEnabled(selected == 1 && static_cast<bool>(info_handler_));
}

void KanjiLookupDialog::copy_results() {
  const std::vector<core::JisCode> codes = selected_result_codes();
  if (codes.empty()) return;
  QApplication::clipboard()->setText(to_qstring(core::decode_jwp_text(codes)));
}

void KanjiLookupDialog::insert_results() {
  const std::vector<core::JisCode> codes = selected_result_codes();
  if (codes.empty() || !insert_handler_) return;
  try {
    insert_handler_(codes);
  } catch (const std::exception& error) {
    status_->setText(QString::fromUtf8(error.what()));
  } catch (...) {
    status_->setText(tr("Could not insert kanji results"));
  }
}

void KanjiLookupDialog::show_information() {
  const std::vector<core::JisCode> codes = selected_result_codes();
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
