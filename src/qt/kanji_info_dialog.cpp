// SPDX-License-Identifier: GPL-2.0-or-later

#include "kanji_info_dialog.h"

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

#include <QDialogButtonBox>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include "jwpqt/core/jis_encoding.h"
#include "jwpqt/core/jwp_text_codec.h"
#include "text_bridge.h"

namespace jwpqt::qt {
namespace {

QString hex_value(std::uint32_t value, int width = 4) {
  return QStringLiteral("%1").arg(value, width, 16, QLatin1Char('0')).toUpper();
}

QString decimal(std::uint32_t value) {
  return QString::number(static_cast<qulonglong>(value));
}

QString skip_code(const core::KanjiInfoSkipCode& skip) {
  if (skip.type == 0) return {};
  return QStringLiteral("%1-%2-%3")
      .arg(skip.type)
      .arg(skip.first)
      .arg(skip.second);
}

QString four_corner(std::uint16_t value, std::uint8_t index) {
  return QStringLiteral("%1.%2")
      .arg(value, 4, 10, QLatin1Char('0'))
      .arg(index);
}

QString join(const std::vector<std::u32string>& values) {
  QStringList rendered;
  rendered.reserve(static_cast<qsizetype>(values.size()));
  for (const std::u32string& value : values) {
    rendered.push_back(to_qstring(value));
  }
  return rendered.join(QStringLiteral(", "));
}

}  // namespace

KanjiInfoDialog::KanjiInfoDialog(const core::KanjiInfoDatabase& database,
                                 QWidget* parent)
    : QDialog(parent),
      database_(database),
      character_(new QLabel(this)),
      status_(new QLabel(this)),
      fields_(new QTableWidget(this)),
      readings_(new QListWidget(this)),
      meanings_(new QListWidget(this)) {
  setObjectName(QStringLiteral("kanjiInfoDialog"));
  setWindowTitle(tr("Kanji Information"));
  setModal(false);
  resize(720, 580);

  auto* outer = new QVBoxLayout(this);
  character_->setObjectName(QStringLiteral("kanjiInfoCharacter"));
  character_->setAlignment(Qt::AlignCenter);
  QFont character_font = character_->font();
  character_font.setPointSize(42);
  character_->setFont(character_font);
  outer->addWidget(character_);

  status_->setObjectName(QStringLiteral("kanjiInfoStatus"));
  status_->setAlignment(Qt::AlignCenter);
  outer->addWidget(status_);

  fields_->setObjectName(QStringLiteral("kanjiInfoFields"));
  fields_->setColumnCount(2);
  fields_->setHorizontalHeaderLabels({tr("Field"), tr("Value")});
  fields_->horizontalHeader()->setStretchLastSection(true);
  fields_->verticalHeader()->hide();
  fields_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  fields_->setSelectionMode(QAbstractItemView::NoSelection);
  outer->addWidget(fields_, 2);

  auto* lists = new QSplitter(Qt::Horizontal, this);
  auto* reading_group = new QGroupBox(tr("Readings"), lists);
  auto* reading_layout = new QVBoxLayout(reading_group);
  readings_->setObjectName(QStringLiteral("kanjiInfoReadings"));
  reading_layout->addWidget(readings_);
  auto* meaning_group = new QGroupBox(tr("Meanings"), lists);
  auto* meaning_layout = new QVBoxLayout(meaning_group);
  meanings_->setObjectName(QStringLiteral("kanjiInfoMeanings"));
  meaning_layout->addWidget(meanings_);
  lists->addWidget(reading_group);
  lists->addWidget(meaning_group);
  outer->addWidget(lists, 1);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  outer->addWidget(buttons);
}

bool KanjiInfoDialog::set_code(core::JisCode code) {
  if (!database_.contains(code)) return false;
  const core::KanjiInfoRecord record = database_.record(code);
  const std::u32string decoded = core::decode_jwp_text({record.code});
  if (decoded.size() != 1) return false;

  code_ = record.code;
  character_->setText(to_qstring(decoded));
  status_->setText(tr("JIS %1, Unicode U+%2")
                       .arg(hex_value(record.code),
                            hex_value(static_cast<std::uint32_t>(decoded[0]))));
  populate_fields(record, static_cast<std::uint32_t>(decoded[0]));
  populate_readings(record);
  meanings_->clear();
  for (const std::u32string& meaning : record.meanings) {
    meanings_->addItem(to_qstring(meaning));
  }
  setWindowTitle(tr("Kanji Information - %1").arg(to_qstring(decoded)));
  return true;
}

core::JisCode KanjiInfoDialog::code() const noexcept { return code_; }

void KanjiInfoDialog::populate_fields(const core::KanjiInfoRecord& record,
                                      std::uint32_t unicode) {
  std::vector<std::pair<QString, QString>> rows;
  rows.emplace_back(tr("JIS"), hex_value(record.code));
  rows.emplace_back(tr("EUC-JP"), hex_value(record.code | 0x8080U));
  rows.emplace_back(tr("Unicode"), QStringLiteral("U+%1").arg(hex_value(unicode)));
  const core::EncodedPair shift = core::encode_shift_jis_pair(record.code);
  const std::uint16_t shift_value = static_cast<std::uint16_t>(
      (static_cast<std::uint16_t>(shift.lead) << 8U) | shift.trail);
  rows.emplace_back(tr("Shift-JIS"), hex_value(shift_value));
  if (record.fixed.strokes != 0)
    rows.emplace_back(tr("Strokes"), decimal(record.fixed.strokes));
  if (record.fixed.grade != 0)
    rows.emplace_back(tr("Grade"), decimal(record.fixed.grade));
  if (record.fixed.bushu != 0)
    rows.emplace_back(tr("Radical"), decimal(record.fixed.bushu));
  if (record.fixed.classical_bushu != 0)
    rows.emplace_back(tr("Classical radical"),
                      decimal(record.fixed.classical_bushu));
  if (record.fixed.skip.type != 0)
    rows.emplace_back(tr("SKIP"), skip_code(record.fixed.skip));
  if (record.fixed.nelson != 0)
    rows.emplace_back(tr("Nelson"), decimal(record.fixed.nelson));
  if (record.fixed.haig != 0)
    rows.emplace_back(tr("New Nelson"), decimal(record.fixed.haig));
  if (record.fixed.halpern != 0)
    rows.emplace_back(tr("Halpern"), decimal(record.fixed.halpern));
  if (!record.pinyin.empty())
    rows.emplace_back(tr("Pinyin"), to_qstring(record.pinyin));
  if (!record.korean.empty())
    rows.emplace_back(tr("Korean"), to_qstring(record.korean));

  if (record.has_extended) {
    const core::KanjiInfoExtended& value = record.extended;
    if (value.four_corner != 0x3fffU &&
        (value.four_corner != 0 || value.four_corner_index != 0))
      rows.emplace_back(tr("Four corner"),
                        four_corner(value.four_corner, value.four_corner_index));
    if (value.morohashi_long != 0 || value.morohashi_volume != 0) {
      QString morohashi;
      if (value.morohashi_long != 0) {
        morohashi = decimal(value.morohashi_long);
        if (value.morohashi_cross) morohashi.append(QLatin1Char('X'));
        else if (value.morohashi_page) morohashi.append(QLatin1Char('P'));
      }
      if (value.morohashi_volume != 0) {
        if (!morohashi.isEmpty()) morohashi.append(QStringLiteral(", "));
        morohashi.append(QStringLiteral("%1.%2")
                             .arg(value.morohashi_volume)
                             .arg(value.morohashi_index, 4, 10,
                                  QLatin1Char('0')));
      }
      rows.emplace_back(tr("Morohashi"), morohashi);
    }
    if (value.spahn_radical_strokes != 0 || value.spahn_other_strokes != 0) {
      rows.emplace_back(
          tr("Spahn-Hadamitzky"),
          QStringLiteral("%1%2%3.%4")
              .arg(value.spahn_radical_strokes)
              .arg(QChar(static_cast<char16_t>('a' + value.spahn_radical)))
              .arg(value.spahn_other_strokes)
              .arg(value.spahn_index));
    }
  }
  for (const core::KanjiInfoCode& reference : record.references) {
    rows.emplace_back(tr("Reference %1").arg(QChar::fromLatin1(reference.kind)),
                      decimal(reference.value));
  }

  fields_->setRowCount(static_cast<int>(rows.size()));
  for (std::size_t row = 0; row < rows.size(); ++row) {
    fields_->setItem(static_cast<int>(row), 0,
                     new QTableWidgetItem(rows[row].first));
    fields_->setItem(static_cast<int>(row), 1,
                     new QTableWidgetItem(rows[row].second));
  }
  fields_->resizeRowsToContents();
}

void KanjiInfoDialog::populate_readings(const core::KanjiInfoRecord& record) {
  readings_->clear();
  if (!record.on_readings.empty())
    readings_->addItem(tr("On: %1").arg(join(record.on_readings)));
  if (!record.kun_readings.empty())
    readings_->addItem(tr("Kun: %1").arg(join(record.kun_readings)));
  if (!record.nanori.empty())
    readings_->addItem(tr("Nanori: %1").arg(join(record.nanori)));
}

}  // namespace jwpqt::qt
