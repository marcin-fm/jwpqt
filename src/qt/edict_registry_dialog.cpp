// SPDX-License-Identifier: GPL-2.0-or-later
#include "edict_registry_dialog.h"

#include <algorithm>
#include <initializer_list>
#include <utility>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStringView>
#include <QVBoxLayout>

#include "edict_resources.h"
#include "japanese_fonts.h"
#include "text_bridge.h"

namespace jwpqt::qt {

EdictRegistryDialog::EdictRegistryDialog(core::EdictRegistry registry, QString directory,
    core::LegacyCodePage code_page, QWidget* parent)
    : QDialog(parent), registry_(std::move(registry)), directory_(std::move(directory)),
      code_page_(code_page) {
  if (registry_.wire_encoding != core::EdictRegistryWireEncoding::kUtf16Le)
    throw core::EdictRegistryError("Convert the ANSI registry to Unicode before editing");
  (void)core::serialize_edict_registry(registry_);
  setObjectName(QStringLiteral("edictRegistryDialog"));
  setWindowTitle(tr("Dictionary Manager"));
  resize(920, 620);
  auto* layout = new QVBoxLayout(this);
  auto* description = new QLabel(tr("Search order is top to bottom. All changes are staged until Save and Reload.\n"
      "Removing an entry does not delete its files. Relative paths use: %1").arg(directory_), this);
  description->setWordWrap(true);
  description->setTextFormat(Qt::PlainText);
  layout->addWidget(description);
  auto* columns = new QHBoxLayout;
  layout->addLayout(columns, 1);
  auto* left = new QVBoxLayout;
  columns->addLayout(left, 1);
  list_ = new QListWidget(this);
  assign_japanese_font(*list_, JapaneseFontRole::kList);
  list_->setObjectName(QStringLiteral("registryEntries"));
  list_->setAccessibleName(tr("Dictionaries in search order"));
  left->addWidget(list_);
  auto* buttons = new QHBoxLayout;
  left->addLayout(buttons);
  const auto button = [this, buttons](const QString& text, const char* name) {
    auto* result = new QPushButton(text, this);
    result->setObjectName(QString::fromLatin1(name));
    result->setAutoDefault(false);
    buttons->addWidget(result);
    return result;
  };
  add_ = button(tr("Add"), "registryAdd");
  remove_ = button(tr("Remove"), "registryRemove");
  up_ = button(tr("Up"), "registryUp");
  down_ = button(tr("Down"), "registryDown");
  auto* form = new QFormLayout;
  columns->addLayout(form, 1);
  name_ = new QLineEdit(this);
  path_ = new QLineEdit(this);
  assign_japanese_font(*name_, JapaneseFontRole::kEdit);
  assign_japanese_font(*path_, JapaneseFontRole::kEdit);
  name_->setObjectName(QStringLiteral("registryName"));
  path_->setObjectName(QStringLiteral("registryPath"));
  name_->setMaxLength(65536);
  path_->setMaxLength(65536);
  form->addRow(tr("&Name:"), name_);
  form->addRow(tr("&File:"), path_);
  auto* browse = new QPushButton(tr("Browse..."), this);
  browse->setObjectName(QStringLiteral("registryBrowse"));
  browse->setAutoDefault(false);
  form->addRow(browse);
  const auto combo = [this, form](const QString& label, const char* name, const QStringList& values) {
    auto* result = new QComboBox(this);
    result->setObjectName(QString::fromLatin1(name));
    result->addItems(values);
    form->addRow(label, result);
    return result;
  };
  encoding_ = combo(tr("&Encoding:"), "registryEncoding", {tr("EUC-JP"), tr("UTF-8"), tr("Mixed EUC / code page")});
  names_ = combo(tr("&Names:"), "registryNames", {tr("No names"), tr("Includes names"), tr("Names only")});
  special_ = combo(tr("&Role:"), "registryRole", {tr("Normal"), tr("Classical"), tr("Editable user dictionary")});
  const auto check = [this, form](const QString& label, const char* name) {
    auto* result = new QCheckBox(label, this);
    result->setObjectName(QString::fromLatin1(name));
    form->addRow(result);
    return result;
  };
  searched_ = check(tr("Include in searches"), "registrySearched");
  indexed_ = check(tr("Use companion .jdx index"), "registryIndexed");
  buffered_ = check(tr("Retain legacy buffered preference"), "registryBuffered");
  keep_ = check(tr("Keep loaded between searches"), "registryKeep");
  quiet_ = check(tr("Quiet resource errors (still shown in diagnostics)"), "registryQuiet");
  auto* note = new QLabel(tr("Native loading and searching are bounded. Buffered and memory preferences use the same native loader.\n"
      "Mixed definitions use code page %1. Encoding is explicit, not guessed from an ambiguous sample.")
      .arg(static_cast<int>(code_page_)), this);
  note->setWordWrap(true);
  form->addRow(note);
  auto* inspect = new QPushButton(tr("Inspect Selected Resource"), this);
  inspect->setObjectName(QStringLiteral("registryInspect"));
  inspect->setAutoDefault(false);
  form->addRow(inspect);
  status_ = new QLabel(this);
  status_->setObjectName(QStringLiteral("registryStatus"));
  status_->setWordWrap(true);
  status_->setTextFormat(Qt::PlainText);
  status_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  layout->addWidget(status_);
  allow_ = new QCheckBox(tr("Allow saving unavailable or invalid searched resources (keep their diagnostics)"), this);
  allow_->setObjectName(QStringLiteral("registryAllowUnavailable"));
  layout->addWidget(allow_);
  auto* box = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults, this);
  box->button(QDialogButtonBox::Save)->setText(tr("Save and Reload"));
  layout->addWidget(box);
  connect(box, &QDialogButtonBox::accepted, this, &EdictRegistryDialog::accept);
  connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(box->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, &EdictRegistryDialog::defaults);
  connect(list_, &QListWidget::currentRowChanged, this, [this] { select_entry(); });
  connect(list_, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
    const int row = list_->row(item);
    if (updating_ || row < 0) return;
    const bool checked = item->checkState() == Qt::Checked;
    if (registry_.entries[row].searched == checked) return;
    registry_.entries[row].searched = checked;
    if (row == list_->currentRow()) {
      const QSignalBlocker blocked(searched_);
      searched_->setChecked(checked);
    }
  });
  for (auto* edit : {name_, path_}) connect(edit, &QLineEdit::textChanged, this, [this] { update_entry(); });
  for (auto* select : {encoding_, names_, special_})
    connect(select, &QComboBox::currentIndexChanged, this, [this] { update_entry(); });
  for (auto* flag : {searched_, indexed_, buffered_, keep_, quiet_})
    connect(flag, &QCheckBox::toggled, this, [this] { update_entry(); });
  connect(add_, &QPushButton::clicked, this, [this] {
    if (registry_.entries.size() >= core::EdictRegistryLimits{}.entries) return;
    const int row = list_->currentRow() + 1;
    core::EdictRegistryEntry entry;
    entry.label = u"New dictionary";
    entry.searched = true;
    registry_.entries.insert(registry_.entries.begin() + row, entry);
    refresh(row);
    name_->setFocus();
    name_->selectAll();
  });
  connect(remove_, &QPushButton::clicked, this, [this] {
    const int row = list_->currentRow();
    if (row < 0 || registry_.entries[row].special == core::EdictRegistrySpecial::kUser) return;
    registry_.entries.erase(registry_.entries.begin() + row);
    refresh(std::min(row, static_cast<int>(registry_.entries.size()) - 1));
  });
  const std::pair<QPushButton*, int> directions[] = {{up_, -1}, {down_, 1}};
  for (const auto direction : directions)
    connect(direction.first, &QPushButton::clicked, this, [this, delta = direction.second] {
      const int row = list_->currentRow(), next = row + delta;
      if (row < 0 || next < 0 || next >= static_cast<int>(registry_.entries.size())) return;
      std::swap(registry_.entries[row], registry_.entries[next]);
      refresh(next);
    });
  connect(inspect, &QPushButton::clicked, this, &EdictRegistryDialog::inspect_entry);
  connect(browse, &QPushButton::clicked, this, [this] {
    const QPointer<EdictRegistryDialog> self(this);
    const int row = list_->currentRow();
    const auto filename = QFileDialog::getOpenFileName(this, tr("Choose Dictionary"), directory_, tr("All files (*)"));
    if (self && row == list_->currentRow() && !filename.isEmpty()) path_->setText(filename);
  });
  refresh(0);
}

