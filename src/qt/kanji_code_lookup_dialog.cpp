// SPDX-License-Identifier: GPL-2.0-or-later

#include "kanji_code_lookup_dialog.h"

#include <exception>
#include <utility>

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include "jwpqt/core/jwp_text_codec.h"
#include "jwpqt/core/kanji_bushu_selector.h"
#include "jwpqt/core/kanji_spahn_selector.h"
#include "lookup_artwork.h"
#include "text_bridge.h"
#include "japanese_fonts.h"

namespace jwpqt::qt {
namespace {

constexpr int kRadicalSourceSize = 16;

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
    InfoHandler info_handler, QWidget* parent, QPixmap radical_sheet)
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
      bushu_radicals_(new QListWidget(this)),
      spahn_radical_strokes_(wildcard_spin(
          11, QStringLiteral("spahnRadicalStrokes"), this)),
      spahn_radical_(wildcard_spin(19, QStringLiteral("spahnRadical"), this)),
      spahn_other_strokes_(wildcard_spin(
          26, QStringLiteral("spahnOtherStrokes"), this)),
      spahn_index_(wildcard_spin(47, QStringLiteral("spahnIndex"), this)),
      spahn_variants_(new QCheckBox(tr("Show &variants"), this)),
      spahn_radicals_(new QListWidget(this)),
      stroke_bushu_radical_strokes_(new QSpinBox(this)),
      stroke_bushu_variants_(new QCheckBox(tr("Include &variants"), this)),
      stroke_bushu_radicals_(new QListWidget(this)),
      stroke_bushu_minimum_strokes_(new QSpinBox(this)),
      stroke_bushu_maximum_strokes_(new QSpinBox(this)),
      stroke_bushu_nelson_(new QCheckBox(tr("&Nelson radical"), this)),
      stroke_bushu_classical_(new QCheckBox(tr("&Classical radical"), this)),
      index_type_(new QComboBox(this)),
      index_value_(new QSpinBox(this)),
      index_volume_(new QSpinBox(this)),
      search_timer_(new QTimer(this)),
      radical_sheet_(std::move(radical_sheet)),
      results_(new QListWidget(this)),
      status_(new QLabel(this)),
      copy_button_(new QPushButton(tr("&Copy"), this)),
      insert_button_(new QPushButton(tr("&Insert"), this)),
      info_button_(new QPushButton(tr("&Information"), this)) {
  setObjectName(QStringLiteral("kanjiCodeLookupDialog"));
  setWindowTitle(tr("Kanji Code Lookup"));
  setModal(false);
  resize(880, 520);

  auto* outer = new QVBoxLayout(this);
  auto* skip_page = new QWidget(tabs_);
  auto* skip_layout = new QFormLayout(skip_page);
  skip_layout->addRow(tr("Type"), skip_type_);
  skip_layout->addRow(tr("First value"), skip_first_);
  skip_layout->addRow(tr("Second value"), skip_second_);
  skip_misclassifications_->setObjectName(
      QStringLiteral("skipMisclassifications"));
  skip_layout->addRow(skip_misclassifications_);
  auto* skip_legend = new QLabel(skip_page);
  skip_legend->setObjectName(QStringLiteral("skipLegend"));
  skip_legend->setPixmap(QPixmap(QStringLiteral(":/jwpqt/skiptype.bmp"))
                            .scaled(236, 96, Qt::KeepAspectRatio));
  skip_layout->addRow(skip_legend);
  tabs_->addTab(skip_page, tr("SKIP"));

  auto* corner_page = new QWidget(tabs_);
  auto* corner_layout = new QFormLayout(corner_page);
  const QStringList corner_names{tr("Upper left"), tr("Upper right"),
                                tr("Lower left"), tr("Lower right"), tr("Fifth digit")};
  for (std::size_t index = 0; index < 5; ++index) {
    QSpinBox* spin = wildcard_spin(
        9, QStringLiteral("fourCorner%1").arg(index + 1), corner_page);
    corner_digits_.push_back(spin);
    corner_layout->addRow(corner_names[static_cast<qsizetype>(index)], spin);
  }
  auto* corner_legend = new QLabel(corner_page);
  corner_legend->setObjectName(QStringLiteral("fourCornerLegend"));
  corner_legend->setPixmap(QPixmap(QStringLiteral(":/jwpqt/fourcorners.bmp"))
                              .scaled(320, 120, Qt::KeepAspectRatio));
  corner_layout->addRow(corner_legend);
  tabs_->addTab(corner_page, tr("Four corner"));

  auto* bushu_page = new QWidget(tabs_);
  auto* bushu_outer = new QVBoxLayout(bushu_page);
  bushu_radicals_->setObjectName(QStringLiteral("bushuRadicals"));
  bushu_radicals_->setViewMode(QListView::IconMode);
  bushu_radicals_->setResizeMode(QListView::Adjust);
  bushu_radicals_->setMovement(QListView::Static);
  const bool has_bushu_sheet = radical_sheet_.width() >= 16 && radical_sheet_.height() >= 241 * 16;
  bushu_radicals_->setIconSize(QSize(16, 16));
  bushu_radicals_->setGridSize(has_bushu_sheet ? QSize(26, 26) : QSize(36, 32));
  bushu_radicals_->setMinimumHeight(has_bushu_sheet ? 9 * 26 + 2 * bushu_radicals_->frameWidth() : 170);
  bushu_radicals_->setStyleSheet(QStringLiteral(
      "QListWidget::item:selected { background: palette(highlight); color: palette(highlighted-text); }"));
  for (std::uint8_t strokes = 1; strokes <= core::kMaximumBushuRadicalStrokes; ++strokes) {
    auto* heading = new QListWidgetItem(QString::number(strokes), bushu_radicals_);
    heading->setFlags(Qt::ItemIsEnabled);
    heading->setBackground(Qt::white);
    heading->setForeground(QColor(176, 0, 32));
    QFont heading_font = heading->font();
    heading_font.setBold(true);
    heading_font.setPixelSize(16);
    heading->setFont(heading_font);
    for (const auto& choice : core::kanji_bushu_choices(strokes, true)) {
      const QPixmap icon = has_bushu_sheet
          ? radical_sheet_.copy(0, choice.sprite_index * 16, 16, 16) : QPixmap{};
      auto* item = new QListWidgetItem(QIcon(icon), icon.isNull()
          ? QString::number(choice.bushu) : QString(), bushu_radicals_);
      item->setData(Qt::UserRole, choice.bushu);
      item->setData(Qt::UserRole + 1, choice.sprite_index);
      item->setToolTip(tr("Bushu %1, %2 radical strokes").arg(choice.bushu).arg(strokes));
    }
  }
  bushu_outer->addWidget(bushu_radicals_, 1);
  auto* bushu_fields = new QHBoxLayout;
  bushu_fields->addWidget(new QLabel(tr("Radical number"), bushu_page));
  bushu_fields->addWidget(bushu_radical_);
  bushu_fields->addWidget(new QLabel(tr("Stroke count"), bushu_page));
  bushu_fields->addWidget(bushu_strokes_);
  bushu_fields->addStretch();
  bushu_outer->addLayout(bushu_fields);
  bushu_nelson_->setObjectName(QStringLiteral("bushuNelson"));
  bushu_classical_->setObjectName(QStringLiteral("bushuClassical"));
  bushu_nelson_->setChecked(true);
  bushu_classical_->setChecked(true);
  auto* bushu_sources = new QHBoxLayout;
  bushu_sources->addWidget(bushu_nelson_);
  bushu_sources->addWidget(bushu_classical_);
  bushu_sources->addStretch();
  bushu_outer->addLayout(bushu_sources);
  tabs_->addTab(bushu_page, tr("Bushu"));

  auto* spahn_page = new QWidget(tabs_);
  auto* spahn_layout = new QFormLayout(spahn_page);
  spahn_radicals_->setObjectName(QStringLiteral("spahnRadicals"));
  spahn_radicals_->setFlow(QListView::LeftToRight);
  spahn_radicals_->setWrapping(false);
  spahn_radicals_->setMovement(QListView::Static);
  spahn_radicals_->setIconSize(QSize(22, 22));
  spahn_radicals_->setFixedHeight(52);
  spahn_radicals_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  spahn_radicals_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  spahn_layout->addRow(spahn_radicals_);
  spahn_variants_->setObjectName(QStringLiteral("spahnVariants"));
  spahn_variants_->setChecked(true);
  spahn_layout->addRow(spahn_variants_);
  spahn_layout->addRow(tr("Radical strokes"), spahn_radical_strokes_);
  spahn_layout->addRow(tr("Radical"), spahn_radical_);
  spahn_radical_->setToolTip(tr("Radical letter code: a=0, b=1, ..., k=10, m=12, ..., t=19."));
  spahn_layout->addRow(tr("Other strokes"), spahn_other_strokes_);
  spahn_layout->addRow(tr("Kanji index"), spahn_index_);
  tabs_->addTab(spahn_page, tr("Spahn-Hadamitzky"));

  auto* stroke_bushu_page = new QWidget(tabs_);
  auto* stroke_bushu_layout = new QVBoxLayout(stroke_bushu_page);
  auto* stroke_bushu_controls = new QFormLayout;
  stroke_bushu_radical_strokes_->setObjectName(
      QStringLiteral("strokeBushuRadicalStrokes"));
  stroke_bushu_radical_strokes_->setRange(
      0, core::kMaximumBushuRadicalStrokes);
  stroke_bushu_variants_->setObjectName(
      QStringLiteral("strokeBushuVariants"));
  stroke_bushu_variants_->setChecked(true);
  stroke_bushu_controls->addRow(tr("Radical strokes"),
                                stroke_bushu_radical_strokes_);
  stroke_bushu_controls->addRow(stroke_bushu_variants_);
  stroke_bushu_layout->addLayout(stroke_bushu_controls);
  stroke_bushu_radicals_->setObjectName(
      QStringLiteral("strokeBushuRadicals"));
  stroke_bushu_radicals_->setSelectionMode(QAbstractItemView::SingleSelection);
  stroke_bushu_radicals_->setViewMode(QListView::IconMode);
  stroke_bushu_radicals_->setResizeMode(QListView::Adjust);
  stroke_bushu_radicals_->setMovement(QListView::Static);
  stroke_bushu_radicals_->setIconSize(QSize(22, 22));
  stroke_bushu_radicals_->setGridSize(QSize(54, 46));
  stroke_bushu_layout->addWidget(stroke_bushu_radicals_, 1);
  auto* stroke_range = new QHBoxLayout;
  stroke_bushu_minimum_strokes_->setObjectName(
      QStringLiteral("strokeBushuMinimumStrokes"));
  stroke_bushu_maximum_strokes_->setObjectName(
      QStringLiteral("strokeBushuMaximumStrokes"));
  stroke_bushu_minimum_strokes_->setRange(0, 30);
  stroke_bushu_maximum_strokes_->setRange(0, 30);
  stroke_bushu_maximum_strokes_->setValue(30);
  stroke_range->addWidget(new QLabel(tr("Kanji strokes from"), this));
  stroke_range->addWidget(stroke_bushu_minimum_strokes_);
  stroke_range->addWidget(new QLabel(tr("to"), this));
  stroke_range->addWidget(stroke_bushu_maximum_strokes_);
  stroke_range->addStretch();
  stroke_bushu_layout->addLayout(stroke_range);
  stroke_bushu_nelson_->setObjectName(QStringLiteral("strokeBushuNelson"));
  stroke_bushu_classical_->setObjectName(
      QStringLiteral("strokeBushuClassical"));
  stroke_bushu_nelson_->setChecked(true);
  stroke_bushu_classical_->setChecked(true);
  auto* stroke_systems = new QHBoxLayout;
  stroke_systems->addWidget(stroke_bushu_nelson_);
  stroke_systems->addWidget(stroke_bushu_classical_);
  stroke_systems->addStretch();
  stroke_bushu_layout->addLayout(stroke_systems);
  tabs_->addTab(stroke_bushu_page, tr("Stroke/Bushu"));

  auto* index_page = new QWidget(tabs_);
  auto* index_layout = new QFormLayout(index_page);
  const QStringList index_names{
      tr("Modern Reader's Japanese-English Character Dictionary, Andrew Nelson"),
      tr("New Nelson Japanese-English Character Dictionary, John Haig"),
      tr("New Japanese-English Character Dictionary, Jack Halpern"),
      tr("School grade"), tr("Morohashi (full index)"), tr("Morohashi (volume/index)"),
      tr("Halpern Kanji Learners' Dictionary"), tr("Spahn-Hadamitzky Kanji & Kana"),
      tr("Henshall"), tr("Gakken"), tr("Heisig"), tr("O'Neill Names"),
      tr("O'Neill Essential Kanji"), tr("De Roo"), tr("Frequency"),
      tr("Read/Write Japanese"), tr("Tuttle Kanji Cards"), tr("The Kanji Way"),
      tr("Kanji in Context"), tr("Japanese for Busy People"), tr("Compact Kanji Guide")};
  const int index_count = (information_.flags() & 0x0008U) == 0 ? 4
      : (information_.flags() & 0x0010U) == 0 ? 6 : static_cast<int>(index_names.size());
  index_type_->setObjectName(QStringLiteral("kanjiIndexType"));
  index_type_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  index_type_->setMinimumContentsLength(30);
  for (int index = 0; index < index_count; ++index) {
    index_type_->addItem(index_names[index], index);
    index_type_->setItemData(index, index_names[index], Qt::ToolTipRole);
  }
  index_type_->setToolTip(index_type_->currentText());
  index_value_->setObjectName(QStringLiteral("kanjiIndexValue"));
  index_value_->setRange(0, 65535);
  index_value_->setToolTip(tr("Exact index number; zero matches an unrecorded value."));
  index_volume_->setObjectName(QStringLiteral("kanjiIndexVolume"));
  index_volume_->setRange(0, 255);
  index_volume_->setEnabled(false);
  index_layout->addRow(tr("Type of index"), index_type_);
  index_layout->addRow(tr("Index"), index_value_);
  index_layout->addRow(tr("Volume"), index_volume_);
  connect(index_type_, &QComboBox::currentIndexChanged, this, [this] {
    const auto type = static_cast<core::KanjiIndexType>(index_type_->currentData().toInt());
    index_volume_->setEnabled(type == core::KanjiIndexType::kMorohashiVolume ||
                              type == core::KanjiIndexType::kBusyPeople);
    index_type_->setToolTip(index_type_->currentText());
  });
  tabs_->addTab(index_page, tr("Index"));

  auto* search_button = new QPushButton(tr("&Search"), this);
  search_button->setObjectName(QStringLiteral("kanjiCodeSearch"));
  search_button->setDefault(true);
  auto* clear_button = new QPushButton(tr("&Clear"), this);
  clear_button->setObjectName(QStringLiteral("kanjiCodeClear"));
  results_->setObjectName(QStringLiteral("kanjiCodeResults"));
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
  status_->setObjectName(QStringLiteral("kanjiCodeStatus"));
  outer->addWidget(status_);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  copy_button_->setObjectName(QStringLiteral("kanjiCodeCopy"));
  insert_button_->setObjectName(QStringLiteral("kanjiCodeInsert"));
  info_button_->setObjectName(QStringLiteral("kanjiCodeInfo"));
  buttons->addButton(search_button, QDialogButtonBox::ActionRole);
  buttons->addButton(clear_button, QDialogButtonBox::ActionRole);
  buttons->addButton(info_button_, QDialogButtonBox::ActionRole);
  buttons->addButton(insert_button_, QDialogButtonBox::ActionRole);
  buttons->addButton(copy_button_, QDialogButtonBox::ActionRole);
  outer->addWidget(buttons);
  auto* automatic = new QCheckBox(tr("Automatic search"), this);
  automatic->setObjectName(QStringLiteral("kanjiCodeAutoSearch"));
  automatic->setChecked(true);
  outer->addWidget(automatic);
  outer->addWidget(tabs_, 1);

  connect(search_button, &QPushButton::clicked, this, [this] { search_current(); });
  connect(clear_button, &QPushButton::clicked, this, [this] { clear_current(); });
  search_timer_->setObjectName(QStringLiteral("kanjiCodeSearchTimer"));
  search_timer_->setSingleShot(true);
  search_timer_->setInterval(150);
  connect(search_timer_, &QTimer::timeout, this, [this, automatic] {
    if (isVisible() && automatic->isChecked() && tabs_->currentIndex() != 5)
      search_current();
  });
  connect(this, &QDialog::finished, search_timer_, &QTimer::stop);
  auto schedule_search = [this, automatic] {
    if (isVisible() && automatic->isChecked() && tabs_->currentIndex() != 5)
      search_timer_->start();
  };
  for (auto* spin : findChildren<QSpinBox*>())
    connect(spin, &QSpinBox::valueChanged, this, schedule_search);
  for (auto* check : findChildren<QCheckBox*>()) {
    if (check != automatic) connect(check, &QCheckBox::toggled, this, schedule_search);
  }
  connect(automatic, &QCheckBox::toggled, this, [this, schedule_search](bool checked) {
    if (checked) schedule_search(); else search_timer_->stop();
  });
  connect(tabs_, &QTabWidget::currentChanged, this, [this, automatic](int index) {
    search_timer_->stop();
    automatic->setVisible(index != 5);
  });
  connect(bushu_radicals_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
    if (item->data(Qt::UserRole).isValid()) bushu_radical_->setValue(item->data(Qt::UserRole).toInt());
  });
  connect(bushu_radical_, &QSpinBox::valueChanged, this, [this](int value) {
    const QSignalBlocker blocker(bushu_radicals_);
    bushu_radicals_->setCurrentRow(-1);
    for (int row = 0; row < bushu_radicals_->count(); ++row) {
      const auto* item = bushu_radicals_->item(row);
      if (item->data(Qt::UserRole).isValid() && item->data(Qt::UserRole).toInt() == value) {
        bushu_radicals_->setCurrentRow(row);
        break;
      }
    }
  });
  connect(spahn_radical_strokes_, &QSpinBox::valueChanged, this,
          [this] { populate_spahn_choices(); });
  connect(spahn_radical_, &QSpinBox::valueChanged, this,
          [this] { populate_spahn_choices(); });
  connect(spahn_variants_, &QCheckBox::toggled, this,
          [this] { populate_spahn_choices(); });
  connect(spahn_radicals_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
    // Changing the stroke filter rebuilds the list, so copy both values first.
    const int strokes = item->data(Qt::UserRole + 1).toInt();
    const int radical = item->data(Qt::UserRole).toInt();
    spahn_radical_strokes_->setValue(strokes);
    spahn_radical_->setValue(radical);
  });
  connect(stroke_bushu_radicals_, &QListWidget::currentRowChanged, this, schedule_search);
  connect(bushu_nelson_, &QCheckBox::toggled, this, [this](bool checked) {
    if (!checked && !bushu_classical_->isChecked())
      bushu_classical_->setChecked(true);
  });
  connect(bushu_classical_, &QCheckBox::toggled, this, [this](bool checked) {
    if (!checked && !bushu_nelson_->isChecked())
      bushu_nelson_->setChecked(true);
  });
  connect(stroke_bushu_radical_strokes_, &QSpinBox::valueChanged, this,
          [this] { populate_stroke_bushu_choices(); });
  connect(stroke_bushu_variants_, &QCheckBox::toggled, this,
          [this] { populate_stroke_bushu_choices(); });
  connect(stroke_bushu_nelson_, &QCheckBox::toggled, this,
          [this](bool checked) {
            if (!checked && !stroke_bushu_classical_->isChecked())
              stroke_bushu_classical_->setChecked(true);
          });
  connect(stroke_bushu_classical_, &QCheckBox::toggled, this,
          [this](bool checked) {
            if (!checked && !stroke_bushu_nelson_->isChecked())
              stroke_bushu_nelson_->setChecked(true);
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
  populate_stroke_bushu_choices();
  populate_spahn_choices();
  update_actions();
  update_artwork();
}

void KanjiCodeLookupDialog::changeEvent(QEvent* event) {
  QDialog::changeEvent(event);
  if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange ||
      event->type() == QEvent::StyleChange) update_artwork();
}

