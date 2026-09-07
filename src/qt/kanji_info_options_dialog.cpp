// SPDX-License-Identifier: GPL-2.0-or-later

#include "kanji_info_options_dialog.h"

#include <algorithm>
#include <array>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "kanji_info_dialog.h"

namespace jwpqt::qt {

KanjiInfoOptionsDialog::KanjiInfoOptionsDialog(const KanjiInfoOptions& options, QWidget* parent)
    : QDialog(parent), options_(options) {
  validate_kanji_info_options(options);
  setObjectName(QStringLiteral("kanjiInfoOptionsDialog"));
  setWindowTitle(tr("Character Info Setup"));
  resize(760, 570);
  auto* layout = new QVBoxLayout(this);
  auto* explanation = new QLabel(tr("Choose the field order or Blank for a spacer. "
      "All rows appear in the main panel; rows 14-26 also appear in More Info. "
      "Bushu and cross-references remain available independently."), this);
  explanation->setWordWrap(true);
  layout->addWidget(explanation);
  auto* columns = new QHBoxLayout;
  std::array<QComboBox*, kKanjiInfoFieldCount> fields{};
  for (std::size_t column = 0; column < 2; ++column) {
    auto* group = new QGroupBox(column == 0 ? tr("Rows 1-13") : tr("Rows 14-26 / More Info"), this);
    auto* form = new QFormLayout(group);
    for (std::size_t row = 0; row < 13; ++row) {
      const auto slot = column * 13 + row;
      auto* combo = new QComboBox(group);
      combo->setObjectName(QStringLiteral("kanjiInfoField%1").arg(slot));
      combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
      combo->setMinimumContentsLength(18);
      for (std::uint8_t field = 0; field <= kKanjiInfoFieldCount; ++field)
        combo->addItem(KanjiInfoDialog::field_name(field));
      combo->setCurrentIndex(options_.fields[slot]);
      fields[slot] = combo;
      form->addRow(QString::number(slot + 1), combo);
    }
    columns->addWidget(group);
  }
  layout->addLayout(columns, 1);
  const auto refresh = [this, fields] {
    for (std::size_t slot = 0; slot < fields.size(); ++slot) {
      const QSignalBlocker block(fields[slot]);
      fields[slot]->setCurrentIndex(options_.fields[slot]);
    }
  };
  for (std::size_t slot = 0; slot < fields.size(); ++slot)
    connect(fields[slot], &QComboBox::currentIndexChanged, this, [this, slot, refresh](int field) {
      if (field < 0) return;
      select_kanji_info_field(options_, slot, static_cast<std::uint8_t>(field));
      refresh();
    });
  auto* compact = new QCheckBox(tr("Compact readings"), this);
  compact->setObjectName(QStringLiteral("kanjiInfoCompact"));
  compact->setChecked(options_.compact);
  auto* headings = new QCheckBox(tr("Show reading headings"), this);
  headings->setObjectName(QStringLiteral("kanjiInfoHeadings"));
  headings->setChecked(options_.headings);
  layout->addWidget(compact);
  layout->addWidget(headings);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel |
                                     QDialogButtonBox::RestoreDefaults, this);
  buttons->button(QDialogButtonBox::RestoreDefaults)->setObjectName(QStringLiteral("kanjiInfoDefaults"));
  connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked,
      this, [this, compact, headings, refresh] {
        const KanjiInfoOptions defaults;
        std::copy_n(defaults.fields.begin(), kKanjiInfoFieldCount, options_.fields.begin());
        compact->setChecked(defaults.compact);
        headings->setChecked(defaults.headings);
        refresh();
      });
  connect(buttons, &QDialogButtonBox::accepted, this, [this, compact, headings] {
    options_.compact = compact->isChecked();
    options_.headings = headings->isChecked();
    accept();
  });
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  layout->addWidget(buttons);
}

}  // namespace jwpqt::qt
