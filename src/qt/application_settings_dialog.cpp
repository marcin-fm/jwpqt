// SPDX-License-Identifier: GPL-2.0-or-later

#include "application_settings_dialog.h"
#include "kanji_lookup_names.h"

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

#include <QCheckBox>
#include <QComboBox>
#include <QColorDialog>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QListWidget>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include "jwpqt/core/jwp_configuration.h"
#include "jwpqt/core/edict_filter.h"
#include "jwpqt/core/jwp_text_codec.h"
#include "kanji_info_options_dialog.h"
#include "kana_input_field.h"
#include "text_bridge.h"

namespace jwpqt::qt {
namespace {

constexpr double kCentimetersPerInch = 2.54;

}  // namespace

ApplicationSettingsDialog::ApplicationSettingsDialog(const ApplicationSettings& settings, QWidget* parent,
                                                     bool dictionary_page,
                                                     QAction* overwrite_action)
    : QDialog(parent), settings_(settings) {
  setObjectName(QStringLiteral("applicationSettingsDialog"));
  setWindowTitle(tr("Options"));
  resize(720, 470);
  auto* outer = new QVBoxLayout(this);
  auto* tabs = new QTabWidget(this);
  tabs->setObjectName(QStringLiteral("applicationSettingsTabs"));
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
  add_boolean("settingsStartupDictionary", tr("Open dictionary on startup without an explicit document"), &ApplicationSettings::startup_dictionary);
  add_boolean("settingsReloadFiles", tr("Restore named files from the previous session before opening command-line files"), &ApplicationSettings::reload_previous_files);
  add_boolean("settingsRevertToKanji", tr("Switch to Kanji input on explicit conversion"), &ApplicationSettings::revert_to_kanji_mode);
  add_boolean("settingsOldKatakana", tr("Use old katakana vowel quote handling"), &ApplicationSettings::old_katakana_input);
  add_boolean("settingsCtrlConvert", tr("Use Ctrl+Up/Down to convert selected kana"), &ApplicationSettings::ctrl_up_down_convert);
  add_boolean("settingsInsertLines", tr("Insert complete result-list entries on separate lines"), &ApplicationSettings::insert_on_separate_lines);
  add_boolean("settingsAutoScroll", tr("Autoscroll while extending a selection near the document edge"), &ApplicationSettings::auto_scroll);
  auto* auto_scroll_speed = new QSpinBox(display);
  auto_scroll_speed->setObjectName(QStringLiteral("settingsAutoScrollSpeed"));
  auto_scroll_speed->setRange(0, 10000);
  auto_scroll_speed->setSuffix(tr(" ms"));
  auto_scroll_speed->setValue(settings_.auto_scroll_speed);
  form->addRow(tr("Autoscroll repeat delay"), auto_scroll_speed);
  add_boolean("settingsKeepBackup", tr("Keep the previous disk version as filename_BAK when saving"), &ApplicationSettings::keep_backup_copy);
  add_boolean("settingsRestoreWindow", tr("Restore main window position, size and maximized state on startup"), &ApplicationSettings::restore_window);
  add_boolean("settingsCloseButtonFile", tr("Window close button closes the current document"), &ApplicationSettings::close_button_closes_file);
  add_boolean("settingsConfirmLastFileExit", tr("Ask whether to exit after closing the last document"), &ApplicationSettings::confirm_last_file_exit);
  auto* duplicate_open = new QComboBox(display);
  duplicate_open->setObjectName(QStringLiteral("settingsDuplicateOpen"));
  duplicate_open->addItem(tr("Ask each time"), static_cast<int>(DuplicateOpenBehavior::kPrompt));
  duplicate_open->addItem(tr("Change to the open document"), static_cast<int>(DuplicateOpenBehavior::kActivateExisting));
  duplicate_open->addItem(
      tr("Open another copy"),
      static_cast<int>(DuplicateOpenBehavior::kOpenAnother));
  duplicate_open->setCurrentIndex(duplicate_open->findData(static_cast<int>(settings_.duplicate_open)));
  form->addRow(tr("When opening an open file"), duplicate_open);
  const auto add_clipboard_formats = [](QComboBox* control, bool include_auto) {
    if (include_auto)
      control->addItem(tr("Automatic"), static_cast<int>(ClipboardTextFormat::kAutoDetect));
    control->addItem(tr("EUC-JP"), static_cast<int>(ClipboardTextFormat::kEucJp));
    control->addItem(tr("Shift-JIS"), static_cast<int>(ClipboardTextFormat::kShiftJis));
    control->addItem(tr("New JIS"), static_cast<int>(ClipboardTextFormat::kNewJis));
    control->addItem(tr("Old JIS"), static_cast<int>(ClipboardTextFormat::kOldJis));
    control->addItem(tr("NEC JIS"), static_cast<int>(ClipboardTextFormat::kNecJis));
    control->addItem(tr("Unicode (UTF-16LE)"), static_cast<int>(ClipboardTextFormat::kUnicode));
    control->addItem(tr("UTF-7"), static_cast<int>(ClipboardTextFormat::kUtf7));
    control->addItem(tr("UTF-8"), static_cast<int>(ClipboardTextFormat::kUtf8));
  };
  auto* clipboard_import = new QComboBox(display);
  clipboard_import->setObjectName(QStringLiteral("settingsClipboardImport"));
  add_clipboard_formats(clipboard_import, true);
  clipboard_import->setCurrentIndex(clipboard_import->findData(
      static_cast<int>(settings_.clipboard_import)));
  form->addRow(tr("Clipboard text import"), clipboard_import);
  auto* clipboard_export = new QComboBox(display);
  clipboard_export->setObjectName(QStringLiteral("settingsClipboardExport"));
  add_clipboard_formats(clipboard_export, false);
  clipboard_export->setCurrentIndex(clipboard_export->findData(
      static_cast<int>(settings_.clipboard_export)));
  form->addRow(tr("Clipboard encoded-text export"), clipboard_export);
  add_boolean(
      "settingsOmitClipboardUnicode",
      tr("Omit standard Unicode and rich text when copying documents"),
      &ApplicationSettings::omit_clipboard_unicode);
  auto* code_page = new QComboBox(display);
  code_page->setObjectName(QStringLiteral("settingsCodePage"));
  code_page->addItem(tr("Automatic (native CP1252)"), 0);
  for (int page = 1250; page <= 1258; ++page) code_page->addItem(QStringLiteral("CP%1").arg(page), page);
  code_page->setCurrentIndex(code_page->findData(settings_.translation_code_page));
  form->addRow(tr("Default JWP code page"), code_page);
  auto* information = new QPushButton(tr("Character Info Setup..."), display);
  information->setObjectName(QStringLiteral("settingsCharacterInfo"));
  connect(information, &QPushButton::clicked, this, [this] {
    KanjiInfoOptionsDialog dialog(settings_.kanji_info, this);
    if (dialog.exec() == QDialog::Accepted) settings_.kanji_info = dialog.options();
  });
  form->addRow(information);
  tabs->addTab(display, tr("Display And Files"));

  auto* colors = new QWidget(tabs);
  auto* color_form = new QFormLayout(colors);
  std::array<QLineEdit*, 3> color_fields{};
  std::array<QString, 3> original_colors;
  const QString color_labels[] = {tr("Information headings"), tr("Kanji list"), tr("Uncommon kanji")};
  for (std::size_t i = 0; i < 3; ++i) {
    const auto raw = settings_.color_refs[i];
    if (raw && !(*raw & 0xff000000U))
      original_colors[i] = QColor(*raw & 255, (*raw >> 8) & 255, (*raw >> 16) & 255).name();
    auto* row = new QWidget(colors);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* edit = color_fields[i] = new QLineEdit(original_colors[i], row);
    edit->setObjectName(QStringLiteral("settingsColor%1").arg(i));
    edit->setMaxLength(7);
    edit->setPlaceholderText(raw ? tr("Special COLORREF retained") : tr("Inherited / theme default"));
    auto* choose = new QPushButton(tr("Choose..."), row);
    auto* inherit = new QPushButton(tr("Inherit"), row);
    inherit->setObjectName(QStringLiteral("settingsColorInherit%1").arg(i));
    connect(inherit, &QPushButton::clicked, edit, [edit] {
      edit->setProperty("jwpqtColorCleared", true);
      edit->clear();
    });
    layout->addWidget(edit); layout->addWidget(choose); layout->addWidget(inherit);
    connect(choose, &QPushButton::clicked, this, [this, edit] {
      const QPointer<QLineEdit> target(edit);
      QPointer<QColorDialog> dialog = new QColorDialog(QColor(edit->text()), this);
      if (dialog->exec() == QDialog::Accepted && dialog && target)
        target->setText(dialog->selectedColor().name());
      if (dialog) delete dialog.data();
    });
    color_form->addRow(color_labels[i], row);
  }
  auto* color_mode = new QComboBox(colors);
  color_mode->setObjectName(QStringLiteral("settingsColorMode"));
  color_mode->addItem(tr("Use native color store"), -1);
  color_mode->addItem(tr("Off"), 0); color_mode->addItem(tr("Color listed kanji"), 1);
  color_mode->addItem(tr("Color unlisted kanji"), 2);
  if (settings_.color_kanji_mode && *settings_.color_kanji_mode > 2)
    color_mode->addItem(tr("Source mode %1 (unlisted)").arg(*settings_.color_kanji_mode), *settings_.color_kanji_mode);
  color_mode->setCurrentIndex(color_mode->findData(settings_.color_kanji_mode.value_or(-1)));
  color_form->addRow(tr("List coloring"), color_mode);
  auto* uncommon = new QComboBox(colors);
  uncommon->setObjectName(QStringLiteral("settingsColorUncommon"));
  uncommon->addItem(tr("Use native color store"), -1); uncommon->addItem(tr("Off"), 0); uncommon->addItem(tr("On"), 1);
  uncommon->setCurrentIndex(uncommon->findData(settings_.colorize_rare ? int(*settings_.colorize_rare) : -1));
  color_form->addRow(tr("Uncommon coloring"), uncommon);
  auto* rare_marks = new QCheckBox(tr("Mark uncommon characters in conversion and lookup bars"), colors);
  rare_marks->setObjectName(QStringLiteral("settingsMarkRare"));
  rare_marks->setChecked(settings_.mark_rare_kanji);
  booleans.push_back({rare_marks, &ApplicationSettings::mark_rare_kanji});
  color_form->addRow(rare_marks);
  auto* color_note = new QLabel(tr("Colors use #RRGGBB. Empty values inherit existing native settings. "
      "Heading colors adapt when necessary for readable theme contrast. Marks do not change copied text."), colors);
  color_note->setWordWrap(true); color_form->addRow(color_note);

  auto* fonts = new QWidget(tabs);
  auto* grid = new QGridLayout(fonts);
  auto* show_all_fonts = new QCheckBox(tr("Show all installed families in Japanese font lists"), fonts);
  show_all_fonts->setObjectName(QStringLiteral("settingsShowAllFonts"));
  show_all_fonts->setChecked(settings_.show_all_fonts);
  booleans.push_back({show_all_fonts, &ApplicationSettings::show_all_fonts});
  grid->addWidget(show_all_fonts, 0, 0, 1, 4);
  grid->addWidget(new QLabel(tr("Japanese content"), fonts), 1, 0);
  grid->addWidget(new QLabel(tr("Font family or .f00 file (blank uses native default)"), fonts), 1, 1);
  grid->addWidget(new QLabel(tr("Size"), fonts), 1, 2);
  grid->addWidget(new QLabel(tr("Automatic"), fonts), 1, 3);
  const char* labels[] = {"System", "Query fields", "Lists and readings", "Candidate bar", "Document", "Large character", "Character Table", "Clipboard bitmap"};
  auto all_families = QFontDatabase::families();
  for (auto it = all_families.begin(); it != all_families.end();)
    if (it->startsWith(QStringLiteral("JwpqtRaster-")) || it->startsWith(QStringLiteral("JwpqtAscii-")) ||
        it->startsWith(QStringLiteral("JwpqtVertical-"))) it = all_families.erase(it); else ++it;
  const auto listed_families = [all_families](bool show_all) {
    if (show_all) return all_families;
    QStringList recommended;
    for (const auto& family : all_families)
      if (QFontDatabase::writingSystems(family).contains(QFontDatabase::Japanese))
        recommended.push_back(family);
    return recommended;
  };
  struct FontControl { QComboBox* family; QSpinBox* size; QCheckBox* automatic; };
  std::vector<FontControl> font_controls;
  std::vector<QComboBox*> japanese_family_controls;
  auto* vertical_bitmap = new QCheckBox(tr("Vertical Japanese glyphs in clipboard images"), fonts);
  vertical_bitmap->setObjectName(QStringLiteral("settingsVerticalClipboardBitmap"));
  vertical_bitmap->setChecked(settings_.vertical_clipboard_bitmap);
  vertical_bitmap->setToolTip(tr("Uses the bitmap font's own settings even when Automatic is checked. Latin stays horizontal; the image is intended to be read after a clockwise turn."));
  booleans.push_back({vertical_bitmap, &ApplicationSettings::vertical_clipboard_bitmap});
  for (std::size_t i = 0; i < settings_.fonts.size(); ++i) {
    const auto role = static_cast<JapaneseFontRole>(i);
    const int row = static_cast<int>(i) + 2;
    auto* family = new QComboBox(fonts);
    family->addItems(listed_families(settings_.show_all_fonts));
    family->setObjectName(QStringLiteral("settingsFont%1").arg(i));
    family->setEditable(true);
    family->setEditText(settings_.fonts[i].family);
    japanese_family_controls.push_back(family);
    grid->addWidget(new QLabel(tr(labels[i]), fonts), row, 0);
    grid->addWidget(family, row, 1);
    QSpinBox* size = nullptr;
    if (role == JapaneseFontRole::kBig || role == JapaneseFontRole::kTable) {
      grid->addWidget(new QLabel(role == JapaneseFontRole::kBig ? tr("Fit to pane") : tr("16 px"), fonts), row, 2);
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
      const auto enable = [family, size, automatic, role, vertical_bitmap](bool inherit) {
        const bool vertical = role == JapaneseFontRole::kBitmap && vertical_bitmap->isChecked();
        family->setEnabled(!inherit || vertical);
        if (size) size->setEnabled(!inherit || vertical);
        automatic->setEnabled(!vertical);
      };
      connect(automatic, &QCheckBox::toggled, this, enable);
      if (role == JapaneseFontRole::kBitmap)
        connect(vertical_bitmap, &QCheckBox::toggled, this, [enable, automatic] { enable(automatic->isChecked()); });
      enable(automatic->isChecked());
      grid->addWidget(automatic, row, 3);
    }
    font_controls.push_back({family, size, automatic});
  }
  connect(show_all_fonts, &QCheckBox::toggled, this,
          [listed_families, japanese_family_controls](bool checked) {
            const auto families = listed_families(checked);
            for (auto* control : japanese_family_controls) {
              const QString current = control->currentText();
              const QSignalBlocker blocker(control);
              control->clear();
              control->addItems(families);
              control->setEditText(current);
            }
          });
  auto* explanation = new QLabel(tr("Automatic query/document fonts inherit System; lists and candidates inherit query fields. "
      "These settings do not change desktop menu fonts. Unavailable families or invalid raster files use a native fallback; their names remain stored."), fonts);
  explanation->setWordWrap(true);
  const int ascii_row = static_cast<int>(settings_.fonts.size()) + 2;
  auto* ascii_family = new QComboBox(fonts);
  ascii_family->setObjectName(QStringLiteral("settingsAsciiFont"));
  ascii_family->addItems(all_families);
  ascii_family->setEditable(true);
  ascii_family->setEditText(settings_.ascii_font.family);
  grid->addWidget(new QLabel(tr("ASCII and legacy extensions"), fonts), ascii_row, 0);
  grid->addWidget(ascii_family, ascii_row, 1);
  grid->addWidget(new QLabel(tr("Matched height"), fonts), ascii_row, 2, 1, 2);
  const int last_font_row = ascii_row + 1;
  grid->addWidget(explanation, last_font_row, 0, 1, 4);
  auto* omit_bitmap = new QCheckBox(tr("Omit bitmap images when copying document text"), fonts);
  omit_bitmap->setObjectName(QStringLiteral("settingsOmitClipboardBitmap"));
  omit_bitmap->setChecked(settings_.omit_clipboard_bitmap);
  booleans.push_back({omit_bitmap, &ApplicationSettings::omit_clipboard_bitmap});
  grid->addWidget(omit_bitmap, last_font_row + 1, 0, 1, 4);
  grid->addWidget(vertical_bitmap, last_font_row + 2, 0, 1, 4);
  auto* color_bitmap = new QCheckBox(tr("Apply kanji colors to clipboard images when list coloring is active"), fonts);
  color_bitmap->setObjectName(QStringLiteral("settingsColorClipboardBitmap"));
  color_bitmap->setChecked(settings_.color_clipboard_bitmap);
  booleans.push_back({color_bitmap, &ApplicationSettings::color_clipboard_bitmap});
  grid->addWidget(color_bitmap, last_font_row + 3, 0, 1, 4);
  grid->setRowStretch(last_font_row + 4, 1);
  tabs->addTab(fonts, tr("Fonts"));
  auto* dictionary = new QWidget(tabs);
  auto* dictionary_form = new QFormLayout(dictionary);
  dictionary_form->setSizeConstraint(QLayout::SetMinimumSize);
  struct DictionaryControl { QCheckBox* widget; bool EdictLookupOptions::*member; };
  std::vector<DictionaryControl> dictionary_controls;
  const auto add_dictionary = [&](const char* name, const QString& label, bool EdictLookupOptions::*member) {
    auto* box = new QCheckBox(label, dictionary);
    box->setObjectName(QString::fromLatin1(name));
    box->setChecked(settings_.dictionary.*member);
    dictionary_form->addRow(box);
    dictionary_controls.push_back({box, member});
    return box;
  };
  add_dictionary("settingsDictionaryBegin", tr("Begin With"), &EdictLookupOptions::require_beginning);
  add_dictionary("settingsDictionaryEnd", tr("End With"), &EdictLookupOptions::require_end);
  add_dictionary("settingsDictionaryNames", tr("Include personal names"), &EdictLookupOptions::personal_names);
  add_dictionary("settingsDictionaryPlaces", tr("Include place names"), &EdictLookupOptions::place_names);
  add_dictionary("settingsDictionaryClassical", tr("Classical dictionaries"), &EdictLookupOptions::classical);
  auto* advanced = add_dictionary("settingsDictionaryAdvanced", tr("Advanced search"), &EdictLookupOptions::advanced);
  for (auto* box : {
       add_dictionary("settingsDictionaryAlways", tr("Always search inflected forms"), &EdictLookupOptions::advanced_always),
       add_dictionary("settingsDictionaryShowAll", tr("Keep searching all inflected forms"), &EdictLookupOptions::advanced_show_all),
       add_dictionary("settingsDictionaryIAdjectives", tr("Include I-adjectives"), &EdictLookupOptions::i_adjectives)}) {
    box->setEnabled(advanced->isChecked());
    connect(advanced, &QCheckBox::toggled, box, &QWidget::setEnabled);
  }
  add_dictionary("settingsDictionaryFullAscii", tr("ASCII boundaries match the complete definition"), &EdictLookupOptions::full_ascii);
  add_dictionary("settingsDictionaryJascii", tr("Treat JASCII as ASCII"), &EdictLookupOptions::jascii_to_ascii);
  add_dictionary("settingsDictionaryContingent", tr("Contingent search after eligible exact searches fail"), &EdictLookupOptions::contingent);
  add_dictionary("settingsDictionaryClipboard", tr("Monitor future external clipboard changes while Dictionary Lookup is visible"), &EdictLookupOptions::monitor_clipboard);
  add_dictionary("settingsDictionaryAutomatic", tr("Search a document selection when opening dictionary lookup"), &EdictLookupOptions::automatic_search);
  add_dictionary("settingsDictionaryCompact", tr("Compact results: headword and definitions together"), &EdictLookupOptions::compact);
  add_dictionary("settingsDictionaryPriority", tr("Show priority entries first within search sections"), &EdictLookupOptions::priority_first);
  add_dictionary("settingsDictionaryPrioritySeparator", tr("Mark the end of priority entries"), &EdictLookupOptions::priority_separator);
  add_dictionary("settingsDictionaryAdvancedSeparator", tr("Separate advanced results from earlier searches"), &EdictLookupOptions::advanced_separator);
  add_dictionary("settingsDictionaryLinkNames", tr("Link Advanced with exclusion of personal and place names"), &EdictLookupOptions::link_advanced_names);
  const char* category_labels[] = {
      QT_TR_NOOP("Vulgar expressions"), QT_TR_NOOP("Rude or X-rated terms"),
      QT_TR_NOOP("Colloquialisms"), QT_TR_NOOP("Manga slang"), QT_TR_NOOP("Slang"),
      QT_TR_NOOP("Martial arts terms"), QT_TR_NOOP("Idiomatic expressions"),
      QT_TR_NOOP("Archaisms"), QT_TR_NOOP("Obsolete terms"), QT_TR_NOOP("Obscure terms"),
      QT_TR_NOOP("Outdated kana usage"), QT_TR_NOOP("Abbreviations"),
      QT_TR_NOOP("Familiar language"), QT_TR_NOOP("Polite language"),
      QT_TR_NOOP("Humble language"), QT_TR_NOOP("Honorific language"),
      QT_TR_NOOP("Female terms or language"), QT_TR_NOOP("Male terms or language"),
      QT_TR_NOOP("Prefixes"), QT_TR_NOOP("Suffixes"), QT_TR_NOOP("Outdated kanji usage")};
  static_assert(std::size(category_labels) == core::kEdictCategoryTags.size());
  auto* categories = new QListWidget(dictionary);
  categories->setObjectName(QStringLiteral("settingsDictionaryCategories"));
  categories->setAccessibleName(tr("Exclude dictionary categories"));
  categories->setMinimumHeight(220);
  for (std::size_t i = 0; i < core::kEdictCategoryTags.size(); ++i) {
    auto* item = new QListWidgetItem(tr("%1 (%2)").arg(tr(category_labels[i]),
        to_qstring(core::kEdictCategoryTags[i])), categories);
    const auto bit = std::uint32_t{1} << (i + 4);
    item->setData(Qt::UserRole, bit);
    item->setCheckState((settings_.dictionary.category_exclusions & bit) ? Qt::Checked : Qt::Unchecked);
  }
  dictionary_form->addRow(tr("Exclude tagged senses:"), categories);
  auto* dictionary_note = new QLabel(tr("These settings apply to new searches. Existing queries and results stay unchanged. "
      "A checked category removes matching senses, not unrelated definitions. Mixed recognized tags keep their allowed types. "
      "Filtering stops at an unrecognized tag within a group. "
      "Unknown exclusion bits and other dictionary policies remain retained and disclosed."), dictionary);
  dictionary_note->setWordWrap(true);
  dictionary_note->setObjectName(QStringLiteral("settingsDictionaryNote"));
  dictionary_form->addRow(dictionary_note);
  auto* dictionary_scroll = new QScrollArea(tabs);
  dictionary_scroll->setObjectName(QStringLiteral("settingsDictionaryScroll"));
  dictionary_scroll->setFrameShape(QFrame::NoFrame);
  dictionary_scroll->setWidgetResizable(true);
  dictionary_scroll->setWidget(dictionary);
  tabs->addTab(dictionary_scroll, tr("Dictionary"));
  if (dictionary_page) tabs->setCurrentWidget(dictionary_scroll);
  auto* history = new QWidget(tabs);
  auto* history_form = new QFormLayout(history);
  auto* history_size = new QSpinBox(history);
  history_size->setObjectName(QStringLiteral("settingsHistorySize"));
  history_size->setRange(0, 30000);
  history_size->setValue(settings_.history_size);
  history_form->addRow(tr("Storage cells per history"), history_size);
  auto* conversion_choices = new QSpinBox(history);
  conversion_choices->setObjectName(QStringLiteral("settingsConversionChoices"));
  conversion_choices->setRange(10, 2000);
  conversion_choices->setValue(settings_.conversion_choices);
  history_form->addRow(tr("Learned conversion choices"), conversion_choices);
  auto* undo_levels = new QSpinBox(history);
  undo_levels->setObjectName(QStringLiteral("settingsUndoLevels"));
  undo_levels->setRange(3, 1000);
  undo_levels->setValue(settings_.maximum_undo_levels);
  history_form->addRow(tr("Native Japanese undo levels"), undo_levels);
  auto* undo_note = new QLabel(tr("Reducing this limit discards the oldest native undo/redo entries. Unicode editors retain Qt history. Finish active conversions before changing it."), history);
  undo_note->setWordWrap(true);
  history_form->addRow(undo_note);
  auto* save_histories = new QCheckBox(tr("Save query histories on exit"), history);
  save_histories->setObjectName(QStringLiteral("settingsSaveHistories"));
  save_histories->setChecked(settings_.save_histories);
  history_form->addRow(save_histories);
  booleans.push_back({save_histories, &ApplicationSettings::save_histories});
  auto* history_note = new QLabel(tr("Each dictionary, search and replace history has its own budget. "
      "The default 300 cells allow 31 entries and 267 text characters. "
      "Reducing the size drops older or oversized entries; zero disables retention.\n\n"
      "If reducing history may lose saved entries, automatic saving pauses until an explicit Save or Reload.\n\n"
      "Turning off automatic saving leaves an existing file unchanged. Tools > Query History "
      "provides explicit Save, Import, Reload and Clear commands. Find and Replace use their "
      "own independent history controls."), history);
  history_note->setWordWrap(true);
  history_form->addRow(history_note);
  tabs->addTab(history, tr("History"));
  auto* defaults_page = new QWidget(tabs);
  auto* defaults_form = new QFormLayout(defaults_page);
  auto* metric_units = new QCheckBox(tr("Display measurements in centimeters"), defaults_page);
  metric_units->setObjectName(QStringLiteral("settingsMetricUnits"));
  metric_units->setChecked(settings_.metric_units);
  defaults_form->addRow(metric_units);
  auto* line_width_mode = new QComboBox(defaults_page);
  line_width_mode->setObjectName(QStringLiteral("settingsLineWidthMode"));
  line_width_mode->addItem(tr("Follow the document window"),
                           static_cast<int>(LineWidthMode::kDynamic));
  line_width_mode->addItem(tr("Fixed character width"),
                           static_cast<int>(LineWidthMode::kFixed));
  line_width_mode->addItem(tr("Match the printed page"),
                           static_cast<int>(LineWidthMode::kPrinter));
  line_width_mode->setCurrentIndex(line_width_mode->findData(
      static_cast<int>(settings_.line_width_mode)));
  defaults_form->addRow(tr("Document line width"), line_width_mode);
  auto* fixed_line_width = new QSpinBox(defaults_page);
  fixed_line_width->setObjectName(QStringLiteral("settingsFixedLineWidth"));
  fixed_line_width->setRange(5, 1000);
  fixed_line_width->setSuffix(tr(" characters"));
  fixed_line_width->setValue(settings_.fixed_line_width);
  fixed_line_width->setEnabled(settings_.line_width_mode ==
                               LineWidthMode::kFixed);
  connect(line_width_mode, &QComboBox::currentIndexChanged, this,
          [line_width_mode, fixed_line_width] {
            fixed_line_width->setEnabled(
                line_width_mode->currentData().toInt() ==
                static_cast<int>(LineWidthMode::kFixed));
          });
  defaults_form->addRow(tr("Fixed line width"), fixed_line_width);
  auto* relax_punctuation = new QCheckBox(
      tr("Allow closing punctuation beyond the right margin"), defaults_page);
  relax_punctuation->setObjectName(QStringLiteral("settingsRelaxPunctuation"));
  relax_punctuation->setChecked(settings_.relax_margin_punctuation);
  defaults_form->addRow(relax_punctuation);
  auto* relax_small_kana = new QCheckBox(
      tr("Allow small kana beyond the right margin"), defaults_page);
  relax_small_kana->setObjectName(QStringLiteral("settingsRelaxSmallKana"));
  relax_small_kana->setChecked(settings_.relax_margin_small_kana);
  defaults_form->addRow(relax_small_kana);
  std::array<QDoubleSpinBox*, 4> default_margins{};
  const char* margin_labels[] = {"Left", "Right", "Top", "Bottom"};
  for (std::size_t i = 0; i < 4; ++i) {
    auto* spin = new QDoubleSpinBox(defaults_page);
    spin->setObjectName(QStringLiteral("settingsDefaultMargin%1").arg(i));
    spin->setRange(0, settings_.metric_units ? 10 * kCentimetersPerInch : 10);
    spin->setDecimals(8);
    spin->setSingleStep(0.1);
    spin->setSuffix(settings_.metric_units ? tr(" cm") : tr(" in"));
    spin->setValue(settings_.default_page.margins[i] *
                   (settings_.metric_units ? kCentimetersPerInch : 1));
    connect(spin, &QDoubleSpinBox::valueChanged, spin,
            [spin] { spin->setProperty("jwpqtChanged", true); });
    default_margins[i] = spin;
    defaults_form->addRow(tr(margin_labels[i]), spin);
  }
  connect(metric_units, &QCheckBox::toggled, this,
          [default_margins, displayed_metric = settings_.metric_units](bool metric) mutable {
            for (auto* margin : default_margins) {
              const double inches = displayed_metric
                                         ? margin->value() / kCentimetersPerInch
                                         : margin->value();
              const QSignalBlocker blocker(margin);
              margin->setRange(0, metric ? 10 * kCentimetersPerInch : 10);
              margin->setSuffix(metric ? ApplicationSettingsDialog::tr(" cm")
                                       : ApplicationSettingsDialog::tr(" in"));
              margin->setValue(metric ? inches * kCentimetersPerInch : inches);
            }
            displayed_metric = metric;
          });
  auto* default_landscape = new QCheckBox(tr("Landscape"), defaults_page);
  auto* default_vertical = new QCheckBox(tr("Vertical printing"), defaults_page);
  default_landscape->setObjectName(QStringLiteral("settingsDefaultLandscape"));
  default_vertical->setObjectName(QStringLiteral("settingsDefaultVertical"));
  default_landscape->setChecked(settings_.default_page.landscape);
  default_vertical->setChecked(settings_.default_page.vertical);
  defaults_form->addRow(default_landscape); defaults_form->addRow(default_vertical);
  auto* defaults_note = new QLabel(tr("Defaults apply to new Japanese documents. Existing documents keep their layout. Page Layout can copy defaults into the current document or stage its margins as new defaults."), defaults_page);
  defaults_note->setWordWrap(true); defaults_form->addRow(defaults_note);
  tabs->addTab(defaults_page, tr("Default Page"));
  auto* printing = new QWidget(tabs);
  auto* print_form = new QFormLayout(printing);
  auto* print_family = new QComboBox(printing);
  print_family->addItems(listed_families(settings_.show_all_fonts));
  print_family->setEditable(true);
  print_family->setObjectName(QStringLiteral("settingsPrintFamily"));
  print_family->setEditText(settings_.print_font.family);
  connect(show_all_fonts, &QCheckBox::toggled, this,
          [listed_families, print_family](bool checked) {
            const QString current = print_family->currentText();
            const QSignalBlocker blocker(print_family);
            print_family->clear();
            print_family->addItems(listed_families(checked));
            print_family->setEditText(current);
          });
  auto* print_size = new QDoubleSpinBox(printing);
  print_size->setObjectName(QStringLiteral("settingsPrintSize"));
  print_size->setRange(1, 144); print_size->setDecimals(1); print_size->setSingleStep(0.1);
  print_size->setSuffix(tr(" pt")); print_size->setValue(settings_.print_font.size / 10.0);
  auto* print_auto = new QCheckBox(tr("Use document font family"), printing);
  print_auto->setObjectName(QStringLiteral("settingsPrintAutomatic"));
  print_auto->setChecked(settings_.print_font.automatic);
  print_form->addRow(tr("Printer font"), print_family); print_form->addRow(tr("Physical size"), print_size);
  print_form->addRow(print_auto);
  auto* print_colors = new QCheckBox(tr("Print kanji-list colors (requires an active list mode)"), printing);
  print_colors->setObjectName(QStringLiteral("settingsPrintColors"));
  print_colors->setChecked(settings_.color_printing);
  print_form->addRow(print_colors);
  booleans.push_back({print_colors, &ApplicationSettings::color_printing});
  auto* print_justify = new QCheckBox(tr("Align ASCII runs to the Japanese print grid"), printing);
  print_justify->setObjectName(QStringLiteral("settingsPrintJustifyAscii"));
  print_justify->setChecked(settings_.print_formatting.justify_ascii);
  print_justify->setToolTip(tr("Adjust spacing before tabs and on wrapped lines, not at the final paragraph end. Native JWP tabs advance one Japanese cell."));
  print_form->addRow(print_justify);
  std::array<KanaInputField*, 4> print_patterns{};
  std::array<QString, 4> original_patterns{};
  std::array<QDoubleSpinBox*, 4> print_positions{};
  const auto pattern_page = settings_.translation_code_page ? static_cast<core::LegacyCodePage>(settings_.translation_code_page) : core::kDefaultLegacyCodePage;
  const char* pattern_labels[] = {"Date pattern", "Time pattern", "AM text", "PM text"};
  const char* position_labels[] = {"Header left extension", "Header right extension", "Header distance", "Footer distance"};
  for (std::size_t i = 0; i < 4; ++i) {
    auto* field = new KanaInputField(QStringLiteral("settingsPrintPattern%1").arg(i), printing);
    field->set_input_mode(InputMode::kAscii);
    field->set_overwrite_action(overwrite_action);
    const auto& raw = settings_.print_formatting.patterns[i];
    try {
      original_patterns[i] = to_qstring(core::decode_jwp_text(core::JwpText(raw.begin(), std::find(raw.begin(), raw.end(), 0)), pattern_page));
    } catch (const core::JwpTextCodecError&) {
      field->edit()->setReadOnly(true);
      field->edit()->setPlaceholderText(tr("Not displayable in this code page; original bytes retained"));
    }
    field->edit()->setText(original_patterns[i]);
    print_form->addRow(tr(pattern_labels[i]), field); print_patterns[i] = field;
  }
  for (std::size_t i = 0; i < 4; ++i) {
    auto* position = new QDoubleSpinBox(printing);
    position->setObjectName(QStringLiteral("settingsPrintPosition%1").arg(i));
    position->setRange(0, 10); position->setDecimals(2); position->setSingleStep(0.1);
    position->setValue(settings_.print_formatting.position[i] / 100.0);
    position->setSuffix(i < 2 ? tr(" characters") : tr(" lines"));
    print_form->addRow(tr(position_labels[i]), position); print_positions[i] = position;
  }
  auto* print_note = new QLabel(tr("Printing uses physical point sizes independently of screen zoom. "
      "Page Layout controls margins, headers and vertical glyphs. Preview and output use a frozen document snapshot. "
      "Vertical mode follows JWP: Japanese glyphs are rotated for quarter-turn reading of the paper. "
      "Patterns: &Y year, &y two digits, &M month, &D day, &H 24-hour, &h 12-hour, &N minutes, &A AM/PM, && ampersand. "
      "Legacy noon uses AM and midnight uses hour zero. Date/time allow 19 JWP characters; AM/PM allow 9."), printing);
  print_note->setWordWrap(true); print_form->addRow(print_note);
  auto* print_scroll = new QScrollArea(tabs); print_scroll->setWidgetResizable(true);
  print_scroll->setObjectName(QStringLiteral("settingsPrintScroll"));
  print_scroll->setFrameShape(QFrame::NoFrame); print_scroll->setWidget(printing);
  tabs->addTab(print_scroll, tr("Printing"));
  auto* lookup = new QWidget(tabs);
  auto* lookup_form = new QFormLayout(lookup);
  auto* lookup_auto = new QCheckBox(tr("Automatic kanji lookup"), lookup);
  lookup_auto->setObjectName(QStringLiteral("settingsLookupAutomatic"));
  lookup_auto->setChecked(settings_.automatic_kanji_lookup);
  auto* lookup_rare = new QCheckBox(tr("Show rare kanji last in radical results"), lookup);
  lookup_rare->setObjectName(QStringLiteral("settingsLookupRareLast"));
  lookup_rare->setChecked(settings_.rare_kanji_last);
  lookup_form->addRow(lookup_auto);
  lookup_form->addRow(lookup_rare);
  booleans.push_back({lookup_auto, &ApplicationSettings::automatic_kanji_lookup});
  booleans.push_back({lookup_rare, &ApplicationSettings::rare_kanji_last});
  const struct { const char* label; const char* object; bool ApplicationSettings::*member; } lookup_flags[] = {
      {"Match Nelson radicals", "settingsBushuNelson", &ApplicationSettings::bushu_nelson},
      {"Match classical radicals", "settingsBushuClassical", &ApplicationSettings::bushu_classical},
      {"Flexible kun-yomi matching", "settingsFlexibleKun", &ApplicationSettings::flexible_kun},
      {"Allow partial-word meanings", "settingsPartialMeanings", &ApplicationSettings::partial_meanings},
      {"Include SKIP miscodes", "settingsSkipMiscodes", &ApplicationSettings::skip_miscodes},
      {"Hide equivalent Stroke/Bushu and Spahn radical variants", "settingsReduceRadicals", &ApplicationSettings::reduce_radical_choices},
      {"Deemphasize source rare radical choices", "settingsDeemphasizeRadicals", &ApplicationSettings::deemphasize_rare_radicals}};
  for (const auto& flag : lookup_flags) {
    auto* check = new QCheckBox(tr(flag.label), lookup);
    check->setObjectName(QString::fromLatin1(flag.object));
    check->setChecked(settings_.*(flag.member));
    lookup_form->addRow(check);
    booleans.push_back({check, flag.member});
  }
  auto* index_type = new QComboBox(lookup);
  index_type->setObjectName(QStringLiteral("settingsIndexType"));
  for (auto* name : kKanjiIndexNames) index_type->addItem(tr(name));
  index_type->setCurrentIndex(settings_.index_type);
  index_type->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  index_type->setMinimumContentsLength(25);
  lookup_form->addRow(tr("Default index type"), index_type);
  auto* reading_type = new QComboBox(lookup);
  reading_type->setObjectName(QStringLiteral("settingsReadingType"));
  for (auto* name : kKanjiReadingNames) reading_type->addItem(tr(name));
  reading_type->setCurrentIndex(settings_.reading_type);
  lookup_form->addRow(tr("Default reading type"), reading_type);
  auto* lookup_note = new QLabel(tr("Auto is shared by radical and code lookup windows. "
      "Changing preferences does not run a search or reorder current results. "
      "Reading and Index lookup remain explicit searches."), lookup);
  lookup_note->setWordWrap(true);
  lookup_form->addRow(lookup_note);
  tabs->addTab(lookup, tr("Kanji Lookup"));
  tabs->addTab(colors, tr("Colors"));
  if (!settings_.unapplied.isEmpty()) {
    auto* retained = new QPlainTextEdit(tabs);
    retained->setReadOnly(true);
    retained->setPlainText(tr("These imported settings are retained, but not yet applied:\n\n") + settings_.unapplied.join(QLatin1Char('\n')));
    tabs->addTab(retained, tr("Retained Settings"));
  }
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  outer->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(buttons, &QDialogButtonBox::accepted, this,
          [this, booleans, font_controls, dictionary_controls, code_page, history_size, conversion_choices, undo_levels, categories,
           print_family, print_size, print_auto, print_justify, ascii_family, print_patterns, print_positions, original_patterns,
            index_type, reading_type, duplicate_open, clipboard_import,
            clipboard_export, auto_scroll_speed, default_margins, metric_units,
            line_width_mode, fixed_line_width, relax_punctuation,
            relax_small_kana,
           default_landscape, default_vertical,
           color_fields, original_colors, color_mode, uncommon] {
    auto next = settings_;
    for (std::size_t i = 0; i < 4; ++i)
      if (default_margins[i]->property("jwpqtChanged").toBool())
        next.default_page.margins[i] = static_cast<float>(
            default_margins[i]->value() /
            (metric_units->isChecked() ? kCentimetersPerInch : 1));
    next.metric_units = metric_units->isChecked();
    next.line_width_mode = static_cast<LineWidthMode>(
        line_width_mode->currentData().toInt());
    next.fixed_line_width = fixed_line_width->value();
    next.relax_margin_punctuation = relax_punctuation->isChecked();
    next.relax_margin_small_kana = relax_small_kana->isChecked();
    next.default_page.landscape = default_landscape->isChecked();
    next.default_page.vertical = default_vertical->isChecked();
    for (const auto& control : booleans) next.*(control.member) = control.widget->isChecked();
    for (const auto& control : dictionary_controls) next.dictionary.*(control.member) = control.widget->isChecked();
    next.dictionary.category_exclusions = 0;
    for (int i = 0; i < categories->count(); ++i)
      if (categories->item(i)->checkState() == Qt::Checked)
        next.dictionary.category_exclusions |= categories->item(i)->data(Qt::UserRole).toUInt();
    for (std::size_t i = 0; i < font_controls.size(); ++i) {
      const auto& control = font_controls[i];
      next.fonts[i].family = control.family->currentText();
      if (control.size) next.fonts[i].size = control.size->value();
      if (control.automatic) next.fonts[i].automatic = control.automatic->isChecked();
    }
    next.translation_code_page = code_page->currentData().toInt();
    next.history_size = history_size->value();
    next.conversion_choices = conversion_choices->value();
    next.maximum_undo_levels = undo_levels->value();
    next.auto_scroll_speed = auto_scroll_speed->value();
    next.duplicate_open = static_cast<DuplicateOpenBehavior>(duplicate_open->currentData().toInt());
    next.clipboard_import = static_cast<ClipboardTextFormat>(
        clipboard_import->currentData().toInt());
    next.clipboard_export = static_cast<ClipboardTextFormat>(
        clipboard_export->currentData().toInt());
    next.index_type = index_type->currentIndex();
    next.reading_type = reading_type->currentIndex();
    next.print_font = {print_family->currentText(), qRound(print_size->value() * 10), print_auto->isChecked()};
    next.ascii_font.family = ascii_family->currentText();
    next.print_formatting.justify_ascii = print_justify->isChecked();
    const QPointer<ApplicationSettingsDialog> self(this);
    try {
      for (std::size_t i = 0; i < 3; ++i) if (color_fields[i]->text() != original_colors[i] || color_fields[i]->property("jwpqtColorCleared").toBool()) {
        const auto text = color_fields[i]->text();
        if (text.isEmpty()) next.color_refs[i].reset();
        else {
          const QColor color(text);
          if (text.size() != 7 || !text.startsWith(QLatin1Char('#')) || !color.isValid())
            throw core::JwpConfigurationError("Color must be #RRGGBB");
          next.color_refs[i] = std::uint32_t(color.red()) | (std::uint32_t(color.green()) << 8) | (std::uint32_t(color.blue()) << 16);
        }
      }
      if (color_mode->currentData().toInt() < 0) next.color_kanji_mode.reset();
      else next.color_kanji_mode = color_mode->currentData().toInt();
      if (uncommon->currentData().toInt() < 0) next.colorize_rare.reset();
      else next.colorize_rare = uncommon->currentData().toInt() != 0;
      const auto encoding = next.translation_code_page ? static_cast<core::LegacyCodePage>(next.translation_code_page) : core::kDefaultLegacyCodePage;
      for (std::size_t i = 0; i < 4; ++i) {
        print_patterns[i]->finish_input();
        if (!self) return;
        const auto text = print_patterns[i]->edit()->text();
        if (text != original_patterns[i]) {
          const auto unicode = from_qstring(text);
          if (to_qstring(unicode) != text) throw core::JwpConfigurationError("Invalid Unicode in print pattern");
          const auto converted = core::encode_jwp_text(unicode, encoding);
          if (std::find(converted.begin(), converted.end(), 0) != converted.end())
            throw core::JwpConfigurationError("Print pattern contains a NUL character");
          auto& raw = next.print_formatting.patterns[i];
          if (converted.size() >= raw.size()) throw core::JwpConfigurationError("Print pattern exceeds its legacy capacity");
          std::copy(converted.begin(), converted.end(), raw.begin()); raw[converted.size()] = 0;
        }
        next.print_formatting.position[i] = qRound(print_positions[i]->value() * 100);
      }
      (void)write_application_settings(next);
      settings_ = std::move(next);
      accept();
    } catch (const std::exception& error) {
      if (!self) return;
      QMessageBox::warning(this, tr("Invalid settings"), QString::fromUtf8(error.what()));
    }
  });
}

}  // namespace jwpqt::qt