void KanjiCodeLookupDialog::update_artwork() {
  const bool dark = palette().color(QPalette::Window).lightness() < 128;
  for (auto* list : {bushu_radicals_, stroke_bushu_radicals_, spahn_radicals_})
    list->setPalette(palette());
  const QSignalBlocker blocker(bushu_radicals_);
  for (int row = 0; row < bushu_radicals_->count(); ++row) {
    auto* item = bushu_radicals_->item(row);
    if (item->data(Qt::UserRole).isValid()) continue;
    item->setBackground(dark ? palette().color(QPalette::Window) : QColor(Qt::white));
    item->setForeground(dark ? QColor(255, 128, 128) : QColor(176, 0, 32));
  }
  auto refresh_icons = [this](QListWidget* list, const QPixmap& sheet, int sprite_role) {
    if (sheet.isNull()) return;
    const QSignalBlocker signal_blocker(list);
    for (int row = 0; row < list->count(); ++row) {
      auto* item = list->item(row);
      const QVariant sprite = item->data(sprite_role);
      if (sprite.isValid())
        item->setIcon(themed_lookup_icon(sheet.copy(0, sprite.toInt() * 16, 16, 16), palette()));
    }
  };
  if (radical_sheet_.width() >= 16 && radical_sheet_.height() >= 241 * 16) {
    refresh_icons(bushu_radicals_, radical_sheet_, Qt::UserRole + 1);
    refresh_icons(stroke_bushu_radicals_, radical_sheet_, Qt::UserRole + 1);
  }
  refresh_icons(spahn_radicals_, QPixmap(QStringLiteral(":/jwpqt/hsradicals.bmp")), Qt::UserRole + 2);
  if (auto* legend = findChild<QLabel*>(QStringLiteral("skipLegend")))
    legend->setPixmap(themed_lookup_artwork(QPixmap(QStringLiteral(":/jwpqt/skiptype.bmp")), palette())
                          .scaled(236, 96, Qt::KeepAspectRatio));
  if (auto* legend = findChild<QLabel*>(QStringLiteral("fourCornerLegend")))
    legend->setPixmap(themed_lookup_artwork(QPixmap(QStringLiteral(":/jwpqt/fourcorners.bmp")), palette())
                          .scaled(320, 120, Qt::KeepAspectRatio));
}