bool EdictRegistryDialog::allow_unavailable() const { return allow_->isChecked(); }

void EdictRegistryDialog::refresh(int row) {
  updating_ = true;
  list_->clear();
  for (const auto& entry : registry_.entries) {
    auto* item = new QListWidgetItem(QStringView(entry.label).toString(), list_);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(entry.searched ? Qt::Checked : Qt::Unchecked);
    item->setToolTip(QStringView(entry.path).toString());
  }
  list_->setCurrentRow(row);
  updating_ = false;
  select_entry();
}

void EdictRegistryDialog::select_entry() {
  if (updating_) return;
  const int row = list_->currentRow();
  const bool exists = row >= 0;
  updating_ = true;
  for (QWidget* field : std::initializer_list<QWidget*>{name_, path_, encoding_, names_, special_,
                         searched_, indexed_, buffered_, keep_, quiet_}) field->setEnabled(exists);
  add_->setEnabled(registry_.entries.size() < core::EdictRegistryLimits{}.entries);
  up_->setEnabled(row > 0);
  down_->setEnabled(exists && row + 1 < static_cast<int>(registry_.entries.size()));
  remove_->setEnabled(exists && registry_.entries[row].special != core::EdictRegistrySpecial::kUser);
  if (exists) {
    const auto& entry = registry_.entries[row];
    name_->setText(QStringView(entry.label).toString());
    path_->setText(QStringView(entry.path).toString());
    encoding_->setCurrentIndex(static_cast<int>(entry.encoding));
    names_->setCurrentIndex(static_cast<int>(entry.names));
    special_->setCurrentIndex(static_cast<int>(entry.special));
    searched_->setChecked(entry.searched);
    indexed_->setChecked(entry.indexed);
    buffered_->setChecked(entry.buffered);
    keep_->setChecked(entry.keep);
    quiet_->setChecked(entry.quiet);
    // Only the existing user entry may have this role; its storage contract is fixed.
    special_->setEnabled(entry.special != core::EdictRegistrySpecial::kUser);
    if (entry.special == core::EdictRegistrySpecial::kUser)
      for (QWidget* field : std::initializer_list<QWidget*>{encoding_, indexed_, buffered_, keep_}) field->setEnabled(false);
  }
  updating_ = false;
}

