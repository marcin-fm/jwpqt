// SPDX-License-Identifier: GPL-2.0-or-later

#include "kanji_lookup_page.h"

#include <algorithm>
#include <exception>
#include <utility>

#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QEvent>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPalette>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

#include "jwpqt/core/jis_unicode.h"
#include "jwpqt/core/kanji_bushu_selector.h"
#include "lookup_artwork.h"
#include "text_bridge.h"

namespace jwpqt::qt {
namespace {

constexpr int kRadicalColumns = 28;
constexpr int kRadicalSourceSize = 16;

class RadicalStrokeSpin final : public QSpinBox {
 public:
  using QSpinBox::QSpinBox;
  std::function<std::size_t()> estimate;

 protected:
  void stepBy(int steps) override {
    setValue(core::step_kanji_strokes(value(), steps,
                                      estimate ? estimate() : 0));
  }

  StepEnabled stepEnabled() const override {
    return isReadOnly() ? StepNone : StepUpEnabled | StepDownEnabled;
  }
};

}  // namespace

KanjiLookupPage::KanjiLookupPage(
    const core::KanjiLookupLists& radical_lists,
    const core::KanjiLookupLists& stroke_lists,
    const core::KanjiInfoDatabase& information, QPixmap radical_sheet,
    KanjiLookupPageMode page_mode, QWidget* parent)
    : QWidget(parent),
      radical_lists_(radical_lists),
      stroke_lists_(stroke_lists),
      information_(information),
      page_mode_(page_mode),
      radical_sheet_(std::move(radical_sheet)),
      stroke_count_(new RadicalStrokeSpin(this)),
      tolerance_(new QComboBox(this)),
      stroke_estimate_(new QLabel(this)),
      minimum_strokes_(new QSpinBox(this)),
      maximum_strokes_(new QSpinBox(this)) {
  const bool radical_mode = page_mode_ == KanjiLookupPageMode::kRadical;
  const QString prefix = radical_mode ? QStringLiteral("kanjiRadicalLookup")
                                      : QStringLiteral("kanjiStrokeLookup");
  setObjectName(prefix + QStringLiteral("Page"));

  auto* outer = new QVBoxLayout(this);
  auto* quick_controls = new QHBoxLayout;
  stroke_count_->setObjectName(prefix + QStringLiteral("StrokeCount"));
  stroke_count_->setRange(0, 30);
  stroke_count_->setSpecialValueText(tr("Any"));
  stroke_count_->setAccessibleName(tr("Kanji stroke count"));
  stroke_count_->setToolTip(
      tr("Zero means any count. Arrows skip below the selected-radical estimate."));
  static_cast<RadicalStrokeSpin*>(stroke_count_)->estimate = [this] {
    return radical_buttons_.size() == core::kRadicalListGroups
               ? core::kanji_radical_stroke_estimate(selected_radicals())
               : 0;
  };
  tolerance_->setObjectName(prefix + QStringLiteral("Tolerance"));
  tolerance_->setAccessibleName(tr("Stroke tolerance"));
  tolerance_->addItems({tr("Exact"), tr("+/- 1"), tr("+/- 2")});
  stroke_estimate_->setObjectName(prefix + QStringLiteral("StrokeEstimate"));
  stroke_estimate_->setVisible(radical_mode);
  quick_controls->addWidget(new QLabel(tr("Stroke count"), this));
  quick_controls->addWidget(stroke_count_);
  quick_controls->addWidget(tolerance_);
  quick_controls->addWidget(stroke_estimate_, 1);
  auto* from_clipboard = new QPushButton(tr("From &Clipboard"), this);
  from_clipboard->setObjectName(prefix + QStringLiteral("FromClipboard"));
  from_clipboard->setVisible(radical_mode);
  quick_controls->addWidget(from_clipboard);
  outer->addLayout(quick_controls);

  auto* stroke_controls = new QHBoxLayout;
  minimum_strokes_->setObjectName(prefix + QStringLiteral("MinimumStrokes"));
  maximum_strokes_->setObjectName(prefix + QStringLiteral("MaximumStrokes"));
  minimum_strokes_->setRange(1, 30);
  maximum_strokes_->setRange(1, 30);
  minimum_strokes_->setValue(1);
  maximum_strokes_->setValue(30);
  stroke_controls->addWidget(new QLabel(tr("Strokes from"), this));
  stroke_controls->addWidget(minimum_strokes_);
  stroke_controls->addWidget(new QLabel(tr("to"), this));
  stroke_controls->addWidget(maximum_strokes_);
  auto* any_strokes = new QPushButton(tr("Any strokes"), this);
  any_strokes->setObjectName(prefix + QStringLiteral("AnyStrokes"));
  stroke_controls->addWidget(any_strokes);
  stroke_controls->addStretch();
  outer->addLayout(stroke_controls);

  if (radical_mode) {
    auto* radical_group = new QGroupBox(tr("Radicals"), this);
    auto* radical_outer = new QVBoxLayout(radical_group);
    auto* scroll = new QScrollArea(radical_group);
    scroll->setWidgetResizable(true);
    auto* radical_widget = new QWidget(scroll);
    auto* radical_grid = new QGridLayout(radical_widget);
    radical_grid->setSpacing(0);
    const std::size_t radical_count = radical_lists_.group_count();
    radical_buttons_.reserve(radical_count);
    const bool has_sheet = !radical_sheet_.isNull() &&
                           radical_sheet_.width() >= kRadicalSourceSize &&
                           radical_sheet_.height() >=
                               static_cast<int>(radical_count) *
                                   kRadicalSourceSize;
    std::vector<std::size_t> stroke_starts;
    if (radical_count == core::kRadicalListGroups) {
      for (std::uint8_t strokes = 1;
           strokes <= core::kMaximumBushuRadicalStrokes; ++strokes) {
        stroke_starts.push_back(
            core::kanji_bushu_choices(strokes, true).front().sprite_index);
      }
    }
    std::size_t next_heading = 0;
    int cell = 0;
    for (std::size_t index = 0; index < radical_count; ++index) {
      if (next_heading < stroke_starts.size() &&
          stroke_starts[next_heading] == index) {
        auto* heading =
            new QLabel(QString::number(++next_heading), radical_widget);
        heading->setObjectName(
            QStringLiteral("radicalStrokeHeader%1").arg(next_heading));
        heading->setFixedSize(28, 28);
        heading->setAlignment(Qt::AlignCenter);
        heading->setAutoFillBackground(true);
        QFont font = heading->font();
        font.setPixelSize(16);
        font.setBold(true);
        heading->setFont(font);
        stroke_headings_.push_back(heading);
        radical_grid->addWidget(heading, cell / kRadicalColumns,
                                cell % kRadicalColumns);
        ++cell;
      }
      auto* button = new QToolButton(radical_widget);
      button->setCheckable(true);
      button->setStyleSheet(QStringLiteral(
          "QToolButton:checked { background: palette(highlight); "
          "color: palette(highlighted-text); border: 2px solid "
          "palette(highlight); }"));
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
      radical_grid->addWidget(button, cell / kRadicalColumns,
                              cell % kRadicalColumns);
      ++cell;
      radical_buttons_.push_back(button);
    }
    scroll->setWidget(radical_widget);
    radical_outer->addWidget(scroll);
    outer->addWidget(radical_group, 1);
  } else {
    outer->addStretch();
  }

  for (std::size_t index = 0; index < radical_buttons_.size(); ++index) {
    connect(radical_buttons_[index], &QToolButton::toggled, this,
            [this, index](bool checked) {
      if (radical_buttons_.size() == core::kRadicalListGroups) {
        for (const auto variant : core::linked_kanji_radicals(index)) {
          const QSignalBlocker blocker(radical_buttons_[variant]);
          radical_buttons_[variant]->setChecked(checked);
        }
      }
      update_stroke_estimate();
      notify_change();
    });
  }
  connect(stroke_count_, &QSpinBox::valueChanged, this,
          [this] { update_quick_strokes(); });
  connect(tolerance_, &QComboBox::currentIndexChanged, this,
          [this] { update_quick_strokes(); });
  for (auto* spin : {minimum_strokes_, maximum_strokes_}) {
    connect(spin, &QSpinBox::valueChanged, this, [this] {
      const QSignalBlocker count_block(stroke_count_);
      const QSignalBlocker tolerance_block(tolerance_);
      const int minimum = minimum_strokes_->value();
      const int maximum = maximum_strokes_->value();
      stroke_count_->setSpecialValueText(
          minimum == 1 && maximum == 30 ? tr("Any") : tr("Custom range"));
      stroke_count_->setValue(minimum == maximum ? minimum : 0);
      tolerance_->setCurrentIndex(0);
      notify_change();
    });
  }
  connect(any_strokes, &QPushButton::clicked, this,
          [this] { set_stroke_range(1, 30); });
  connect(from_clipboard, &QPushButton::clicked, this, [this] {
    const QString text = QApplication::clipboard()->text();
    const auto decoded = from_qstring(text);
    const auto code = decoded.empty()
                          ? std::nullopt
                          : core::unicode_to_jis_x0208(decoded.front());
    if (!code) {
      if (error_handler_)
        error_handler_(tr("Clipboard must start with a JIS kanji"));
      return;
    }
    (void)select_kanji(*code);
  });

  update_stroke_estimate();
  update_artwork();
}

void KanjiLookupPage::set_change_handler(ChangeHandler handler) {
  change_handler_ = std::move(handler);
}

void KanjiLookupPage::set_error_handler(ErrorHandler handler) {
  error_handler_ = std::move(handler);
}

void KanjiLookupPage::notify_change() {
  if (!suppress_changes_ && change_handler_) change_handler_();
}

void KanjiLookupPage::set_deemphasize_radicals(bool enabled) {
  if (deemphasize_radicals_ == enabled) return;
  deemphasize_radicals_ = enabled;
  update_artwork();
}

void KanjiLookupPage::changeEvent(QEvent* event) {
  QWidget::changeEvent(event);
  if (event->type() == QEvent::PaletteChange ||
      event->type() == QEvent::ApplicationPaletteChange ||
      event->type() == QEvent::StyleChange) {
    update_artwork();
  }
}

void KanjiLookupPage::update_artwork() {
  const bool dark = palette().color(QPalette::Window).lightness() < 128;
  for (auto* heading : stroke_headings_) {
    QPalette colors = heading->palette();
    colors.setColor(QPalette::Window,
                    dark ? palette().color(QPalette::Window)
                         : QColor(Qt::white));
    colors.setColor(QPalette::WindowText,
                    dark ? QColor(255, 128, 128) : QColor(176, 0, 32));
    heading->setPalette(colors);
  }
  const bool has_sheet = radical_sheet_.width() >= kRadicalSourceSize &&
                         radical_sheet_.height() >=
                             static_cast<int>(radical_buttons_.size()) *
                                 kRadicalSourceSize;
  for (std::size_t index = 0; index < radical_buttons_.size(); ++index) {
    auto* button = radical_buttons_[index];
    const bool subdued =
        deemphasize_radicals_ && is_rare_radical(index + 1);
    QPalette colors = palette();
    if (subdued) colors.setColor(QPalette::ButtonText, Qt::gray);
    button->setPalette(colors);
    if (has_sheet) {
      button->setIcon(themed_lookup_icon(
          radical_sheet_.copy(0, static_cast<int>(index) *
                                     kRadicalSourceSize,
                              kRadicalSourceSize, kRadicalSourceSize),
          palette(), subdued));
    }
  }
}

void KanjiLookupPage::set_selected_radicals(
    const std::vector<std::size_t>& radicals) {
  for (const std::size_t index : radicals) {
    if (index >= radical_buttons_.size())
      throw core::KanjiLookupListError("Selected radical is out of range");
  }
  std::vector<bool> selected(radical_buttons_.size());
  for (const std::size_t index : radicals) {
    if (radical_buttons_.size() == core::kRadicalListGroups) {
      for (const auto variant : core::linked_kanji_radicals(index))
        selected[variant] = true;
    } else {
      selected[index] = true;
    }
  }
  for (std::size_t index = 0; index < radical_buttons_.size(); ++index) {
    const QSignalBlocker blocker(radical_buttons_[index]);
    radical_buttons_[index]->setChecked(selected[index]);
  }
  update_stroke_estimate();
  notify_change();
}

std::vector<std::size_t> KanjiLookupPage::selected_radicals() const {
  std::vector<std::size_t> selected;
  for (std::size_t index = 0; index < radical_buttons_.size(); ++index) {
    if (radical_buttons_[index]->isChecked()) selected.push_back(index);
  }
  return selected;
}

void KanjiLookupPage::set_stroke_range(std::uint8_t minimum,
                                       std::uint8_t maximum) {
  if (minimum < 1 || maximum > 30 || minimum > maximum)
    throw core::KanjiLookupListError("Kanji stroke range is invalid");
  const QSignalBlocker minimum_block(minimum_strokes_);
  const QSignalBlocker maximum_block(maximum_strokes_);
  const QSignalBlocker count_block(stroke_count_);
  const QSignalBlocker tolerance_block(tolerance_);
  minimum_strokes_->setValue(minimum);
  maximum_strokes_->setValue(maximum);
  stroke_count_->setSpecialValueText(
      minimum == 1 && maximum == 30 ? tr("Any") : tr("Custom range"));
  stroke_count_->setValue(minimum == maximum ? minimum : 0);
  tolerance_->setCurrentIndex(0);
  notify_change();
}

bool KanjiLookupPage::select_kanji(core::JisCode code) {
  try {
    const auto selected =
        core::kanji_radicals_for_character(radical_lists_, code);
    if (selected.empty())
      throw core::KanjiLookupListError("No radical data for this kanji");
    suppress_changes_ = true;
    set_selected_radicals(selected);
    set_stroke_range(1, 30);
    suppress_changes_ = false;
    notify_change();
    return true;
  } catch (const std::exception& error) {
    suppress_changes_ = false;
    if (error_handler_) error_handler_(QString::fromUtf8(error.what()));
    return false;
  }
}

void KanjiLookupPage::clear() {
  suppress_changes_ = true;
  set_selected_radicals({});
  set_stroke_range(1, 30);
  suppress_changes_ = false;
  notify_change();
}

void KanjiLookupPage::update_stroke_estimate() {
  if (page_mode_ != KanjiLookupPageMode::kRadical) return;
  if (radical_buttons_.size() != core::kRadicalListGroups) {
    stroke_estimate_->setText(
        tr("Stroke estimate unavailable for this catalog"));
    return;
  }
  stroke_estimate_->setText(
      tr("Selected-radical estimate: %1 (spinner hint only)")
          .arg(core::kanji_radical_stroke_estimate(selected_radicals())));
}

void KanjiLookupPage::update_quick_strokes() {
  const int count = stroke_count_->value();
  const int tolerance = tolerance_->currentIndex();
  const QSignalBlocker minimum_block(minimum_strokes_);
  const QSignalBlocker maximum_block(maximum_strokes_);
  stroke_count_->setSpecialValueText(tr("Any"));
  minimum_strokes_->setValue(count == 0 ? 1 : std::max(1, count - tolerance));
  maximum_strokes_->setValue(count == 0 ? 30 : std::min(30, count + tolerance));
  notify_change();
}

core::KanjiLookupReport KanjiLookupPage::search(bool rare_last) const {
  core::KanjiLookupOptions options;
  options.radicals = selected_radicals();
  options.minimum_strokes =
      static_cast<std::uint8_t>(minimum_strokes_->value());
  options.maximum_strokes =
      static_cast<std::uint8_t>(maximum_strokes_->value());
  options.rare_last = rare_last;
  return core::search_kanji_radicals(radical_lists_, stroke_lists_,
                                    &information_, options);
}

}  // namespace jwpqt::qt
