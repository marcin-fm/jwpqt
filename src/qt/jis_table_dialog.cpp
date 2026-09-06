// SPDX-License-Identifier: GPL-2.0-or-later

#include "jis_table_dialog.h"

#include <exception>
#include <utility>

#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

#include "text_bridge.h"

namespace jwpqt::qt {
namespace {

QString hex_value(std::uint32_t value, int width) {
  return QStringLiteral("%1").arg(value, width, 16, QLatin1Char('0')).toUpper();
}

QString character_text(char32_t value) {
  return to_qstring(std::u32string(1, value));
}

std::optional<std::uint32_t> parse_hex(const QLineEdit& field,
                                       std::uint32_t maximum) {
  bool ok = false;
  const uint value = field.text().trimmed().toUInt(&ok, 16);
  return ok && value <= maximum
             ? std::optional<std::uint32_t>(value)
             : std::nullopt;
}

std::optional<core::JisTableEntry> describe_pair(std::uint32_t value,
                                                 bool shift_jis) {
  if (value > 0xffffU) return std::nullopt;
  try {
    const core::EncodedPair pair{static_cast<std::uint8_t>(value >> 8U),
                                 static_cast<std::uint8_t>(value & 0xffU)};
    const core::JisCode jis = shift_jis ? core::decode_shift_jis_pair(pair)
                                        : core::decode_euc_jp_pair(pair);
    return core::describe_jis_character(jis);
  } catch (const core::JisEncodingError&) {
    return std::nullopt;
  }
}

}  // namespace

JisTableDialog::JisTableDialog(InsertHandler insert_handler,
                               InfoHandler info_handler, QWidget* parent)
    : QDialog(parent),
      insert_handler_(std::move(insert_handler)),
      info_handler_(std::move(info_handler)),
      page_(new QSpinBox(this)),
      table_(new QTableWidget(6, 16, this)),
      jis_(new QLineEdit(this)),
      euc_(new QLineEdit(this)),
      shift_jis_(new QLineEdit(this)),
      unicode_(new QLineEdit(this)),
      status_(new QLabel(this)),
      copy_button_(new QPushButton(tr("&Copy"), this)),
      insert_button_(new QPushButton(tr("&Insert"), this)),
      info_button_(new QPushButton(tr("&Information"), this)) {
  setObjectName(QStringLiteral("jisTableDialog"));
  setWindowTitle(tr("Character Table"));
  setModal(false);
  resize(760, 310);

  auto* outer = new QVBoxLayout(this);
  auto* content = new QHBoxLayout;
  auto* code_group = new QGroupBox(tr("Character codes"), this);
  auto* codes = new QFormLayout(code_group);
  page_->setObjectName(QStringLiteral("jisTablePage"));
  page_->setRange(0x21, 0x74);
  page_->setDisplayIntegerBase(16);
  page_->setPrefix(QStringLiteral("0x"));
  codes->addRow(tr("JIS page"), page_);

  table_->setObjectName(QStringLiteral("jisTableGrid"));
  table_->setSelectionMode(QAbstractItemView::SingleSelection);
  table_->setSelectionBehavior(QAbstractItemView::SelectItems);
  table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  QFont content_font = table_->font();
  content_font.setPixelSize(16);
  table_->setFont(content_font);
  table_->horizontalHeader()->hide();
  table_->verticalHeader()->hide();
  table_->horizontalHeader()->setMinimumSectionSize(24);
  table_->verticalHeader()->setMinimumSectionSize(24);
  table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  table_->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  table_->setMinimumSize(16 * 24, 6 * 24);

  jis_->setObjectName(QStringLiteral("jisTableJis"));
  euc_->setObjectName(QStringLiteral("jisTableEuc"));
  shift_jis_->setObjectName(QStringLiteral("jisTableShiftJis"));
  unicode_->setObjectName(QStringLiteral("jisTableUnicode"));
  for (QLineEdit* field : {jis_, euc_, shift_jis_, unicode_}) {
    field->setMaxLength(6);
    field->setMaximumWidth(110);
  }
  codes->addRow(tr("JIS"), jis_);
  codes->addRow(tr("EUC-JP"), euc_);
  codes->addRow(tr("Shift-JIS"), shift_jis_);
  codes->addRow(tr("Unicode"), unicode_);
  content->addWidget(code_group);
  content->addWidget(table_, 1);
  outer->addLayout(content, 1);

  status_->setObjectName(QStringLiteral("jisTableStatus"));
  outer->addWidget(status_);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  copy_button_->setObjectName(QStringLiteral("jisTableCopy"));
  insert_button_->setObjectName(QStringLiteral("jisTableInsert"));
  info_button_->setObjectName(QStringLiteral("jisTableInfo"));
  buttons->addButton(copy_button_, QDialogButtonBox::ActionRole);
  buttons->addButton(insert_button_, QDialogButtonBox::ActionRole);
  buttons->addButton(info_button_, QDialogButtonBox::ActionRole);
  outer->addWidget(buttons);

  connect(page_, &QSpinBox::valueChanged, this,
          [this](int value) { populate_page(static_cast<std::uint8_t>(value)); });
  connect(table_, &QTableWidget::currentCellChanged, this,
          [this](int row, int column, int, int) {
            if (synchronizing_ || row < 0 || column < 0) return;
            QTableWidgetItem* item = table_->item(row, column);
            if (item == nullptr) return;
            const auto entry = core::describe_jis_character(
                static_cast<core::JisCode>(item->data(Qt::UserRole).toUInt()));
            if (entry.has_value()) select_entry(*entry);
          });
  connect(jis_, &QLineEdit::editingFinished, this,
          [this] { parse_code_field(*jis_, 0); });
  connect(euc_, &QLineEdit::editingFinished, this,
          [this] { parse_code_field(*euc_, 1); });
  connect(shift_jis_, &QLineEdit::editingFinished, this,
          [this] { parse_code_field(*shift_jis_, 2); });
  connect(unicode_, &QLineEdit::editingFinished, this,
          [this] { parse_code_field(*unicode_, 3); });
  connect(copy_button_, &QPushButton::clicked, this,
          [this] { copy_character(); });
  connect(insert_button_, &QPushButton::clicked, this,
          [this] { insert_character(); });
  connect(info_button_, &QPushButton::clicked, this,
          [this] { show_information(); });
  connect(table_, &QTableWidget::itemDoubleClicked, this,
          [this](QTableWidgetItem*) { insert_character(); });
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  populate_page(0x24U);
  (void)set_jis(0x2421U);
}

void JisTableDialog::populate_page(std::uint8_t page) {
  const QScopedValueRollback<bool> synchronizing(synchronizing_, true);
  table_->clearContents();
  for (const core::JisTableEntry& entry : core::jis_table_page(page)) {
    const int offset = static_cast<int>((entry.jis & 0xffU) - 0x21U);
    const int row = offset / 16;
    const int column = offset % 16;
    auto* item = new QTableWidgetItem(character_text(entry.unicode));
    item->setTextAlignment(Qt::AlignCenter);
    item->setData(Qt::UserRole, entry.jis);
    table_->setItem(row, column, item);
  }
}

bool JisTableDialog::set_jis(core::JisCode code) {
  const auto entry = core::describe_jis_character(code);
  if (!entry.has_value()) return false;
  select_entry(*entry);
  return true;
}

bool JisTableDialog::set_unicode(char32_t code_point) {
  const auto entry = core::describe_unicode_character(code_point);
  if (!entry.has_value()) return false;
  select_entry(*entry);
  return true;
}

std::optional<core::JisTableEntry> JisTableDialog::current() const noexcept {
  return current_;
}

void JisTableDialog::select_entry(const core::JisTableEntry& entry) {
  const QScopedValueRollback<bool> synchronizing(synchronizing_, true);
  const std::uint8_t page = static_cast<std::uint8_t>(entry.jis >> 8U);
  if (page_->value() != page) {
    const QSignalBlocker blocker(page_);
    page_->setValue(page);
    populate_page(page);
  }
  const int offset = static_cast<int>((entry.jis & 0xffU) - 0x21U);
  table_->setCurrentCell(offset / 16, offset % 16);
  jis_->setText(hex_value(entry.jis, 4));
  euc_->setText(hex_value((static_cast<std::uint32_t>(entry.euc.lead) << 8U) |
                              entry.euc.trail,
                          4));
  shift_jis_->setText(hex_value(
      (static_cast<std::uint32_t>(entry.shift_jis.lead) << 8U) |
          entry.shift_jis.trail,
      4));
  unicode_->setText(hex_value(static_cast<std::uint32_t>(entry.unicode), 4));
  current_ = entry;
  status_->setText(character_text(entry.unicode));
  update_actions();
}

void JisTableDialog::parse_code_field(QLineEdit& field, int kind) {
  if (synchronizing_) return;
  std::optional<core::JisTableEntry> entry;
  const auto value = parse_hex(field, kind == 3 ? 0x10ffffU : 0xffffU);
  if (value.has_value()) {
    if (kind == 0)
      entry = core::describe_jis_character(static_cast<core::JisCode>(*value));
    else if (kind == 1)
      entry = describe_pair(*value, false);
    else if (kind == 2)
      entry = describe_pair(*value, true);
    else
      entry = core::describe_unicode_character(static_cast<char32_t>(*value));
  }
  if (entry.has_value()) {
    select_entry(*entry);
  } else {
    if (current_.has_value()) select_entry(*current_);
    status_->setText(tr("The code is not an assigned JIS character"));
  }
}

void JisTableDialog::update_actions() {
  const bool available = current_.has_value();
  copy_button_->setEnabled(available);
  insert_button_->setEnabled(available && static_cast<bool>(insert_handler_));
  info_button_->setEnabled(available && static_cast<bool>(info_handler_));
}

void JisTableDialog::copy_character() {
  if (!current_.has_value()) return;
  QApplication::clipboard()->setText(character_text(current_->unicode));
}

void JisTableDialog::insert_character() {
  if (!current_.has_value() || !insert_handler_) return;
  try {
    insert_handler_(*current_);
  } catch (const std::exception& error) {
    status_->setText(QString::fromUtf8(error.what()));
  } catch (...) {
    status_->setText(tr("JIS table insertion failed"));
  }
}

void JisTableDialog::show_information() {
  if (!current_.has_value() || !info_handler_) return;
  try {
    info_handler_(current_->jis);
  } catch (const std::exception& error) {
    status_->setText(QString::fromUtf8(error.what()));
  } catch (...) {
    status_->setText(tr("Kanji information lookup failed"));
  }
}

}  // namespace jwpqt::qt