void EdictRegistryDialog::update_entry() {
  const int row = list_->currentRow();
  if (updating_ || row < 0) return;
  auto& entry = registry_.entries[row];
  entry.label = name_->text().toStdU16String();
  entry.path = path_->text().toStdU16String();
  entry.encoding = static_cast<core::EdictRegistryEncoding>(encoding_->currentIndex());
  entry.names = static_cast<core::EdictRegistryNames>(names_->currentIndex());
  if (entry.special != core::EdictRegistrySpecial::kUser) {
    if (special_->currentIndex() == 2) {
      updating_ = true;
      special_->setCurrentIndex(static_cast<int>(entry.special));
      updating_ = false;
    } else entry.special = static_cast<core::EdictRegistrySpecial>(special_->currentIndex());
  }
  entry.searched = searched_->isChecked();
  entry.indexed = indexed_->isChecked();
  entry.buffered = buffered_->isChecked();
  entry.keep = keep_->isChecked();
  entry.quiet = quiet_->isChecked();
  list_->item(row)->setText(QStringView(entry.label).toString());
  list_->item(row)->setToolTip(QStringView(entry.path).toString());
  list_->item(row)->setCheckState(entry.searched ? Qt::Checked : Qt::Unchecked);
  status_->clear();
}

void EdictRegistryDialog::inspect_entry() {
  const int row = list_->currentRow();
  if (row < 0) return;
  try {
    core::EdictRegistry candidate;
    candidate.entries.push_back(registry_.entries[row]);
    candidate.entries.front().searched = true;
    EdictResourceLoadOptions options;
    options.mixed_code_page = code_page_;
    const auto loaded = load_edict_resources(candidate, directory_, options);
    if (loaded.truncated) throw core::EdictRegistryError("Resource inspection exceeded its limits");
    if (!loaded.failures.empty()) throw core::EdictRegistryError(loaded.failures.front().message.toStdString());
    const auto& resource = loaded.resources.at(0);
    auto message = tr("%1 records loaded from %2%3").arg(resource.dictionary.records().size())
        .arg(resource.source_path, resource.index_path ? tr(" (validated index)") : tr(" (linear search)"));
    const auto& errors = resource.dictionary.record_errors();
    if (!errors.empty()) message += tr("\n%1 invalid records skipped. %2").arg(errors.size())
        .arg(QString::fromStdString(errors.front()));
    status_->setText(message);
  } catch (const std::exception& error) { status_->setText(QString::fromUtf8(error.what())); }
}

