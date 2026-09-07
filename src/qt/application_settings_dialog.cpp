// SPDX-License-Identifier: GPL-2.0-or-later

#include "application_settings_dialog.h"

#include <utility>
#include <vector>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include "jwpqt/core/jwp_configuration.h"

namespace jwpqt::qt {

ApplicationSettingsDialog::ApplicationSettingsDialog(const ApplicationSettings& settings, QWidget* parent)
    : QDialog(parent), settings_(settings) {
  setObjectName(QStringLiteral("applicationSettingsDialog"));
  setWindowTitle(tr("Options"));
  resize(720, 470);
  auto* outer = new QVBoxLayout(this);
  auto* tabs = new QTabWidget(this);
  outer->addWidget(tabs);
  auto* display = new QWidget(tabs);
  auto* form = new QFormLayout(display);
  struct BooleanControl { QCheckBox* widget; bool ApplicationSettings::*member; };
  std::vector<BooleanControl> booleans;
  const auto add_boolean = [&](const char* name, const QString& label, bool ApplicationSettings::*member) {
    auto* box = new QCheckBox(label, display);
    box->setObjectName(QString::fromLatin1(name));
    box->setChecked(settings_.*member);
    form->addRow(box);
    booleans.push_back({box, member});
  };
  add_boolean("settingsToolbar", tr("Show toolbar"), &ApplicationSettings::show_toolbar);
  add_boolean("settingsStatusBar", tr("Show status bar"), &ApplicationSettings::show_status_bar);
  add_boolean("settingsKanjiBar", tr("Show conversion candidate bar"), &ApplicationSettings::show_kanji_bar);
  add_boolean("settingsKanjiBarTop", tr("Place candidate bar above the document"), &ApplicationSettings::kanji_bar_at_top);
  add_boolean("settingsVerticalScroll", tr("Vertical document scrollbar"), &ApplicationSettings::vertical_scrollbar);
  add_boolean("settingsHorizontalScroll", tr("Horizontal document scrollbar"), &ApplicationSettings::horizontal_scrollbar);
  add_boolean("settingsCandidateScroll", tr("Candidate bar scrollbar"), &ApplicationSettings::kanji_bar_scrollbar);
  add_boolean("settingsSaveOnExit", tr("Save settings on exit"), &ApplicationSettings::save_settings_on_exit);
  add_boolean("settingsSaveRecent", tr("Remember recent files on disk"), &ApplicationSettings::save_recent_files);
  auto* code_page = new QComboBox(display);
  code_page->setObjectName(QStringLiteral("settingsCodePage"));
  code_page->addItem(tr("Automatic (native CP1252)"), 0);
  for (int page = 1250; page <= 1258; ++page) code_page->addItem(QStringLiteral("CP%1").arg(page), page);
  code_page->setCurrentIndex(code_page->findData(settings_.translation_code_page));
  form->addRow(tr("Default JWP code page"), code_page);
  tabs->addTab(display, tr("Display And Files"));

  auto* fonts = new QWidget(tabs);
  auto* grid = new QGridLayout(fonts);
  grid->addWidget(new QLabel(tr("Japanese content"), fonts), 0, 0);
  grid->addWidget(new QLabel(tr("Font family (blank uses native default)"), fonts), 0, 1);
  grid->addWidget(new QLabel(tr("Size"), fonts), 0, 2);
  grid->addWidget(new QLabel(tr("Automatic"), fonts), 0, 3);
  const char* labels[] = {"System", "Query fields", "Lists and readings", "Candidate bar", "Document", "Large character", "Character Table"};
  struct FontControl { QFontComboBox* family; QSpinBox* size; QCheckBox* automatic; };
  std::vector<FontControl> font_controls;
  for (std::size_t i = 0; i < settings_.fonts.size(); ++i) {
    const auto role = static_cast<JapaneseFontRole>(i);
    const int row = static_cast<int>(i) + 1;
    auto* family = new QFontComboBox(fonts);
    family->setObjectName(QStringLiteral("settingsFont%1").arg(i));
    family->setEditable(true);
    family->setEditText(settings_.fonts[i].family);
    grid->addWidget(new QLabel(tr(labels[i]), fonts), row, 0);
    grid->addWidget(family, row, 1);
    QSpinBox* size = nullptr;
    if (role == JapaneseFontRole::kBig || role == JapaneseFontRole::kTable) {
      grid->addWidget(new QLabel(role == JapaneseFontRole::kBig ? tr("Default glyph size") : tr("16 px"), fonts), row, 2);
    } else {
      size = new QSpinBox(fonts);
      size->setObjectName(QStringLiteral("settingsFontSize%1").arg(i));
      size->setRange(1, 1024);
      size->setSuffix(tr(" px"));
      size->setValue(settings_.fonts[i].size);
      grid->addWidget(size, row, 2);
    }
    QCheckBox* automatic = nullptr;
    if (role != JapaneseFontRole::kSystem) {
      automatic = new QCheckBox(fonts);
      automatic->setObjectName(QStringLiteral("settingsFontAuto%1").arg(i));
      automatic->setChecked(settings_.fonts[i].automatic);
      const auto enable = [family, size](bool inherit) { family->setEnabled(!inherit); if (size) size->setEnabled(!inherit); };
      connect(automatic, &QCheckBox::toggled, this, enable);
      enable(automatic->isChecked());
      grid->addWidget(automatic, row, 3);
    }
    font_controls.push_back({family, size, automatic});
  }
  auto* explanation = new QLabel(tr("Automatic query/document fonts inherit System; lists and candidates inherit query fields. "
      "These settings do not change desktop menu fonts. Unavailable or legacy bitmap families use a native fallback; their names remain stored."), fonts);
  explanation->setWordWrap(true);
  grid->addWidget(explanation, 8, 0, 1, 4);
  grid->setRowStretch(9, 1);
  tabs->addTab(fonts, tr("Fonts"));
  if (!settings_.unapplied.isEmpty()) {
    auto* retained = new QPlainTextEdit(tabs);
    retained->setReadOnly(true);
    retained->setPlainText(tr("These imported settings are retained, but not yet applied:\n\n") + settings_.unapplied.join(QLatin1Char('\n')));
    tabs->addTab(retained, tr("Retained Settings"));
  }
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  outer->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(buttons, &QDialogButtonBox::accepted, this, [this, booleans, font_controls, code_page] {
    auto next = settings_;
    for (const auto& control : booleans) next.*(control.member) = control.widget->isChecked();
    for (std::size_t i = 0; i < font_controls.size(); ++i) {
      const auto& control = font_controls[i];
      next.fonts[i].family = control.family->currentText();
      if (control.size) next.fonts[i].size = control.size->value();
      if (control.automatic) next.fonts[i].automatic = control.automatic->isChecked();
    }
    next.translation_code_page = code_page->currentData().toInt();
    try {
      (void)write_application_settings(next);
      settings_ = std::move(next);
      accept();
    } catch (const core::JwpConfigurationError& error) {
      QMessageBox::warning(this, tr("Invalid settings"), QString::fromUtf8(error.what()));
    }
  });
}

}  // namespace jwpqt::qt