void KanjiCodeLookupDialog::search_current() {
  search_timer_->stop();
  switch (tabs_->currentIndex()) {
    case 0: (void)search_skip(); break;
    case 1: (void)search_four_corner(); break;
    case 2: (void)search_bushu(); break;
    case 3: (void)search_spahn(); break;
    case 4: (void)search_stroke_bushu(); break;
    case 5: (void)search_index(); break;
  }
}

void KanjiCodeLookupDialog::clear_current() {
  switch (tabs_->currentIndex()) {
    case 0:
      for (auto* spin : {skip_type_, skip_first_, skip_second_}) spin->setValue(-1);
      break;
    case 1:
      for (auto* spin : corner_digits_) spin->setValue(-1);
      break;
    case 2:
      bushu_radical_->setValue(-1);
      bushu_strokes_->setValue(-1);
      break;
    case 3:
      for (auto* spin : {spahn_radical_strokes_, spahn_radical_, spahn_other_strokes_, spahn_index_})
        spin->setValue(-1);
      break;
    case 4:
      stroke_bushu_radical_strokes_->setValue(0);
      stroke_bushu_radicals_->setCurrentRow(0);
      stroke_bushu_minimum_strokes_->setValue(0);
      stroke_bushu_maximum_strokes_->setValue(30);
      break;
    case 5:
      index_value_->setValue(0);
      index_volume_->setValue(0);
      index_value_->setFocus();
      break;
  }
  search_timer_->stop();
  results_->clear();
  status_->clear();
  update_actions();
}

