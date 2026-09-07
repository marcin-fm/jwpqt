// SPDX-License-Identifier: GPL-2.0-or-later

#include "kanji_lookup_dialog.h"

#include <exception>
#include <utility>

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QEvent>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "jwpqt/core/jwp_text_codec.h"
#include "jwpqt/core/kanji_bushu_selector.h"
#include "lookup_artwork.h"
#include "text_bridge.h"
#include "japanese_fonts.h"

namespace jwpqt::qt {
namespace {

constexpr int kRadicalColumns = 28;
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
      radical_sheet_(std::move(radical_sheet)),
      minimum_strokes_(new QSpinBox(this)),
      maximum_strokes_(new QSpinBox(this)),
      search_timer_(new QTimer(this)),
      results_(new QListWidget(this)),
      status_(new QLabel(this)),
      copy_button_(new QPushButton(tr("&Copy"), this)),
      insert_button_(new QPushButton(tr("&Insert"), this)),
      info_button_(new QPushButton(tr("&Information"), this)) {
  setObjectName(QStringLiteral("kanjiLookupDialog"));
  setWindowTitle(tr("Radical and Stroke Lookup"));
  setModal(false);
  resize(900, 540);

  auto* outer = new QVBoxLayout(this);
  auto* radical_group = new QGroupBox(tr("Radicals"), this);
  auto* radical_outer = new QVBoxLayout(radical_group);
  auto* scroll = new QScrollArea(radical_group);
  scroll->setWidgetResizable(true);
  auto* radical_widget = new QWidget(scroll);
  auto* radical_grid = new QGridLayout(radical_widget);
  radical_grid->setSpacing(0);
  radical_buttons_.reserve(radical_lists_.group_count());
  const bool has_sheet = !radical_sheet_.isNull() &&
                         radical_sheet_.width() >= kRadicalSourceSize &&
                         radical_sheet_.height() >=
                             static_cast<int>(radical_lists_.group_count()) *
                                 kRadicalSourceSize;
  std::vector<std::size_t> stroke_starts;
  if (radical_lists_.group_count() == 241) {
    for (std::uint8_t strokes = 1; strokes <= core::kMaximumBushuRadicalStrokes; ++strokes)
      stroke_starts.push_back(core::kanji_bushu_choices(strokes, true).front().sprite_index);
  }
  std::size_t next_heading = 0;
  int cell = 0;
  for (std::size_t index = 0; index < radical_lists_.group_count(); ++index) {
    if (next_heading < stroke_starts.size() && stroke_starts[next_heading] == index) {
      auto* heading = new QLabel(QString::number(++next_heading), radical_widget);
      heading->setObjectName(QStringLiteral("radicalStrokeHeader%1").arg(next_heading));
      heading->setFixedSize(28, 28);
      heading->setAlignment(Qt::AlignCenter);
      heading->setAutoFillBackground(true);
      stroke_headings_.push_back(heading);
      QFont font = heading->font();
      font.setPixelSize(16);
      font.setBold(true);
      heading->setFont(font);
      radical_grid->addWidget(heading, cell / kRadicalColumns, cell % kRadicalColumns);
      ++cell;
    }
    auto* button = new QToolButton(radical_widget);
    button->setCheckable(true);
    button->setStyleSheet(QStringLiteral(
        "QToolButton:checked { background: palette(highlight); "
        "color: palette(highlighted-text); border: 2px solid palette(highlight); }"));
    button->setObjectName(
        QStringLiteral("radicalButton%1").arg(index + 1));
    button->setToolTip(tr("Radical group %1").arg(index + 1));
    button->setFixedSize(28, 28);
    if (has_sheet) {
      button->setIcon(QIcon(radical_sheet_.copy(
          0, static_cast<int>(index) * kRadicalSourceSize,
          kRadicalSourceSize, kRadicalSourceSize)));
      button->setIconSize(QSize(22, 22));
    } else {
      button->setText(QString::number(index + 1));
    }
    radical_grid->addWidget(button, cell / kRadicalColumns, cell % kRadicalColumns);
    ++cell;
    radical_buttons_.push_back(button);
  }
  scroll->setWidget(radical_widget);
  radical_outer->addWidget(scroll);

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
  search_button->setDefault(true);
  auto* automatic = new QCheckBox(tr("Automatic search"), this);
  automatic->setObjectName(QStringLiteral("kanjiLookupAutoSearch"));
  automatic->setChecked(true);
  controls->addWidget(automatic);
  auto* any_strokes = new QPushButton(tr("Any strokes"), this);
  any_strokes->setObjectName(QStringLiteral("kanjiLookupAnyStrokes"));
  controls->addWidget(any_strokes);
  auto* clear_button = new QPushButton(tr("&Clear"), this);
  clear_button->setObjectName(QStringLiteral("kanjiLookupClear"));
  controls->addStretch();

  results_->setObjectName(QStringLiteral("kanjiLookupResults"));
  results_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  QFont content_font = results_->font();
  content_font.setPixelSize(16);
  results_->setFont(content_font);
  assign_japanese_font(*results_, JapaneseFontRole::kList, true);
  results_->setFlow(QListView::LeftToRight);
  results_->setWrapping(false);
  results_->setMovement(QListView::Static);
  results_->setSpacing(4);
  results_->setUniformItemSizes(true);
  results_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  results_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  results_->setFixedHeight(results_->fontMetrics().height() + 12 +
                          style()->pixelMetric(QStyle::PM_ScrollBarExtent));
  outer->addWidget(results_);
  status_->setObjectName(QStringLiteral("kanjiLookupStatus"));
  outer->addWidget(status_);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  copy_button_->setObjectName(QStringLiteral("kanjiLookupCopy"));
  insert_button_->setObjectName(QStringLiteral("kanjiLookupInsert"));
  info_button_->setObjectName(QStringLiteral("kanjiLookupInfo"));
  buttons->addButton(search_button, QDialogButtonBox::ActionRole);
  buttons->addButton(clear_button, QDialogButtonBox::ActionRole);
  buttons->addButton(info_button_, QDialogButtonBox::ActionRole);
  buttons->addButton(insert_button_, QDialogButtonBox::ActionRole);
  buttons->addButton(copy_button_, QDialogButtonBox::ActionRole);
  outer->addWidget(buttons);
  outer->addLayout(controls);
  outer->addWidget(radical_group, 1);

  connect(search_button, &QPushButton::clicked, this,
          [this] { (void)search(); });
  search_timer_->setObjectName(QStringLiteral("kanjiLookupSearchTimer"));
  search_timer_->setSingleShot(true);
  search_timer_->setInterval(150);
  connect(search_timer_, &QTimer::timeout, this, [this, automatic] {
    if (isVisible() && automatic->isChecked()) (void)search();
  });
  connect(this, &QDialog::finished, search_timer_, &QTimer::stop);
  auto schedule_search = [this, automatic] {
    if (isVisible() && automatic->isChecked()) search_timer_->start();
  };
  for (auto* button : radical_buttons_)
    connect(button, &QToolButton::toggled, this, schedule_search);
  for (auto* spin : {minimum_strokes_, maximum_strokes_})
    connect(spin, &QSpinBox::valueChanged, this, schedule_search);
  connect(automatic, &QCheckBox::toggled, this, [this, schedule_search](bool checked) {
    if (checked) schedule_search(); else search_timer_->stop();
  });
  connect(any_strokes, &QPushButton::clicked, this, [this] { set_stroke_range(1, 30); });
  connect(clear_button, &QPushButton::clicked, this, [this] {
    set_selected_radicals({});
    set_stroke_range(1, 30);
    search_timer_->stop();
    results_->clear();
    status_->clear();
    update_result_actions();
  });
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
  update_artwork();
}

void KanjiLookupDialog::changeEvent(QEvent* event) {
  QDialog::changeEvent(event);
  if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange ||
      event->type() == QEvent::StyleChange) update_artwork();
}

void KanjiLookupDialog::update_artwork() {
  const bool dark = palette().color(QPalette::Window).lightness() < 128;
  for (auto* heading : stroke_headings_) {
    QPalette colors = heading->palette();
    colors.setColor(QPalette::Window, dark ? palette().color(QPalette::Window) : QColor(Qt::white));
    colors.setColor(QPalette::WindowText, dark ? QColor(255, 128, 128) : QColor(176, 0, 32));
    heading->setPalette(colors);
  }
  const bool has_sheet = radical_sheet_.width() >= 16 &&
      radical_sheet_.height() >= static_cast<int>(radical_buttons_.size()) * 16;
  for (std::size_t index = 0; index < radical_buttons_.size(); ++index) {
    auto* button = radical_buttons_[index];
    button->setPalette(palette());
    if (has_sheet) button->setIcon(themed_lookup_icon(
        radical_sheet_.copy(0, static_cast<int>(index) * 16, 16, 16), palette()));
  }
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
  search_timer_->stop();
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
    if (results_->count() != 0) results_->setCurrentRow(0);
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