void EdictRegistryDialog::defaults() {
  auto user = std::find_if(registry_.entries.begin(), registry_.entries.end(), [](const auto& entry) {
    return entry.special == core::EdictRegistrySpecial::kUser;
  });
  if (user == registry_.entries.end()) return;
  const auto retained = *user;
  registry_.entries.clear();
  core::EdictRegistryEntry entry;
  entry.label = u"CLASSICAL"; entry.path = u"classical";
  entry.special = core::EdictRegistrySpecial::kClassical;
  entry.searched = QFileInfo(directory_ + QStringLiteral("/classical")).isFile();
  registry_.entries.push_back(entry);
  entry = {};
  entry.label = u"EDICT"; entry.path = u"edict";
  entry.indexed = entry.searched = entry.keep = true;
  registry_.entries.push_back(entry);
  entry.label = u"ENAMDICT"; entry.path = u"enamdict";
  entry.names = core::EdictRegistryNames::kNamesOnly; entry.quiet = true;
  registry_.entries.push_back(entry);
  registry_.entries.push_back(retained);
  refresh(0);
  status_->setText(tr("Default corpus entries staged; your user dictionary path and flags are retained. No files downloaded."));
}

void EdictRegistryDialog::accept() {
  try {
    if (std::count_if(registry_.entries.begin(), registry_.entries.end(), [](const auto& entry) {
          return entry.special == core::EdictRegistrySpecial::kUser;
        }) != 1) throw core::EdictRegistryError("Exactly one editable user dictionary is required");
    for (const auto& entry : registry_.entries) {
      if (entry.label.empty() || entry.path.empty()) throw core::EdictRegistryError("Every dictionary needs a name and path");
      if (entry.special == core::EdictRegistrySpecial::kUser &&
          (entry.encoding != core::EdictRegistryEncoding::kMixed || entry.indexed))
        throw core::EdictRegistryError("The user dictionary must be mixed and unindexed");
    }
    (void)core::serialize_edict_registry(registry_);
    QDialog::accept();
  } catch (const std::exception& error) { status_->setText(QString::fromUtf8(error.what())); }
}

}  // namespace jwpqt::qt