void KanjiCodeLookupDialog::populate_spahn_choices() {
  const QSignalBlocker blocker(spahn_radicals_);
  spahn_radicals_->clear();
  const int strokes = spahn_radical_strokes_->value();
  if (strokes == 0) return;
  const auto choices = core::kanji_spahn_choices(
      static_cast<std::uint8_t>(strokes < 0 ? 0 : strokes), spahn_variants_->isChecked());
  const QPixmap sheet(QStringLiteral(":/jwpqt/hsradicals.bmp"));
  for (const auto& choice : choices) {
    const QPixmap icon = sheet.copy(0, choice.sprite_index * 16, 16, 16);
    const QString code = QStringLiteral("%1%2").arg(choice.radical_strokes)
        .arg(QChar(static_cast<char>('a' + choice.radical)));
    auto* item = new QListWidgetItem(themed_lookup_icon(icon, palette()),
                                    icon.isNull() ? code : QString(), spahn_radicals_);
    item->setToolTip(code);
    item->setData(Qt::UserRole, choice.radical);
    item->setData(Qt::UserRole + 1, choice.radical_strokes);
    item->setData(Qt::UserRole + 2, choice.sprite_index);
    if (spahn_radicals_->currentRow() < 0 && choice.radical == spahn_radical_->value())
      spahn_radicals_->setCurrentItem(item);
  }
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

void KanjiCodeLookupDialog::select_stroke_bushu_mode() {
  tabs_->setCurrentIndex(4);
}

void KanjiCodeLookupDialog::select_index_mode() { tabs_->setCurrentIndex(5); }

void KanjiCodeLookupDialog::set_index_query(const core::KanjiIndexQuery& query) {
  const int type = static_cast<int>(query.type);
  if (type < 0 || type >= index_type_->count() || query.index > 65535 || query.volume > 255)
    throw core::KanjiInfoError("Native kanji index query is unavailable or out of range");
  index_type_->setCurrentIndex(type);
  index_value_->setValue(static_cast<int>(query.index));
  index_volume_->setValue(static_cast<int>(query.volume));
  select_index_mode();
}

bool KanjiCodeLookupDialog::search_index() {
  const core::KanjiIndexQuery query{
      static_cast<core::KanjiIndexType>(index_type_->currentData().toInt()),
      static_cast<std::uint32_t>(index_value_->value()),
      static_cast<std::uint32_t>(index_volume_->value())};
  try {
    return publish(core::search_kanji_index(information_, query));
  } catch (const std::exception& error) {
    status_->setText(QString::fromUtf8(error.what()));
  }
  return false;
}

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

void KanjiCodeLookupDialog::populate_stroke_bushu_choices() {
  const auto choices = core::kanji_bushu_choices(
      static_cast<std::uint8_t>(stroke_bushu_radical_strokes_->value()),
      stroke_bushu_variants_->isChecked());
  stroke_bushu_radicals_->clear();
  auto* any = new QListWidgetItem(tr("Any"), stroke_bushu_radicals_);
  any->setData(Qt::UserRole, 0U);
  any->setSelected(true);
  const bool has_sheet = !radical_sheet_.isNull() &&
                         radical_sheet_.width() >= kRadicalSourceSize &&
                         radical_sheet_.height() >= 241 * kRadicalSourceSize;
  for (const core::KanjiBushuChoice& choice : choices) {
    auto* item = new QListWidgetItem(
        has_sheet
            ? themed_lookup_icon(radical_sheet_.copy(
                  0, static_cast<int>(choice.sprite_index) * kRadicalSourceSize,
                  kRadicalSourceSize, kRadicalSourceSize), palette())
            : QIcon(),
        QString::number(choice.bushu), stroke_bushu_radicals_);
    item->setToolTip(tr("Bushu %1").arg(choice.bushu));
    item->setData(Qt::UserRole, choice.bushu);
    item->setData(Qt::UserRole + 1, choice.sprite_index);
  }
  stroke_bushu_radicals_->setCurrentRow(0);
}

bool KanjiCodeLookupDialog::search_stroke_bushu() {
  core::KanjiBushuQuery query;
  const QListWidgetItem* selected = stroke_bushu_radicals_->currentItem();
  const std::uint8_t bushu = selected == nullptr
                                 ? 0
                                 : static_cast<std::uint8_t>(
                                       selected->data(Qt::UserRole).toUInt());
  query.radical = bushu == 0 ? core::KanjiNumericRange{0, 255}
                             : core::KanjiNumericRange{bushu, bushu};
  query.strokes = {
      static_cast<std::uint8_t>(stroke_bushu_minimum_strokes_->value()),
      static_cast<std::uint8_t>(stroke_bushu_maximum_strokes_->value())};
  query.nelson = stroke_bushu_nelson_->isChecked();
  query.classical = stroke_bushu_classical_->isChecked();
  try {
    return publish(core::search_kanji_bushu(information_, query));
  } catch (const std::exception& error) {
    status_->setText(QString::fromUtf8(error.what()));
  } catch (...) {
    status_->setText(tr("Stroke/Bushu lookup failed"));
  }
  return false;
}

bool KanjiCodeLookupDialog::publish(core::KanjiCodeSearchReport report) {
  search_timer_->stop();
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
  if (results_->count() != 0) results_->setCurrentRow(0);
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
