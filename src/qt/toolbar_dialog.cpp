// SPDX-License-Identifier: GPL-2.0-or-later
#include "toolbar_dialog.h"
#include <QAction>
#include <QComboBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace jwpqt::qt {
ToolbarDialog::ToolbarDialog(const ToolbarSettings& settings, const QList<QAction*>& catalog, QWidget* parent)
    : QDialog(parent), settings_(settings) {
  validate_toolbar(settings);
  if (catalog.size() != static_cast<int>(kToolbarCommands.size())) throw std::runtime_error("Invalid toolbar catalog");
  setObjectName(QStringLiteral("toolbarDialog"));
  setWindowTitle(tr("Customize Toolbar"));
  resize(760, 540);
  auto* layout = new QVBoxLayout(this);
  auto* note = new QLabel(tr("Add commands or separators, then select an item to move or remove. "
      "Repeated commands share the same state. Changes apply only on OK; hide an unused toolbar with View > Toolbar."), this);
  note->setWordWrap(true); layout->addWidget(note);
  auto* row = new QHBoxLayout;
  auto* available = new QListWidget(this); available->setObjectName("toolbarAvailable");
  auto* selected = new QListWidget(this); selected->setObjectName("toolbarSelected");
  available->setAccessibleName(tr("Available commands")); selected->setAccessibleName(tr("Toolbar layout"));
  for (int id = 0; id < catalog.size(); ++id) {
    if (id && !catalog[id]) throw std::runtime_error("Missing toolbar command");
    QString label = id ? catalog[id]->text().remove(QLatin1Char('&')) : tr("Separator");
    auto* item = new QListWidgetItem(id ? catalog[id]->icon() : QIcon{}, label, available);
    item->setData(Qt::UserRole, id);
  }
  auto populate = [selected, available](const ToolbarSettings& value) {
    selected->clear();
    const auto defaults = ToolbarSettings{};
    const auto& effective = value.count ? value : defaults;
    for (int i = 0; i < effective.count; ++i) {
      selected->addItem(available->item(effective.buttons[i])->clone());
    }
    selected->setCurrentRow(0);
  };
  auto* available_column = new QVBoxLayout;
  available_column->addWidget(new QLabel(tr("Available commands"), this)); available_column->addWidget(available);
  row->addLayout(available_column);
  auto* controls = new QVBoxLayout;
  const auto button = [&](const char* name, const QString& text) {
    auto* result = new QPushButton(text, this); result->setObjectName(name);
    result->setAutoDefault(false); controls->addWidget(result); return result;
  };
  auto* add = button("toolbarAdd", tr("Add >"));
  auto* remove = button("toolbarRemove", tr("Remove"));
  auto* up = button("toolbarUp", tr("Move Up"));
  auto* down = button("toolbarDown", tr("Move Down"));
  auto* reset = button("toolbarReset", tr("Reset Layout"));
  controls->addStretch(); row->addLayout(controls);
  auto* selected_column = new QVBoxLayout;
  selected_column->addWidget(new QLabel(tr("Toolbar layout"), this)); selected_column->addWidget(selected);
  row->addLayout(selected_column); layout->addLayout(row);
  connect(add, &QPushButton::clicked, this, [available, selected] {
    if (!available->currentItem() || selected->count() >= 100) return;
    const int at = selected->currentRow() < 0 ? selected->count() : selected->currentRow() + 1;
    selected->insertItem(at, available->currentItem()->clone()); selected->setCurrentRow(at);
  });
  connect(remove, &QPushButton::clicked, this, [selected] {
    if (selected->count() > 1) delete selected->takeItem(selected->currentRow());
  });
  const auto move = [selected](int delta) {
    const int at = selected->currentRow(), next = at + delta;
    if (at < 0 || next < 0 || next >= selected->count()) return;
    auto* item = selected->takeItem(at); selected->insertItem(next, item); selected->setCurrentRow(next);
  };
  connect(up, &QPushButton::clicked, this, [move] { move(-1); });
  connect(down, &QPushButton::clicked, this, [move] { move(1); });
  connect(reset, &QPushButton::clicked, this, [populate] { populate(ToolbarSettings{}); });
  populate(settings); available->setCurrentRow(0);
  for (int id = 1; id < catalog.size(); ++id) {
    auto* source = catalog[id];
    connect(source, &QAction::changed, this, [source, available, selected, id] {
      available->item(id)->setIcon(source->icon());
      for (int i = 0; i < selected->count(); ++i)
        if (selected->item(i)->data(Qt::UserRole).toInt() == id) selected->item(i)->setIcon(source->icon());
    });
  }
  const auto update = [=] {
    add->setEnabled(available->currentRow() >= 0 && selected->count() < 100);
    remove->setEnabled(selected->currentRow() >= 0 && selected->count() > 1);
    up->setEnabled(selected->currentRow() > 0);
    down->setEnabled(selected->currentRow() >= 0 && selected->currentRow() + 1 < selected->count());
  };
  connect(selected, &QListWidget::currentRowChanged, this, update);
  connect(available, &QListWidget::currentRowChanged, this, update);
  connect(selected->model(), &QAbstractItemModel::rowsInserted, this, update);
  connect(selected->model(), &QAbstractItemModel::rowsRemoved, this, update);
  update();
  auto* form = new QFormLayout;
  auto* area = new QComboBox(this); area->setObjectName("toolbarArea");
  area->addItems({tr("Top"), tr("Bottom"), tr("Left"), tr("Right")}); area->setCurrentIndex(settings.area);
  auto* size = new QSpinBox(this); size->setObjectName("toolbarIconSize"); size->setRange(16, 48); size->setValue(settings.icon_size);
  auto* style = new QComboBox(this); style->setObjectName("toolbarTextStyle");
  style->addItems({tr("Icons"), tr("Text beside icons"), tr("Text below icons"), tr("Text only")}); style->setCurrentIndex(settings.text_style);
  auto* locked = new QCheckBox(tr("Lock toolbar position"), this); locked->setObjectName("toolbarLocked"); locked->setChecked(settings.locked);
  form->addRow(tr("Dock:"), area); form->addRow(tr("Icon size (logical pixels):"), size);
  form->addRow(tr("Display:"), style); form->addRow(locked); layout->addLayout(form);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this); layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(buttons, &QDialogButtonBox::accepted, this, [this, selected, area, size, style, locked] {
    settings_.count = selected->count();
    for (int i = 0; i < settings_.count; ++i) settings_.buttons[i] = selected->item(i)->data(Qt::UserRole).toUInt();
    settings_.area = area->currentIndex(); settings_.icon_size = size->value();
    settings_.text_style = style->currentIndex(); settings_.locked = locked->isChecked();
    validate_toolbar(settings_); accept();
  });
}
}
