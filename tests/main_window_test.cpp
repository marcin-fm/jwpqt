// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <QAction>
#include <QComboBox>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFontMetricsF>
#include <QFontDatabase>
#include <QLabel>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPageLayout>
#include <QPushButton>
#include <QPrinter>
#include <QSettings>
#include <QSpinBox>
#include <QStatusBar>
#include <QSignalBlocker>
#include <QScrollBar>
#include <QScrollArea>
#include <QTabWidget>
#include <QInputDialog>
#include <QInputMethodEvent>
#include <QPointer>
#include <QTextEdit>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextBlockFormat>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>

#include "file_io.h"
#include "edict_lookup_dialog.h"
#include "edict_results_window.h"
#include "edict_resources.h"
#include "edict_user_dictionary_dialog.h"
#include "jwp_editor.h"
#include "jwpqt/core/jis_unicode.h"
#include "jwpqt/core/jwp_text_codec.h"
#include "kanji_color_settings.h"
#include "kanji_count_dialog.h"
#include "kanji_info_dialog.h"
#include "kana_input_field.h"
#include "jis_table_dialog.h"
#include "main_window.h"
#include "application_settings_dialog.h"
#include "clipboard_mime.h"
#include <QTest>
#include "text_bridge.h"
#include "wnn_user_dictionary_dialog.h"

namespace {

void require(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

QByteArray read_bytes(const QString& path) {
  QFile file(path);
  require(file.open(QIODevice::ReadOnly), "Could not open saved test file");
  return file.readAll();
}

void write_bytes(const QString& path, const QByteArray& bytes) {
  QFile file(path);
  require(file.open(QIODevice::WriteOnly | QIODevice::Truncate),
          "Could not create binary test file");
  require(file.write(bytes) == bytes.size(),
          "Could not write complete binary test file");
}

struct WnnFixture {
  QString index_path;
  QString data_path;
  QString preferences_path;
};

WnnFixture write_wnn_fixture(const QString& directory) {
  const WnnFixture fixture{
      directory + QStringLiteral("/wnn.dix"),
      directory + QStringLiteral("/wnn.dat"),
      directory + QStringLiteral("/user.sel"),
  };
  QByteArray index;
  index.append(static_cast<char>(0xa2));
  index.append(static_cast<char>(0x80));
  index.append(static_cast<char>(0x80));
  index.append(static_cast<char>(0x77));
  index.append(4, '\0');
  QByteArray data;
  data.append(static_cast<char>(0xa2));
  data.append('*');
  data.append(static_cast<char>(0xb0));
  data.append(static_cast<char>(0xa1));
  data.append('/');
  data.append(static_cast<char>(0xb0));
  data.append(static_cast<char>(0xa2));
  data.append('\n');
  write_bytes(fixture.index_path, index);
  write_bytes(fixture.data_path, data);
  return fixture;
}

void append_u32_le(QByteArray& bytes, quint32 value) {
  for (int shift = 0; shift < 32; shift += 8) {
    bytes.append(static_cast<char>((value >> shift) & 0xffU));
  }
}

void append_wnn_index_entry(QByteArray& index, unsigned char first,
                            unsigned char second, quint32 offset) {
  index.append(static_cast<char>(first));
  index.append(static_cast<char>(second));
  index.append(static_cast<char>(0x80));
  index.append(static_cast<char>(0x77));
  append_u32_le(index, offset);
}

WnnFixture write_automatic_wnn_fixture(const QString& directory) {
  const WnnFixture fixture{
      directory + QStringLiteral("/automatic.dix"),
      directory + QStringLiteral("/automatic.dat"),
      directory + QStringLiteral("/automatic-user.sel"),
  };
  QByteArray data;
  data.append(static_cast<char>(0xab));
  data.append('*');
  data.append(static_cast<char>(0xb0));
  data.append(static_cast<char>(0xa1));
  data.append('/');
  data.append(static_cast<char>(0xb0));
  data.append(static_cast<char>(0xa2));
  data.append('\n');
  const quint32 second_offset = static_cast<quint32>(data.size());
  data.append(static_cast<char>(0xab));
  data.append(static_cast<char>(0xad));
  data.append('*');
  data.append(static_cast<char>(0xb0));
  data.append(static_cast<char>(0xa3));
  data.append('/');
  data.append(static_cast<char>(0xb0));
  data.append(static_cast<char>(0xa4));
  data.append('\n');

  QByteArray index;
  append_wnn_index_entry(index, 0xab, 0x80, 0);
  append_wnn_index_entry(index, 0xab, 0xad, second_offset);
  write_bytes(fixture.index_path, index);
  write_bytes(fixture.data_path, data);
  return fixture;
}

class PromptingWindow : public jwpqt::qt::MainWindow {
 public:
  std::optional<jwpqt::core::TextEncoding> next_encoding;
  std::optional<jwpqt::qt::SearchRequest> next_search;
  std::optional<jwpqt::qt::ReplaceRequest> next_replace;
  std::optional<jwpqt::core::JwpParagraphFormat> next_paragraph_format;
  std::optional<jwpqt::core::JwpParagraphFormat> offered_paragraph_format;
  std::optional<jwpqt::core::JwpDocument> next_page_layout;
  std::optional<jwpqt::core::JwpDocument> offered_page_layout;
  std::optional<jwpqt::core::KanjiColorPolicy> next_kanji_color_policy;
  std::optional<jwpqt::core::KanjiColorPolicy> offered_kanji_color_policy;
  std::optional<jwpqt::qt::KanjiColorListEditRequest>
      next_kanji_color_list_edit;
  std::vector<jwpqt::core::TextEncoding> offered_encodings;
  QString explanation;
  int search_prompt_count = 0;
  int replace_prompt_count = 0;
  int paragraph_format_prompt_count = 0;
  int page_layout_prompt_count = 0;
  int print_prompt_count = 0;
  int printer_setup_prompt_count = 0;
  bool accept_print_prompt = true;
  bool accept_printer_setup_prompt = true;
  bool printer_setup_landscape = false;
  QString print_output_path;
  int kanji_color_prompt_count = 0;
  int kanji_color_list_prompt_count = 0;

 protected:
  std::optional<jwpqt::core::TextEncoding> prompt_for_encoding(
      const std::vector<jwpqt::core::TextEncoding>& candidates,
      const QString& prompt) override {
    offered_encodings = candidates;
    explanation = prompt;
    return next_encoding;
  }

  std::optional<jwpqt::qt::SearchRequest> prompt_for_search(
      const jwpqt::qt::SearchRequest&) override {
    ++search_prompt_count;
    return next_search;
  }

  std::optional<jwpqt::qt::ReplaceRequest> prompt_for_replace(
      const jwpqt::qt::ReplaceRequest&) override {
    ++replace_prompt_count;
    return next_replace;
  }

  std::optional<jwpqt::core::JwpParagraphFormat>
  prompt_for_paragraph_format(
      const jwpqt::core::JwpParagraphFormat& initial) override {
    ++paragraph_format_prompt_count;
    offered_paragraph_format = initial;
    return next_paragraph_format;
  }

  std::optional<jwpqt::core::JwpDocument> prompt_for_page_layout(
      const jwpqt::core::JwpDocument& initial) override {
    ++page_layout_prompt_count;
    offered_page_layout = initial;
    return next_page_layout;
  }

  bool prompt_for_print(QPrinter& printer) override {
    ++print_prompt_count;
    if (!accept_print_prompt)
      return false;
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(print_output_path);
    return true;
  }

  bool prompt_for_printer_setup(QPrinter& printer) override {
    ++printer_setup_prompt_count;
    if (accept_printer_setup_prompt) {
      QPageLayout layout = printer.pageLayout();
      layout.setOrientation(printer_setup_landscape ? QPageLayout::Landscape
                                                    : QPageLayout::Portrait);
      (void)printer.setPageLayout(layout);
    }
    return accept_printer_setup_prompt;
  }

  std::optional<jwpqt::core::KanjiColorPolicy>
  prompt_for_kanji_color_policy(
      const jwpqt::core::KanjiColorPolicy& initial) override {
    ++kanji_color_prompt_count;
    offered_kanji_color_policy = initial;
    return next_kanji_color_policy;
  }

  std::optional<jwpqt::qt::KanjiColorListEditRequest>
  prompt_for_kanji_color_list_edit() override {
    ++kanji_color_list_prompt_count;
    return next_kanji_color_list_edit;
  }
};

QAction* find_encoding_action(jwpqt::qt::MainWindow& window,
                              const QString& name) {
  for (QAction* action : window.findChildren<QAction*>()) {
    if (action->text() == name) {
      return action;
    }
  }
  return nullptr;
}

QAction* find_action(jwpqt::qt::MainWindow& window, const char* name) {
  return window.findChild<QAction*>(QString::fromLatin1(name));
}

void send_text_key(QTextEdit* editor, int key, const QString& text,
                   Qt::KeyboardModifiers modifiers = Qt::NoModifier);

jwpqt::core::JwpParagraph paragraph(std::u32string_view text,
                                    std::int16_t line_spacing = 100) {
  jwpqt::core::JwpParagraph result;
  result.text = jwpqt::core::encode_jwp_text(text);
  result.line_spacing = line_spacing;
  return result;
}

jwpqt::core::JwpDocument sample_jwp_document() {
  jwpqt::core::JwpDocument document;
  document.margins = {1.0F, 1.25F, 1.5F, 1.75F};
  document.landscape = true;
  document.summary[0] = jwpqt::core::encode_jwp_text(U"Sample title");
  document.headers[0][0] = jwpqt::core::encode_jwp_text(U"Header");
  document.paragraphs = {paragraph(U"A\u65e5\u672c", 125),
                         paragraph(U"\u00e9", 150)};
  document.paragraphs[0].first_indent = -2;
  document.paragraphs[0].left_indent = 3;
  document.paragraphs[0].right_indent = 4;
  return document;
}

void require_jwp_layout(QTextEdit* editor,
                        const jwpqt::core::JwpDocument& document) {
  require(editor != nullptr &&
              editor->document()->blockCount() ==
                  static_cast<int>(document.paragraphs.size()),
          "JWP editor layout has the wrong paragraph count");
  const QFontMetricsF metrics(editor->font());
  const qreal ideographic_space = metrics.horizontalAdvance(QChar(0x3000));
  const qreal unit =
      ideographic_space > 0.0 ? ideographic_space : metrics.height();
  QTextBlock block = editor->document()->begin();
  bool follows_page_break = false;
  for (const jwpqt::core::JwpParagraph& paragraph : document.paragraphs) {
    const QTextBlockFormat format = block.blockFormat();
    require(std::abs(format.leftMargin() - paragraph.left_indent * unit) <
                    0.01 &&
                std::abs(format.rightMargin() - paragraph.right_indent * unit) <
                    0.01 &&
                std::abs(format.textIndent() - paragraph.first_indent * unit) <
                    0.01,
            "JWP editor did not retain paragraph indents");
    require(format.lineHeightType() ==
                    QTextBlockFormat::ProportionalHeight &&
                std::abs(format.lineHeight() -
                         std::max<std::int16_t>(paragraph.line_spacing, 1)) <
                    0.01,
            "JWP editor did not retain paragraph line spacing");
    require(format.property(jwpqt::qt::JwpEditor::kPageBreakProperty)
                    .toBool() == paragraph.page_break,
            "JWP editor did not retain hard-page-break metadata");
    const bool starts_new_page =
        (format.pageBreakPolicy() & QTextFormat::PageBreak_AlwaysBefore) != 0;
    require(starts_new_page == follows_page_break,
            "JWP editor did not retain hard-page-break layout policy");
    follows_page_break = paragraph.page_break;
    block = block.next();
  }
}

void test_new_document_workflow(const QString& directory) {
  jwpqt::qt::MainWindow window;
  QTextEdit* editor = window.findChild<QTextEdit*>();
  require(editor != nullptr && window.is_jwp_document() &&
              window.current_path().isEmpty() && !window.document_modified() &&
              window.current_jwp_document()->paragraphs.size() == 1 &&
              window.current_jwp_document()->margins[0] == 1.0F,
          "Startup did not create a clean native Japanese document");
  for (const char* name : {"kanaInputAction", "formatFileAction",
                           "formatParagraphAction", "pageLayoutAction",
                           "insertPageBreakAction", "jisTableAction",
                           "kanjiCountAction"}) {
    const QAction* action = find_action(window, name);
    require(action != nullptr && action->isEnabled(),
            "A document-only command is unavailable at startup");
  }
  require(!find_action(window, "undoAction")->isEnabled(),
          "New document started with stale undo history");

  find_action(window, "kanaInputAction")->setChecked(true);
  send_text_key(editor, Qt::Key_A, QStringLiteral("a"));
  require(editor->toPlainText() == QStringLiteral("\u3042") &&
              window.document_modified(),
          "Fresh document cannot compose kana");
  const WnnFixture fixture = write_wnn_fixture(directory);
  require(window.load_wnn_resources(fixture.index_path, fixture.data_path,
                                   fixture.preferences_path),
          "Could not load startup conversion fixture");
  editor->selectAll();
  require(window.convert_selection() && window.accept_conversion(),
          "Fresh document cannot convert kana");
  require(window.insert_edict_text(U"\u65e5"),
          "Fresh document cannot insert dictionary text");
  const auto saved = *window.current_jwp_document();
  find_action(window, "undoAction")->trigger();
  require(*window.current_jwp_document() != saved,
          "Fresh document insertion did not support undo");
  find_action(window, "redoAction")->trigger();
  require(*window.current_jwp_document() == saved,
          "Fresh document insertion did not support redo");
  const QString path = directory + QStringLiteral("/new-document.jwp");
  require(window.save_path(path) && !window.document_modified() &&
              jwpqt::qt::read_jwp_file(path) == saved,
          "Fresh Japanese document did not save correctly");

  find_action(window, "newTextDocumentAction")->trigger();
  editor = window.active_editor();
  require(!window.is_jwp_document() && !window.document_modified() &&
              window.document_count() == 2 &&
              window.current_path().isEmpty() &&
              !find_action(window, "kanaInputAction")->isEnabled(),
          "Explicit New Text did not create an independent Unicode document");
  const QString unicode = QString::fromUtf8("\xf0\x9f\x98\x80");
  editor->insertPlainText(unicode);
  const QString text_path = directory + QStringLiteral("/new-text.txt");
  require(window.save_path(text_path) && read_bytes(text_path) == unicode.toUtf8(),
          "Explicit New Text lost supplementary Unicode");
  editor->insertPlainText(QStringLiteral("unsaved"));
  const auto* unicode_editor = editor;
  find_action(window, "newDocumentAction")->trigger();
  require(window.document_count() == 3 && window.is_jwp_document() &&
              window.active_editor()->toPlainText().isEmpty() &&
              unicode_editor->toPlainText() == unicode + QStringLiteral("unsaved"),
          "New tab discarded another document's unsaved Unicode");
  require(window.activate_document(1), "Could not reactivate Unicode tab");
  QTimer::singleShot(0, [] {
    auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    if (prompt != nullptr) prompt->button(QMessageBox::Cancel)->click();
  });
  find_action(window, "closeDocumentAction")->trigger();
  require(!window.is_jwp_document() && window.document_modified() &&
              window.document_count() == 3 &&
              editor->toPlainText() == unicode + QStringLiteral("unsaved"),
          "Cancelled Close discarded unsaved Unicode text");
  require(window.save_path(text_path), "Could not save before New");
  require(window.activate_document(2), "Could not reactivate new Japanese tab");
  editor = window.active_editor();
  require(window.is_jwp_document() && window.current_path().isEmpty() &&
              !window.document_modified() && editor->toPlainText().isEmpty() &&
              !find_action(window, "undoAction")->isEnabled(),
          "New did not reset the document and history");
  require(window.open_jwp_path(path) && *window.current_jwp_document() == saved,
          "Could not reopen a document created through New");
}

void test_duplicate_open_policy(const QString& directory) {
  using jwpqt::core::TextEncoding;
  using jwpqt::qt::ApplicationSettings;
  using jwpqt::qt::DuplicateOpenBehavior;
  using jwpqt::qt::MainWindow;
  using jwpqt::qt::OpenMode;

  const QString path = directory + QStringLiteral("/duplicate-open.txt");
  write_bytes(path, QByteArray("first"));
  MainWindow window;
  require(window.open_path(path, TextEncoding::kUtf8, OpenMode::kNonInteractive),
          "Could not open duplicate-policy fixture");
  QPointer<jwpqt::qt::JwpEditor> original = window.active_editor();

  ApplicationSettings settings = window.application_settings();
  jwpqt::qt::ApplicationSettingsDialog cancelled(settings);
  auto* cancelled_choice = cancelled.findChild<QComboBox*>(QStringLiteral("settingsDuplicateOpen"));
  require(cancelled_choice != nullptr, "Duplicate-open Options control is missing");
  cancelled_choice->setCurrentIndex(
      cancelled_choice->findData(static_cast<int>(DuplicateOpenBehavior::kOpenAnother)));
  cancelled.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Cancel)->click();
  require(cancelled.settings().duplicate_open == DuplicateOpenBehavior::kPrompt,
          "Cancelling Options changed duplicate-open behavior");
  jwpqt::qt::ApplicationSettingsDialog options(settings);
  auto* choice = options.findChild<QComboBox*>(QStringLiteral("settingsDuplicateOpen"));
  require(choice != nullptr, "Duplicate-open Options control is missing");
  choice->setCurrentIndex(choice->findData(
      static_cast<int>(DuplicateOpenBehavior::kOpenAnother)));
  options.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
  settings = options.settings();
  require(window.apply_application_settings(settings) &&
              window.open_path(path, TextEncoding::kUtf8, OpenMode::kInteractive, true),
          "Open-another policy did not open the duplicate");
  QPointer<jwpqt::qt::JwpEditor> duplicate = window.active_editor();
  require(window.document_count() == 2 && duplicate && duplicate != original &&
              duplicate->toPlainText() == QStringLiteral("first"),
          "Duplicate did not own an independent editor with the requested encoding");
  duplicate->selectAll();
  duplicate->insertPlainText(QStringLiteral("copy"));
  require(window.save_path(path) &&
              jwpqt::qt::read_text_file(path, TextEncoding::kUtf8).text == U"copy" &&
              original->toPlainText() == QStringLiteral("first"),
          "A duplicate could not save its own source or mutated the other buffer");

  settings.duplicate_open = DuplicateOpenBehavior::kPrompt;
  require(window.apply_application_settings(settings) && window.activate_document(1),
          "Could not prepare duplicate-open prompt");
  QTimer::singleShot(0, [] {
    auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    require(prompt != nullptr, "Duplicate-open prompt was not shown");
    auto* reload = prompt->findChild<QPushButton*>(QStringLiteral("duplicateReloadButton"));
    require(reload != nullptr, "Duplicate-open Reload choice is missing");
    reload->click();
  });
  require(window.open_path(path, TextEncoding::kUtf16Be, OpenMode::kInteractive, true) &&
              window.document_count() == 2 && window.active_editor() == original &&
              original->toPlainText() == QStringLiteral("copy") &&
              window.text_encoding() == TextEncoding::kUtf8,
          "Reload choice did not retain the existing format and replace its disk snapshot");

  QTimer::singleShot(0, [] {
    auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    require(prompt != nullptr && prompt->button(QMessageBox::Cancel) != nullptr,
            "Duplicate-open Cancel choice is missing");
    prompt->button(QMessageBox::Cancel)->click();
  });
  require(!window.open_path(path, TextEncoding::kUtf8, OpenMode::kInteractive, true) &&
              window.document_count() == 2 && window.active_editor() == original,
          "Cancelling duplicate open changed the workspace");

  QTimer::singleShot(0, [] {
    auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    require(prompt != nullptr, "Duplicate-open prompt was not shown for another copy");
    auto* another = prompt->findChild<QPushButton*>(QStringLiteral("duplicateOpenAnotherButton"));
    require(another != nullptr, "Duplicate-open Another choice is missing");
    another->click();
  });
  require(window.open_path(path, TextEncoding::kUtf8, OpenMode::kInteractive, true) &&
              window.document_count() == 3 && window.active_editor() != original,
          "Prompted Open Another did not create a document");

  settings.duplicate_open = DuplicateOpenBehavior::kActivateExisting;
  require(window.apply_application_settings(settings) &&
              window.open_path(path, TextEncoding::kUtf16Le, OpenMode::kInteractive, true) &&
              window.document_count() == 3 && window.active_editor() == original &&
              window.text_encoding() == TextEncoding::kUtf8,
          "Change policy did not activate the original document with its retained format");

  settings.duplicate_open = DuplicateOpenBehavior::kOpenAnother;
  require(window.apply_application_settings(settings) && window.activate_document(2) &&
              window.open_path(path, TextEncoding::kUtf16Le, OpenMode::kNonInteractive, true) &&
              window.document_count() == 3 && window.active_editor() == original,
          "Noninteractive duplicate open stopped using deterministic activation");

  const QString settings_path = directory + QStringLiteral("/duplicate-open.cfg");
  jwpqt::qt::write_application_settings_file(settings_path, settings);
  MainWindow restarted;
  require(restarted.load_application_settings(settings_path) &&
              restarted.application_settings().duplicate_open ==
                  DuplicateOpenBehavior::kOpenAnother,
          "Duplicate-open policy did not survive settings restart");

  const QString project_document = directory + QStringLiteral("/duplicate-project.txt");
  const QString project_path = directory + QStringLiteral("/duplicate-open.jpr");
  write_bytes(project_document, QByteArray("project"));
  MainWindow project;
  require(project.open_path(project_document, TextEncoding::kUtf8,
                            OpenMode::kNonInteractive) &&
              project.apply_application_settings(settings) &&
              project.save_project_path(project_path, true, OpenMode::kNonInteractive),
          "Could not save duplicate-open project settings");
  MainWindow restored;
  require(restored.open_project_path(project_path, {}, OpenMode::kNonInteractive) &&
              restored.application_settings().duplicate_open ==
                  DuplicateOpenBehavior::kOpenAnother,
          "Duplicate-open policy did not survive JPR restoration");
}

void test_document_line_width_policy(const QString& directory) {
  using jwpqt::qt::ApplicationSettingsDialog;
  using jwpqt::qt::LineWidthMode;
  using jwpqt::qt::MainWindow;
  using jwpqt::qt::OpenMode;

  auto portrait = sample_jwp_document();
  portrait.margins[0] = 0.25F;
  portrait.margins[2] = 0.25F;
  auto narrow = portrait;
  narrow.margins[0] = 2.0F;
  narrow.margins[2] = 2.0F;
  const QString portrait_path = directory + QStringLiteral("/line-width.jwp");
  const QString narrow_path = directory + QStringLiteral("/line-width-narrow.jwp");
  const QString text_path = directory + QStringLiteral("/line-width.txt");
  jwpqt::qt::write_jwp_file(portrait_path, portrait);
  jwpqt::qt::write_jwp_file(narrow_path, narrow);
  write_bytes(text_path, QByteArray("Unicode"));

  MainWindow window;
  require(window.open_jwp_path(portrait_path, jwpqt::core::kDefaultLegacyCodePage,
                               OpenMode::kNonInteractive),
          "Could not open line-width fixture");
  const auto original = *window.current_jwp_document();
  auto settings = window.application_settings();
  require(settings.line_width_mode == LineWidthMode::kDynamic &&
              settings.fixed_line_width == 35 &&
              settings.relax_margin_punctuation &&
              settings.relax_margin_small_kana &&
              !window.active_editor()->configured_character_line_width().has_value(),
          "Document line-width or margin defaults differ from the source");

  ApplicationSettingsDialog cancelled(settings);
  auto* cancelled_mode =
      cancelled.findChild<QComboBox*>(QStringLiteral("settingsLineWidthMode"));
  auto* cancelled_width =
      cancelled.findChild<QSpinBox*>(QStringLiteral("settingsFixedLineWidth"));
  auto* cancelled_punctuation =
      cancelled.findChild<QCheckBox*>(QStringLiteral("settingsRelaxPunctuation"));
  auto* cancelled_small_kana =
      cancelled.findChild<QCheckBox*>(QStringLiteral("settingsRelaxSmallKana"));
  require(cancelled_mode != nullptr && cancelled_width != nullptr &&
              cancelled_punctuation != nullptr && cancelled_small_kana != nullptr,
          "Document line-width or margin Options controls are missing");
  cancelled_mode->setCurrentIndex(cancelled_mode->findData(
      static_cast<int>(LineWidthMode::kFixed)));
  cancelled_width->setValue(12);
  cancelled_punctuation->setChecked(false);
  cancelled_small_kana->setChecked(false);
  cancelled.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Cancel)->click();
  require(cancelled.settings().line_width_mode == LineWidthMode::kDynamic &&
              cancelled.settings().fixed_line_width == 35 &&
              cancelled.settings().relax_margin_punctuation &&
              cancelled.settings().relax_margin_small_kana,
          "Cancelling Options changed document line or margin policy");

  ApplicationSettingsDialog options(settings);
  auto* mode = options.findChild<QComboBox*>(QStringLiteral("settingsLineWidthMode"));
  auto* width = options.findChild<QSpinBox*>(QStringLiteral("settingsFixedLineWidth"));
  auto* punctuation =
      options.findChild<QCheckBox*>(QStringLiteral("settingsRelaxPunctuation"));
  auto* small_kana =
      options.findChild<QCheckBox*>(QStringLiteral("settingsRelaxSmallKana"));
  require(mode != nullptr && width != nullptr && punctuation != nullptr &&
              small_kana != nullptr && !width->isEnabled(),
          "Document line-width or margin controls are incomplete");
  mode->setCurrentIndex(mode->findData(static_cast<int>(LineWidthMode::kFixed)));
  require(width->isEnabled(), "Fixed mode did not enable its width control");
  width->setValue(12);
  punctuation->setChecked(false);
  small_kana->setChecked(false);
  options.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
  settings = options.settings();
  require(window.apply_application_settings(settings) &&
              !settings.relax_margin_punctuation &&
              !settings.relax_margin_small_kana &&
              window.active_editor()->configured_character_line_width() == 12 &&
              window.active_editor()->character_page_width() == 12 &&
              *window.current_jwp_document() == original &&
              !window.document_modified(),
          "Fixed line width changed the document or failed to reflow it");
  const QString converted_text = directory + QStringLiteral("/line-width-converted.txt");
  const QString converted_jwp = directory + QStringLiteral("/line-width-converted.jwp");
  require(window.save_as_path(converted_text, jwpqt::core::TextEncoding::kUtf8,
                              true, false, OpenMode::kNonInteractive) &&
              !window.active_editor()->configured_character_line_width().has_value() &&
              window.save_as_path(converted_jwp, std::nullopt, true, false,
                                  OpenMode::kNonInteractive) &&
              window.active_editor()->configured_character_line_width() == 12,
          "Changing a document container did not update its line-width policy");

  settings.line_width_mode = LineWidthMode::kPrinter;
  require(window.apply_application_settings(settings),
          "Could not apply printer-based line width");
  const auto portrait_width =
      window.active_editor()->configured_character_line_width();
  require(portrait_width.has_value() && *portrait_width > 0 &&
              *portrait_width <= 1000,
          "Printer-based line width was not derived from the page");
  require(window.open_jwp_path(narrow_path, jwpqt::core::kDefaultLegacyCodePage,
                               OpenMode::kNonInteractive, true),
          "Could not open narrow printer-width fixture");
  const auto narrow_width = window.active_editor()->configured_character_line_width();
  require(narrow_width.has_value() && *narrow_width < *portrait_width,
          "Printer-based line width ignored document margins");

  require(window.open_path(text_path, jwpqt::core::TextEncoding::kUtf8,
                           OpenMode::kNonInteractive, true) &&
              !window.active_editor()->configured_character_line_width().has_value() &&
              window.active_editor()->lineWrapMode() == QTextEdit::WidgetWidth,
          "JWP line-width policy changed an unrestricted Unicode document");

  settings.line_width_mode = LineWidthMode::kFixed;
  settings.fixed_line_width = 19;
  const QString settings_path = directory + QStringLiteral("/line-width.cfg");
  jwpqt::qt::write_application_settings_file(settings_path, settings);
  MainWindow restarted;
  require(restarted.load_application_settings(settings_path) &&
              restarted.open_jwp_path(portrait_path, jwpqt::core::kDefaultLegacyCodePage,
                                      OpenMode::kNonInteractive) &&
              restarted.active_editor()->configured_character_line_width() == 19,
          "Document line width did not survive settings restart");

  const QString project_path = directory + QStringLiteral("/line-width.jpr");
  require(restarted.save_project_path(project_path, true, OpenMode::kNonInteractive),
          "Could not save line-width project settings");
  MainWindow restored;
  require(restored.open_project_path(project_path, {}, OpenMode::kNonInteractive) &&
              restored.application_settings().line_width_mode == LineWidthMode::kFixed &&
              restored.application_settings().fixed_line_width == 19 &&
              !restored.application_settings().relax_margin_punctuation &&
              !restored.application_settings().relax_margin_small_kana &&
              restored.active_editor()->configured_character_line_width() == 19,
          "Document line width or margin policy did not survive JPR restoration");
}

void test_document_tabs(const QString& directory) {
  using jwpqt::qt::OpenMode;
  using jwpqt::core::TextEncoding;
  jwpqt::qt::MainWindow window;
  window.show();
  const QString native_path = directory + QStringLiteral("/tab-&native.jwp");
  const QString unicode_path = directory + QStringLiteral("/tab-unicode.txt");
  auto source = sample_jwp_document();
  for (int i = 0; i < 80; ++i) source.paragraphs.push_back(paragraph(U"row"));
  jwpqt::qt::write_jwp_file(native_path, source);
  jwpqt::qt::write_text_file(unicode_path, {U"Unicode \U0001f600", TextEncoding::kUtf16Le, true});
  require(window.open_jwp_path(native_path), "Could not load first tab");
  QPointer<jwpqt::qt::JwpEditor> native = window.active_editor();
  native->moveCursor(QTextCursor::End);
  native->insertPlainText(QStringLiteral("X"));
  const auto edited = *window.current_jwp_document();
  QTextCursor selection = native->textCursor();
  selection.setPosition(1);
  selection.setPosition(3, QTextCursor::KeepAnchor);
  native->setTextCursor(selection);
  QApplication::processEvents();
  native->verticalScrollBar()->setValue(native->verticalScrollBar()->maximum() / 2);
  const int scroll = native->verticalScrollBar()->value();
  require(window.open_path(unicode_path, TextEncoding::kUtf16Le,
                            OpenMode::kNonInteractive, true), "Could not append Unicode tab");
  QPointer<jwpqt::qt::JwpEditor> unicode = window.active_editor();
  require(window.document_count() == 2 && window.current_document_index() == 1 &&
              unicode != native && !window.is_jwp_document(), "Text tab did not own an independent editor");
  unicode->moveCursor(QTextCursor::End);
  unicode->insertPlainText(QStringLiteral("Y"));
  auto* tabs = window.findChild<QTabWidget*>(QStringLiteral("documentTabs"));
  require(tabs && tabs->tabText(0).endsWith(QStringLiteral(" *")) &&
              tabs->tabText(0).contains(QStringLiteral("&&native")) &&
              tabs->tabText(1).endsWith(QStringLiteral(" *")), "Tab modified markers are incorrect");
  require(window.activate_document(0) && window.active_editor() == native &&
              *window.current_jwp_document() == edited &&
              native->textCursor().anchor() == 1 && native->textCursor().position() == 3 &&
              native->verticalScrollBar()->value() == scroll &&
              find_action(window, "copyAction")->isEnabled(),
          "Tab activation lost native metadata, selection, scroll or action state");
  require_jwp_layout(native, edited);
  const auto prior_bytes = read_bytes(unicode_path);
  require(!window.save_as_path(unicode_path, TextEncoding::kUtf8, true) &&
              read_bytes(unicode_path) == prior_bytes && window.current_path() == native_path,
          "Save As overwrote another open document");
  const QString malformed = directory + QStringLiteral("/tab-invalid.txt");
  write_bytes(malformed, QByteArray::fromHex("fffe00d8"));
  require(!window.open_path_detected(malformed, OpenMode::kNonInteractive, true) &&
              window.document_count() == 2 && window.active_editor() == native &&
              *window.current_jwp_document() == edited,
          "Invalid tab open changed existing documents");
  find_action(window, "nextFileAction")->trigger();
  require(window.active_editor() == unicode && !find_action(window, "copyAction")->isEnabled(),
          "Next File did not restore Unicode action state");
  require(window.save_all_documents(OpenMode::kNonInteractive) &&
              window.active_editor() == unicode && !window.document_modified() &&
              jwpqt::qt::read_jwp_file(native_path) == edited &&
              jwpqt::qt::read_text_file(unicode_path, TextEncoding::kUtf16Le).text == U"Unicode \U0001f600Y",
          "Save All lost an encoding, document or active tab");
  find_action(window, "undoAction")->trigger();
  require(unicode->toPlainText().toStdU32String() == U"Unicode \U0001f600" && window.document_modified(),
          "Unicode tab lost its Qt undo history");
  require(window.activate_document(0), "Could not switch back to native undo");
  find_action(window, "undoAction")->trigger();
  require(*window.current_jwp_document() == source && window.document_modified(),
          "Native tab lost its portable undo history");
  find_action(window, "redoAction")->trigger();
  require(*window.current_jwp_document() == edited && !window.document_modified(),
          "Native redo did not restore its own saved baseline");
  require(window.open_path_detected(native_path, OpenMode::kNonInteractive, true) &&
              window.document_count() == 2 && window.active_editor() == native,
          "Opening an existing path duplicated or reloaded its tab");
  find_action(window, "previousFileAction")->trigger();
  require(window.active_editor() == unicode, "Previous File did not wrap around");
  find_action(window, "redoAction")->trigger();
  require(!window.document_modified(), "Unicode redo did not restore its saved baseline");
  QTimer::singleShot(0, [] {
    auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
    require(dialog != nullptr, "Files chooser was not shown");
    dialog->setTextValue(dialog->comboBoxItems().front());
    dialog->accept();
  });
  find_action(window, "filesAction")->trigger();
  require(window.active_editor() == native, "Files chooser did not activate the selected document");

  native->moveCursor(QTextCursor::End);
  native->insertPlainText(QStringLiteral("discard A"));
  window.activate_document(1);
  unicode->moveCursor(QTextCursor::End);
  unicode->insertPlainText(QStringLiteral("cancel B"));
  int prompts = 0;
  QTimer timer;
  QObject::connect(&timer, &QTimer::timeout, [&] {
    auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    if (!prompt) return;
    prompt->button(prompts++ == 0 ? QMessageBox::Discard : QMessageBox::Cancel)->click();
  });
  timer.start(0);
  require(!window.close_all_documents(), "Close All ignored cancellation");
  timer.stop();
  require(prompts == 2 && window.document_count() == 2 && native && unicode &&
              native->toPlainText().endsWith(QStringLiteral("discard A")) &&
              unicode->toPlainText().endsWith(QStringLiteral("cancel B")),
          "Close All discarded an earlier tab before later cancellation");
  QTimer::singleShot(0, [] {
    auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    require(prompt != nullptr, "Exit did not inspect dirty background tabs");
    prompt->button(QMessageBox::Cancel)->click();
  });
  require(!window.close() && window.document_count() == 2,
          "Exit discarded dirty background documents");

  require(window.new_document_tab(false) == 2, "Could not create unnamed tab");
  QPointer<jwpqt::qt::JwpEditor> unnamed = window.active_editor();
  unnamed->insertPlainText(QStringLiteral("unsaved"));
  require(!window.save_all_documents(OpenMode::kNonInteractive) &&
              window.active_editor() == unnamed && window.document_count() == 3 &&
              window.document_modified(), "Save All silently skipped an unnamed tab");
  require(!window.close_all_documents(OpenMode::kNonInteractive) &&
              window.document_count() == 3, "Noninteractive Close All discarded changes");
  QTimer::singleShot(0, [] {
    auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    require(prompt != nullptr, "Closing unnamed tab did not prompt");
    prompt->button(QMessageBox::Discard)->click();
  });
  require(window.close_document(2) && unnamed.isNull() && window.document_count() == 2,
          "Closing a tab did not release only its own editor");
  require(window.close_document(1, OpenMode::kNonInteractive) && unicode.isNull() &&
              window.active_editor() == native && window.document_count() == 1,
          "Closing Unicode tab damaged the remaining document");
  require(window.close_all_documents(OpenMode::kNonInteractive) &&
              window.document_count() == 1 && window.current_path().isEmpty() &&
              window.active_editor()->toPlainText().isEmpty() && window.is_jwp_document() &&
              !window.document_modified() && !find_action(window, "nextFileAction")->isEnabled(),
          "Closing all tabs did not leave a clean usable Japanese document");
}

void test_workspace_kanji_count() {
  using Window = jwpqt::qt::MainWindow;
  Window window;
  require(window.insert_edict_text(U"\u65e5\u65e5"), "Could not seed first count document");
  const auto first = *window.current_jwp_document();
  QPointer<QTextEdit> first_editor = window.active_editor();
  require(window.new_document_tab(false) == 1, "Could not create Unicode count document");
  QPointer<QTextEdit> unicode_editor = window.active_editor();
  unicode_editor->insertPlainText(QString::fromStdU32String(U"\U0001f600\u672c"));
  const QString unicode_text = unicode_editor->toPlainText();
  auto* action = find_action(window, "kanjiCountAction");
  require(action->isEnabled(), "Unicode document disabled read-only kanji counting");
  action->trigger();
  auto* dialog = dynamic_cast<jwpqt::qt::KanjiCountDialog*>(
      window.findChild<QDialog*>(QStringLiteral("kanjiCountDialog")));
  require(dialog != nullptr && dialog->results().size() == 1 &&
              dialog->results()[0].code == *jwpqt::core::unicode_to_jis_x0208(U'\u672c'),
          "Kanji count did not follow the current Unicode document");
  auto* all = dialog->findChild<QCheckBox*>(QStringLiteral("kanjiCountAllDocuments"));
  require(all->isEnabled(), "Multiple documents did not enable all-document counting");
  all->setChecked(true);
  require(dialog->count() && dialog->results().size() == 2 &&
              dialog->results()[0].count == 2 &&
              dialog->findChild<QLabel*>(QStringLiteral("kanjiCountStatus"))
                  ->text().contains(QStringLiteral("4 characters; 3 kanji (2 unique)")) &&
              unicode_editor->toPlainText() == unicode_text &&
              unicode_editor->document()->isUndoAvailable(),
          "All-document count changed Unicode input/history or returned wrong counts");
  QTextCursor edit = unicode_editor->textCursor();
  edit.beginEditBlock();
  edit.insertText(QStringLiteral("\u65e5"));
  edit.endEditBlock();
  unicode_editor->setTextCursor(edit);
  dialog->findChild<QPushButton*>(QStringLiteral("kanjiCountSearch"))->click();
  require(dialog->results()[0].count == 3,
          "Count button reused stale snapshots after an editor change");
  require(window.activate_document(0) && *window.current_jwp_document() == first,
          "Read-only count modified the native document");
  all->setChecked(false);
  require(dialog->count() && dialog->results().size() == 1 &&
              dialog->results()[0].count == 2,
          "Count button did not follow document activation");
  require(window.activate_document(1) &&
              window.view_kanji_color_list(jwpqt::qt::OpenMode::kNonInteractive) &&
              window.document_count() == 3 && unicode_editor != nullptr &&
              unicode_editor->toPlainText() == unicode_text + QStringLiteral("\u65e5"),
          "Color-list View replaced a modified source document");
  require(window.activate_document(1), "Could not return from color-list View");
  find_action(window, "undoAction")->trigger();
  require(unicode_editor->toPlainText() == unicode_text,
          "Color-list View or counting damaged Unicode undo");
  require(window.close_document(2, jwpqt::qt::OpenMode::kNonInteractive),
          "Could not close the empty color-list document");
  first_editor->document()->setModified(false);
  require(window.close_document(0, jwpqt::qt::OpenMode::kNonInteractive) &&
              first_editor == nullptr && dialog->count() && !all->isEnabled() &&
              dialog->results().size() == 1 && dialog->results()[0].count == 1,
          "Count retained a closed source or failed after its editor was deleted");
}

void test_tab_conversion_lifetimes(const QString& directory) {
  using jwpqt::qt::OpenMode;
  jwpqt::qt::MainWindow window;
  const auto fixture = write_wnn_fixture(directory);
  require(window.load_wnn_resources(fixture.index_path, fixture.data_path,
                                    fixture.preferences_path), "Could not load tab conversion resources");
  QPointer<jwpqt::qt::JwpEditor> first = window.active_editor();
  first->insertPlainText(QStringLiteral("\u3042"));
  first->selectAll();
  find_action(window, "kanjiCountAction")->trigger();
  auto* count_dialog = dynamic_cast<jwpqt::qt::KanjiCountDialog*>(
      window.findChild<QDialog*>(QStringLiteral("kanjiCountDialog")));
  require(count_dialog != nullptr, "Could not open count before conversion");
  require(window.convert_selection(), "Could not start first tab conversion");
  const QString glyph = first->toPlainText();
  require(!count_dialog->count() && window.conversion_active() &&
              first->toPlainText() == glyph,
          "Read-only count accepted or changed an active conversion preview");
  const QString invalid_text = directory + QStringLiteral("/tab-preview-invalid.txt");
  write_bytes(invalid_text, QByteArray::fromHex("fffe00d8"));
  jwpqt::core::JwpDocument unmapped;
  unmapped.paragraphs = {jwpqt::core::JwpParagraph{{0x81}}};
  const QString invalid_native = directory + QStringLiteral("/tab-preview-invalid.jwp");
  jwpqt::qt::write_jwp_file(invalid_native, unmapped);
  require(!window.open_path_detected(invalid_text, OpenMode::kNonInteractive, true) &&
              !window.open_jwp_path(invalid_native, jwpqt::core::kDefaultLegacyCodePage,
                                    OpenMode::kNonInteractive, true) &&
              window.document_count() == 1 && window.active_editor() == first &&
              window.conversion_active() && first->toPlainText() == glyph,
          "Rejected tab imports accepted or discarded an active preview");
  find_action(window, "kanjiInfoAction")->trigger();
  QPointer<QDialog> information = window.findChild<QDialog*>(QStringLiteral("kanjiInfoDialog"));
  require(information, "Could not inspect a tab's conversion candidate");
  require(window.new_document_tab() == 1 && !window.conversion_active() &&
              !first->isReadOnly() && first->toPlainText() == glyph,
          "Tab creation did not settle the shared conversion transaction");
  auto* second = window.active_editor();
  second->insertPlainText(QStringLiteral("\u3042"));
  second->selectAll();
  require(window.convert_selection(), "Shared conversion could not start in a second tab");
  const QString second_glyph = second->toPlainText();
  require(window.activate_document(0) && !window.conversion_active() && !second->isReadOnly(),
          "Tab activation left a shared conversion attached to the previous document");
  find_action(window, "undoAction")->trigger();
  require(first->toPlainText() == QStringLiteral("\u3042") && second->toPlainText() == second_glyph,
          "Undo followed the shared conversion session into another document");
  require(window.save_path(directory + QStringLiteral("/tab-info-origin.jwp")) &&
              window.close_document(0, OpenMode::kNonInteractive) && first.isNull() &&
              information && window.active_editor() == second,
          "Closing a source tab destroyed its independent information window");
  second->moveCursor(QTextCursor::End);
  auto* character = information->findChild<QLabel*>(QStringLiteral("kanjiInfoCharacter"));
  require(character != nullptr, "Information lost its character after origin closes");
  const QPoint center = character->rect().center();
  QMouseEvent insert(QEvent::MouseButtonDblClick, QPointF(center),
                     QPointF(character->mapToGlobal(center)), Qt::LeftButton,
                     Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(character, &insert);
  require(second->toPlainText() == second_glyph + glyph,
          "Modeless insertion used a closed editor rather than the active document");
  find_action(window, "undoAction")->trigger();
  require(second->toPlainText() == second_glyph, "Modeless insertion used the wrong tab's history");
  require(window.load_wnn_resources(fixture.index_path, fixture.data_path,
                                    fixture.preferences_path, OpenMode::kNonInteractive),
          "Settled tab transactions retained a stale shared dictionary reference");
}

void test_recent_file_workflow(const QString& directory) {
  using jwpqt::core::TextEncoding;
  using jwpqt::qt::OpenMode;
  constexpr auto mode = OpenMode::kNonInteractive;
  const QString history = directory + QStringLiteral("/recent-ui.json");
  const QString ascii = directory + QStringLiteral("/recent&a.txt");
  const QString unicode = directory + QStringLiteral("/recent-unmarked.txt");
  const QString jwp = directory + QStringLiteral("/recent-cyrillic.jwp");
  jwpqt::qt::write_text_file(ascii, {U"ASCII", TextEncoding::kOldJis, false});
  jwpqt::qt::write_text_file(unicode, {U"A\U0001f600", TextEncoding::kUtf16Be, false});
  auto native = sample_jwp_document();
  native.paragraphs.front().text = {0x80, 0x81};
  jwpqt::qt::write_jwp_file(jwp, native);
  PromptingWindow window;
  require(window.recent_documents().empty() &&
              !find_action(window, "clearRecentFilesAction")->isEnabled() &&
              window.load_recent_file_configuration(history) && !QFile::exists(history),
          "Recent history was not initially empty and read-only");
  require(window.open_path(ascii, TextEncoding::kOldJis, mode) &&
              window.open_path(unicode, TextEncoding::kUtf16Be, mode, true) &&
              window.open_jwp_path(jwp, jwpqt::core::LegacyCodePage::k1251, mode, true),
          "Could not create recent-file workflow documents");
  require(window.recent_documents().size() == 3 &&
              window.recent_documents()[0].path == jwp &&
              !window.recent_documents()[0].encoding &&
              window.recent_documents()[0].code_page == jwpqt::core::LegacyCodePage::k1251 &&
              window.recent_documents()[1].encoding == TextEncoding::kUtf16Be &&
              window.recent_documents()[2].encoding == TextEncoding::kOldJis &&
              find_action(window, "recentFile3Action")->text().contains(QStringLiteral("&&")),
          "Recent paths, ordering, encoding or menu escaping was wrong");
  const auto stored = jwpqt::qt::read_recent_documents(history);
  require(stored.size() == 3 && stored[1].encoding == TextEncoding::kUtf16Be,
          "Successful opens did not persist their explicit formats");
  QPointer<QAction> fixed_action = find_action(window, "recentFile2Action");
  require(window.activate_document(1), "Could not select recent Unicode document");
  auto* editor = window.active_editor();
  editor->moveCursor(QTextCursor::End);
  editor->insertPlainText(QStringLiteral("X"));
  require(window.activate_document(2), "Could not select recent JWP document");
  QTimer::singleShot(0, [] {
    auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    require(prompt != nullptr, "Recent-file duplicate prompt was not shown");
    auto* activate = prompt->findChild<QPushButton*>(QStringLiteral("duplicateActivateButton"));
    require(activate != nullptr, "Recent-file duplicate activation is missing");
    activate->click();
  });
  fixed_action->trigger();
  require(fixed_action && window.document_count() == 3 &&
              window.active_editor() == editor && window.document_modified() &&
              window.recent_documents().front().path == unicode,
          "Recent-menu activation reloaded an open buffer or deleted its action");
  find_action(window, "undoAction")->trigger();
  require(!window.document_modified(), "Recent activation lost Unicode undo");
  const QString saved = directory + QStringLiteral("/recent-saved.utf8");
  require(window.save_as_path(saved, TextEncoding::kUtf8) &&
              window.recent_documents().front().path == saved &&
              window.recent_documents().front().encoding == TextEncoding::kUtf8,
          "Save As did not record the new path and encoding");
  const QByteArray before = read_bytes(history);
  require(window.save_as_path(directory + QStringLiteral("/recent-copy.txt"),
                              TextEncoding::kUtf8, false, true) &&
              !window.save_as_path(directory, TextEncoding::kUtf8) &&
              read_bytes(history) == before,
          "Export Copy or failed Save As changed recent history");
  const QString invalid = directory + QStringLiteral("/recent-invalid.txt");
  write_bytes(invalid, QByteArray::fromHex("fffe00"));
  require(!window.open_path_detected(invalid, mode, true) &&
              !window.open_path_detected(ascii + QStringLiteral(".missing"), mode, true) &&
              read_bytes(history) == before && window.document_count() == 3,
          "Failed opening changed recent history or tabs");
  const QString ambiguous = directory + QStringLiteral("/recent-ambiguous.txt");
  write_bytes(ambiguous, QByteArray("ASCII"));
  require(!window.open_path_detected(ambiguous, OpenMode::kInteractive, true) &&
              read_bytes(history) == before,
          "Cancelled encoding selection changed recent history");
  bool cancelled = false;
  QTimer::singleShot(0, [&] {
    if (auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget())) {
      cancelled = true;
      dialog->reject();
    }
  });
  find_action(window, "openDocumentAction")->trigger();
  require(cancelled && read_bytes(history) == before,
          "Cancelled Open dialog changed recent history");

  PromptingWindow restored;
  require(restored.load_recent_file_configuration(history) &&
              restored.open_recent_document(1) &&
              restored.text_encoding() == TextEncoding::kUtf16Be &&
              restored.active_editor()->toPlainText() == QString::fromStdU32String(U"A\U0001f600"),
          "Recent unmarked UTF-16 was redetected or lost text");
  const auto native_entry = std::find_if(restored.recent_documents().begin(),
      restored.recent_documents().end(), [&](const auto& entry) { return entry.path == jwp; });
  require(native_entry != restored.recent_documents().end() &&
              restored.open_recent_document(static_cast<int>(native_entry - restored.recent_documents().begin())) &&
              restored.jwp_code_page() == jwpqt::core::LegacyCodePage::k1251 &&
              restored.current_jwp_document()->paragraphs.front().text == native.paragraphs.front().text,
          "Recent JWP did not retain its code page");
  const int count = window.document_count();
  find_action(window, "clearRecentFilesAction")->trigger();
  require(window.recent_documents().empty() && window.document_count() == count &&
              jwpqt::qt::read_recent_documents(history).empty() && fixed_action &&
              !fixed_action->isVisible() && !find_action(window, "clearRecentFilesAction")->isEnabled(),
          "Clear recent files changed open documents or left stale menu entries");

  PromptingWindow bounded;
  for (int i = 0; i < 11; ++i) {
    const QString path = directory + QStringLiteral("/recent-%1.txt").arg(i);
    write_bytes(path, QByteArray("ASCII"));
    require(bounded.open_path(path, TextEncoding::kUtf8, mode), "Could not fill recent history");
  }
  require(bounded.recent_documents().size() == 9 &&
              bounded.recent_documents().front().path.endsWith(QStringLiteral("recent-10.txt")) &&
              bounded.recent_documents().back().path.endsWith(QStringLiteral("recent-2.txt")) &&
              bounded.resource_report().contains(QStringLiteral("Recent files: memory only")),
          "Recent history limit, ordering or memory-only constructor failed");
}

void test_recent_file_failures(const QString& directory) {
  using jwpqt::core::TextEncoding;
  using jwpqt::qt::OpenMode;
  constexpr auto mode = OpenMode::kNonInteractive;
  const QString source = directory + QStringLiteral("/recent-failure-source.txt");
  const QString history = directory + QStringLiteral("/recent-corrupt.json");
  const QByteArray corrupt("preserve invalid history");
  write_bytes(source, QByteArray("ASCII"));
  write_bytes(history, corrupt);
  PromptingWindow window;
  require(!window.load_recent_file_configuration(history) &&
              !window.recent_file_warning().isEmpty() &&
              window.open_path(source, TextEncoding::kUtf8, mode) &&
              window.recent_documents().size() == 1 && read_bytes(history) == corrupt,
          "Corrupt history blocked opening or was overwritten automatically");
  require(window.clear_recent_documents() && window.recent_file_warning().isEmpty() &&
              jwpqt::qt::read_recent_documents(history).empty(),
          "Explicit Clear did not recover corrupt history");
  write_bytes(history, corrupt);
  require(window.save_as_path(directory + QStringLiteral("/recent-after-corruption.txt"), TextEncoding::kUtf8) &&
              read_bytes(history) == corrupt && !window.recent_file_warning().isEmpty(),
          "Automatic persistence overwrote history corrupted after loading");
  require(window.clear_recent_documents(), "Could not reset corrupt history");
  const QString blocked = directory + QStringLiteral("/absent-recent-parent/history.json");
  require(window.load_recent_file_configuration(blocked) &&
              window.save_as_path(directory + QStringLiteral("/recent-success.txt"), TextEncoding::kUtf8) &&
              !window.document_modified() && !window.recent_file_warning().isEmpty() &&
              window.resource_report().contains(QStringLiteral("Could not save recent files")),
          "History write failure turned a successful document save into failure");
  require(!window.clear_recent_documents() && window.recent_documents().size() == 1,
          "Failed Clear discarded the in-memory history");

  jwpqt::qt::write_recent_documents(history, {{source, TextEncoding::kUtf8,
                                            jwpqt::core::kDefaultLegacyCodePage}});
  const QByteArray original = read_bytes(history);
  require(window.load_recent_file_configuration(history) &&
              window.open_path(history, TextEncoding::kUtf8, mode, true) &&
              !window.clear_recent_documents() && read_bytes(history) == original,
          "Automatic history persistence overwrote an open history document");

  PromptingWindow closing;
  require(closing.open_path(source, TextEncoding::kOldJis, mode), "Could not open saved-codec fixture");
  find_encoding_action(closing, QStringLiteral("UTF-16BE"))->trigger();
  require(closing.document_modified(), "Codec selection was not pending");
  QTimer::singleShot(0, [] {
    if (auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
      message->button(QMessageBox::Discard)->click();
  });
  require(closing.close_all_documents() &&
              closing.recent_documents().front().encoding == TextEncoding::kOldJis &&
              closing.open_recent_document(0) && closing.text_encoding() == TextEncoding::kOldJis,
          "Discarded codec selection replaced the saved recent-file encoding");
  const auto before = closing.recent_documents();
  closing.active_editor()->insertPlainText(QStringLiteral("X"));
  require(!closing.close_document(closing.current_document_index(), mode) &&
              closing.recent_documents().size() == before.size() &&
              closing.recent_documents().front().path == before.front().path,
          "Cancelled close changed recent history");
  const int dirty_index = closing.current_document_index();
  const QString other = directory + QStringLiteral("/recent-close-other.txt");
  write_bytes(other, QByteArray("other"));
  require(closing.open_path(other, TextEncoding::kUtf8, mode, true),
          "Could not create close-cancellation history fixture");
  QTimer::singleShot(0, [] {
    if (auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
      message->button(QMessageBox::Cancel)->click();
  });
  require(!closing.close_document(dirty_index) && closing.document_modified() &&
              closing.recent_documents().front().path == other,
          "Cancelled background close reordered recent files");
  require(QFile::remove(source) && closing.open_recent_document(1),
          "Already-open recent activation unexpectedly failed");
  PromptingWindow missing;
  jwpqt::qt::write_recent_documents(history, before);
  require(missing.load_recent_file_configuration(history) && !missing.open_recent_document(0) &&
              missing.recent_documents().size() == before.size() && missing.document_count() == 1,
          "Missing recent file changed history or created a tab");
}

void test_document_path_identity(const QString& directory) {
  using jwpqt::core::TextEncoding;
  constexpr auto mode = jwpqt::qt::OpenMode::kNonInteractive;
  const QString target = directory + QStringLiteral("/recent-target");
  const QString alias = directory + QStringLiteral("/recent-link");
  require(QDir().mkpath(target + QStringLiteral("/deep")) &&
              QFile::link(target + QStringLiteral("/deep"), alias),
          "Could not create directory-symlink fixture");
  const QString root = directory + QStringLiteral("/identity.txt");
  const QString physical = target + QStringLiteral("/identity.txt");
  const QString linked = alias + QStringLiteral("/../identity.txt");
  write_bytes(root, QByteArray("root"));
  write_bytes(physical, QByteArray("nested"));
  PromptingWindow window;
  require(window.open_path(root, TextEncoding::kUtf8, mode) &&
              window.open_path(linked, TextEncoding::kUtf8, mode, true) &&
              window.document_count() == 2 && window.active_editor()->toPlainText() == QStringLiteral("nested") &&
              window.recent_documents().size() == 2 && window.recent_documents().front().path == linked,
          "Lexical path cleaning conflated distinct files through a directory symlink");
  require(window.open_path(physical, TextEncoding::kUtf8, mode, true) && window.document_count() == 2 &&
              window.recent_documents().size() == 2 && window.close_document(0, mode) &&
              window.save_as_path(root, TextEncoding::kUtf8, false, true) && read_bytes(root) == QByteArray("nested"),
          "Canonical alias activation or distinct-file Export Copy identity failed");
}

void test_explicit_open_and_encoding_action(const QString& directory) {
  const QString path = directory + QStringLiteral("/explicit.euc");
  const jwpqt::core::TextFile file{
      U"ASCII \u65e5\u672c\u8a9e\n", jwpqt::core::TextEncoding::kEucJp,
      false};
  jwpqt::qt::write_text_file(path, file);

  jwpqt::qt::MainWindow window;
  require(window.open_path(path, jwpqt::core::TextEncoding::kEucJp),
          "Could not explicitly open EUC-JP file");
  require(window.text_encoding() == jwpqt::core::TextEncoding::kEucJp,
          "Window did not retain EUC-JP encoding");

  QTextEdit* editor = window.findChild<QTextEdit*>();
  require(editor != nullptr, "Window has no editor");
  require(editor->toPlainText() == QStringLiteral("ASCII \u65e5\u672c\u8a9e\n"),
          "Window decoded the EUC-JP text incorrectly");
  require(!editor->document()->isModified(),
          "Opening a document marked it modified");

  QLabel* label = window.findChild<QLabel*>(QStringLiteral("documentEncoding"));
  require(label != nullptr && label->text() == QStringLiteral("EUC-JP"),
          "Encoding status did not show EUC-JP");

  QAction* shift_jis_action =
      find_encoding_action(window, QStringLiteral("Shift-JIS"));
  require(shift_jis_action != nullptr, "Shift-JIS action was not created");
  shift_jis_action->trigger();
  require(window.text_encoding() == jwpqt::core::TextEncoding::kShiftJis,
          "Encoding action did not select Shift-JIS");
  require(editor->document()->isModified(),
          "Changing the save encoding did not mark the document modified");
  require(label->text() == QStringLiteral("Shift-JIS"),
          "Encoding status did not update to Shift-JIS");

  const QString saved_path = directory + QStringLiteral("/saved.sjis");
  require(window.save_path(saved_path), "Could not save as Shift-JIS");
  require(read_bytes(saved_path) ==
              QByteArray::fromHex("41534349492093fa967b8cea0a"),
          "Encoding selection did not control saved bytes");
  require(!editor->document()->isModified(),
          "Successful explicit save did not clear modified state");

  QAction* old_jis_action =
      find_encoding_action(window, QStringLiteral("Old JIS"));
  require(old_jis_action != nullptr, "Old JIS action was not created");
  old_jis_action->trigger();
  const QString old_jis_path = directory + QStringLiteral("/saved.old");
  require(window.save_path(old_jis_path), "Could not save as Old JIS");
  require(read_bytes(old_jis_path) ==
              QByteArray::fromHex(
                  "4153434949201b2440467c4b5c386c1b284a0a"),
          "Old JIS action did not control saved bytes");
}

void test_utf16_workflow(const QString& directory) {
  using jwpqt::core::TextEncoding;
  using jwpqt::qt::OpenMode;
  for (const auto encoding : {TextEncoding::kUtf16Le, TextEncoding::kUtf16Be}) {
    const QString name = encoding == TextEncoding::kUtf16Le
                             ? QStringLiteral("UTF-16LE") : QStringLiteral("UTF-16BE");
    const QString filter = name + QStringLiteral(" text (*.txt *.utf16)");
    const QString source = directory + "/" + name + ".txt";
    const QString saved = directory + "/saved-" + name + ".txt";
    for (const bool bom : {false, true}) {
      const jwpqt::core::TextFile file{U"\u65e5\u672c\n\U0001f600", encoding, bom};
      jwpqt::qt::write_text_file(source, file);
      PromptingWindow window;
      require(bom ? window.open_path_detected(source, OpenMode::kNonInteractive)
                  : window.open_path(source, encoding, OpenMode::kNonInteractive),
              "UTF-16 open/detection failed");
      if (!bom) {
        auto open_settings = window.application_settings();
        open_settings.duplicate_open = jwpqt::qt::DuplicateOpenBehavior::kActivateExisting;
        require(window.apply_application_settings(open_settings),
                "Could not isolate UTF-16 file-dialog behavior from duplicate prompting");
        bool offered = false;
        QTimer::singleShot(0, &window, [&] {
          if (auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget())) {
            offered = dialog->nameFilters().contains(filter);
            dialog->selectNameFilter(filter);
            dialog->selectFile(source);
            QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
          }
        });
        find_encoding_action(window, QStringLiteral("&Open..."))->trigger();
        require(offered && window.text_encoding() == encoding,
                "UTF-16 Open filter did not select unmarked byte order");
      }
      auto* editor = window.findChild<QTextEdit*>();
      require(editor != nullptr && editor->toPlainText() ==
                  QString::fromStdU32String(file.text) &&
                  window.text_encoding() == encoding &&
                  find_encoding_action(window, name)->isChecked() &&
                  window.save_path(saved) && read_bytes(saved) == read_bytes(source),
              "UTF-16 native lifecycle lost text, byte order or BOM");
      editor->insertPlainText(QStringLiteral("edit"));
      const auto modified = editor->toPlainText();
      const QString invalid = directory + QStringLiteral("/invalid-utf16.txt");
      write_bytes(invalid, QByteArray::fromHex("fffe3dd8"));
      require(!window.open_path_detected(invalid, OpenMode::kNonInteractive) &&
                  window.current_path() == saved && window.document_modified() &&
                  editor->toPlainText() == modified,
              "Invalid UTF-16 open destroyed live text");
      require(window.revert_current_document(OpenMode::kNonInteractive) &&
                  editor->toPlainText() == QString::fromStdU32String(file.text) &&
                  !window.document_modified(),
              "UTF-16 Revert did not preserve file policy");
      bool filtered = false;
      const QString dialog_saved = directory + "/dialog-" + name +
                                   (bom ? "-bom.txt" : "-plain.txt");
      QTimer::singleShot(0, &window, [&] {
        if (auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget())) {
          filtered = dialog->selectedNameFilter() == filter;
          dialog->selectFile(dialog_saved);
          QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
        }
      });
      find_encoding_action(window, QStringLiteral("Save &As..."))->trigger();
      require(filtered && read_bytes(dialog_saved) == read_bytes(source),
              "UTF-16 Save As did not retain the filter or bytes");
    }
  }
  const QString plain = directory + QStringLiteral("/utf16-menu.txt");
  jwpqt::qt::write_text_file(plain, {U"\u65e5", TextEncoding::kUtf8, false});
  PromptingWindow window;
  require(window.open_path(plain, TextEncoding::kUtf8, OpenMode::kNonInteractive),
          "Could not open UTF-16 menu fixture");
  find_encoding_action(window, QStringLiteral("UTF-16BE"))->trigger();
  require(window.document_modified() && window.save_path(plain) &&
              read_bytes(plain) == QByteArray::fromHex("feff65e5"),
          "New UTF-16 encoding selection did not emit a BOM");
}

void test_jfc_open_save_and_revert(const QString& directory) {
  using jwpqt::core::TextEncoding;
  using jwpqt::qt::OpenMode;
  const QString path = directory + QStringLiteral("/cards.JfC");
  const QByteArray old_euc = QByteArray::fromHex("c6fc098e268fabb10a");
  write_bytes(path, old_euc);
  PromptingWindow window;
  require(window.open_path_detected(path, OpenMode::kNonInteractive),
          "JFC extension did not select the old-EUC decoder noninteractively");
  QTextEdit* editor = window.findChild<QTextEdit*>();
  QLabel* label = window.findChild<QLabel*>(QStringLiteral("documentEncoding"));
  QAction* jfc_action = find_encoding_action(window, QStringLiteral("JFC"));
  const QString text = QStringLiteral("\u65e5\t\u00a6\u00e9\n");
  require(editor != nullptr && editor->toPlainText() == text &&
              window.text_encoding() == TextEncoding::kJfc &&
              window.is_jwp_document() && !window.uses_jwp_format() &&
              !window.document_modified() &&
              label != nullptr && label->text() == QStringLiteral("JFC") &&
              jfc_action != nullptr && jfc_action->isChecked(),
          "JFC open did not retain native text and format state");
  editor->selectAll();
  editor->insertPlainText(QStringLiteral("changed"));
  require(window.revert_current_document(OpenMode::kNonInteractive) &&
              editor->toPlainText() == text && !window.document_modified() &&
              window.text_encoding() == TextEncoding::kJfc,
          "JFC Revert did not restore old-EUC text and save policy");
  require(window.save_path(path) &&
              read_bytes(path) == QByteArray::fromHex("e697a509c2a6c3a90a") &&
              !window.document_modified(),
          "JFC save did not replace old EUC with canonical UTF-8");
  require(window.open_path_detected(path, OpenMode::kNonInteractive) &&
              editor->toPlainText() == text &&
              window.text_encoding() == TextEncoding::kJfc,
          "JFC UTF-8 reopen lost its format");

  const QString ascii_path = directory + QStringLiteral("/ascii.JFC");
  write_bytes(ascii_path, QByteArray("question\tanswer\n"));
  require(window.open_path_detected(ascii_path) && window.explanation.isEmpty() &&
              window.text_encoding() == TextEncoding::kJfc,
          "ASCII JFC prompted for a save encoding");
  const QString invalid_path = directory + QStringLiteral("/invalid.jfc");
  write_bytes(invalid_path, QByteArray::fromHex("8fb0a1"));
  editor->insertPlainText(QStringLiteral("unsaved"));
  const QString unsaved = editor->toPlainText();
  require(!window.open_path_detected(invalid_path, OpenMode::kNonInteractive) &&
              window.current_path() == ascii_path && window.document_modified() &&
              editor->toPlainText() == unsaved &&
              window.text_encoding() == TextEncoding::kJfc,
          "Failed JFC open changed the live document");

  write_bytes(path, QByteArray::fromHex("c6fc"));
  require(window.open_path(path, TextEncoding::kEucJp, OpenMode::kNonInteractive) &&
              window.text_encoding() == TextEncoding::kEucJp,
          "JFC extension overrode an explicit encoding selection");
  const QString bom_path = directory + QStringLiteral("/jfc-bom.txt");
  write_bytes(bom_path, QByteArray::fromHex("efbbbfc3a9"));
  require(window.open_path(bom_path, TextEncoding::kUtf8, OpenMode::kNonInteractive),
          "Could not open JFC encoding-action fixture");
  jfc_action->trigger();
  require(window.text_encoding() == TextEncoding::kJfc &&
              window.document_modified() && window.save_path(path) &&
              read_bytes(path) == QByteArray::fromHex("c3a9"),
          "JFC encoding action did not select UTF-8 output without a BOM");
}

void test_jfc_file_dialogs(const QString& directory) {
  const QString source_path = directory + QStringLiteral("/cards-without-suffix.txt");
  const QString saved_path = directory + QStringLiteral("/dialog-saved.jfc");
  const QString filter = QStringLiteral("JFC text (*.jfc)");
  write_bytes(source_path, QByteArray::fromHex("8e268fabb1"));
  PromptingWindow window;
  QAction* open = find_encoding_action(window, QStringLiteral("&Open..."));
  QAction* save_as = find_encoding_action(window, QStringLiteral("Save &As..."));
  require(open != nullptr && save_as != nullptr, "File dialog actions are missing");
  bool open_filter_present = false;
  QTimer::singleShot(0, &window, [&] {
    auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
    if (dialog != nullptr) {
      open_filter_present = dialog->nameFilters().contains(filter);
      dialog->selectNameFilter(filter);
      dialog->selectFile(source_path);
      QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
    } else if (auto* modal =
                   qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
      modal->reject();
    }
  });
  open->trigger();
  require(open_filter_present && window.current_path() == source_path &&
              window.text_encoding() == jwpqt::core::TextEncoding::kJfc,
          "JFC Open filter did not select the codec without a .jfc suffix");

  bool save_filter_selected = false;
  QTimer::singleShot(0, &window, [&] {
    auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
    if (dialog != nullptr) {
      save_filter_selected = dialog->selectedNameFilter() == filter;
      dialog->selectFile(saved_path);
      QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
    } else if (auto* modal =
                   qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
      modal->reject();
    }
  });
  save_as->trigger();
  require(save_filter_selected && window.current_path() == saved_path &&
              window.text_encoding() == jwpqt::core::TextEncoding::kJfc &&
              read_bytes(saved_path) == QByteArray::fromHex("c2a6c3a9"),
          "JFC Save As filter did not preserve the format and write UTF-8");
}

void test_local_file_lifecycle_actions(const QString& directory) {
  const QString path = directory + QStringLiteral("/lifecycle.txt");
  QFile disk(path);
  require(disk.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
              disk.write("original") == 8,
          "Could not create lifecycle fixture");
  disk.close();

  jwpqt::qt::MainWindow window;
  require(window.open_path(path, jwpqt::core::TextEncoding::kUtf8,
                           jwpqt::qt::OpenMode::kNonInteractive),
          "Could not open lifecycle fixture");
  QTextEdit* editor = window.findChild<QTextEdit*>();
  QAction* revert = find_action(window, "revertDocumentAction");
  QAction* close = find_action(window, "closeDocumentAction");
  QAction* remove = find_action(window, "deleteDocumentAction");
  require(editor != nullptr && revert != nullptr && close != nullptr &&
              remove != nullptr && revert->isEnabled() && remove->isEnabled(),
          "Local lifecycle actions were not created or enabled");

  editor->selectAll();
  editor->insertPlainText(QStringLiteral("changed"));
  require(window.document_modified() &&
              window.revert_current_document(
                  jwpqt::qt::OpenMode::kNonInteractive) &&
              editor->toPlainText() == QStringLiteral("original") &&
              !window.document_modified() && window.current_path() == path,
          "Revert did not restore the current file and clean state");

  require(window.delete_current_document(
              jwpqt::qt::OpenMode::kNonInteractive) &&
              !QFile::exists(path) && window.current_path().isEmpty() &&
              editor->toPlainText().isEmpty() && !revert->isEnabled() &&
              !remove->isEnabled(),
          "Delete did not remove the file and reset the document");

  const QString close_path = directory + QStringLiteral("/close.txt");
  QFile close_file(close_path);
  require(close_file.open(QIODevice::WriteOnly) && close_file.write("close") == 5,
          "Could not create close fixture");
  close_file.close();
  require(window.open_path(close_path, jwpqt::core::TextEncoding::kUtf8,
                           jwpqt::qt::OpenMode::kNonInteractive),
          "Could not reopen lifecycle fixture");
  QTimer::singleShot(0, [] {
    auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    if (prompt) prompt->button(QMessageBox::No)->click();
  });
  close->trigger();
  require(window.current_path().isEmpty() && editor->toPlainText().isEmpty(),
          "Close did not return to an unnamed document");
}

void test_backup_policy(const QString& directory) {
  using namespace jwpqt;
  using namespace jwpqt::qt;
  auto preferences = read_application_settings("backup_files=true\nSaveSettingsOnExit=false\n");
  require(preferences.keep_backup_copy && !ApplicationSettings{}.keep_backup_copy &&
      read_application_settings(write_application_settings(preferences)).keep_backup_copy, "Backup policy defaults/aliases lost");
  bool invalid = false;
  try { (void)read_application_settings("KeepBackupCopyWhenSaving=bad\nbackup_files=true\n"); }
  catch (const std::exception&) { invalid = true; }
  require(invalid, "Earlier invalid backup preference accepted");
  ApplicationSettingsDialog options(preferences);
  options.findChild<QCheckBox*>("settingsKeepBackup")->click(); options.reject();
  require(preferences.keep_backup_copy, "Cancel changed backup preference");
  options.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
  require(!options.settings().keep_backup_copy, "Backup checkbox not accepted");
  const auto path = directory + "/policy-backup.txt";
  const auto backup = path + "_BAK";
  write_text_file(path, {U"before", core::TextEncoding::kUtf8, false});
  MainWindow window;
  require(window.apply_application_settings(preferences) && window.open_path(path, core::TextEncoding::kUtf8), "Backup setup failed");
  window.active_editor()->moveCursor(QTextCursor::End);
  window.active_editor()->insertPlainText(" after");
  require(window.save_as_path(path, core::TextEncoding::kUtf8) &&
      read_text_file(backup, core::TextEncoding::kUtf8).text == U"before" && !window.document_modified(),
      "Document save did not use backup policy");
  require(window.open_path(backup, core::TextEncoding::kUtf8, OpenMode::kNonInteractive, true), "Backup tab failed");
  window.activate_document(0); window.active_editor()->insertPlainText("unsaved");
  const auto text = window.active_editor()->toPlainText();
  require(!window.save_as_path(path, core::TextEncoding::kUtf8) && window.document_modified() &&
      window.active_editor()->toPlainText() == text && read_text_file(backup, core::TextEncoding::kUtf8).text == U"before" &&
      read_text_file(path, core::TextEncoding::kUtf8).text == U"before after", "Open backup collision changed data");
  require(window.close_document(1, OpenMode::kNonInteractive) && !window.load_previous_session(backup, false) &&
      !window.save_as_path(path, core::TextEncoding::kUtf8) &&
      read_text_file(backup, core::TextEncoding::kUtf8).text == U"before" && window.document_modified(),
      "Implicit backup overwrote configured session data");
  const auto config = directory + "/backup-policy.cfg";
  require(window.save_application_settings(config), "Backup settings save failed");
  MainWindow restored;
  require(restored.load_application_settings(config) && restored.application_settings().keep_backup_copy &&
      restored.save_project_path(directory + "/backup-policy.jpr", false), "Backup preference restart failed");
  MainWindow project;
  require(project.open_project_path(directory + "/backup-policy.jpr") && project.application_settings().keep_backup_copy,
      "Project lost backup policy");
}

void test_startup_close_policies(const QString& directory) {
  using namespace jwpqt;
  QApplication::setQuitOnLastWindowClosed(false);
  const auto parsed = qt::read_application_settings(
      "startup_dict=true\nclose_does_file=true\nconfirm_exit=false\nRetained_Field = x\n");
  require(parsed.startup_dictionary && parsed.close_button_closes_file && !parsed.confirm_last_file_exit,
          "Source startup/close aliases were not applied");
  const auto bytes = qt::write_application_settings(parsed);
  require(bytes.find("OpenDictionary = true") != std::string::npos &&
              bytes.find("CloseButton_Closes_File = true") != std::string::npos &&
              bytes.find("LastFileConfirmExit = false") != std::string::npos &&
              bytes.find("Retained_Field = x") != std::string::npos,
          "Startup/close serialization lost source settings");
  bool invalid = false;
  try { (void)qt::read_application_settings("LastFileConfirmExit=bad\nconfirm_exit=true\n"); }
  catch (const std::exception&) { invalid = true; }
  require(invalid, "An invalid earlier close setting was ignored");

  qt::MainWindow window;
  auto preferences = parsed;
  preferences.confirm_last_file_exit = true;
  require(window.apply_application_settings(preferences), "Could not set close preferences");
  const QString path = directory + QStringLiteral("/startup-close.cfg");
  require(window.save_application_settings(path), "Could not save startup/close settings");
  const QString project_path = directory + QStringLiteral("/startup-close.jpr");
  qt::ProjectOpenOptions project_options;
  project_options.allow_unapplied_settings = true;
  qt::MainWindow restored;
  require(window.save_project_path(project_path, false) && restored.open_project_path(project_path, project_options) &&
              restored.application_settings().startup_dictionary && restored.application_settings().close_button_closes_file &&
              restored.application_settings().confirm_last_file_exit &&
              !restored.findChild<QDialog*>(QStringLiteral("edictLookupDialog")),
          "Project preferences were lost or incorrectly ran application startup");
  qt::MainWindow restarted;
  require(restarted.load_application_settings(path) && restarted.application_settings().startup_dictionary &&
              restarted.application_settings().close_button_closes_file &&
              restarted.application_settings().confirm_last_file_exit &&
              restarted.open_startup_dictionary(true) && !restarted.open_startup_dictionary(false) &&
              !restarted.findChild<QDialog*>(QStringLiteral("edictLookupDialog")),
          "Startup restoration or missing-resource handling changed");

  qt::ApplicationSettingsDialog options(preferences, &window);
  options.show();
  QCoreApplication::processEvents();
  require(options.grab().save(QDir::current().filePath(QStringLiteral("startup-close-options.png"))),
          "Could not capture startup/close Options");
  for (const char* name : {"settingsStartupDictionary", "settingsCloseButtonFile", "settingsConfirmLastFileExit"}) {
    auto* box = options.findChild<QCheckBox*>(QString::fromLatin1(name));
    require(box && box->isChecked(), "A startup/close Options control is missing");
    box->click();
  }
  options.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
  require(!options.settings().startup_dictionary && !options.settings().close_button_closes_file &&
              !options.settings().confirm_last_file_exit && window.application_settings().confirm_last_file_exit,
          "Staged Options did not capture all close controls independently");
  qt::ApplicationSettingsDialog cancelled(preferences, &window);
  cancelled.findChild<QCheckBox*>(QStringLiteral("settingsCloseButtonFile"))->click();
  cancelled.reject();
  require(cancelled.settings().close_button_closes_file && window.application_settings().close_button_closes_file,
          "Cancelled startup/close Options were applied");

  window.show();
  QCoreApplication::processEvents();
  require(window.new_document_tab() == 1 && !window.close() && window.isVisible() && window.document_count() == 1,
          "Window close did not close only the active document");
  window.active_editor()->insertPlainText(QStringLiteral("unsaved"));
  QTimer::singleShot(0, [] {
    auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    require(prompt && prompt->button(QMessageBox::Cancel), "Expected dirty-close save prompt");
    prompt->button(QMessageBox::Cancel)->click();
  });
  require(!window.close() && window.document_modified() && window.active_editor()->toPlainText() == QStringLiteral("unsaved"),
          "Cancelled close lost the dirty document");
  find_action(window, "undoAction")->trigger();
  require(!window.document_modified(), "Could not reset close fixture");
  const auto answer = [](QMessageBox::StandardButton button) {
    QTimer::singleShot(0, [button] {
      auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
      require(prompt && prompt->objectName() == QStringLiteral("lastFileExitPrompt"), "Expected last-file exit choice");
      prompt->button(button)->click();
    });
  };
  answer(QMessageBox::No);
  find_action(window, "closeDocumentAction")->trigger();
  require(window.isVisible() && window.current_path().isEmpty() && !window.document_modified(),
          "Declining last-file exit did not keep a clean unnamed document");
  const QString original = directory + QStringLiteral("/close-policy.txt");
  qt::write_text_file(original, {U"original", core::TextEncoding::kUtf8, false});
  require(window.open_path(original, core::TextEncoding::kUtf8, qt::OpenMode::kNonInteractive), "Could not open discard fixture");
  window.active_editor()->insertPlainText(QStringLiteral("changed"));
  QTimer::singleShot(0, [&] {
    auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    require(prompt && prompt->button(QMessageBox::Discard), "Expected discard choice");
    answer(QMessageBox::No);
    prompt->button(QMessageBox::Discard)->click();
  });
  find_action(window, "closeDocumentAction")->trigger();
  require(window.current_path().isEmpty() && window.active_editor()->toPlainText().isEmpty() &&
              !find_action(window, "undoAction")->isEnabled() && read_bytes(original) == "original",
          "Declining exit restored discarded edits or changed the closed file");
  answer(QMessageBox::Yes);
  find_action(window, "closeDocumentAction")->trigger();
  QCoreApplication::processEvents();
  require(!window.isVisible(), "Accepted last-file exit did not close the application");
  window.show();
  preferences.confirm_last_file_exit = false;
  require(window.apply_application_settings(preferences), "Could not disable last-file confirmation");
  find_action(window, "closeDocumentAction")->trigger();
  QCoreApplication::processEvents();
  require(!window.isVisible(), "Disabled confirmation did not exit without a prompt");
  window.show();
  require(window.new_document_tab() == 1, "Could not prepare force-Quit fixture");
  find_action(window, "quitAction")->trigger();
  require(!window.isVisible() && window.document_count() == 2, "Explicit Quit used the file-close policy");
  window.show();
  require(!window.close() && window.document_count() == 1 && window.isVisible(), "Force-Quit state leaked to later closes");

  preferences.close_button_closes_file = false;
  require(window.apply_application_settings(preferences) && window.new_document_tab() == 1, "Could not prepare modifier close");
  QTest::keyPress(window.windowHandle(), Qt::Key_Control, Qt::ControlModifier);
  require(!window.close() && window.document_count() == 1 && window.isVisible(), "Ctrl-close did not force file close");
  QTest::keyRelease(window.windowHandle(), Qt::Key_Control, Qt::ControlModifier);
  preferences.close_button_closes_file = true;
  require(window.apply_application_settings(preferences) && window.new_document_tab() == 1, "Could not prepare Alt-close");
  QTest::keyPress(window.windowHandle(), Qt::Key_Alt, Qt::AltModifier);
  require(window.close() && !window.isVisible() && window.document_count() == 2, "Alt-close did not force application close");
  QTest::keyRelease(window.windowHandle(), Qt::Key_Alt, Qt::AltModifier);

  qt::MainWindow changed;
  changed.show();
  QTimer::singleShot(0, [&] {
    auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    require(prompt, "Expected guarded exit prompt");
    changed.active_editor()->insertPlainText(QStringLiteral("new work"));
    prompt->button(QMessageBox::Yes)->click();
  });
  find_action(changed, "closeDocumentAction")->trigger();
  QCoreApplication::processEvents();
  require(changed.isVisible() && changed.document_modified() && changed.active_editor()->toPlainText() == QStringLiteral("new work"),
          "An old exit confirmation closed newer document contents");
  QPointer<qt::MainWindow> doomed = new qt::MainWindow;
  QTimer::singleShot(0, [&] { delete doomed.data(); });
  find_action(*doomed, "closeDocumentAction")->trigger();
  require(doomed.isNull(), "Exit confirmation did not survive owner deletion");
  QApplication::setQuitOnLastWindowClosed(true);
}

void test_leaving_utf8_drops_bom(const QString& directory) {
  const QString source_path = directory + QStringLiteral("/bom.txt");
  jwpqt::qt::write_text_file(
      source_path, {U"ASCII", jwpqt::core::TextEncoding::kUtf8, true});

  jwpqt::qt::MainWindow window;
  require(window.open_path(source_path, jwpqt::core::TextEncoding::kUtf8),
          "Could not open UTF-8 BOM file");
  QAction* euc_action =
      find_encoding_action(window, QStringLiteral("EUC-JP"));
  QAction* utf8_action =
      find_encoding_action(window, QStringLiteral("UTF-8"));
  require(euc_action != nullptr && utf8_action != nullptr,
          "Encoding actions were not created");
  euc_action->trigger();
  utf8_action->trigger();

  const QString saved_path = directory + QStringLiteral("/without-bom.txt");
  require(window.save_path(saved_path), "Could not save switched UTF-8 file");
  require(read_bytes(saved_path) == QByteArray("ASCII"),
          "Switching away from UTF-8 did not clear BOM metadata");
}

void test_detected_open(const QString& directory) {
  const QString path = directory + QStringLiteral("/detected.euc");
  jwpqt::qt::write_text_file(
      path, {U"ASCII \u65e5\u672c\u8a9e\n",
             jwpqt::core::TextEncoding::kEucJp, false});

  jwpqt::qt::MainWindow window;
  require(window.open_path_detected(path),
          "Could not open a certainly detected EUC-JP file");
  require(window.text_encoding() == jwpqt::core::TextEncoding::kEucJp,
          "Detected open did not retain EUC-JP");
  QTextEdit* editor = window.findChild<QTextEdit*>();
  require(editor != nullptr &&
              editor->toPlainText() == QStringLiteral("ASCII \u65e5\u672c\u8a9e\n"),
          "Detected open decoded EUC-JP incorrectly");
}

void test_detected_bom_is_preserved(const QString& directory) {
  const QString source_path = directory + QStringLiteral("/detected-bom.txt");
  jwpqt::qt::write_text_file(
      source_path, {U"\u65e5\u672c\u8a9e", jwpqt::core::TextEncoding::kUtf8,
                    true});

  jwpqt::qt::MainWindow window;
  require(window.open_path_detected(source_path),
          "Could not detect UTF-8 BOM file");
  const QString saved_path = directory + QStringLiteral("/preserved-bom.txt");
  require(window.save_path(saved_path), "Could not save detected BOM file");
  require(read_bytes(saved_path).startsWith(QByteArray::fromHex("efbbbf")),
          "Detected UTF-8 BOM was not preserved");
}

void test_detection_prompt_and_cancellation(const QString& directory) {
  const QString initial_path = directory + QStringLiteral("/initial.txt");
  jwpqt::qt::write_text_file(
      initial_path,
      {U"original", jwpqt::core::TextEncoding::kUtf8, false});
  const QString ambiguous_path = directory + QStringLiteral("/ambiguous.bin");
  QFile ambiguous(ambiguous_path);
  require(ambiguous.open(QIODevice::WriteOnly),
          "Could not create ambiguous fixture");
  require(ambiguous.write(QByteArray::fromHex("e0a1")) == 2,
          "Could not write ambiguous fixture");
  ambiguous.close();

  PromptingWindow window;
  require(window.open_path(initial_path, jwpqt::core::TextEncoding::kUtf8),
          "Could not open initial prompt-state document");
  QTextEdit* editor = window.findChild<QTextEdit*>();
  require(editor != nullptr, "Prompting window has no editor");
  window.next_encoding = std::nullopt;
  require(!window.open_path_detected(ambiguous_path),
          "Cancelled ambiguous detection unexpectedly opened");
  require(editor->toPlainText() == QStringLiteral("original") &&
              window.text_encoding() == jwpqt::core::TextEncoding::kUtf8,
          "Cancelled detection changed current document state");
  require(window.offered_encodings ==
              std::vector<jwpqt::core::TextEncoding>{
                  jwpqt::core::TextEncoding::kEucJp,
                  jwpqt::core::TextEncoding::kShiftJis},
          "Ambiguous detection did not offer exact viable encodings");

  window.next_encoding = jwpqt::core::TextEncoding::kEucJp;
  require(window.open_path_detected(ambiguous_path),
          "Selected ambiguous encoding did not open");
  require(window.text_encoding() == jwpqt::core::TextEncoding::kEucJp,
          "Selected ambiguous encoding was not retained");
  const QString saved_path = directory + QStringLiteral("/ambiguous.euc");
  require(window.save_path(saved_path), "Could not save ambiguous selection");
  require(read_bytes(saved_path) == QByteArray::fromHex("e0a1"),
          "Ambiguous selected encoding did not preserve canonical bytes");
}

void test_ascii_and_unknown_prompts(const QString& directory) {
  const QString ascii_path = directory + QStringLiteral("/ascii.txt");
  QFile ascii(ascii_path);
  require(ascii.open(QIODevice::WriteOnly), "Could not create ASCII fixture");
  require(ascii.write("ASCII") == 5, "Could not write ASCII fixture");
  ascii.close();

  PromptingWindow window;
  window.next_encoding = jwpqt::core::TextEncoding::kShiftJis;
  require(window.open_path_detected(ascii_path),
          "Selected ASCII encoding did not open");
  require(window.offered_encodings.size() == 6 &&
              window.text_encoding() ==
                  jwpqt::core::TextEncoding::kShiftJis,
          "ASCII prompt did not expose and retain explicit encoding");

  const QString invalid_path = directory + QStringLiteral("/invalid.bin");
  QFile invalid(invalid_path);
  require(invalid.open(QIODevice::WriteOnly),
          "Could not create invalid fixture");
  require(invalid.write(QByteArray::fromHex("ff")) == 1,
          "Could not write invalid fixture");
  invalid.close();
  window.next_encoding = std::nullopt;
  require(!window.open_path_detected(invalid_path),
          "Cancelled unknown detection unexpectedly opened");
  require(window.offered_encodings.empty(),
          "Unknown detection unexpectedly constrained encoding choices");
}

void test_plain_text_find_actions(const QString& directory, bool unicode = false) {
  const QString path = directory + QStringLiteral("/find.txt");
  jwpqt::qt::write_text_file(
      path, jwpqt::core::TextFile{std::u32string(U"x Alpha alpha \u00c9 \u00e9") +
                                     (unicode ? U"\U0001f600" : U""),
                                 jwpqt::core::TextEncoding::kUtf8, false});

  PromptingWindow window;
  require(window.open_path(path, jwpqt::core::TextEncoding::kUtf8),
          "Could not open plain search fixture");
  window.next_search = jwpqt::qt::SearchRequest{
      QStringLiteral("ALPHA"), jwpqt::core::JwpSearchOptions{}};
  QAction* find = find_action(window, "findAction");
  QAction* find_next = find_action(window, "findNextAction");
  QAction* find_previous = find_action(window, "findPreviousAction");
  require(find != nullptr && find_next != nullptr && find_previous != nullptr,
          "Native find actions were not created");

  find->trigger();
  QTextEdit* editor = window.findChild<QTextEdit*>();
  require(window.search_prompt_count == 1 && editor != nullptr &&
              editor->textCursor().selectedText() == QStringLiteral("Alpha"),
          "Find dialog action did not select the first plain-text match");
  find_next->trigger();
  require(editor->textCursor().selectedText() == QStringLiteral("alpha"),
          "Find Next did not select the following plain-text match");
  find_previous->trigger();
  require(editor->textCursor().selectedText() == QStringLiteral("Alpha"),
          "Find Previous did not select the preceding plain-text match");

  const QTextCursor original = editor->textCursor();
  require(!window.find_text(QStringLiteral("missing")),
          "Missing plain text unexpectedly matched");
  require(editor->textCursor().selectionStart() == original.selectionStart() &&
              editor->textCursor().selectionEnd() == original.selectionEnd(),
          "Failed plain-text search changed the selection");

  QTextCursor after_upper_accent = editor->textCursor();
  after_upper_accent.setPosition(editor->toPlainText().indexOf(u'\u00c9') + 1);
  editor->setTextCursor(after_upper_accent);
  require(!window.find_text(QStringLiteral("\u00c9")),
          "ASCII-only case folding matched a non-ASCII case variant");

  editor->selectAll();
  editor->insertPlainText(QStringLiteral("x aaa"));
  QTextCursor overlap = editor->textCursor();
  overlap.setPosition(1);
  editor->setTextCursor(overlap);
  require(window.find_text(QStringLiteral("aa")) &&
              editor->textCursor().selectionStart() == 2,
          "Plain search did not find the first overlapping match");
  find_next->trigger();
  require(editor->textCursor().selectionStart() == 3,
          "Find Next skipped an overlapping plain-text match");

  editor->selectAll();
  editor->insertPlainText(QStringLiteral("x only"));
  QTextCursor before_only = editor->textCursor();
  before_only.setPosition(1);
  editor->setTextCursor(before_only);
  jwpqt::core::JwpSearchOptions wrap;
  wrap.wrap = true;
  require(window.find_text(QStringLiteral("only"), wrap),
          "Plain search did not find its only match");
  const QTextCursor only_match = editor->textCursor();
  require(!window.find_text(QStringLiteral("only"), wrap) &&
              editor->textCursor().selectionStart() ==
                  only_match.selectionStart() &&
              editor->textCursor().selectionEnd() == only_match.selectionEnd(),
          "Wrapped plain search reselected its only current match");
}

void test_jwp_find_uses_legacy_comparison(const QString& directory) {
  jwpqt::core::JwpDocument source;
  source.paragraphs = {paragraph(U"x\uff21y alpha"), paragraph(U""),
                       paragraph(U"beta alpha")};
  source.paragraphs[1].page_break = true;
  const QString path = directory + QStringLiteral("/find.jwp");
  jwpqt::qt::write_jwp_file(path, source);

  jwpqt::qt::MainWindow window;
  require(window.open_jwp_path(path), "Could not open JWP search fixture");
  QTextEdit* editor = window.findChild<QTextEdit*>();
  require(editor != nullptr, "JWP search window has no editor");

  require(window.find_text(QStringLiteral("A")),
          "JASCII equivalence did not find full-width ASCII");
  require(editor->textCursor().selectedText() == QStringLiteral("\uff21"),
          "JASCII search selected the wrong text");

  jwpqt::core::JwpSearchOptions exact;
  exact.ignore_ascii_case = false;
  exact.jascii_ascii_equivalence = false;
  QTextCursor cursor = editor->textCursor();
  cursor.setPosition(0);
  editor->setTextCursor(cursor);
  require(!window.find_text(QStringLiteral("A"), exact),
          "Exact JWP search matched full-width ASCII");

  require(window.find_text(QStringLiteral("ALPHA")),
          "Case-folded JWP search did not find ASCII text");
  require(editor->textCursor().selectedText() == QStringLiteral("alpha"),
          "JWP search selected the wrong ASCII match");

  QAction* find_next = find_action(window, "findNextAction");
  QAction* find_previous = find_action(window, "findPreviousAction");
  require(find_next != nullptr && find_previous != nullptr,
          "JWP repeat-search actions were not created");
  find_next->trigger();
  require(editor->textCursor().selectedText() == QStringLiteral("alpha") &&
              editor->textCursor().selectionStart() > 10,
          "JWP Find Next did not cross the hard-page-break paragraph");
  find_previous->trigger();
  require(editor->textCursor().selectionStart() < 10,
          "JWP Find Previous did not return to the first match");

  jwpqt::core::JwpSearchOptions wrapped;
  wrapped.direction = jwpqt::core::JwpSearchDirection::kBackward;
  wrapped.wrap = true;
  require(window.find_text(QStringLiteral("ALPHA"), wrapped) &&
              editor->textCursor().selectionStart() > 10,
          "Backward JWP search did not wrap to the final match");

  const QTextCursor selection = editor->textCursor();
  require(!window.find_text(QString::fromUtf8("\xF0\x9F\x98\x80")),
          "Unrepresentable JWP search pattern unexpectedly matched");
  require(editor->textCursor().selectionStart() == selection.selectionStart() &&
              editor->textCursor().selectionEnd() == selection.selectionEnd(),
          "Failed JWP search changed the current selection");
  require(!editor->document()->isModified() &&
              window.current_jwp_document() != nullptr &&
              *window.current_jwp_document() == source,
          "Searching changed the JWP source model");
}

void test_plain_text_replace_actions(const QString& directory, bool unicode = false) {
  const QString path = directory + QStringLiteral("/replace.txt");
  const QString suffix = unicode ? QString::fromStdU32String(U"\U0001f600") : QString();
  jwpqt::qt::write_text_file(
      path, jwpqt::core::TextFile{std::u32string(U"aa AA \u00e9 \u00c9") + suffix.toStdU32String(),
                                 jwpqt::core::TextEncoding::kUtf8, false});

  PromptingWindow window;
  require(window.open_path(path, jwpqt::core::TextEncoding::kUtf8),
          "Could not open plain replace fixture");
  window.next_replace = jwpqt::qt::ReplaceRequest{
      QStringLiteral("a"), QStringLiteral("x"),
      jwpqt::core::JwpSearchOptions{}, jwpqt::qt::ReplaceMode::kAll};
  QAction* replace = find_action(window, "replaceAction");
  require(replace != nullptr, "Native replace action was not created");
  replace->trigger();

  QTextEdit* editor = window.findChild<QTextEdit*>();
  require(window.replace_prompt_count == 1 && editor != nullptr &&
              editor->toPlainText() == QStringLiteral("xx xx \u00e9 \u00c9") + suffix,
          "Replace All action did not use ASCII-only comparison");
  find_action(window, "undoAction")->trigger();
  require(editor->toPlainText() ==
              (unicode ? QStringLiteral("ax xx \u00e9 \u00c9")
                       : QStringLiteral("xx xA \u00e9 \u00c9")) + suffix,
          "Replace All merged independent occurrences into one undo");
  for (int count = 0; count < 3; ++count) {
    find_action(window, "undoAction")->trigger();
  }
  require(editor->toPlainText() == QStringLiteral("aa AA \u00e9 \u00c9") + suffix,
          "Replace All occurrences were not separately undoable");

  QTextCursor cursor = editor->textCursor();
  cursor.setPosition(0);
  editor->setTextCursor(cursor);
  require(window.replace_next(QStringLiteral("a"), QStringLiteral("z")) &&
              editor->toPlainText() == QStringLiteral("az AA \u00e9 \u00c9") + suffix,
          "Replace Next did not replace the next candidate");
}

void test_jwp_replace_preserves_structure(const QString& directory) {
  jwpqt::core::JwpDocument source;
  source.summary[0] = jwpqt::core::encode_jwp_text(U"replace fixture");
  source.paragraphs = {paragraph(U"\uff21a", 125), paragraph(U"", 150),
                       paragraph(U"A", 175)};
  source.paragraphs[1].page_break = true;
  const QString path = directory + QStringLiteral("/replace.jwp");
  jwpqt::qt::write_jwp_file(path, source);

  jwpqt::qt::MainWindow window;
  require(window.open_jwp_path(path), "Could not open JWP replace fixture");
  QTextEdit* editor = window.findChild<QTextEdit*>();
  require(editor != nullptr, "JWP replace window has no editor");
  require(window.replace_all(QStringLiteral("a"), QStringLiteral("B")) == 3,
          "JWP Replace All did not use legacy comparison");
  require(editor->toPlainText() == QStringLiteral("BB\n\nB"),
          "JWP Replace All produced the wrong visible text");
  const jwpqt::core::JwpDocument* replaced = window.current_jwp_document();
  require(replaced != nullptr && replaced->summary == source.summary &&
              replaced->paragraphs.size() == 3 &&
              replaced->paragraphs[0].line_spacing == 125 &&
              replaced->paragraphs[1].page_break &&
              replaced->paragraphs[1].line_spacing == 150 &&
              replaced->paragraphs[2].line_spacing == 175,
          "JWP Replace All changed document structure or metadata");

  QAction* undo = find_action(window, "undoAction");
  QAction* redo = find_action(window, "redoAction");
  require(undo != nullptr && redo != nullptr && undo->isEnabled(),
          "Portable JWP undo action was not enabled");
  undo->trigger();
  require(editor->toPlainText() == QStringLiteral("BB\n\nA"),
          "JWP Replace All merged independent occurrences into one undo");
  undo->trigger();
  undo->trigger();
  require(window.current_jwp_document() != nullptr &&
              *window.current_jwp_document() == source,
          "JWP Replace All occurrences were not separately undoable");
  require(redo->isEnabled(), "Portable JWP redo action was not enabled");
  redo->trigger();
  redo->trigger();
  redo->trigger();
  require(editor->toPlainText() == QStringLiteral("BB\n\nB"),
          "Portable JWP redo did not restore Replace All");
  const int replace_all_caret = editor->textCursor().position();
  editor->insertPlainText(QStringLiteral("C"));
  undo->trigger();
  require(editor->toPlainText() == QStringLiteral("BB\n\nB") &&
              editor->textCursor().position() == replace_all_caret,
          "Edit after Replace All restored a stale portable caret");
  undo->trigger();
  undo->trigger();
  undo->trigger();

  QTextCursor selection = editor->textCursor();
  selection.setPosition(2);
  selection.setPosition(3, QTextCursor::KeepAnchor);
  editor->setTextCursor(selection);
  const QString before = editor->toPlainText();
  require(!window.replace_next(QStringLiteral("a"),
                               QString::fromUtf8("\xF0\x9F\x98\x80")) &&
              editor->toPlainText() == before &&
              editor->textCursor().selectionStart() == 2 &&
              editor->textCursor().selectionEnd() == 3 &&
              *window.current_jwp_document() == source,
          "Unrepresentable JWP replacement changed the document");
}

void test_jwp_open_edit_and_save(const QString& directory) {
  const QString source_path = directory + QStringLiteral("/source.jwp");
  const jwpqt::core::JwpDocument source = sample_jwp_document();
  jwpqt::qt::write_jwp_file(source_path, source);
  require(jwpqt::core::has_jwp_document_magic(
              std::string_view(read_bytes(source_path).constData(), 4)),
          "Written JWP file did not contain the JWP magic");

  jwpqt::qt::MainWindow window;
  require(window.open_path_detected(source_path),
          "Detected open did not recognize JWP document");
  require(window.is_jwp_document(), "Window did not retain JWP mode");
  require(window.current_jwp_document() != nullptr &&
              *window.current_jwp_document() == source,
          "JWP open did not retain document metadata");

  QTextEdit* editor = window.findChild<QTextEdit*>();
  require(editor != nullptr, "JWP window has no editor");
  require(editor->toPlainText() == QStringLiteral("A\u65e5\u672c\n\u00e9"),
          "JWP body was not exposed as Unicode text");
  require_jwp_layout(editor, source);
  QLabel* label = window.findChild<QLabel*>(QStringLiteral("documentEncoding"));
  require(label != nullptr &&
              label->text() == QStringLiteral("JWP / windows-1252"),
          "JWP status did not show the active code page");

  QTextCursor cursor = editor->textCursor();
  cursor.setPosition(0);
  cursor.setPosition(1, QTextCursor::KeepAnchor);
  cursor.insertText(QStringLiteral("B"));
  require(editor->document()->isModified(),
          "JWP text edit did not mark the document modified");

  jwpqt::core::JwpDocument expected = source;
  expected.paragraphs[0].text =
      jwpqt::core::encode_jwp_text(U"B\u65e5\u672c");
  require(window.current_jwp_document() != nullptr &&
              *window.current_jwp_document() == expected,
          "JWP text edit did not preserve non-body metadata and formatting");

  const QString saved_path = directory + QStringLiteral("/saved.jwp");
  require(window.save_path(saved_path), "Could not save edited JWP document");
  require(jwpqt::qt::read_jwp_file(saved_path) == expected,
          "Saved JWP document did not match the edited model");
  require(!editor->document()->isModified(),
          "Saving JWP did not clear modified state");

  const QString plain_path = directory + QStringLiteral("/after-jwp.txt");
  jwpqt::qt::write_text_file(
      plain_path,
      {U"plain", jwpqt::core::TextEncoding::kUtf8, false});
  require(window.open_path(plain_path, jwpqt::core::TextEncoding::kUtf8),
          "Could not switch from JWP to plain text");
  const QTextBlockFormat plain_format =
      editor->document()->begin().blockFormat();
  require(plain_format.leftMargin() == 0.0 &&
              plain_format.rightMargin() == 0.0 &&
              plain_format.textIndent() == 0.0 &&
              !plain_format
                   .property(jwpqt::qt::JwpEditor::kPageBreakProperty)
                   .toBool(),
          "Switching to plain text retained JWP paragraph layout");
}

void test_jwp_clipboard_changes(const QString& directory) {
  struct Case {
    QString before;
    int begin;
    int end;
    QString pasted;
    QString expected;
  };
  const Case cases[]{
      {{}, 0, 0, QStringLiteral("abc"), QStringLiteral("abc")},
      {QStringLiteral("prefix"), 6, 6, QStringLiteral("\n\u65e5\u672c\n"),
       QStringLiteral("prefix\n\u65e5\u672c\n")},
      {QStringLiteral("before"), 0, 6, QStringLiteral("\u65e5\u672c"),
       QStringLiteral("\u65e5\u672c")},
      {QStringLiteral("before"), 2, 4, QStringLiteral("\u65e5\u672c"),
       QStringLiteral("be\u65e5\u672cre")}};
  for (const auto& test : cases) {
    jwpqt::core::JwpDocument source;
    source.paragraphs = {paragraph(test.before.toStdU32String())};
    source.paragraphs[0].left_indent = 2;
    const QString path = directory + QStringLiteral("/clipboard.jwp");
    jwpqt::qt::write_jwp_file(path, source);
    jwpqt::qt::MainWindow window;
    require(window.open_jwp_path(path), "Could not open clipboard fixture");
    auto* editor = window.findChild<QTextEdit*>();
    QTextCursor cursor = editor->textCursor();
    cursor.setPosition(test.begin);
    cursor.setPosition(test.end, QTextCursor::KeepAnchor);
    editor->setTextCursor(cursor);
    QApplication::clipboard()->setText(test.pasted);
    editor->paste();
    require(editor->toPlainText() == test.expected && window.document_modified(),
            "Clipboard change including the terminal paragraph marker was rejected");
    find_action(window, "undoAction")->trigger();
    require(editor->toPlainText() == test.before && !window.document_modified() &&
                *window.current_jwp_document() == source,
            "Clipboard undo did not restore the original text and metadata");
    find_action(window, "redoAction")->trigger();
    require(editor->toPlainText() == test.expected && window.save_path(path),
            "Clipboard redo/save failed");
    jwpqt::qt::MainWindow reopened;
    require(reopened.open_jwp_path(path) &&
                reopened.findChild<QTextEdit*>()->toPlainText() == test.expected,
            "Clipboard text did not survive a native file round trip");
    const auto saved = *window.current_jwp_document();
    QApplication::clipboard()->setText(QString::fromUcs4(U"\U0001f600"));
    editor->paste();
    require(editor->toPlainText() == test.expected && !window.document_modified() &&
                *window.current_jwp_document() == saved,
            "Clipboard normalization bypassed lossless JWP encoding checks");
  }

  jwpqt::core::JwpDocument fragment_source;
  fragment_source.paragraphs = {paragraph(U"AB"), paragraph(U"CD")};
  fragment_source.paragraphs[0].text.insert(
      fragment_source.paragraphs[0].text.begin() + 1, 0x80);
  fragment_source.paragraphs[0].left_indent = 3;
  fragment_source.paragraphs[1].left_indent = 4;
  const QString source_path = directory + QStringLiteral("/clipboard-source.jwp");
  jwpqt::qt::write_jwp_file(source_path, fragment_source);
  jwpqt::qt::MainWindow window;
  require(window.open_jwp_path(source_path, jwpqt::core::LegacyCodePage::k1251),
          "Could not open private clipboard source");
  auto* editor = window.active_editor();
  QTextCursor cursor = editor->textCursor();
  cursor.select(QTextCursor::Document);
  editor->setTextCursor(cursor);
  editor->copy();
  const QMimeData* copied = QApplication::clipboard()->mimeData();
  require(copied->hasFormat(QString::fromLatin1(jwpqt::qt::kJwpClipboardMime)) &&
              copied->hasFormat(QString::fromLatin1(jwpqt::qt::kEncodedClipboardMime)) &&
              copied->hasText(),
          "Native copy did not publish private, encoded, and Unicode formats");

  jwpqt::core::JwpDocument target;
  target.paragraphs = {paragraph(U"xy")};
  target.paragraphs[0].left_indent = 7;
  const QString target_path = directory + QStringLiteral("/clipboard-target.jwp");
  jwpqt::qt::write_jwp_file(target_path, target);
  require(window.open_jwp_path(target_path, jwpqt::core::LegacyCodePage::k1251),
          "Could not open private clipboard target");
  editor = window.active_editor();
  cursor = editor->textCursor();
  cursor.setPosition(1);
  editor->setTextCursor(cursor);
  editor->paste();
  const auto pasted = window.current_jwp_document();
  require(editor->toPlainText() == QStringLiteral("xA\u0402B\nCDy") &&
              pasted != nullptr && pasted->paragraphs.size() == 2 &&
              pasted->paragraphs[0].left_indent == 7 &&
              pasted->paragraphs[0].text.at(2) == 0x80 &&
              pasted->paragraphs[1].left_indent == 4,
          "Private paste did not preserve source paragraph format and destination prefix");
  find_action(window, "undoAction")->trigger();
  require(editor->toPlainText() == QStringLiteral("xy") &&
              *window.current_jwp_document() == target,
           "Private clipboard insertion was not one native undo operation");

  const QString incompatible_path =
      directory + QStringLiteral("/clipboard-incompatible.jwp");
  jwpqt::qt::write_jwp_file(incompatible_path, target);
  require(window.open_jwp_path(incompatible_path),
          "Could not open incompatible clipboard target");
  editor = window.active_editor();
  editor->moveCursor(QTextCursor::End);
  editor->paste();
  require(editor->toPlainText() == QStringLiteral("xy") &&
              *window.current_jwp_document() == target &&
              !window.document_modified(),
          "Unrepresentable private clipboard data changed the destination");

  auto* fallback = new QMimeData;
  fallback->setData(QString::fromLatin1(jwpqt::qt::kJwpClipboardMime),
                    QByteArrayLiteral("malformed"));
  fallback->setText(QStringLiteral("z"));
  QApplication::clipboard()->setMimeData(fallback);
  editor->moveCursor(QTextCursor::End);
  editor->paste();
  require(editor->toPlainText() == QStringLiteral("xyz"),
          "Malformed private clipboard data did not fall back to Unicode text");
  find_action(window, "undoAction")->trigger();

  auto settings = window.application_settings();
  settings.clipboard_export = jwpqt::qt::ClipboardTextFormat::kShiftJis;
  settings.omit_clipboard_unicode = true;
  require(window.apply_application_settings(settings),
          "Could not apply clipboard export settings");
  cursor = editor->textCursor();
  cursor.select(QTextCursor::Document);
  editor->setTextCursor(cursor);
  editor->copy();
  copied = QApplication::clipboard()->mimeData();
  require(!copied->hasFormat(QStringLiteral("text/plain")) &&
              !copied->hasFormat(QStringLiteral("text/html")) &&
              copied->hasFormat(QString::fromLatin1(jwpqt::qt::kJwpClipboardMime)) &&
              copied->hasFormat(QString::fromLatin1(jwpqt::qt::kEncodedClipboardMime)),
          "Unicode omission did not retain only native and encoded text forms");

  const QString unicode_path = directory + QStringLiteral("/clipboard-unicode.txt");
  jwpqt::qt::write_text_file(
      unicode_path, {U"unicode:", jwpqt::core::TextEncoding::kUtf8, false});
  require(window.open_path(unicode_path, jwpqt::core::TextEncoding::kUtf8),
          "Could not open Unicode clipboard target");
  editor = window.active_editor();
  editor->moveCursor(QTextCursor::End);
  editor->paste();
  require(editor->toPlainText() == QStringLiteral("unicode:xy"),
          "Unicode document did not import the encoded clipboard fallback");
  find_action(window, "undoAction")->trigger();
  require(editor->toPlainText() == QStringLiteral("unicode:"),
          "Encoded clipboard fallback was not one Unicode undo operation");

  jwpqt::qt::ApplicationSettingsDialog clipboard_options(
      window.application_settings());
  auto* import_format =
      clipboard_options.findChild<QComboBox*>(QStringLiteral("settingsClipboardImport"));
  auto* export_format =
      clipboard_options.findChild<QComboBox*>(QStringLiteral("settingsClipboardExport"));
  auto* omit_unicode = clipboard_options.findChild<QCheckBox*>(
      QStringLiteral("settingsOmitClipboardUnicode"));
  require(import_format != nullptr && export_format != nullptr && omit_unicode != nullptr,
          "Clipboard Options controls were not exposed");
  import_format->setCurrentIndex(import_format->findData(
      static_cast<int>(jwpqt::qt::ClipboardTextFormat::kOldJis)));
  export_format->setCurrentIndex(export_format->findData(
      static_cast<int>(jwpqt::qt::ClipboardTextFormat::kUtf7)));
  omit_unicode->setChecked(false);
  clipboard_options.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
  require(clipboard_options.settings().clipboard_import ==
                  jwpqt::qt::ClipboardTextFormat::kOldJis &&
              clipboard_options.settings().clipboard_export ==
                  jwpqt::qt::ClipboardTextFormat::kUtf7 &&
              !clipboard_options.settings().omit_clipboard_unicode,
          "Clipboard Options did not retain selected source formats");
}

void test_application_settings_workflow(const QString& directory) {
  using namespace jwpqt::qt;
  MainWindow window;
  const QString settings_path = directory + QStringLiteral("/native-options.cfg");
  require(window.load_application_settings(settings_path) && !QFile::exists(settings_path),
          "Loading absent preferences wrote a file");
  const QString history_path = directory + QStringLiteral("/options-history.json");
  require(window.load_recent_file_configuration(history_path), "Could not configure history for settings test");
  window.show();
  auto* native = window.active_editor();
  native->insertPlainText(QStringLiteral("\u3042\u3044"));
  require(window.save_as_path(directory + QStringLiteral("/font-native.jwp"), std::nullopt),
          "Could not save native font fixture");
  native->moveCursor(QTextCursor::Start);
  native->moveCursor(QTextCursor::End);
  native->insertPlainText(QStringLiteral("\u3046"));
  find_action(window, "undoAction")->trigger();
  require(native->toPlainText() == QStringLiteral("\u3042\u3044") && !window.document_modified(),
          "Native font fixture did not establish a separate undo step");
  find_action(window, "redoAction")->trigger();
  auto selection = native->textCursor();
  selection.setPosition(1);
  selection.setPosition(2, QTextCursor::KeepAnchor);
  native->setTextCursor(selection);
  const auto model = *window.current_jwp_document();
  require(window.new_document_tab(false) == 1, "Could not create Unicode font fixture");
  auto* unicode = window.active_editor();
  unicode->insertPlainText(QStringLiteral("\U0001f600text"));
  require(window.save_as_path(directory + QStringLiteral("/font-unicode.txt"),
                             jwpqt::core::TextEncoding::kUtf8), "Could not save Unicode font fixture");
  auto edit = unicode->textCursor();
  edit.beginEditBlock();
  edit.insertText(QStringLiteral("!"));
  edit.endEditBlock();
  unicode->setTextCursor(edit);
  const QString unicode_text = unicode->toPlainText();
  const auto history_before = read_bytes(history_path);
  KanaInputField existing_query(QStringLiteral("existingFontQuery"), &window);

  auto settings = read_application_settings("Future_Option = untouched\nFile.Vert = true\n",
                                             window.application_settings());
  const auto file_role = static_cast<std::size_t>(JapaneseFontRole::kFile);
  settings.fonts[file_role] = {QFontDatabase::families().first(), 24, false};
  settings.fonts[static_cast<std::size_t>(JapaneseFontRole::kSystem)].size = 18;
  settings.fonts[static_cast<std::size_t>(JapaneseFontRole::kKanjiBar)] = {QString(), 22, false};
  settings.show_toolbar = false;
  settings.show_status_bar = false;
  settings.kanji_bar_at_top = true;
  settings.vertical_scrollbar = false;
  settings.kanji_bar_scrollbar = false;
  settings.save_recent_files = false;
  settings.save_settings_on_exit = false;
  settings.translation_code_page = 1251;
  require(window.apply_application_settings(settings), "Could not apply native font/settings options");
  require(window.active_editor() == unicode && unicode->toPlainText() == unicode_text &&
          window.document_modified() && unicode->document()->isUndoAvailable() &&
          native->font().pixelSize() == 24 && unicode->font().pixelSize() == 24 &&
          native->textCursor().anchor() == 1 && native->textCursor().position() == 2 &&
          existing_query.edit()->font().pixelSize() == 18 &&
          unicode->verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff &&
          window.findChild<QToolBar*>(QStringLiteral("mainToolBar"))->isHidden() && window.statusBar()->isHidden(),
          "Applying fonts lost editor state or failed to update existing widgets");
  auto* candidates = window.findChild<QListWidget*>(QStringLiteral("conversionCandidates"));
  require(candidates->font().pixelSize() == 22 &&
          candidates->horizontalScrollBarPolicy() == Qt::ScrollBarAlwaysOff,
          "Candidate font/scroll options were not applied");
  KanaInputField new_query(QStringLiteral("newFontQuery"), &window);
  require(new_query.edit()->font().pixelSize() == 18 &&
          window.resource_report().contains(QStringLiteral("Future_Option")),
          "New query fields or retained-settings warnings ignored preferences");
  find_action(window, "undoAction")->trigger();
  require(unicode->toPlainText() == QStringLiteral("\U0001f600text") && !window.document_modified(),
          "A font change replaced Unicode undo or its saved baseline");
  require(window.activate_document(0) && *window.current_jwp_document() == model && window.document_modified() &&
          window.jwp_code_page() == jwpqt::core::LegacyCodePage::k1252,
          "Background native model/history was altered by fonts");
  find_action(window, "undoAction")->trigger();
  require(native->toPlainText() == QStringLiteral("\u3042\u3044") && !window.document_modified(),
          "A font change replaced native undo or its saved baseline");
  require(window.new_document_tab() == 2 && window.active_editor()->font().pixelSize() == 24 &&
          window.jwp_code_page() == jwpqt::core::LegacyCodePage::k1251,
          "New documents ignored the configured font/code page");
  require(window.save_application_settings() && read_bytes(settings_path).contains("Future_Option = untouched"),
          "Saving settings lost imported fields");

  bool visited = false;
  QTimer::singleShot(0, [&] {
    auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
    if (!dialog) return;
    auto* size = dialog->findChild<QSpinBox*>(QStringLiteral("settingsFontSize4"));
    visited = size != nullptr;
    if (size) size->setValue(26);
    dialog->reject();
  });
  find_action(window, "applicationOptionsAction")->trigger();
  require(visited && window.application_settings().fonts[file_role].size == 24,
          "Cancelling Options applied edited fields");
  QTimer::singleShot(0, [&] {
    auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
    if (!dialog) return;
    auto* size = dialog->findChild<QSpinBox*>(QStringLiteral("settingsFontSize4"));
    auto* family = dialog->findChild<QComboBox*>(QStringLiteral("settingsFont4"));
    visited = size && family;
    if (!visited) { dialog->reject(); return; }
    size->setValue(26);
    family->setEditText(QString());
    dialog->findChild<QTabWidget*>()->setCurrentIndex(1);
    visited = dialog->grab().save(QDir::current().filePath(QStringLiteral("application-options-fonts.png")));
    auto* button = dialog->findChild<QDialogButtonBox*>();
    button->button(QDialogButtonBox::Ok)->click();
  });
  find_action(window, "applicationOptionsAction")->trigger();
  require(visited && window.application_settings().fonts[file_role].size == 26 &&
          window.application_settings().fonts[file_role].family.isEmpty() && native->font().pixelSize() == 26,
          "Options did not apply accepted fields or retain an automatic family");

  const auto accepted = window.application_settings();
  const auto accepted_text = write_application_settings(accepted);
  QTimer::singleShot(0, [] {
    auto* warning = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    if (warning) warning->button(QMessageBox::Cancel)->click();
  });
  find_action(window, "defaultSettingsAction")->trigger();
  require(write_application_settings(window.application_settings()) == accepted_text,
          "Cancelling Default Settings changed preferences");
  QTimer::singleShot(0, [] {
    auto* warning = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    if (warning) warning->button(QMessageBox::Yes)->click();
  });
  find_action(window, "defaultSettingsAction")->trigger();
  require(window.application_settings().fonts[file_role].size == 16 &&
          native->font().pixelSize() == 16 &&
          window.application_settings().source.find("Future_Option = untouched") != std::string::npos,
          "Default Settings did not reset native fields while retaining unknown source");
  require(window.apply_application_settings(accepted), "Could not restore preferences after defaults test");

  QTimer::singleShot(0, [] {
    auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
    if (dialog) dialog->reject();
  });
  find_action(window, "importSettingsAction")->trigger();
  require(write_application_settings(window.application_settings()) == accepted_text,
          "Cancelling Import Settings changed preferences");
  const QString import_path = directory + QStringLiteral("/import-options.cfg");
  write_bytes(import_path, "File.Auto=false\nFile.Size=28\nImported_Field=retained\n");
  visited = false;
  QTimer::singleShot(0, [&] {
    auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
    visited = dialog != nullptr;
    if (dialog) {
      dialog->selectFile(import_path);
      QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
    }
  });
  find_action(window, "importSettingsAction")->trigger();
  require(visited && native->font().pixelSize() == 28 &&
          !window.application_settings().save_recent_files &&
          window.application_settings().unapplied.contains(QStringLiteral("Imported_Field")),
          "Import Settings did not overlay native values or retain unknown imported fields");
  require(window.apply_application_settings(accepted), "Could not restore preferences after import test");
  find_action(window, "saveSettingsAction")->trigger();
  require(read_application_settings_file(settings_path).fonts[file_role].size == 26 &&
          read_bytes(settings_path).contains("Future_Option = untouched"),
          "Save Settings changed destination after an import or lost retained fields");

  const auto before = write_application_settings(window.application_settings());
  auto invalid = window.application_settings();
  invalid.fonts[file_role].size = 0;
  require(!window.apply_application_settings(invalid) &&
          write_application_settings(window.application_settings()) == before && native->font().pixelSize() == 26,
          "Invalid preferences partially changed the interface");
  require(window.save_application_settings(), "Could not save accepted preferences");
  const auto disk = read_bytes(settings_path);
  require(window.open_path(settings_path, jwpqt::core::TextEncoding::kUtf8, OpenMode::kNonInteractive, true) &&
          !window.save_application_settings() && read_bytes(settings_path) == disk,
          "Settings overwrote their own open document");
  require(read_bytes(history_path) == history_before && !window.recent_documents().empty(),
          "Disabling recent-file persistence wrote history or disabled the in-memory list");
}

void test_application_settings_preview(const QString& directory) {
  using namespace jwpqt::qt;
  QTemporaryDir preview_directory(directory + QStringLiteral("/font-preview-XXXXXX"));
  require(preview_directory.isValid(), "Could not isolate font conversion preferences");
  MainWindow window;
  const auto fixture = write_wnn_fixture(preview_directory.path());
  require(window.load_wnn_resources(fixture.index_path, fixture.data_path, fixture.preferences_path,
                                     OpenMode::kNonInteractive), "Could not load font preview conversion data");
  auto* editor = window.active_editor();
  editor->insertPlainText(QStringLiteral("\u3042"));
  editor->selectAll();
  require(window.convert_selection(), "Could not start conversion before font changes");
  const auto text = editor->toPlainText();
  const auto model = *window.current_jwp_document();
  auto* candidates = window.findChild<QListWidget*>(QStringLiteral("conversionCandidates"));
  const auto row = candidates->currentRow();
  auto settings = window.application_settings();
  settings.fonts[static_cast<std::size_t>(JapaneseFontRole::kSystem)].size = 20;
  settings.show_kanji_bar = false;
  require(window.apply_application_settings(settings) && window.conversion_active() &&
          *window.current_jwp_document() == model && editor->toPlainText() == text &&
          candidates->isHidden() && candidates->currentRow() == row,
          "Changing display settings accepted or damaged a conversion preview");
  settings.show_kanji_bar = true;
  require(window.apply_application_settings(settings) && !candidates->isHidden() &&
          window.cycle_conversion() && window.accept_conversion(),
          "Conversion could not continue after a font/display change");
  find_action(window, "undoAction")->trigger();
  require(editor->toPlainText() == QStringLiteral("\u3042"),
          "Font changes changed the conversion's undo transaction");
}

void test_application_settings_exit(const QString& directory) {
  using namespace jwpqt::qt;
  const QString path = directory + QStringLiteral("/exit-options.cfg");
  MainWindow window;
  require(window.load_application_settings(path), "Could not configure settings persistence");
  auto settings = window.application_settings();
  settings.show_toolbar = false;
  require(window.apply_application_settings(settings) && window.close() &&
          !read_application_settings_file(path).show_toolbar, "Settings were not saved on exit");

  MainWindow corrupted;
  write_bytes(path, "File.Size=bad");
  require(!corrupted.load_application_settings(path) && corrupted.close() &&
          read_bytes(path) == "File.Size=bad", "Corrupt startup settings were overwritten on exit");

  write_application_settings_file(path, ApplicationSettings{});
  MainWindow changed;
  require(changed.load_application_settings(path), "Could not load settings before external change");
  write_bytes(path, "File.Auto=bad");
  QTimer::singleShot(0, [] {
    auto* warning = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    if (warning) warning->button(QMessageBox::Cancel)->click();
  });
  require(!changed.close() && changed.document_count() == 1 && read_bytes(path) == "File.Auto=bad",
          "Failed automatic settings save ignored cancellation or replaced corrupt bytes");

  MainWindow configured;
  write_bytes(path, "TranslationCodePage=1251");
  require(configured.load_application_settings(path), "Could not load the default translation code page");
  jwpqt::core::TextFile text;
  text.text = U"\u0402";
  const auto text_path = directory + QStringLiteral("/default-cp1251.txt");
  write_text_file(text_path, text);
  require(configured.open_path(text_path, jwpqt::core::TextEncoding::kUtf8, OpenMode::kNonInteractive) &&
          configured.is_jwp_document() && configured.jwp_code_page() == jwpqt::core::LegacyCodePage::k1251 &&
          configured.active_editor()->toPlainText() == QStringLiteral("\u0402"),
          "A fresh imported text file ignored the configured translation code page");
}

void test_jwp_code_page_switch(const QString& directory) {
  jwpqt::core::JwpDocument source;
  source.paragraphs = {jwpqt::core::JwpParagraph{{0xc0}}};
  source.paragraphs[0].left_indent = 2;
  source.paragraphs[0].line_spacing = 125;
  const QString source_path = directory + QStringLiteral("/code-page.jwp");
  jwpqt::qt::write_jwp_file(source_path, source);

  jwpqt::qt::MainWindow window;
  require(window.open_jwp_path(source_path),
          "Could not open JWP code-page fixture");
  QTextEdit* editor = window.findChild<QTextEdit*>();
  require(editor != nullptr &&
              editor->toPlainText() == QStringLiteral("\u00c0"),
          "Default CP1252 interpretation was incorrect");

  QAction* cp1251 =
      find_encoding_action(window, QStringLiteral("windows-1251"));
  require(cp1251 != nullptr, "CP1251 action was not created");
  cp1251->trigger();
  require(window.jwp_code_page() == jwpqt::core::LegacyCodePage::k1251 &&
              editor->toPlainText() == QStringLiteral("\u0410"),
          "Changing JWP code page did not reinterpret extended bytes");
  require_jwp_layout(editor, source);
  require(!editor->document()->isModified(),
          "Changing JWP code page changed the source document");

  const QString saved_path = directory + QStringLiteral("/code-page-saved.jwp");
  require(window.save_path(saved_path), "Could not save code-page fixture");
  require(jwpqt::qt::read_jwp_file(saved_path).paragraphs[0].text ==
              jwpqt::core::JwpText{0xc0},
          "Code-page reinterpretation changed unchanged JWP tokens");
}

void test_jwp_code_page_can_be_selected_before_open(const QString& directory) {
  jwpqt::core::JwpDocument source;
  source.paragraphs = {jwpqt::core::JwpParagraph{{0x80}}};
  const QString source_path = directory + QStringLiteral("/cp1251-only.jwp");
  jwpqt::qt::write_jwp_file(source_path, source);

  jwpqt::qt::MainWindow window;
  QAction* cp1251 =
      find_encoding_action(window, QStringLiteral("windows-1251"));
  require(cp1251 != nullptr && cp1251->isEnabled(),
          "JWP code page could not be selected before opening a file");
  cp1251->trigger();
  require(window.jwp_code_page() == jwpqt::core::LegacyCodePage::k1251,
          "Preselected JWP code page was not retained");
  require(window.open_path_detected(source_path,
                                    jwpqt::qt::OpenMode::kNonInteractive),
          "JWP file did not use the preselected code page");
  QTextEdit* editor = window.findChild<QTextEdit*>();
  require(editor != nullptr &&
              editor->toPlainText() == QStringLiteral("\u0402"),
          "CP1251-only JWP byte did not decode after preselection");
}

void test_zero_paragraph_jwp_save_is_not_normalized(const QString& directory) {
  jwpqt::core::JwpDocument source;
  source.summary[0] = {static_cast<jwpqt::core::JisCode>('Z')};
  const QString source_path = directory + QStringLiteral("/empty-source.jwp");
  const QString saved_path = directory + QStringLiteral("/empty-saved.jwp");
  jwpqt::qt::write_jwp_file(source_path, source);

  jwpqt::qt::MainWindow window;
  require(window.open_jwp_path(source_path),
          "Could not open zero-paragraph JWP fixture");
  require(window.save_path(saved_path),
          "Could not save zero-paragraph JWP fixture");
  require(jwpqt::qt::read_jwp_file(saved_path) == source,
          "Unedited zero-paragraph JWP file was normalized on save");

  QTextEdit* editor = window.findChild<QTextEdit*>();
  require(editor != nullptr, "Editor was not created for zero-paragraph JWP");
  editor->insertPlainText(QStringLiteral("A"));
  require(editor->document()->isModified(),
          "Editing zero-paragraph JWP did not mark it modified");
  QAction* undo = find_action(window, "undoAction");
  require(undo != nullptr && undo->isEnabled(),
          "Zero-paragraph edit did not enable portable undo");
  undo->trigger();
  require(editor->toPlainText().isEmpty() &&
              !editor->document()->isModified(),
          "Undo did not restore zero-paragraph JWP saved state");
  const QString undo_saved_path =
      directory + QStringLiteral("/empty-undo-saved.jwp");
  require(window.save_path(undo_saved_path),
          "Could not save undone zero-paragraph JWP fixture");
  require(jwpqt::qt::read_jwp_file(undo_saved_path) == source,
          "Edit then undo normalized a zero-paragraph JWP file");
}

void test_jwp_rejects_lossy_edits(const QString& directory) {
  jwpqt::core::JwpDocument source;
  source.paragraphs = {paragraph(U"A"), paragraph(U""), paragraph(U"B")};
  source.paragraphs[1].page_break = true;
  const QString source_path = directory + QStringLiteral("/page-break.jwp");
  jwpqt::qt::write_jwp_file(source_path, source);

  jwpqt::qt::MainWindow window;
  require(window.open_jwp_path(source_path),
          "Could not open page-break fixture");
  QTextEdit* editor = window.findChild<QTextEdit*>();
  require(editor != nullptr && editor->toPlainText() == QStringLiteral("A\n\nB"),
          "Page-break fixture was not rendered predictably");

  QTextCursor cursor = editor->textCursor();
  cursor.setPosition(1);
  cursor.setPosition(2, QTextCursor::KeepAnchor);
  cursor.removeSelectedText();
  require(editor->toPlainText() == QStringLiteral("A\n\nB") &&
              window.current_jwp_document() != nullptr &&
              *window.current_jwp_document() == source,
          "Edit crossing a hard page break was not reverted");
  require(editor->textCursor().hasSelection() &&
              editor->textCursor().selectionStart() == 1 &&
              editor->textCursor().selectionEnd() == 2,
          "Rejected deletion did not restore the deleted selection");

  cursor = editor->textCursor();
  cursor.setPosition(1);
  cursor.insertText(QStringLiteral("C"));
  require(editor->toPlainText() == QStringLiteral("AC\n\nB"),
          "Representable JWP edit did not apply before rejection test");

  cursor = editor->textCursor();
  cursor.setPosition(0);
  cursor.setPosition(1, QTextCursor::KeepAnchor);
  editor->setTextCursor(cursor);
  cursor.insertText(QString::fromUtf8("\xF0\x9F\x98\x80"));
  require(editor->toPlainText() == QStringLiteral("AC\n\nB"),
          "Unrepresentable JWP text edit was not reverted");
  require(editor->textCursor().hasSelection() &&
              editor->textCursor().selectionStart() == 0 &&
              editor->textCursor().selectionEnd() == 1,
          "Rejected replacement did not restore the prior selection");
  require_jwp_layout(editor, *window.current_jwp_document());
  QAction* undo = find_action(window, "undoAction");
  require(undo != nullptr && undo->isEnabled(),
          "Valid JWP edit did not enable portable undo");
  undo->trigger();
  require(editor->toPlainText() == QStringLiteral("A\n\nB") &&
              *window.current_jwp_document() == source &&
              !editor->document()->isModified(),
          "Rejected edit destroyed the prior valid undo entry");
}

void test_jwp_history_actions(const QString& directory) {
  jwpqt::core::JwpDocument source;
  source.paragraphs = {paragraph(U"A")};
  source.paragraphs[0].right_indent = 2;
  source.paragraphs[0].line_spacing = 125;
  const QString source_path = directory + QStringLiteral("/history.jwp");
  const QString saved_path = directory + QStringLiteral("/history-saved.jwp");
  jwpqt::qt::write_jwp_file(source_path, source);

  jwpqt::qt::MainWindow window;
  require(window.open_jwp_path(source_path),
          "Could not open portable history fixture");
  QTextEdit* editor = window.findChild<QTextEdit*>();
  QAction* undo = find_action(window, "undoAction");
  QAction* redo = find_action(window, "redoAction");
  require(editor != nullptr && undo != nullptr && redo != nullptr &&
              !undo->isEnabled() && !redo->isEnabled(),
          "Fresh JWP document did not start with empty history");
  QAction* context_undo = nullptr;
  for (QAction* action : editor->actions()) {
    if (action->objectName() == QStringLiteral("undoAction")) {
      context_undo = action;
    }
  }
  require(editor->contextMenuPolicy() == Qt::ActionsContextMenu &&
              context_undo == undo,
          "Editor context menu does not route through portable JWP undo");

  QTextCursor cursor = editor->textCursor();
  cursor.movePosition(QTextCursor::End);
  editor->setTextCursor(cursor);
  editor->insertPlainText(QStringLiteral("B"));
  editor->insertPlainText(QStringLiteral("C"));
  require(editor->toPlainText() == QStringLiteral("ABC") &&
              undo->isEnabled(),
          "JWP typing was not recorded in portable history");
  context_undo->trigger();
  require(editor->toPlainText() == QStringLiteral("A") &&
              !undo->isEnabled() && redo->isEnabled(),
          "Consecutive JWP typing did not coalesce into one undo");
  redo->trigger();
  require(editor->toPlainText() == QStringLiteral("ABC") &&
              undo->isEnabled() && !redo->isEnabled(),
          "Portable JWP redo did not restore typing");
  require_jwp_layout(editor, *window.current_jwp_document());

  require(window.save_path(saved_path), "Could not save history fixture");
  require(undo->isEnabled(), "Saving unexpectedly cleared portable history");
  undo->trigger();
  require(editor->toPlainText() == QStringLiteral("A") &&
              editor->document()->isModified(),
          "Undo after save did not restore pre-edit state");
}

void test_jwp_paragraph_formatting(const QString& directory) {
  jwpqt::core::JwpDocument source;
  source.paragraphs = {paragraph(U"A"), paragraph(U""), paragraph(U"B")};
  source.paragraphs[0].left_indent = 1;
  source.paragraphs[0].right_indent = 2;
  source.paragraphs[0].first_indent = -1;
  source.paragraphs[0].line_spacing = 110;
  source.paragraphs[1].left_indent = 3;
  source.paragraphs[1].right_indent = 4;
  source.paragraphs[1].first_indent = -2;
  source.paragraphs[1].line_spacing = 120;
  source.paragraphs[1].page_break = true;
  source.paragraphs[2].left_indent = 5;
  source.paragraphs[2].right_indent = 6;
  source.paragraphs[2].first_indent = -3;
  source.paragraphs[2].line_spacing = 130;
  const QString source_path = directory + QStringLiteral("/paragraph-format.jwp");
  const QString saved_path =
      directory + QStringLiteral("/paragraph-format-saved.jwp");
  jwpqt::qt::write_jwp_file(source_path, source);

  PromptingWindow window;
  require(window.open_jwp_path(source_path),
          "Could not open paragraph-format fixture");
  window.show();
  QApplication::processEvents();
  QTextEdit* editor = window.findChild<QTextEdit*>();
  QAction* format = find_action(window, "formatParagraphAction");
  QAction* format_file = find_action(window, "formatFileAction");
  QAction* page_layout = find_action(window, "pageLayoutAction");
  QAction* undo = find_action(window, "undoAction");
  QAction* redo = find_action(window, "redoAction");
  require(editor != nullptr && format != nullptr && format_file != nullptr &&
              page_layout != nullptr && undo != nullptr && redo != nullptr &&
              format->isEnabled() && format_file->isEnabled() &&
              format->shortcut() ==
                  QKeySequence(QStringLiteral("Alt+Shift+F")) &&
              format_file->shortcut() ==
                  QKeySequence(QStringLiteral("Alt+Ctrl+F")) &&
              page_layout->shortcut() ==
                  QKeySequence(QStringLiteral("Alt+L")),
          "JWP paragraph-format controls were not enabled");

  QTextCursor selection = editor->textCursor();
  selection.setPosition(0);
  selection.setPosition(2, QTextCursor::KeepAnchor);
  editor->setTextCursor(selection);

  format->trigger();
  require(window.paragraph_format_prompt_count == 1 &&
              window.offered_paragraph_format ==
                  jwpqt::core::JwpParagraphFormat{3, 4, -2, 120} &&
              *window.current_jwp_document() == source,
          "Cancelled paragraph formatting changed the JWP document");

  const jwpqt::core::JwpParagraphFormat applied{12, 23, -7, 175};
  window.next_paragraph_format = applied;
  format->trigger();
  require(window.paragraph_format_prompt_count == 2,
          "Paragraph-format action did not prompt exactly once");
  const jwpqt::core::JwpDocument formatted = *window.current_jwp_document();
  require(formatted.paragraphs[0].left_indent == 12 &&
              formatted.paragraphs[0].right_indent == 23 &&
              formatted.paragraphs[0].first_indent == -7 &&
              formatted.paragraphs[0].line_spacing == 175 &&
              formatted.paragraphs[1].left_indent == 12 &&
              formatted.paragraphs[1].right_indent == 23 &&
              formatted.paragraphs[1].first_indent == -7 &&
              formatted.paragraphs[1].line_spacing == 175 &&
              formatted.paragraphs[1].page_break &&
              formatted.paragraphs[2] == source.paragraphs[2],
          "Paragraph formatting did not apply to the inclusive selection");
  require(editor->textCursor().selectionStart() == 0 &&
              editor->textCursor().selectionEnd() == 2 &&
              editor->document()->isModified() && undo->isEnabled(),
          "Paragraph formatting did not preserve selection and history");
  require_jwp_layout(editor, formatted);

  require(!window.format_paragraphs({1, 0, -2, 100}) &&
              *window.current_jwp_document() == formatted,
          "Invalid native paragraph format changed the document");
  require(window.format_paragraphs(applied),
          "No-op paragraph formatting was rejected");
  undo->trigger();
  require(*window.current_jwp_document() == source && redo->isEnabled(),
          "Paragraph formatting did not undo as one transaction");
  redo->trigger();
  require(*window.current_jwp_document() == formatted,
          "Paragraph formatting redo did not restore the transaction");
  window.resize(320, 300);
  QApplication::processEvents();
  require(window.format_paragraphs(applied) &&
              *window.current_jwp_document() == formatted,
          "Unchanged paragraph format failed after the page narrowed");
  require(!window.format_paragraphs({255, 255, -127, 100}) &&
              *window.current_jwp_document() == formatted,
          "Paragraph formatting accepted margins wider than the page");

  require(window.save_path(saved_path) &&
              jwpqt::qt::read_jwp_file(saved_path) == formatted,
          "Paragraph formatting did not survive a JWP save");

  const jwpqt::core::JwpParagraphFormat file_format{7, 8, -4, 220};
  window.resize(900, 680);
  QApplication::processEvents();
  selection.setPosition(0);
  selection.setPosition(2, QTextCursor::KeepAnchor);
  editor->setTextCursor(selection);
  window.next_paragraph_format = file_format;
  format_file->trigger();
  const jwpqt::core::JwpDocument formatted_file =
      *window.current_jwp_document();
  require(window.paragraph_format_prompt_count == 3 &&
              formatted_file.paragraphs.size() == source.paragraphs.size(),
          "Format File did not prompt or preserve paragraph count");
  for (std::size_t index = 0; index < formatted_file.paragraphs.size(); ++index) {
    const auto& actual = formatted_file.paragraphs[index];
    const auto& original = source.paragraphs[index];
    require(actual.left_indent == file_format.left_indent &&
                actual.right_indent == file_format.right_indent &&
                actual.first_indent == file_format.first_indent &&
                actual.line_spacing == file_format.line_spacing &&
                actual.text == original.text &&
                actual.page_break == original.page_break,
            "Format File did not apply one format without changing content");
  }
  require(editor->textCursor().selectionStart() == 0 &&
              editor->textCursor().selectionEnd() == 2,
          "Format File did not preserve the editor selection");
  undo->trigger();
  require(*window.current_jwp_document() == formatted,
          "Format File did not undo as one transaction");
  redo->trigger();
  require(*window.current_jwp_document() == formatted_file,
          "Format File redo did not restore every paragraph");

  const QString text_path = directory + QStringLiteral("/paragraph-format.txt");
  write_bytes(text_path, QByteArray("plain"));
  require(window.open_path(text_path, jwpqt::core::TextEncoding::kUtf8) &&
              format->isEnabled() && format_file->isEnabled() &&
              window.format_paragraphs(applied) && !window.uses_jwp_format(),
          "Native text editing did not support paragraph formatting");

  PromptingWindow mismatched;
  require(mismatched.open_jwp_path(source_path),
          "Could not open paragraph layout failure fixture");
  QTextEdit* mismatched_editor = mismatched.findChild<QTextEdit*>();
  QAction* mismatched_undo = find_action(mismatched, "undoAction");
  require(mismatched_editor != nullptr && mismatched_undo != nullptr,
          "Paragraph layout failure fixture has no controls");
  {
    const QSignalBlocker blocker(mismatched_editor->document());
    mismatched_editor->append(QStringLiteral("orphan block"));
  }
  mismatched_editor->document()->setModified(false);
  require(!mismatched.format_paragraphs(applied) &&
              *mismatched.current_jwp_document() == source &&
              !mismatched_undo->isEnabled() &&
              !mismatched_editor->document()->isModified(),
          "Failed paragraph layout published document or history state");

  PromptingWindow hidden;
  require(hidden.open_jwp_path(source_path) &&
              hidden.format_paragraphs({20, 20, -10, 150}) &&
              hidden.current_jwp_document()->paragraphs[0].left_indent == 20,
          "Hidden editor geometry rejected a valid paragraph format");
}

void test_jwp_page_break_insertion(const QString& directory) {
  jwpqt::core::JwpDocument source;
  source.paragraphs = {paragraph(U"AB"), paragraph(U"CD")};
  source.paragraphs[0].left_indent = 3;
  source.paragraphs[0].right_indent = 4;
  source.paragraphs[0].first_indent = -2;
  source.paragraphs[0].line_spacing = 125;
  const QString source_path = directory + QStringLiteral("/page-break.jwp");
  const QString saved_path =
      directory + QStringLiteral("/page-break-saved.jwp");
  jwpqt::qt::write_jwp_file(source_path, source);

  jwpqt::qt::MainWindow window;
  require(window.open_jwp_path(source_path),
          "Could not open page-break fixture");
  QTextEdit* editor = window.findChild<QTextEdit*>();
  QAction* insert = find_action(window, "insertPageBreakAction");
  QAction* undo = find_action(window, "undoAction");
  QAction* redo = find_action(window, "redoAction");
  require(editor != nullptr && insert != nullptr && undo != nullptr &&
              redo != nullptr && insert->isEnabled() &&
              insert->shortcuts().contains(
                  QKeySequence(Qt::CTRL | Qt::Key_Return)) &&
              insert->shortcuts().contains(
                  QKeySequence(Qt::CTRL | Qt::Key_Enter)),
          "JWP page-break controls were not enabled");

  QTextCursor selection = editor->textCursor();
  selection.setPosition(1);
  selection.setPosition(4, QTextCursor::KeepAnchor);
  editor->setTextCursor(selection);
  insert->trigger();

  const jwpqt::core::JwpDocument inserted =
      *window.current_jwp_document();
  require(inserted.paragraphs.size() == 3 &&
              jwpqt::core::decode_jwp_text(inserted.paragraphs[0].text) ==
                  U"A" &&
              inserted.paragraphs[1].text.empty() &&
              inserted.paragraphs[1].page_break &&
              jwpqt::core::decode_jwp_text(inserted.paragraphs[2].text) ==
                  U"D" &&
              inserted.paragraphs[1].left_indent == 3 &&
              inserted.paragraphs[2].line_spacing == 125,
          "Selected text was not replaced by one formatted page break");
  require(editor->toPlainText() == QStringLiteral("A\n\nD") &&
              !editor->textCursor().hasSelection() &&
              editor->textCursor().position() == 3 &&
              editor->document()->isModified() && undo->isEnabled(),
          "Page-break insertion did not restore the following caret");
  require_jwp_layout(editor, inserted);

  undo->trigger();
  require(*window.current_jwp_document() == source &&
              editor->toPlainText() == QStringLiteral("AB\nCD") &&
              !editor->document()->isModified() && redo->isEnabled(),
          "Page-break insertion did not undo as one transaction");
  redo->trigger();
  require(*window.current_jwp_document() == inserted &&
              editor->textCursor().position() == 3,
          "Page-break insertion redo did not restore structure and caret");
  require(window.save_path(saved_path) &&
              jwpqt::qt::read_jwp_file(saved_path) == inserted,
          "Inserted page break did not survive a JWP save");

  const QString plain_path = directory + QStringLiteral("/page-break.txt");
  write_bytes(plain_path, QByteArray("plain"));
  require(window.open_path(plain_path, jwpqt::core::TextEncoding::kUtf8) &&
              insert->isEnabled() && window.insert_page_break() &&
              !window.uses_jwp_format(),
          "Native text editing did not support structural page breaks");
}

void test_jwp_rejects_non_bmp_edit(const QString& directory) {
  jwpqt::core::JwpDocument source;
  source.paragraphs = {paragraph(U"AB")};
  const QString source_path = directory + QStringLiteral("/non-bmp.jwp");
  jwpqt::qt::write_jwp_file(source_path, source);

  jwpqt::qt::MainWindow window;
  require(window.open_jwp_path(source_path),
          "Could not open non-BMP rejection fixture");
  QTextEdit* editor = window.findChild<QTextEdit*>();
  require(editor != nullptr, "Editor was not created for non-BMP fixture");
  QTextCursor cursor = editor->textCursor();
  cursor.setPosition(1);
  cursor.insertText(QString::fromUtf8("\xF0\x9F\x98\x80"));
  require(editor->toPlainText() == QStringLiteral("AB") &&
              window.current_jwp_document() != nullptr &&
              *window.current_jwp_document() == source &&
              !editor->document()->isModified(),
          "Rejected non-BMP edit corrupted the JWP document");
}

void test_katakana_policy(const QString& directory) {
  using namespace jwpqt::core;
  using namespace jwpqt::qt;
  require(!ApplicationSettings{}.old_katakana_input &&
              read_application_settings("old_katakana_input = true\n").old_katakana_input,
          "Katakana policy defaults or alias changed");
  bool rejected = false;
  try { (void)read_application_settings("OldKatakanaVowelHandling = invalid\nold_katakana_input = true\n"); }
  catch (const std::exception&) { rejected = true; }
  require(rejected, "Invalid earlier katakana policy accepted");
  for (const bool old : {false, true}) {
    MainWindow window;
    MainWindow other;
    auto policy = window.application_settings();
    ApplicationSettingsDialog dialog(policy);
    auto* option = dialog.findChild<QCheckBox*>(QStringLiteral("settingsOldKatakana"));
    require(option && !option->isChecked(), "Old katakana option missing");
    option->setChecked(old);
    dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
    policy = dialog.settings();
    require(!window.application_settings().old_katakana_input, "Staged policy changed live input");
    auto* editor = window.active_editor();
    send_text_key(editor, Qt::Key_A, QStringLiteral("A"), Qt::ShiftModifier);
    require(editor->toPlainText().isEmpty(), "Pending vowel unexpectedly emitted");
    require(window.apply_application_settings(policy), "Could not apply pending vowel policy");
    send_text_key(editor, Qt::Key_Apostrophe, QStringLiteral("'"));
    const auto expected = old ? QStringLiteral("\u30a2\u300d") : QStringLiteral("\u30a2");
    require(editor->toPlainText() == expected, "Document ignored live vowel policy");
    find_action(window, "undoAction")->trigger();
    require(editor->toPlainText().isEmpty() && !window.document_modified(), "Vowel policy input did not undo cleanly");

    KanaInputField query(QStringLiteral("oldKanaQuery"), &window);
    KanaInputField isolated(QStringLiteral("isolatedKanaQuery"), &other);
    const auto key = [](QLineEdit* edit, int code, const QString& text) {
      QKeyEvent event(QEvent::KeyPress, code, Qt::NoModifier, text);
      QApplication::sendEvent(edit, &event);
    };
    key(query.edit(), Qt::Key_A, "A");
    auto changed = policy; changed.old_katakana_input = !old;
    require(window.apply_application_settings(changed) && query.edit()->text().isEmpty(), "Query policy change flushed pending input");
    key(query.edit(), Qt::Key_Apostrophe, "'");
    require(query.edit()->text() == (!old ? QStringLiteral("\u30a2\u300d") : QStringLiteral("\u30a2")),
            std::string("Query ignored owning workspace policy: ") + query.edit()->text().toUtf8().toHex().constData() +
                " setting=" + std::to_string(window.property("jwpqtOldKatakanaInput").toBool()));
    key(isolated.edit(), Qt::Key_A, "A"); key(isolated.edit(), Qt::Key_Apostrophe, "'");
    require(isolated.edit()->text() == QStringLiteral("\u30a2"), "Katakana preference leaked to another workspace");

    require(window.apply_application_settings(policy), "Could not restore replay policy");
    editor->insertPlainText(QStringLiteral("A'"));
    editor->selectAll();
    require(window.convert_selection() && editor->textCursor().selectedText() == expected,
            "Selected replay ignored katakana policy");
    find_action(window, "undoAction")->trigger();
    require(editor->toPlainText() == QStringLiteral("A'"), "Katakana replay lost original text on undo");
    const auto path = directory + (old ? "/old-kana" : "/modern-kana");
    require(window.save_as_path(path + ".jwp", std::nullopt) &&
                window.save_application_settings(path + ".cfg") && window.save_project_path(path + ".jpr", false), "Could not save katakana policy");
    MainWindow restored;
    require(restored.load_application_settings(path + ".cfg") && restored.application_settings().old_katakana_input == old &&
                restored.open_project_path(path + ".jpr") && restored.application_settings().old_katakana_input == old,
            "Katakana preference did not survive settings and project restoration");
  }
}

void test_control_arrow_conversion(const QString& directory) {
  using namespace jwpqt::qt;
  using namespace jwpqt::core;
  require(!ApplicationSettings{}.ctrl_up_down_convert &&
              read_application_settings("ctrl_up_down_convert = true\n").ctrl_up_down_convert,
          "Control conversion source policy changed");
  bool rejected = false;
  try { (void)read_application_settings("CtrlUpDownConvertKanji = invalid\nctrl_up_down_convert = true\n"); }
  catch (const std::exception&) { rejected = true; }
  require(rejected, "Invalid earlier control conversion policy accepted");
  const QString root = directory + "/control-conversion";
  require(QDir().mkpath(root), "Could not isolate control conversion preferences");
  const auto fixture = write_wnn_fixture(root);
  JwpDocument source; source.paragraphs.resize(1); source.paragraphs[0].text = {0x2422};
  const auto path = root + "/reading.jwp";
  write_jwp_file(path, source);
  MainWindow window;
  require(window.open_jwp_path(path) && window.load_wnn_resources(fixture.index_path, fixture.data_path, fixture.preferences_path),
          "Could not prepare control conversion");
  auto* editor = window.active_editor();
  const auto key = [&](int code, Qt::KeyboardModifiers modifiers = Qt::ControlModifier) {
    QKeyEvent override(QEvent::ShortcutOverride, code, modifiers);
    QApplication::sendEvent(editor, &override);
    QKeyEvent press(QEvent::KeyPress, code, modifiers);
    QApplication::sendEvent(editor, &press);
  };
  editor->selectAll(); key(Qt::Key_Up);
  require(!window.conversion_active() && *window.current_jwp_document() == source &&
              editor->textCursor().hasSelection(),
          "Disabled control conversion altered the document selection");
  ApplicationSettingsDialog options(window.application_settings());
  options.findChild<QCheckBox*>("settingsCtrlConvert")->setChecked(true);
  require(!window.application_settings().ctrl_up_down_convert, "Staged control setting changed live policy");
  options.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
  require(window.apply_application_settings(options.settings()), "Could not apply control conversion policy");
  editor->moveCursor(QTextCursor::End); key(Qt::Key_Up);
  require(!window.conversion_active(), "Unselected control arrow started conversion");
  editor->selectAll(); key(Qt::Key_Down, Qt::ControlModifier | Qt::AltModifier);
  require(!window.conversion_active() && *window.current_jwp_document() == source, "Alt-modified arrow started conversion");
  editor->setReadOnly(true); editor->selectAll(); key(Qt::Key_Down);
  require(!window.conversion_active() && *window.current_jwp_document() == source, "Read-only document started conversion");
  editor->setReadOnly(false);
  editor->selectAll(); key(Qt::Key_Down);
  require(window.conversion_active() && window.current_jwp_document()->paragraphs[0].text == JwpText{0x3021},
          "Control Down did not start the normal first candidate");
  auto resized = window.application_settings(); resized.maximum_undo_levels = 3;
  require(!window.apply_application_settings(resized) && window.conversion_active() &&
              window.application_settings().maximum_undo_levels == 50,
          "Undo resizing accepted or invalidated an active conversion");
  key(Qt::Key_Up);
  require(window.conversion_active() && window.current_jwp_document()->paragraphs[0].text == JwpText{0x3022},
          "Control Up did not cycle forward without acceptance");
  key(Qt::Key_Down, Qt::ControlModifier | Qt::ShiftModifier);
  require(window.conversion_active() && window.current_jwp_document()->paragraphs[0].text == JwpText{0x3021},
          "Control Shift Down did not cycle backward without acceptance");
  key(Qt::Key_Down);
  require(window.conversion_active() && *window.current_jwp_document() == source, "Backward cycling skipped the original kana");
  key(Qt::Key_Up);
  require(window.conversion_active() && window.current_jwp_document()->paragraphs[0].text == JwpText{0x3021},
          "Forward cycling did not wrap from original kana");
  require(window.accept_conversion(), "Could not finish control conversion");
  find_action(window, "undoAction")->trigger();
  require(*window.current_jwp_document() == source && !window.document_modified(), "Control cycling damaged undo or metadata");
  require(window.save_application_settings(root + "/settings.cfg") &&
              window.save_project_path(root + "/project.jpr", false), "Could not persist control policy");
  MainWindow restored;
  require(restored.load_application_settings(root + "/settings.cfg") && restored.application_settings().ctrl_up_down_convert &&
              restored.open_project_path(root + "/project.jpr") && restored.application_settings().ctrl_up_down_convert,
          "Control conversion policy did not survive restart/project");
  find_action(window, "newTextDocumentAction")->trigger();
  editor = window.active_editor(); editor->insertPlainText("a"); editor->selectAll(); key(Qt::Key_Down);
  require(!window.conversion_active() && document_plain_text(*editor->document()) == "a",
          "Control policy changed an unrestricted Unicode document");
  QString lines;
  for (int i = 0; i < 80; ++i) lines += QStringLiteral("line %1\n").arg(i);
  editor->setPlainText(lines);
  window.resize(420, 240); window.show(); QApplication::processEvents();
  require(editor->verticalScrollBar()->maximum() > 0,
          "Control-scroll window fixture does not overflow");
  editor->verticalScrollBar()->setValue(editor->verticalScrollBar()->maximum() / 2);
  QTextCursor selected(editor->document()); selected.setPosition(12);
  selected.setPosition(25, QTextCursor::KeepAnchor); editor->setTextCursor(selected);
  const int position = selected.position(); const int anchor = selected.anchor();
  const int before = editor->verticalScrollBar()->value();
  key(Qt::Key_Down);
  require(editor->verticalScrollBar()->value() > before &&
              editor->textCursor().position() == position &&
              editor->textCursor().anchor() == anchor,
          "Control Down invoked Qt navigation or changed the selection");
  const int down = editor->verticalScrollBar()->value();
  key(Qt::Key_Up, Qt::ControlModifier | Qt::ShiftModifier);
  require(editor->verticalScrollBar()->value() < down &&
              editor->textCursor().position() == position &&
              editor->textCursor().anchor() == anchor,
          "Control Shift Up did not retain source scrolling semantics");
}

void test_undo_depth_policy(const QString& directory) {
  using namespace jwpqt::qt;
  require(ApplicationSettings{}.maximum_undo_levels == 50 &&
              read_application_settings("undo_number = 3\n").maximum_undo_levels == 3,
          "Undo source defaults or alias changed");
  bool rejected = false;
  try { (void)read_application_settings("MaximumUndoLevels = 2\nundo_number = 50\n"); }
  catch (const std::exception&) { rejected = true; }
  require(rejected, "Invalid earlier undo limit accepted");
  MainWindow window;
  const auto fill = [&](char32_t character) {
    for (int i = 0; i < 6; ++i) {
      window.active_editor()->moveCursor(QTextCursor::End);
      require(window.insert_edict_text(std::u32string(1, character)), "Could not prepare independent edits");
    }
  };
  const auto undo_count = [&] {
    auto* undo = find_action(window, "undoAction");
    int count = 0;
    while (undo->isEnabled() && count < 10) { undo->trigger(); ++count; }
    return count;
  };
  fill(U'A');
  find_action(window, "newDocumentAction")->trigger(); fill(U'B');
  const auto original = *window.current_jwp_document();
  ApplicationSettingsDialog cancelled(window.application_settings());
  cancelled.findChild<QSpinBox*>("settingsUndoLevels")->setValue(3);
  cancelled.reject();
  require(window.application_settings().maximum_undo_levels == 50, "Cancel changed undo capacity");
  ApplicationSettingsDialog options(window.application_settings());
  options.findChild<QSpinBox*>("settingsUndoLevels")->setValue(3);
  auto* option_tabs = options.findChild<QTabWidget*>();
  for (int i = 0; i < option_tabs->count(); ++i)
    if (option_tabs->widget(i)->isAncestorOf(options.findChild<QSpinBox*>("settingsUndoLevels"))) option_tabs->setCurrentIndex(i);
  options.show(); QApplication::processEvents();
  require(options.grab().save(QDir::current().filePath("undo-depth-options.png")), "Could not capture history controls");
  options.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
  require(window.apply_application_settings(options.settings()) && *window.current_jwp_document() == original &&
              window.document_modified(), "Resizing undo changed text or dirty state");
  require(undo_count() == 3 && document_plain_text(*window.active_editor()->document()) == "BBB",
          "Active native history did not retain only the nearest edits");
  require(window.activate_document(0) && undo_count() == 3 &&
              document_plain_text(*window.active_editor()->document()) == "AAA", "Inactive history was not resized");
  find_action(window, "newDocumentAction")->trigger(); fill(U'C');
  require(undo_count() == 3 && document_plain_text(*window.active_editor()->document()) == "CCC", "New history ignored configured limit");
  find_action(window, "newTextDocumentAction")->trigger(); fill(U'D');
  auto larger = window.application_settings(); larger.maximum_undo_levels = 8;
  require(window.apply_application_settings(larger) && undo_count() == 6 && window.active_editor()->document()->isEmpty(),
          "Native history setting damaged unrestricted Unicode history or text");
  const auto root = directory + "/undo-depth";
  require(QDir().mkpath(root) && window.save_application_settings(root + "/settings.cfg"), "Could not save undo preference");
  MainWindow restored;
  require(restored.load_application_settings(root + "/settings.cfg") && restored.application_settings().maximum_undo_levels == 8 &&
              restored.save_project_path(root + "/empty.jpr", false), "Undo preference restart failed");
  MainWindow project;
  require(project.open_project_path(root + "/empty.jpr") && project.application_settings().maximum_undo_levels == 8,
          "Project did not preserve undo preference");
  for (int i = 0; i < 10; ++i) {
    project.active_editor()->moveCursor(QTextCursor::End);
    require(project.insert_edict_text(U"E"), "Could not populate restored history");
  }
  int restored_undos = 0;
  while (find_action(project, "undoAction")->isEnabled() && restored_undos < 12) {
    find_action(project, "undoAction")->trigger(); ++restored_undos;
  }
  require(restored_undos == 8 && document_plain_text(*project.active_editor()->document()) == "EE",
          "Restored project editor did not apply its undo limit");
}

void test_conversion_choice_policy(const QString& directory) {
  using namespace jwpqt::core;
  using namespace jwpqt::qt;
  const QString root = directory + "/conversion-choices";
  require(QDir().mkpath(root), "Could not isolate conversion-choice settings");
  const WnnFixture fixture = write_wnn_fixture(root);
  write_bytes(fixture.preferences_path, QByteArray(20 * 8, '\0'));
  JwpDocument source;
  source.paragraphs.resize(1);
  source.paragraphs[0].text = {0x2422};
  const QString source_path = root + "/reading.jwp";
  write_jwp_file(source_path, source);

  MainWindow window;
  ApplicationSettingsDialog cancelled(window.application_settings());
  cancelled.findChild<QSpinBox*>("settingsConversionChoices")->setValue(10);
  cancelled.reject();
  require(window.application_settings().conversion_choices == 200,
          "Cancel changed conversion-choice storage");
  ApplicationSettingsDialog options(window.application_settings());
  options.findChild<QSpinBox*>("settingsConversionChoices")->setValue(10);
  options.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
  require(window.apply_application_settings(options.settings()) &&
              window.open_jwp_path(source_path) &&
              window.load_wnn_resources(fixture.index_path, fixture.data_path,
                                        fixture.preferences_path) &&
              window.resource_report().contains(
                  QStringLiteral("Learned conversion choices: 10 slots")),
          "Configured conversion-choice capacity was not used while loading");

  window.active_editor()->selectAll();
  require(window.convert_selection() && window.conversion_active(),
          "Could not start conversion before capacity guard");
  auto changed = window.application_settings();
  changed.conversion_choices = 11;
  require(!window.apply_application_settings(changed) &&
              window.application_settings().conversion_choices == 10 &&
              window.resource_report().contains(
                  QStringLiteral("Learned conversion choices: 10 slots")),
          "Capacity change was accepted during an active conversion");
  require(window.cycle_conversion() && window.accept_conversion() &&
              read_bytes(fixture.preferences_path).size() == 10 * 8,
          "Accepted conversion did not write the configured preference capacity");

  changed = window.application_settings();
  changed.conversion_choices = 12;
  require(window.apply_application_settings(changed) &&
              window.resource_report().contains(
                  QStringLiteral("Learned conversion choices: 12 slots")),
          "Live conversion-choice capacity did not resize");
  changed.conversion_choices = 10;
  require(window.apply_application_settings(changed) &&
              window.save_application_settings(root + "/settings.cfg") &&
              window.save_project_path(root + "/project.jpr", false),
          "Could not persist conversion-choice capacity");

  MainWindow restored;
  require(restored.load_application_settings(root + "/settings.cfg") &&
              restored.application_settings().conversion_choices == 10 &&
              restored.open_project_path(root + "/project.jpr") &&
              restored.application_settings().conversion_choices == 10 &&
              restored.load_wnn_resources(fixture.index_path, fixture.data_path,
                                          fixture.preferences_path) &&
              restored.resource_report().contains(
                  QStringLiteral("Learned conversion choices: 10 slots")),
          "Conversion-choice capacity did not survive settings and project restoration");
}

void test_selection_autoscroll_policy(const QString& directory) {
  using namespace jwpqt::qt;
  MainWindow window;
  auto* first = window.active_editor();
  require(first->selection_autoscroll_enabled() &&
              first->selection_autoscroll_interval() == 100,
          "New editor did not use source autoscroll defaults");
  find_action(window, "newTextDocumentAction")->trigger();
  auto* second = window.active_editor();

  ApplicationSettingsDialog cancelled(window.application_settings());
  cancelled.findChild<QCheckBox*>(QStringLiteral("settingsAutoScroll"))->setChecked(false);
  cancelled.findChild<QSpinBox*>(QStringLiteral("settingsAutoScrollSpeed"))->setValue(35);
  cancelled.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Cancel)->click();
  require(window.application_settings().auto_scroll &&
              second->selection_autoscroll_interval() == 100,
          "Cancelling Options changed selection autoscroll");

  ApplicationSettingsDialog options(window.application_settings());
  options.findChild<QCheckBox*>(QStringLiteral("settingsAutoScroll"))->setChecked(false);
  options.findChild<QSpinBox*>(QStringLiteral("settingsAutoScrollSpeed"))->setValue(35);
  options.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
  require(window.apply_application_settings(options.settings()) &&
              !first->selection_autoscroll_enabled() &&
              !second->selection_autoscroll_enabled() &&
              first->selection_autoscroll_interval() == 35 &&
              second->selection_autoscroll_interval() == 35,
          "Autoscroll Options did not update every existing editor");
  find_action(window, "newDocumentAction")->trigger();
  require(!window.active_editor()->selection_autoscroll_enabled() &&
              window.active_editor()->selection_autoscroll_interval() == 35,
          "New document did not inherit selection autoscroll settings");

  const QString root = directory + QStringLiteral("/selection-autoscroll");
  require(QDir().mkpath(root) &&
              window.save_application_settings(root + QStringLiteral("/settings.cfg")) &&
              window.save_project_path(root + QStringLiteral("/workspace.jpr"), false),
          "Could not persist selection autoscroll settings");
  MainWindow restored;
  require(restored.load_application_settings(root + QStringLiteral("/settings.cfg")) &&
              !restored.application_settings().auto_scroll &&
              restored.application_settings().auto_scroll_speed == 35 &&
              restored.open_project_path(root + QStringLiteral("/workspace.jpr")) &&
              !restored.active_editor()->selection_autoscroll_enabled() &&
              restored.active_editor()->selection_autoscroll_interval() == 35,
          "Settings restart or JPR changed selection autoscroll policy");
}

void test_result_list_insertion_policy(const QString& directory) {
  using namespace jwpqt::core;
  using namespace jwpqt::qt;
  require(ApplicationSettings{}.insert_on_separate_lines &&
              !read_application_settings("paste_newpara = false\n").insert_on_separate_lines &&
              write_application_settings(ApplicationSettings{}).find("InsertOnSeparateLines = true") !=
                  std::string::npos,
          "Result-list insertion source defaults or alias changed");
  bool invalid = false;
  try {
    (void)read_application_settings(
        "InsertOnSeparateLines = invalid\npaste_newpara = true\n");
  } catch (const std::exception&) {
    invalid = true;
  }
  require(invalid, "Invalid earlier result-list insertion policy accepted");

  const QString root = directory + QStringLiteral("/result-list-insertion");
  require(QDir().mkpath(root), "Could not create result-list insertion fixture directory");
  JwpDocument original;
  original.paragraphs = {paragraph(U"source")};
  original.paragraphs[0].first_indent = 2;
  original.summary[0] = {'I'};
  const QString native_path = root + QStringLiteral("/native.jwp");
  write_jwp_file(native_path, original);
  MainWindow window;
  require(window.open_jwp_path(native_path), "Could not open native insertion fixture");
  auto* editor = window.active_editor();
  editor->selectAll();
  require(window.insert_list_text(U"A\nB") &&
              document_plain_text(*editor->document()) == QStringLiteral("A\nB\n") &&
              window.current_jwp_document()->paragraphs.size() == 3,
          "Separate-line insertion did not retain complete logical rows and trailing paragraph");
  find_action(window, "undoAction")->trigger();
  require(*window.current_jwp_document() == original && !window.document_modified(),
          "Separate-line insertion was not one clean native undo transaction");

  auto joined = window.application_settings();
  joined.insert_on_separate_lines = false;
  require(window.apply_application_settings(joined), "Could not disable separate-line insertion");
  editor->selectAll();
  require(window.insert_list_text(U"A\nB") &&
              document_plain_text(*editor->document()) == QStringLiteral("A\tB") &&
              window.current_jwp_document()->paragraphs.size() == 1,
          "Joined ASCII rows did not use the source tab separator");
  find_action(window, "undoAction")->trigger();
  editor->selectAll();
  require(window.insert_list_text(U"\u65e5\nB") &&
              document_plain_text(*editor->document()) == QStringLiteral("\u65e5B"),
          "Joined rows inserted spacing after a Japanese source row");
  find_action(window, "undoAction")->trigger();
  editor->selectAll();
  require(window.insert_list_text(U"single") &&
              document_plain_text(*editor->document()) == QStringLiteral("single"),
          "Joined single-row insertion added a trailing separator");
  find_action(window, "undoAction")->trigger();

  ApplicationSettingsDialog options(window.application_settings());
  auto* separate = options.findChild<QCheckBox*>(QStringLiteral("settingsInsertLines"));
  require(separate && !separate->isChecked(), "Result-list insertion option is missing or stale");
  separate->setChecked(true);
  options.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
  require(window.apply_application_settings(options.settings()) &&
              window.application_settings().insert_on_separate_lines,
          "Result-list insertion option did not apply");

  const QString unicode_path = root + QStringLiteral("/unicode.txt");
  const std::u32string unicode_original = U"\ufeff\U0001f600\u00a0";
  write_text_file(unicode_path, {unicode_original, TextEncoding::kUtf16Be, true});
  require(window.open_path(unicode_path, TextEncoding::kUtf16Be, OpenMode::kNonInteractive, true),
          "Could not open Unicode insertion fixture");
  editor = window.active_editor();
  editor->selectAll();
  require(window.insert_list_text(U"X\nY") &&
              document_plain_text(*editor->document()) == QStringLiteral("X\nY\n"),
          "Separate-line insertion failed in an unrestricted Unicode document");
  find_action(window, "undoAction")->trigger();
  require(from_qstring(document_plain_text(*editor->document())) == unicode_original &&
              !window.document_modified(),
          "Unicode result-list insertion did not undo to its exact source");

  require(window.save_application_settings(root + QStringLiteral("/settings.cfg")) &&
              window.save_project_path(root + QStringLiteral("/workspace.jpr"), false),
          "Could not persist result-list insertion preference");
  MainWindow restored;
  require(restored.load_application_settings(root + QStringLiteral("/settings.cfg")) &&
              restored.application_settings().insert_on_separate_lines &&
              restored.open_project_path(root + QStringLiteral("/workspace.jpr")) &&
              restored.application_settings().insert_on_separate_lines,
          "Settings restart or JPR changed result-list insertion preference");
}

void test_selected_romaji(const QString& directory) {
  using namespace jwpqt::core;
  using namespace jwpqt::qt;
  const QString root = directory + QStringLiteral("/selected-romaji");
  require(QDir().mkpath(root), "Could not create isolated romaji fixture directory");
  require(!read_application_settings("revert_to_K_mode = false\n").revert_to_kanji_mode &&
              read_application_settings("RevertToKanjiMode = true\n").revert_to_kanji_mode &&
              write_application_settings(ApplicationSettings{}).find("RevertToKanjiMode") != std::string::npos,
          "Conversion mode configuration did not round trip");
  bool invalid_mode = false;
  try { (void)read_application_settings("RevertToKanjiMode = invalid\nrevert_to_K_mode = true\n"); }
  catch (const std::exception&) { invalid_mode = true; }
  require(invalid_mode, "Invalid earlier conversion mode preference accepted");
  const auto select = [](QTextEdit* editor, int begin, int end, bool reversed = false) {
    QTextCursor cursor(editor->document());
    cursor.setPosition(reversed ? end : begin);
    cursor.setPosition(reversed ? begin : end, QTextCursor::KeepAnchor);
    editor->setTextCursor(cursor);
  };

  JwpDocument original;
  original.paragraphs.resize(1);
  original.paragraphs[0].text = encode_jwp_text(U"L nihon R");
  original.paragraphs[0].first_indent = 1;
  original.paragraphs[0].line_spacing = 125;
  original.summary[0] = {'M'};
  JwpDocument converted = original;
  converted.paragraphs[0].text = encode_jwp_text(U"L \u306b\u307b\u3093 R");
  QString native_path;
  for (const bool reversed : {false, true}) {
    native_path = root + (reversed ? QStringLiteral("/reverse.jwp") : QStringLiteral("/forward.jwp"));
    write_jwp_file(native_path, original);
    MainWindow window;
    require(window.open_jwp_path(native_path), "Could not open romaji document");
    auto mode_policy = window.application_settings();
    require(mode_policy.revert_to_kanji_mode, "Source conversion mode default changed");
    ApplicationSettingsDialog mode_options(mode_policy);
    auto* reset = mode_options.findChild<QCheckBox*>(QStringLiteral("settingsRevertToKanji"));
    require(reset && reset->isChecked(), "Conversion mode option missing");
    reset->setChecked(!reversed);
    mode_options.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
    mode_policy = mode_options.settings();
    require(window.application_settings().revert_to_kanji_mode && mode_policy.revert_to_kanji_mode == !reversed,
            "Conversion mode options changed live preferences prematurely");
    require(window.apply_application_settings(mode_policy), "Could not apply conversion mode policy");
    find_action(window, reversed ? "jasciiInputAction" : "asciiInputAction")->trigger();
    auto* editor = window.active_editor();
    auto* action = find_action(window, "convertSelectionAction");
    require(!action->isEnabled(), "Convert was enabled without input or resources");
    find_action(window, "overwriteModeAction")->setChecked(true);
    select(editor, 2, 7, reversed);
    require(action->isEnabled(), "Selected romaji was gated on WNN resources");
    action->trigger();
    require(find_action(window, reversed ? "jasciiInputAction" : "kanaInputAction")->isChecked(),
            "Explicit conversion did not honor input-mode policy");
    require(!window.conversion_active() && *window.current_jwp_document() == converted &&
                editor->textCursor().selectedText() == QStringLiteral("\u306b\u307b\u3093") &&
                editor->textCursor().position() == (reversed ? 2 : 5) &&
                editor->textCursor().anchor() == (reversed ? 5 : 2) && window.current_path() == native_path,
            std::string("Romaji conversion changed selection direction, suffix or native metadata: text=") +
                editor->toPlainText().toUtf8().toHex().constData() +
                " cursor=" + std::to_string(editor->textCursor().position()) +
                " anchor=" + std::to_string(editor->textCursor().anchor()) +
                " document=" + std::to_string(*window.current_jwp_document() == converted));
    find_action(window, "undoAction")->trigger();
    require(*window.current_jwp_document() == original && !window.document_modified() &&
                !find_action(window, "undoAction")->isEnabled(),
            "Romaji conversion did not undo as one clean transaction");
    find_action(window, "redoAction")->trigger();
    require(*window.current_jwp_document() == converted && window.save_as_path(native_path, std::nullopt) &&
                read_jwp_file(native_path) == converted, "Romaji redo/save lost metadata or content");
    require(window.save_application_settings(root + "/mode.cfg") &&
                window.save_project_path(root + "/mode.jpr", false), "Could not save conversion mode preferences");
    MainWindow restored;
    require(restored.load_application_settings(root + "/mode.cfg") &&
                restored.application_settings().revert_to_kanji_mode == !reversed &&
                restored.open_project_path(root + "/mode.jpr") &&
                restored.application_settings().revert_to_kanji_mode == !reversed,
            "Conversion mode settings/restart/JPR changed the policy");
  }

  const QString unicode_path = root + QStringLiteral("/unicode.txt");
  const std::u32string unicode_original = U"\ufeff\U0001f600nihon\u00a0Z";
  const std::u32string unicode_converted = U"\ufeff\U0001f600\u306b\u307b\u3093\u00a0Z";
  write_text_file(unicode_path, {unicode_original, TextEncoding::kUtf16Be, true});
  MainWindow unicode;
  require(unicode.open_jwp_path(native_path) &&
              unicode.open_path(unicode_path, TextEncoding::kUtf16Be, OpenMode::kNonInteractive, true),
          "Could not open Unicode romaji workspace");
  auto* editor = unicode.active_editor();
  require(!unicode.is_jwp_document(), "Unicode fixture unexpectedly acquired a JIS editing model");
  bool observed_edit = false;
  bool reentered = false;
  const auto listener = QObject::connect(editor->document(), &QTextDocument::contentsChanged, &unicode, [&] {
    observed_edit = true;
    reentered = unicode.activate_document(0) || reentered;
  });
  select(editor, 3, 8, true);
  require(find_action(unicode, "convertSelectionAction")->isEnabled() && unicode.convert_selection(),
          "Unrestricted Unicode romaji conversion was unavailable");
  QObject::disconnect(listener);
  require(observed_edit && !reentered && unicode.active_editor() == editor &&
              !unicode.is_jwp_document() && unicode.text_encoding() == TextEncoding::kUtf16Be &&
              document_plain_text(*editor->document()) == to_qstring(unicode_converted) &&
              editor->textCursor().position() == 3 && editor->textCursor().anchor() == 6,
          "Unicode replay changed engine, scalar content, selection, codec or active document");
  find_action(unicode, "undoAction")->trigger();
  require(document_plain_text(*editor->document()) == to_qstring(unicode_original) &&
              !unicode.document_modified() && !find_action(unicode, "undoAction")->isEnabled(),
          "Unicode romaji replay lost its saved undo baseline");
  select(editor, 3, 8);
  editor->setReadOnly(true);
  require(!unicode.convert_selection() && document_plain_text(*editor->document()) == to_qstring(unicode_original),
          "Romaji replay edited a read-only document");
  editor->setReadOnly(false);
  require(unicode.convert_selection() && unicode.save_as_path(unicode_path, TextEncoding::kUtf16Be),
          "Unicode romaji replay did not save");
  const auto saved = read_text_file(unicode_path, TextEncoding::kUtf16Be);
  require(saved.text == unicode_converted && saved.has_byte_order_mark,
          "Unicode romaji replay changed literal signatures or BOM policy");

  for (const QString& invalid : {QStringLiteral("k!"), QStringLiteral("ka k"),
                                 QStringLiteral("ka\nki"), QStringLiteral("ka\u3042"),
                                 QStringLiteral("a\0b"), QString(65536, QLatin1Char('a'))}) {
    MainWindow rejected;
    find_action(rejected, "newTextDocumentAction")->trigger();
    rejected.active_editor()->insertPlainText(invalid);
    rejected.active_editor()->selectAll();
    const int position = rejected.active_editor()->textCursor().position();
    const int anchor = rejected.active_editor()->textCursor().anchor();
    require(!rejected.convert_selection() &&
                document_plain_text(*rejected.active_editor()->document()) == invalid &&
                rejected.active_editor()->textCursor().position() == position &&
                rejected.active_editor()->textCursor().anchor() == anchor,
            "Rejected romaji replay partially replaced its source or selection");
    find_action(rejected, "undoAction")->trigger();
    require(rejected.active_editor()->document()->isEmpty(), "Failed replay added an undo entry");
  }

  const WnnFixture fixture = write_wnn_fixture(root);
  JwpDocument reading;
  reading.paragraphs.resize(1);
  reading.paragraphs[0].text = {'a'};
  const QString reading_path = root + QStringLiteral("/reading.jwp");
  write_jwp_file(reading_path, reading);
  MainWindow wnn;
  require(wnn.load_wnn_resources(fixture.index_path, fixture.data_path, fixture.preferences_path) &&
              wnn.open_jwp_path(reading_path), "Could not open selected-romaji WNN fixture");
  editor = wnn.active_editor();
  editor->selectAll();
  require(wnn.convert_selection() && !wnn.conversion_active() &&
              editor->textCursor().selectedText() == QStringLiteral("\u3042"),
          "Lowercase romaji did not remain selected kana for the next Convert");
  require(wnn.convert_selection() && wnn.conversion_active(), "Replayed kana could not start WNN conversion");
  require(wnn.cycle_conversion(true) && wnn.accept_conversion() &&
              editor->toPlainText() == QStringLiteral("\u3042"),
          "Returning to original kana after replay changed the input");
  find_action(wnn, "undoAction")->trigger();
  require(*wnn.current_jwp_document() == reading && !wnn.document_modified(),
          "Undo after original-kana acceptance did not restore selected romaji");
  const auto mode_root = root + QStringLiteral("/mode-wnn");
  require(QDir().mkpath(mode_root), "Could not isolate conversion mode learning");
  const auto mode_fixture = write_wnn_fixture(mode_root);
  MainWindow mode_window;
  require(mode_window.load_wnn_resources(mode_fixture.index_path, mode_fixture.data_path, mode_fixture.preferences_path) &&
              mode_window.open_jwp_path(reading_path), "Could not load mode conversion fixture");
  auto mode_policy = mode_window.application_settings();
  mode_policy.revert_to_kanji_mode = false;
  require(mode_window.apply_application_settings(mode_policy), "Could not disable conversion mode reset");
  find_action(mode_window, "asciiInputAction")->trigger();
  mode_window.active_editor()->selectAll();
  require(mode_window.convert_selection() && mode_window.convert_selection() && mode_window.conversion_active() &&
              find_action(mode_window, "asciiInputAction")->isChecked(), "Disabled mode reset changed WNN input mode");
  mode_policy.revert_to_kanji_mode = true;
  require(mode_window.apply_application_settings(mode_policy) && mode_window.conversion_active() &&
              find_action(mode_window, "asciiInputAction")->isChecked() && mode_window.convert_selection() &&
              mode_window.conversion_active() && find_action(mode_window, "kanaInputAction")->isChecked(),
          "Mode reset accepted or lost the active conversion preview");
  require(mode_window.accept_conversion(), "Could not accept mode-reset preview");
  find_action(mode_window, "undoAction")->trigger();
  if (find_action(mode_window, "undoAction")->isEnabled()) find_action(mode_window, "undoAction")->trigger();
  require(*mode_window.current_jwp_document() == reading && !mode_window.document_modified(), "Mode reset added a document edit");

  reading.paragraphs[0].text = {'L', ' ', 'A', 'k', 'a', ' ', 'R'};
  write_jwp_file(reading_path, reading);
  require(wnn.open_jwp_path(reading_path), "Could not reload capitalized romaji fixture");
  editor = wnn.active_editor();
  select(editor, 2, 5);
  require(wnn.convert_selection() && !wnn.conversion_active() &&
              wnn.current_jwp_document()->paragraphs[0].text == JwpText({'L', ' ', 0x3021, 0x242b, ' ', 'R'}) &&
              editor->textCursor().selectionStart() == 2 && editor->textCursor().selectionEnd() == 4,
          "Capitalized automatic replay left a preview or overwrote the following text");
  find_action(wnn, "undoAction")->trigger();
  require(*wnn.current_jwp_document() == reading && !wnn.document_modified(),
          "Capitalized replay was not one undoable transaction");
}

void test_jwp_wnn_conversion(const QString& directory) {
  const WnnFixture fixture = write_wnn_fixture(directory);
  jwpqt::core::JwpDocument source;
  source.paragraphs = {jwpqt::core::JwpParagraph{}};
  source.paragraphs[0].text = {0x2422};
  source.paragraphs[0].first_indent = 1;
  source.paragraphs[0].line_spacing = 125;
  const QString source_path = directory + QStringLiteral("/convert.jwp");
  jwpqt::qt::write_jwp_file(source_path, source);

  const QString color_settings =
      directory + QStringLiteral("/convert-colors.ini");
  const QString color_list_path =
      directory + QStringLiteral("/convert-colkanji.lst");
  jwpqt::core::KanjiColorList color_list;
  require(color_list.add(0x3021),
          "Could not prepare conversion color list fixture");
  jwpqt::qt::write_kanji_color_list_file(color_list_path, color_list);

  jwpqt::qt::MainWindow window;
  require(window.load_kanji_color_configuration(
              color_settings, color_list_path,
              jwpqt::qt::OpenMode::kNonInteractive) &&
              window.load_wnn_resources(fixture.index_path, fixture.data_path,
                                        fixture.preferences_path),
          "Could not load native WNN resources");
  require(!window.load_wnn_resources(
              directory + QStringLiteral("/missing.dix"), fixture.data_path,
              fixture.preferences_path,
              jwpqt::qt::OpenMode::kNonInteractive),
          "Missing replacement WNN resources unexpectedly loaded");
  require(window.open_jwp_path(source_path),
          "Could not open native WNN conversion fixture");
  QTextEdit* editor = window.findChild<QTextEdit*>();
  QAction* convert = find_action(window, "convertSelectionAction");
  QAction* next = find_action(window, "nextCandidateAction");
  auto* candidates = window.findChild<QListWidget*>(QStringLiteral("conversionCandidates"));
  require(candidates != nullptr && candidates->isHidden() &&
              editor->font().pixelSize() == 16 && candidates->font().pixelSize() == 16,
          "Conversion strip or recovered Japanese content font is missing");
  QAction* accept = find_action(window, "acceptCandidateAction");
  QAction* page_break = find_action(window, "insertPageBreakAction");
  QAction* make_color_list =
      find_action(window, "makeKanjiColorListAction");
  QAction* append_color_list =
      find_action(window, "appendKanjiColorListAction");
  QAction* edit_color_list =
      find_action(window, "editKanjiColorListAction");
  QAction* view_color_list =
      find_action(window, "viewKanjiColorListAction");
  QAction* clear_color_list =
      find_action(window, "clearKanjiColorListAction");
  require(editor != nullptr && convert != nullptr && next != nullptr &&
              accept != nullptr && page_break != nullptr &&
              make_color_list != nullptr && append_color_list != nullptr &&
              edit_color_list != nullptr && view_color_list != nullptr &&
              clear_color_list != nullptr,
          "Native WNN conversion actions were not created");

  editor->selectAll();
  require(convert->isEnabled(),
          "Native WNN conversion was not enabled for selected kana");
  convert->trigger();
  require(window.conversion_active() && editor->isReadOnly() &&
              window.current_jwp_document()->paragraphs[0].text ==
                  jwpqt::core::JwpText{0x3021},
          "Native WNN conversion did not display the preferred candidate");
  require(!candidates->isHidden() && candidates->count() == 3 &&
              candidates->currentRow() == 0 &&
              candidates->item(0)->text() == QString::fromUtf8(u8"亜") &&
              candidates->item(2)->text() == QString::fromUtf8(u8"あ") &&
              candidates->flow() == QListView::LeftToRight && !candidates->isWrapping(),
          "Conversion strip did not list the candidates and original kana");
  jwpqt::core::KanjiColorPolicy conversion_color;
  conversion_color.list_mode = jwpqt::core::KanjiListColorMode::kMatch;
  conversion_color.list_color = {7, 8, 9};
  require(window.set_kanji_color_policy(
              conversion_color, jwpqt::qt::OpenMode::kNonInteractive) &&
              window.conversion_active() && editor->isReadOnly() &&
              editor->textCursor().hasSelection() &&
              editor->extraSelections().size() == 1 &&
              editor->extraSelections()[0].format.foreground().color() ==
                  QColor(7, 8, 9),
          "Changing kanji colors disturbed an active WNN conversion");
  require(!page_break->isEnabled() && !window.insert_page_break() &&
              window.conversion_active() && editor->isReadOnly() &&
              window.current_jwp_document()->paragraphs[0].text ==
                  jwpqt::core::JwpText{0x3021},
          "Page-break insertion accepted an active WNN conversion");
  require(!make_color_list->isEnabled() && !append_color_list->isEnabled() &&
              !edit_color_list->isEnabled() && !view_color_list->isEnabled() &&
              !clear_color_list->isEnabled() &&
              !window.make_kanji_color_list(
                  jwpqt::qt::OpenMode::kNonInteractive) &&
              !window.append_kanji_color_list(
                  jwpqt::qt::OpenMode::kNonInteractive) &&
              !window.edit_kanji_color_list(
                  QStringLiteral("x"), true,
                  jwpqt::qt::OpenMode::kNonInteractive) &&
              !window.view_kanji_color_list(
                  jwpqt::qt::OpenMode::kNonInteractive) &&
              !window.clear_kanji_color_list(
                  jwpqt::qt::OpenMode::kNonInteractive) &&
              window.conversion_active() && editor->isReadOnly(),
          "Kanji list command accepted an active WNN conversion");
  window.show();
  editor->setFocus();
  QApplication::processEvents();
  const auto click_candidate = [&](int row) {
    const QPoint point = candidates->visualItemRect(candidates->item(row)).center();
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(point),
                      QPointF(candidates->viewport()->mapToGlobal(point)),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(point),
                        QPointF(candidates->viewport()->mapToGlobal(point)),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(candidates->viewport(), &press);
    QApplication::sendEvent(candidates->viewport(), &release);
  };
  click_candidate(1);
  require(window.conversion_active() && editor->hasFocus() &&
              window.current_jwp_document()->paragraphs[0].text == jwpqt::core::JwpText{0x3022},
          "Clicking a conversion candidate lost focus or accepted the conversion");
  require(convert->isEnabled(), "Convert button is disabled during candidate selection");
  convert->trigger();
  require(candidates->currentRow() == 2 &&
              window.current_jwp_document()->paragraphs[0].text == jwpqt::core::JwpText{0x2422},
          "Convert button did not cycle the visible candidate strip");
  click_candidate(0);
  QKeyEvent next_key(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier);
  QApplication::sendEvent(editor, &next_key);
  require(window.current_jwp_document()->paragraphs[0].text ==
              jwpqt::core::JwpText{0x3022},
          "Space did not cycle native WNN candidates");
  QKeyEvent accept_key(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
  QApplication::sendEvent(editor, &accept_key);
  require(!window.conversion_active() && !editor->isReadOnly() &&
              QFile::exists(fixture.preferences_path) && candidates->isHidden() &&
              candidates->count() == 0,
          "Escape did not accept and persist native WNN conversion");

  QAction* undo = find_action(window, "undoAction");
  QAction* redo = find_action(window, "redoAction");
  require(undo != nullptr && redo != nullptr && undo->isEnabled(),
          "Native conversion did not create a portable undo entry");
  undo->trigger();
  require(window.current_jwp_document()->paragraphs[0].text ==
              jwpqt::core::JwpText{0x2422},
          "Undo did not restore the original conversion input");
  redo->trigger();
  require(window.current_jwp_document()->paragraphs[0].text ==
              jwpqt::core::JwpText{0x3022},
          "Redo did not restore the accepted conversion candidate");
  require_jwp_layout(editor, *window.current_jwp_document());

  jwpqt::qt::MainWindow reopened;
  require(reopened.load_wnn_resources(fixture.index_path, fixture.data_path,
                                      fixture.preferences_path) &&
              reopened.open_jwp_path(source_path),
          "Could not reopen persisted WNN preference fixture");
  QTextEdit* reopened_editor = reopened.findChild<QTextEdit*>();
  require(reopened_editor != nullptr, "Reopened WNN window has no editor");
  QTextCursor reversed = reopened_editor->textCursor();
  reversed.setPosition(1);
  reversed.setPosition(0, QTextCursor::KeepAnchor);
  reopened_editor->setTextCursor(reversed);
  require(reopened.convert_selection() &&
              reopened.current_jwp_document()->paragraphs[0].text ==
                  jwpqt::core::JwpText{0x3022},
          "Native WNN conversion did not restore the learned candidate");
  require(reopened_editor->textCursor().position() ==
              reopened_editor->textCursor().selectionStart(),
          "Native conversion did not preserve reversed-caret orientation");
  require(reopened.accept_conversion(),
          "Could not accept the restored WNN candidate");
}

void test_jwp_wnn_conversion_boundaries(const QString& directory) {
  const WnnFixture fixture = write_wnn_fixture(directory);
  jwpqt::core::JwpDocument source;
  source.paragraphs = {jwpqt::core::JwpParagraph{},
                       jwpqt::core::JwpParagraph{}};
  source.paragraphs[0].text = {0x2422};
  source.paragraphs[1].text = {0x2422};
  const QString source_path = directory + QStringLiteral("/convert-range.jwp");
  jwpqt::qt::write_jwp_file(source_path, source);

  jwpqt::qt::MainWindow window;
  require(window.load_wnn_resources(fixture.index_path, fixture.data_path,
                                    fixture.preferences_path) &&
              window.open_jwp_path(source_path),
          "Could not prepare WNN range fixture");
  QTextEdit* editor = window.findChild<QTextEdit*>();
  QAction* convert = find_action(window, "convertSelectionAction");
  require(editor != nullptr && convert != nullptr,
          "WNN range fixture has no conversion controls");
  editor->selectAll();
  require(!convert->isEnabled() && !window.convert_selection() &&
              *window.current_jwp_document() == source,
          "Cross-paragraph WNN selection was not rejected");
}

void test_jwp_wnn_user_dictionary(const QString& directory) {
  const WnnFixture fixture = write_wnn_fixture(directory);
  QFile::remove(fixture.preferences_path);
  const QString user_dictionary_directory =
      directory + QStringLiteral("/native-user");
  require(QDir().mkpath(user_dictionary_directory),
          "Could not create native user dictionary directory");
  const QString user_dictionary_path =
      user_dictionary_directory + QStringLiteral("/user.cnv");
  const jwpqt::core::WnnUserDictionary user_dictionary =
      jwpqt::core::WnnUserDictionary::from_entries(
          {{{0x2422}, '*', {{0x3023}}}});
  jwpqt::qt::write_wnn_user_dictionary_file(user_dictionary_path,
                                            user_dictionary);

  jwpqt::core::JwpDocument source;
  source.paragraphs = {jwpqt::core::JwpParagraph{}};
  source.paragraphs[0].text = {0x2422};
  const QString source_path = directory + QStringLiteral("/user-convert.jwp");
  jwpqt::qt::write_jwp_file(source_path, source);

  jwpqt::qt::MainWindow window;
  require(window.load_wnn_resources(
              fixture.index_path, fixture.data_path, fixture.preferences_path,
              user_dictionary_path, jwpqt::qt::OpenMode::kNonInteractive) &&
              window.wnn_user_dictionary() != nullptr &&
              window.wnn_user_dictionary()->entries() ==
                  user_dictionary.entries() &&
              window.open_jwp_path(source_path),
          "Could not load native WNN user dictionary fixture");
  QTextEdit* editor = window.findChild<QTextEdit*>();
  require(editor != nullptr, "Native WNN user dictionary has no editor");
  editor->selectAll();
  require(window.convert_selection() && window.cycle_conversion() &&
              window.cycle_conversion() &&
              window.current_jwp_document()->paragraphs[0].text ==
                  jwpqt::core::JwpText{0x3023} &&
              window.accept_conversion(),
          "Native WNN conversion did not use the user dictionary candidate");

  QFile malformed(user_dictionary_path);
  require(malformed.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
              malformed.write("bad", 3) == 3,
          "Could not corrupt native WNN user dictionary fixture");
  malformed.close();
  require(!window.load_wnn_resources(
              fixture.index_path, fixture.data_path, fixture.preferences_path,
              user_dictionary_path, jwpqt::qt::OpenMode::kNonInteractive) &&
              window.wnn_user_dictionary() != nullptr &&
              window.wnn_user_dictionary()->entries() ==
                  user_dictionary.entries(),
          "Malformed WNN user dictionary reload replaced working resources");

  const jwpqt::core::WnnUserDictionary replacement =
      jwpqt::core::WnnUserDictionary::from_entries(
          {{{0x2422}, '*', {{0x3024}}},
           {{0x242b}, '*', {{0x3025}}},
           {{0x242b, 0x2424}, '*', {{0x3026}}}});
  require(window.set_wnn_user_dictionary(
              replacement, jwpqt::qt::OpenMode::kNonInteractive) &&
              window.wnn_user_dictionary()->entries() ==
                  replacement.entries(),
          "Could not replace the native WNN user dictionary");
  const std::optional<jwpqt::core::WnnUserDictionary> persisted =
      jwpqt::qt::read_wnn_user_dictionary_file(user_dictionary_path);
  require(persisted.has_value() &&
              persisted->entries() == replacement.entries(),
          "Replacing the native WNN user dictionary did not persist it");

  require(window.open_jwp_path(source_path),
          "Could not reopen the user conversion source after replacement");
  editor = window.findChild<QTextEdit*>();
  editor->selectAll();
  require(window.convert_selection(),
          "Could not begin conversion after replacing user entries");
  const jwpqt::core::WnnUserDictionary blocked_replacement =
      jwpqt::core::WnnUserDictionary::from_entries(
          {{{0x2422}, '*', {{0x3027}}}});
  bool found_replacement_candidate = false;
  for (int index = 0; index < 5; ++index) {
    if (window.current_jwp_document()->paragraphs[0].text ==
        jwpqt::core::JwpText{0x3024}) {
      found_replacement_candidate = true;
      break;
    }
    require(window.cycle_conversion(),
            "Could not cycle replacement user candidates");
  }
  require(found_replacement_candidate,
          "Replacement user entries did not reach the live WNN session");
  require(!window.set_wnn_user_dictionary(
              blocked_replacement,
              jwpqt::qt::OpenMode::kNonInteractive) &&
              window.conversion_active() &&
              window.wnn_user_dictionary()->entries() ==
                  replacement.entries(),
          "Active conversion allowed user dictionary replacement");
  require(window.accept_conversion(),
          "Could not accept conversion after blocked dictionary replacement");

  jwpqt::core::JwpDocument empty_source;
  empty_source.paragraphs = {jwpqt::core::JwpParagraph{}};
  const QString empty_source_path =
      directory + QStringLiteral("/empty-user-convert.jwp");
  jwpqt::qt::write_jwp_file(empty_source_path, empty_source);
  require(window.open_jwp_path(empty_source_path),
          "Could not open pending user conversion source");
  editor = window.findChild<QTextEdit*>();
  QAction* kana = find_action(window, "kanaInputAction");
  require(editor != nullptr && kana != nullptr,
          "Pending user conversion fixture has no kana controls");
  kana->trigger();
  window.show();
  editor->setFocus();
  QApplication::processEvents();
  send_text_key(editor, Qt::Key_K, QStringLiteral("K"), Qt::ShiftModifier);
  send_text_key(editor, Qt::Key_A, QStringLiteral("a"));
  require(!window.conversion_active() &&
              window.current_jwp_document()->paragraphs[0].text ==
                  jwpqt::core::JwpText{0x242b},
          "Extendable user conversion did not remain pending");
  require(window.set_wnn_user_dictionary(
              blocked_replacement,
              jwpqt::qt::OpenMode::kNonInteractive) &&
              !window.conversion_active() &&
              window.current_jwp_document()->paragraphs[0].text ==
                  jwpqt::core::JwpText{0x3025} &&
              window.wnn_user_dictionary()->entries() ==
                  blocked_replacement.entries(),
          "Replacing user entries discarded pending automatic conversion");

  const QString retained_directory =
      directory + QStringLiteral("/native-user-retained");
  require(QDir().rename(user_dictionary_directory, retained_directory),
          "Could not retain the persisted user dictionary directory");
  QFile path_blocker(user_dictionary_directory);
  require(path_blocker.open(QIODevice::WriteOnly) &&
              path_blocker.write("blocked", 7) == 7,
          "Could not block the native user dictionary output parent");
  path_blocker.close();
  const jwpqt::core::WnnUserDictionary failed_replacement =
      jwpqt::core::WnnUserDictionary::from_entries(
          {{{0x2422}, '*', {{0x3028}}}});
  require(!window.set_wnn_user_dictionary(
              failed_replacement,
              jwpqt::qt::OpenMode::kNonInteractive) &&
              window.wnn_user_dictionary()->entries() ==
                  blocked_replacement.entries(),
          "Failed user dictionary persistence replaced working resources");
  const std::optional<jwpqt::core::WnnUserDictionary> retained =
      jwpqt::qt::read_wnn_user_dictionary_file(
          retained_directory + QStringLiteral("/user.cnv"));
  require(retained.has_value() &&
              retained->entries() == blocked_replacement.entries(),
          "Failed user dictionary replacement changed persisted bytes");

  jwpqt::qt::MainWindow missing;
  require(missing.load_wnn_resources(
              fixture.index_path, fixture.data_path,
              directory + QStringLiteral("/missing-user.sel"),
              directory + QStringLiteral("/missing-user.cnv"),
              jwpqt::qt::OpenMode::kNonInteractive) &&
              missing.wnn_user_dictionary() != nullptr &&
              missing.wnn_user_dictionary()->entries().empty(),
          "Missing WNN user dictionary did not load as empty");
}

void test_jwp_wnn_user_dictionary_dialog(const QString& directory) {
  const WnnFixture fixture = write_wnn_fixture(directory);
  const QString user_dictionary_path =
      directory + QStringLiteral("/dialog-user.cnv");
  jwpqt::core::JwpDocument source;
  source.paragraphs = {paragraph(U"AB")};
  const QString source_path = directory + QStringLiteral("/dialog-user.jwp");
  jwpqt::qt::write_jwp_file(source_path, source);

  jwpqt::qt::MainWindow window;
  QAction* action = find_action(window, "userDictionaryAction");
  require(action != nullptr && !action->isEnabled(),
          "User dictionary action was enabled without WNN resources");
  require(window.load_wnn_resources(
              fixture.index_path, fixture.data_path,
              fixture.preferences_path, user_dictionary_path,
              jwpqt::qt::OpenMode::kNonInteractive) &&
              window.open_jwp_path(source_path) && action->isEnabled(),
          "Could not prepare native user dictionary dialog fixture");

  action->trigger();
  QApplication::processEvents();
  auto* dialog = dynamic_cast<jwpqt::qt::WnnUserDictionaryDialog*>(
      window.findChild<QDialog*>(QStringLiteral("userDictionaryDialog")));
  require(dialog != nullptr,
          "User dictionary action did not create one modeless dialog");
  action->trigger();
  QApplication::processEvents();
  require(window.findChildren<QDialog*>(QStringLiteral("userDictionaryDialog"))
              .size() == 1,
          "User dictionary action created duplicate modeless dialogs");
  require(window.load_wnn_resources(
              fixture.index_path, fixture.data_path,
              fixture.preferences_path, user_dictionary_path,
              jwpqt::qt::OpenMode::kNonInteractive) &&
              window.findChild<QDialog*>(
                  QStringLiteral("userDictionaryDialog")) == nullptr,
          "Reloaded WNN resources left a stale user dictionary dialog");
  action->trigger();
  QApplication::processEvents();
  dialog = dynamic_cast<jwpqt::qt::WnnUserDictionaryDialog*>(
      window.findChild<QDialog*>(QStringLiteral("userDictionaryDialog")));
  require(dialog != nullptr,
          "User dictionary dialog could not reopen after resource reload");

  const auto entry = jwpqt::core::make_wnn_user_entry(
      {0x2422}, {jwpqt::core::JwpText{0x3023}});
  dialog->add_entry(entry);
  const bool saved = dialog->save_changes();
  const QLabel* dialog_status =
      dialog->findChild<QLabel*>(QStringLiteral("wnnUserStatus"));
  require(saved,
          std::string("Modeless user dictionary dialog could not publish an "
                      "entry: ") +
              (dialog_status == nullptr ? "missing status"
                                        : dialog_status->text().toStdString()));
  const auto* published = window.wnn_user_dictionary();
  const auto persisted =
      jwpqt::qt::read_wnn_user_dictionary_file(user_dictionary_path);
  require(published != nullptr && published->entries().size() == 1 &&
              published->entries()[0] == entry &&
              persisted.has_value() && persisted->entries() == published->entries(),
          "Dialog save did not atomically publish and persist the dictionary");

  QTextEdit* editor = window.findChild<QTextEdit*>();
  require(editor != nullptr, "User dictionary dialog fixture has no editor");
  QTextCursor selection = editor->textCursor();
  selection.setPosition(0);
  selection.setPosition(1, QTextCursor::KeepAnchor);
  editor->setTextCursor(selection);
  QListWidget* entries = dialog->findChild<QListWidget*>();
  require(entries != nullptr, "User dictionary dialog has no entry list");
  entries->setCurrentRow(0);
  require(dialog->insert_selected(),
          "Dialog could not insert the selected user conversion");
  jwpqt::core::JwpText expected = jwpqt::core::render_wnn_user_entry(entry);
  require(window.current_jwp_document()->paragraphs.size() == 2 &&
              window.current_jwp_document()->paragraphs[0].text == expected &&
              window.current_jwp_document()->paragraphs[1].text ==
                  jwpqt::core::JwpText{static_cast<std::uint16_t>('B')} &&
              editor->textCursor().position() == static_cast<int>(expected.size() + 1),
          "Insert to File did not replace selection with the display row");

  QAction* undo = find_action(window, "undoAction");
  QAction* redo = find_action(window, "redoAction");
  require(undo != nullptr && redo != nullptr && undo->isEnabled(),
          "Insert to File did not create one portable history entry");
  undo->trigger();
  require(*window.current_jwp_document() == source && redo->isEnabled(),
          "Insert to File undo did not restore the original document");
  redo->trigger();
  require(window.current_jwp_document()->paragraphs[0].text == expected,
          "Insert to File redo did not restore the display row");

  QTextCursor converting = editor->textCursor();
  converting.setPosition(0);
  converting.setPosition(1, QTextCursor::KeepAnchor);
  editor->setTextCursor(converting);
  require(window.convert_selection(),
          "Could not start WNN conversion for Insert rejection test");
  const jwpqt::core::JwpDocument converting_document =
      *window.current_jwp_document();
  require(!dialog->insert_selected() &&
              *window.current_jwp_document() == converting_document,
          "Insert to File mutated an active WNN conversion");
  require(window.accept_conversion(),
          "Could not finish WNN conversion after Insert rejection");

  jwpqt::qt::MainWindow plain;
  const QString plain_path = directory + QStringLiteral("/dialog-plain.txt");
  jwpqt::qt::write_text_file(
      plain_path,
      jwpqt::core::TextFile{U"plain \U0001f600", jwpqt::core::TextEncoding::kUtf8,
                           false});
  require(plain.open_path(plain_path, jwpqt::core::TextEncoding::kUtf8),
          "Could not open unrestricted user-conversion target");
  plain.active_editor()->moveCursor(QTextCursor::End);
  const auto plain_before = plain.active_editor()->toPlainText();
  require(plain.insert_wnn_user_entry(entry) && !plain.is_jwp_document() &&
               plain.active_editor()->toPlainText() == plain_before +
                   jwpqt::qt::to_qstring(jwpqt::core::decode_jwp_text(
                       jwpqt::core::render_wnn_user_entry(entry))) + QStringLiteral("\n"),
          "User-conversion insertion did not preserve unrestricted Unicode");
  find_action(plain, "undoAction")->trigger();
  require(plain.active_editor()->toPlainText() == plain_before && !plain.document_modified(),
          "Unicode user-conversion insertion lost its saved undo baseline");
}

void test_edict_lookup_integration(const QString& directory) {
  const QString dictionary_path = directory + QStringLiteral("/edict-main");
  const QString registry_path = directory + QStringLiteral("/dict.cfg");
  write_bytes(dictionary_path, QByteArray("cat /feline/\n"));
  jwpqt::core::EdictRegistry registry;
  jwpqt::core::EdictRegistryEntry resource;
  resource.label = u"Main";
  resource.path = u"edict-main";
  resource.encoding = jwpqt::core::EdictRegistryEncoding::kUtf8;
  resource.searched = true;
  resource.keep = true;
  registry.entries.push_back(resource);
  jwpqt::qt::write_edict_registry_file(registry_path, registry);
  const QByteArray valid_registry = read_bytes(registry_path);

  jwpqt::core::JwpDocument source;
  source.paragraphs = {paragraph(U"cat")};
  const QString document_path = directory + QStringLiteral("/lookup.jwp");
  jwpqt::qt::write_jwp_file(document_path, source);

  jwpqt::qt::MainWindow window;
  require(window.load_edict_configuration(
              registry_path, jwpqt::qt::OpenMode::kNonInteractive) &&
              window.edict_resources() != nullptr &&
              window.edict_resources()->resources.size() == 1 &&
              window.open_jwp_path(document_path),
          "Could not prepare native EDICT lookup integration");
  QTextEdit* editor = window.findChild<QTextEdit*>();
  QAction* lookup = find_action(window, "edictLookupAction");
  require(editor != nullptr && lookup != nullptr && lookup->isEnabled() &&
              lookup->shortcuts().contains(
                  QKeySequence(QStringLiteral("Ctrl+D"))) &&
              lookup->shortcuts().contains(QKeySequence(Qt::Key_F6)),
          "Native EDICT lookup action is missing or has wrong shortcuts");
  window.show();
  QApplication::processEvents();
  editor->selectAll();
  auto* overwrite = find_action(window, "overwriteModeAction");
  auto lookup_preferences = window.application_settings();
  lookup_preferences.dictionary.automatic_search = false;
  require(window.apply_application_settings(lookup_preferences),
          "Could not isolate manual dictionary search from automatic selection lookup");
  overwrite->setChecked(true);
  lookup->trigger();
  QApplication::processEvents();
  auto* dialog = dynamic_cast<jwpqt::qt::EdictLookupDialog*>(
      window.findChild<QDialog*>(QStringLiteral("edictLookupDialog")));
  auto* query_edit = dialog == nullptr
                         ? nullptr
                         : dialog->findChild<QLineEdit*>(
                               QStringLiteral("edictQuery"));
  require(dialog != nullptr && query_edit != nullptr &&
              query_edit->text() == QStringLiteral("cat"),
          "Native EDICT dialog was not seeded from the JWP selection");
  query_edit->setText(QStringLiteral("ABC"));
  query_edit->setCursorPosition(1);
  QKeyEvent ascii_mode(QEvent::KeyPress, Qt::Key_F4, Qt::NoModifier);
  QApplication::sendEvent(query_edit, &ascii_mode);
  QKeyEvent typed(QEvent::KeyPress, Qt::Key_X, Qt::NoModifier, QStringLiteral("X"));
  QApplication::sendEvent(query_edit, &typed);
  require(query_edit->text() == QStringLiteral("AXC") && editor->overwriteMode() &&
              *window.current_jwp_document() == source,
          "Dictionary query did not inherit the window mode independently of its document");
  query_edit->undo();
  query_edit->setSelection(1, 1);
  QKeyEvent copy(QEvent::KeyPress, Qt::Key_Insert, Qt::ControlModifier);
  QApplication::sendEvent(query_edit, &copy);
  require(QApplication::clipboard()->text() == QStringLiteral("B"),
          "Dictionary query Copy used the document selection");
  query_edit->setCursorPosition(1);
  QApplication::clipboard()->setText(QStringLiteral("xy"));
  QKeyEvent paste(QEvent::KeyPress, Qt::Key_Insert, Qt::ControlModifier | Qt::ShiftModifier);
  QApplication::sendEvent(query_edit, &paste);
  require(query_edit->text() == QStringLiteral("AxyBC") &&
              *window.current_jwp_document() == source && overwrite->isChecked(),
          "Dictionary query paste reached the document or overwrote its suffix");
  query_edit->undo();
  QKeyEvent toggle(QEvent::KeyPress, Qt::Key_Insert, Qt::NoModifier);
  QApplication::sendEvent(query_edit, &toggle);
  require(query_edit->text() == QStringLiteral("ABC") && !overwrite->isChecked() &&
              !editor->overwriteMode() && !editor->document()->isModified(),
          "Dictionary query Insert did not share the document action without mutation");
  dialog->set_query(U"ca");
  require(!dialog->search() &&
              window.findChild<QWidget*>(
                  QStringLiteral("edictResultsWindow")) == nullptr,
          "Failed EDICT search created an empty accumulated-results window");
  dialog->set_query(U"cat");
  require(dialog->search(),
          "Native EDICT dialog could not search the configured resource");
  QTextEdit* results =
      dialog->findChild<QTextEdit*>(QStringLiteral("edictResults"));
  require(results != nullptr && results->toPlainText() == QStringLiteral("cat\nfeline"),
          "Native EDICT lookup did not expose the configured result");
  auto* accumulated =
      dynamic_cast<jwpqt::qt::EdictResultsWindow*>(window.findChild<QWidget*>(
          QStringLiteral("edictResultsWindow")));
  QAction* accumulated_action = find_action(window, "edictResultsAction");
  require(accumulated != nullptr && accumulated_action != nullptr &&
              accumulated_action->isEnabled() &&
              accumulated->result_count() == 1 && !accumulated->isVisible(),
          "Dictionary search opened a second window instead of updating in place");
  const QTextCursor saved_selection = editor->textCursor();
  for (const int position : {0, 4}) {
    QTextCursor cursor(results->document());
    cursor.setPosition(position);
    const QRect first = results->cursorRect(cursor);
    cursor.setPosition(position + 1);
    const QPoint point((first.left() + results->cursorRect(cursor).left()) / 2, first.center().y());
    QContextMenuEvent context(QContextMenuEvent::Mouse, point,
                             results->viewport()->mapToGlobal(point), Qt::ShiftModifier);
    QApplication::sendEvent(results->viewport(), &context);
  }
  const auto information = window.findChildren<QDialog*>(QStringLiteral("kanjiInfoDialog"));
  require(information.size() == 2 &&
              dynamic_cast<jwpqt::qt::KanjiInfoDialog*>(information[0])->code() == 'c' &&
              dynamic_cast<jwpqt::qt::KanjiInfoDialog*>(information[1])->code() == 'f' &&
              *window.current_jwp_document() == source &&
              editor->textCursor().position() == saved_selection.position() &&
              editor->textCursor().anchor() == saved_selection.anchor(),
          "Dictionary context navigation did not create independent character windows safely");
  require(dialog->search() && !accumulated->isVisible() && accumulated->result_count() == 2,
          "Repeated search raised the accumulated-results window");
  accumulated_action->trigger();
  require(accumulated->isVisible(),
          "Dictionary Results action did not reopen accumulated results");
  results->selectAll();
  require(dialog->insert_selected(),
          "Native EDICT result could not be inserted into JWP");
  const jwpqt::core::JwpText expected =
      jwpqt::core::encode_jwp_text(U"cat /feline/");
  require(window.current_jwp_document()->paragraphs.size() == 2 &&
              window.current_jwp_document()->paragraphs[0].text == expected &&
              window.current_jwp_document()->paragraphs[1].text.empty() &&
              editor->toPlainText() == QStringLiteral("cat /feline/\n"),
          "Native EDICT insertion did not replace the selected JWP text");
  QAction* undo = find_action(window, "undoAction");
  require(undo != nullptr && undo->isEnabled(),
          "Native EDICT insertion did not create portable history");
  undo->trigger();
  require(*window.current_jwp_document() == source,
          "Native EDICT insertion undo did not restore the source");

  lookup->trigger();
  QApplication::processEvents();
  require(window.findChildren<QDialog*>(QStringLiteral("edictLookupDialog"))
                  .size() == 1,
          "Native EDICT action created duplicate modeless dialogs");
  write_bytes(registry_path, QByteArray("bad"));
  require(!window.load_edict_configuration(
              registry_path, jwpqt::qt::OpenMode::kNonInteractive) &&
              window.edict_resources()->resources.size() == 1 &&
              window.findChild<QDialog*>(QStringLiteral("edictLookupDialog")) ==
                  dialog &&
              dynamic_cast<jwpqt::qt::EdictResultsWindow*>(
                  window.findChild<QWidget*>(
                      QStringLiteral("edictResultsWindow"))) == accumulated,
          "Malformed EDICT reload discarded the prior working state");
  write_bytes(registry_path, valid_registry);
  require(window.load_edict_configuration(
              registry_path, jwpqt::qt::OpenMode::kNonInteractive) &&
              window.findChild<QDialog*>(QStringLiteral("edictLookupDialog")) ==
                  nullptr &&
              window.findChild<QWidget*>(
                  QStringLiteral("edictResultsWindow")) == nullptr &&
              !accumulated_action->isEnabled(),
          "Successful EDICT reload retained stale dictionary windows");

  jwpqt::qt::MainWindow plain;
  const QString plain_path = directory + QStringLiteral("/lookup.txt");
  jwpqt::qt::write_text_file(
      plain_path,
      jwpqt::core::TextFile{U"plain \U0001f600", jwpqt::core::TextEncoding::kUtf8,
                           false});
  require(plain.open_path(plain_path, jwpqt::core::TextEncoding::kUtf8) &&
              plain.load_edict_configuration(registry_path),
          "Could not prepare unrestricted dictionary target");
  plain.active_editor()->moveCursor(QTextCursor::End);
  find_action(plain, "edictLookupAction")->trigger();
  auto* plain_dialog = dynamic_cast<jwpqt::qt::EdictLookupDialog*>(
      plain.findChild<QDialog*>(QStringLiteral("edictLookupDialog")));
  require(plain_dialog != nullptr, "Unicode dictionary lookup did not open");
  plain_dialog->set_query(U"cat");
  require(plain_dialog->search() && plain_dialog->insert_selected() &&
              !plain.is_jwp_document() && plain.active_editor()->toPlainText() ==
                  QString::fromStdU32String(U"plain \U0001f600cat /feline/\n"),
          "Dictionary result did not reach the unrestricted Unicode target");
  find_action(plain, "undoAction")->trigger();
  require(!plain.document_modified() && plain.active_editor()->toPlainText() ==
              QString::fromStdU32String(U"plain \U0001f600"),
          "Unicode dictionary insertion did not preserve Qt undo");

  auto owner = std::make_unique<jwpqt::qt::MainWindow>();
  require(owner->load_edict_configuration(
              registry_path, jwpqt::qt::OpenMode::kNonInteractive),
          "Could not prepare EDICT dialog owner-destruction fixture");
  QAction* owner_lookup = find_action(*owner, "edictLookupAction");
  require(owner_lookup != nullptr && owner_lookup->isEnabled(),
          "EDICT lookup was unavailable for owner-destruction fixture");
  owner_lookup->trigger();
  QApplication::processEvents();
  auto* owner_dialog = dynamic_cast<jwpqt::qt::EdictLookupDialog*>(
      owner->findChild<QDialog*>(QStringLiteral("edictLookupDialog")));
  if (owner_dialog != nullptr) {
    owner_dialog->set_query(U"cat");
  }
  require(owner_dialog != nullptr && owner_dialog->search() &&
              owner->findChild<QWidget*>(
                  QStringLiteral("edictResultsWindow")) != nullptr,
          "EDICT owner-destruction fixture did not create child windows");
  owner.reset();
  QApplication::processEvents();
}

void test_edict_automatic_search(const QString& directory) {
  using namespace jwpqt;
  const QString base = directory + QStringLiteral("/edict-automatic");
  require(QDir().mkpath(base), "Could not create automatic-search fixture");
  write_bytes(base + "/edict", QStringLiteral("\u3042 /cat/\n\u3044 /dog/\n").toUtf8());
  core::EdictRegistry registry;
  core::EdictRegistryEntry entry;
  entry.label = u"Automatic"; entry.path = u"edict";
  entry.encoding = core::EdictRegistryEncoding::kUtf8;
  entry.searched = true; entry.keep = true;
  registry.entries.push_back(entry);
  qt::write_edict_registry_file(base + "/dict.cfg", registry);
  qt::MainWindow window;
  require(window.load_edict_configuration(base + "/dict.cfg"), "Could not load automatic-search fixture");
  find_action(window, "newTextDocumentAction")->trigger();
  auto* editor = window.active_editor();
  editor->insertPlainText(QStringLiteral("cat\ndog"));
  const auto select = [&](int first, int last) {
    auto cursor = editor->textCursor();
    cursor.setPosition(first); cursor.setPosition(last, QTextCursor::KeepAnchor);
    editor->setTextCursor(cursor);
  };
  auto* action = find_action(window, "edictLookupAction");
  const auto lookup = [&] {
    return dynamic_cast<qt::EdictLookupDialog*>(window.findChild<QDialog*>("edictLookupDialog"));
  };
  select(0, 3);
  action->trigger();
  auto* dialog = lookup();
  require(dialog && dialog->query() == U"cat" && dialog->report().results.size() == 1 &&
              window.query_histories().dictionary.find(U"cat") &&
              editor->textCursor().selectionStart() == 0 && editor->textCursor().selectionEnd() == 3,
          "New lookup did not search its selection without changing the document");
  auto preferences = window.application_settings();
  preferences.dictionary.automatic_search = false;
  require(window.apply_application_settings(preferences), "Could not disable automatic lookup");
  dialog->set_query(U"");
  auto* query = dialog->findChild<QLineEdit*>("edictQuery");
  QKeyEvent k(QEvent::KeyPress, Qt::Key_K, Qt::NoModifier, "k");
  QApplication::sendEvent(query, &k);
  select(4, 7);
  action->trigger();
  QKeyEvent a(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, "a");
  QApplication::sendEvent(query, &a);
  require(dialog->query() == U"\u304b" && dialog->report().results.size() == 1 &&
              !window.query_histories().dictionary.find(U"dog"),
          "Disabled automatic lookup replaced a pending draft or searched");
  dialog->close(); QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  action->trigger(); dialog = lookup();
  require(dialog && dialog->query() == U"dog" && dialog->report().results.empty(),
          "New disabled lookup failed to prefill or searched unexpectedly");
  preferences.dictionary.automatic_search = true;
  require(window.apply_application_settings(preferences), "Could not enable automatic lookup");
  action->trigger();
  require(dialog->report().results.size() == 1 && window.query_histories().dictionary.find(U"dog"),
          "Existing lookup did not automatically search a new selection");
  select(1, 1);
  dialog->set_query(U"draft"); action->trigger();
  require(dialog->query() == U"draft", "Unselected invocation overwrote an existing query");
  dialog->close(); QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  action->trigger(); dialog = lookup();
  require(dialog && dialog->query() == U"cat" && dialog->report().results.empty(),
          "Native word-under-cursor prefill was lost or searched automatically");
  select(0, 7); action->trigger();
  require(dialog->query() == U"cat" && dialog->report().results.size() == 1,
          "Automatic lookup did not use the selected first-paragraph span");
  auto* results = dialog->findChild<QTextEdit*>("edictResults");
  const QPointer<QTextDocument> old_results(results->document());
  select(0, 2); action->trigger();
  require(old_results && results->document() == old_results &&
              !window.query_histories().dictionary.find(U"ca"),
          "Failed automatic lookup discarded results or remembered an invalid query");
  class ShowObserver final : public QObject {
   public:
    qt::EdictLookupDialog* dialog = nullptr;
    bool eventFilter(QObject*, QEvent* event) override {
      if (event->type() == QEvent::Show) dialog->set_query(U"external");
      return false;
    }
  } observer;
  observer.dialog = dialog;
  dialog->hide(); dialog->installEventFilter(&observer);
  select(0, 3); action->trigger();
  require(dialog->query() == U"external" && old_results && results->document() == old_results &&
              !window.query_histories().dictionary.find(U"external"),
          "Automatic lookup searched a query changed while showing its window");
  dialog->removeEventFilter(&observer);
  require(editor->toPlainText() == QStringLiteral("cat\ndog") && window.document_modified(),
          "Automatic lookup changed document text or its dirty state");
}

void test_edict_search_controls(const QString& directory) {
  using namespace jwpqt;
  const QString base = directory + QStringLiteral("/edict-controls");
  require(QDir().mkpath(base), "Could not prepare dictionary controls directory");
  write_bytes(base + QStringLiteral("/edict"), QStringLiteral(
      "\u3042 /cat/\n\u3044 /wild cat/\n\u3046 /cattle/\n"
      "\u3048 /bobcat/\n\u3055 /cat food/\n"
      "\u3042\u3044 /direct/\n\u3042\u304f /first/\n\u3042\u308b /later/\n"
      "\u4e9c\u304b\u3044 /adjective/\n"
      "\u3042\u3044\u3046\u3048 /leading/\n\u304b\u3042\u3044\u3046\u3048 /embedded/\n"
      "\u306a\u307e\u3048 /(s) human/\n\u306a\u307e\u3048 /(p) location/\n").toUtf8());
  core::EdictRegistry registry;
  core::EdictRegistryEntry resource;
  resource.label = u"Controls";
  resource.path = u"edict";
  resource.encoding = core::EdictRegistryEncoding::kUtf8;
  resource.searched = true;
  resource.keep = true;
  registry.entries.push_back(resource);
  const QString registry_path = base + QStringLiteral("/dict.cfg");
  qt::write_edict_registry_file(registry_path, registry);
  qt::MainWindow window;
  require(window.load_edict_configuration(registry_path, qt::OpenMode::kNonInteractive),
          "Could not load dictionary controls fixture");
  auto* action = find_action(window, "edictLookupAction");
  auto startup = window.application_settings();
  startup.startup_dictionary = true;
  require(window.apply_application_settings(startup) && window.open_startup_dictionary(true) &&
              !window.findChild<QDialog*>(QStringLiteral("edictLookupDialog")) &&
              window.open_startup_dictionary(false),
          "Dictionary startup ignored explicit-document precedence or available resources");
  auto* dialog = dynamic_cast<qt::EdictLookupDialog*>(
      window.findChild<QDialog*>(QStringLiteral("edictLookupDialog")));
  require(dialog != nullptr, "Could not open dictionary controls");
  const auto check = [&](const char* name, bool checked) {
    auto* box = dialog->findChild<QCheckBox*>(QString::fromLatin1(name));
    require(box && box->isEnabled(), "Dictionary policy control is missing or unavailable");
    if (box->isChecked() != checked) box->click();
  };
  const auto count = [&](std::u32string_view query) {
    dialog->set_query(query);
    require(dialog->search(), "Dictionary policy search failed");
    return dialog->report().results.size();
  };
  require(count(U"cat") == 4, "Default Begin With did not reject an embedded ASCII match");
  require(count(U"\u306a\u307e\u3048") == 0 && dialog->search(false, true) &&
              dialog->report().results.size() == 2 &&
              !window.application_settings().dictionary.personal_names &&
              !window.application_settings().dictionary.place_names &&
              dialog->search() && dialog->report().results.empty(),
          "One-shot Names did not filter actual resource records without changing preferences");
  check("edictBeginning", false);
  require(count(U"cat") == 5, "Open beginning did not include the embedded match");
  auto* sort_button = dialog->findChild<QPushButton*>(QStringLiteral("edictSort"));
  require(sort_button && sort_button->isEnabled(), "Native dictionary Sort button is unavailable");
  sort_button->click();
  require(dialog->sort_results(Qt::ControlModifier) &&
              dialog->report().results.front().result.record.headword == U"\u3055",
          "Native dictionary Sort did not reverse reading order");
  dialog->copy_selected();
  require(QApplication::clipboard()->text() == QStringLiteral("\u3055\ncat food") &&
               dialog->insert_selected() &&
               window.active_editor()->toPlainText() == QStringLiteral("\u3055 /cat food/\n"),
          "Sorted native dictionary selection lost its copy/insertion mapping");
  find_action(window, "undoAction")->trigger();
  require(!window.document_modified() && window.active_editor()->toPlainText().isEmpty(),
          "Sorted native dictionary insertion did not undo to its clean baseline");
  check("edictEnd", true);
  require(count(U"cat") == 4, "End With did not reject a trailing ASCII suffix");
  check("edictBeginning", true);
  require(count(U"cat") == 3, "Combined boundaries did not match complete words");
  check("edictFullAscii", true);
  require(count(U"cat") == 1, "Full ASCII did not require a complete definition");
  require(count(U"\uff43\uff41\uff54") == 0, "JASCII query was normalized without permission");
  check("edictJasciiToAscii", true);
  require(count(U"\uff43\uff41\uff54") == 1, "JASCII-to-ASCII policy did not reach preprocessing");
  require(count(U"\u3042\u3044") == 1, "Advanced-off search did not stay direct");
  require(count(U"\u3042\u3044\u3046") == 0, "Disabled contingent policy expanded an exact query");
  check("edictContingent", true);
  require(count(U"\u3042\u3044\u3046") == 1 &&
              dialog->report().results.front().result.stage == core::EdictSearchStage::kContingent,
          "Automatic contingent retry did not preserve its heuristic beginning boundary");
  require(dialog->search(true) && dialog->report().results.size() == 2,
          "Forced contingent retry did not relax the heuristic boundary");
  check("edictContingent", false);
  require(dialog->search(true) && dialog->report().results.size() == 2 &&
              !window.application_settings().dictionary.contingent &&
              dialog->search() && dialog->report().results.empty(),
          "Forced contingent request depended on or changed the saved preference");
  check("edictContingent", true);
  require(count(U"cat") == 1 && dialog->search(true) && dialog->report().results.size() == 1 &&
              dialog->report().results.front().result.stage == core::EdictSearchStage::kDirect,
          "Contingent forcing bypassed ASCII or successful-direct gates");
  check("edictContingent", false);
  check("edictAdvanced", true);
  check("edictAdvancedAlways", false);
  require(count(U"\u3042\u3044") == 1 && dialog->report().queries == 1,
          "Disabled Always policy did not stop after a direct result");
  check("edictAdvancedAlways", true);
  require(count(U"\u3042\u3044") == 2, "Advanced Always did not search sibling ending variants");
  check("edictAdvancedShowAll", true);
  require(count(U"\u3042\u3044") >= 3, "Advanced Show All did not continue after its first pass");
  check("edictAdvancedShowAll", false);
  check("edictIAdjectives", false);
  require(count(U"\u4e9c\u304b") == 0, "Disabled i-adjectives still generated the adjective match");
  check("edictIAdjectives", true);
  require(count(U"\u4e9c\u304b") == 1, "I-adjective policy was not forwarded to deinflection");
  auto* results = dialog->findChild<QTextEdit*>(QStringLiteral("edictResults"));
  const QPointer<QTextDocument> old_document = results->document();
  const int position = results->textCursor().position();
  const int anchor = results->textCursor().anchor();
  dialog->set_query(U"ca");
  require(!dialog->search() && old_document && results->document() == old_document &&
              results->textCursor().position() == position && results->textCursor().anchor() == anchor,
          "Invalid controlled search discarded its previous results or selection");
  check("edictAdvanced", false);
  require(dialog->insert_selected(), "Controlled dictionary result lost insertion ownership");
  find_action(window, "undoAction")->trigger();
  require(!window.document_modified(), "Controlled dictionary insertion did not undo to its baseline");
  const QPointer<QDialog> old_dialog = dialog;
  dialog->close();
  QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  require(!old_dialog, "Closed dictionary control dialog was not deleted");
  action->trigger();
  dialog = dynamic_cast<qt::EdictLookupDialog*>(
      window.findChild<QDialog*>(QStringLiteral("edictLookupDialog")));
  require(dialog != nullptr, "Dictionary history owner was not recreated");
  auto* history_query = dialog->findChild<QLineEdit*>(QStringLiteral("edictQuery"));
  QKeyEvent older_query(QEvent::KeyPress, Qt::Key_Up, Qt::NoModifier);
  QApplication::sendEvent(history_query, &older_query);
  require(history_query->text() == QStringLiteral("\u4e9c\u304b") &&
              dialog->report().results.empty() && !window.document_modified(),
          "Reopened history lost the last valid query, remembered a failure, or searched automatically");
  require(dialog && count(U"cat") == 1 &&
              dialog->findChild<QCheckBox*>(QStringLiteral("edictJasciiToAscii"))->isChecked(),
          "Dictionary policies did not survive close/reopen");
  require(window.load_edict_configuration(registry_path, qt::OpenMode::kNonInteractive),
          "Could not reload resources after a controlled query");
  action->trigger();
  dialog = dynamic_cast<qt::EdictLookupDialog*>(
      window.findChild<QDialog*>(QStringLiteral("edictLookupDialog")));
  require(dialog != nullptr, "Dictionary history owner was not recreated after reload");
  history_query = dialog->findChild<QLineEdit*>(QStringLiteral("edictQuery"));
  QApplication::sendEvent(history_query, &older_query);
  require(history_query->text() == QStringLiteral("cat") && dialog->report().results.empty(),
          "Resource replacement discarded query history or submitted a recall");
  require(dialog && count(U"cat") == 1,
          "Resource replacement discarded the window's dictionary policies");
  QPointer<QDialog> history_owner = dialog;
  QPointer<QDialog> history_popup;
  bool reloaded = false;
  QTimer::singleShot(0, &window, [&] {
    history_popup = qobject_cast<QDialog*>(QApplication::activeModalWidget());
    reloaded = history_popup && window.load_edict_configuration(registry_path, qt::OpenMode::kNonInteractive);
  });
  dialog->findChild<QPushButton*>(QStringLiteral("edictHistory"))->click();
  require(reloaded && !history_owner && !history_popup && !window.document_modified(),
          "Resource replacement retained a modal history owner or changed the document");
  action->trigger();
  dialog = dynamic_cast<qt::EdictLookupDialog*>(
      window.findChild<QDialog*>(QStringLiteral("edictLookupDialog")));
  require(dialog != nullptr, "Dictionary could not reopen after history chooser teardown");
  QApplication::sendEvent(dialog->findChild<QLineEdit*>(QStringLiteral("edictQuery")), &older_query);
  require(dialog->findChild<QLineEdit*>(QStringLiteral("edictQuery"))->text() == QStringLiteral("cat"),
          "Closing a modal history chooser discarded the shared query cache");

  const QString preferences_path = base + QStringLiteral("/preferences.cfg");
  check("edictMonitorClipboard", true);
  check("edictContingent", true);
  require(window.application_settings().dictionary.full_ascii &&
              window.application_settings().dictionary.require_end &&
              window.application_settings().dictionary.jascii_to_ascii &&
              window.save_application_settings(preferences_path),
          "Manual dictionary controls did not reach saved application settings");
  qt::MainWindow restored;
  require(restored.load_application_settings(preferences_path) &&
              restored.load_edict_configuration(registry_path),
          "Could not load dictionary preferences into a fresh window");
  find_action(restored, "edictLookupAction")->trigger();
  auto* restored_dialog = dynamic_cast<qt::EdictLookupDialog*>(
      restored.findChild<QDialog*>(QStringLiteral("edictLookupDialog")));
  require(restored_dialog != nullptr, "Restored dictionary did not open");
  require(restored.application_settings().dictionary.monitor_clipboard &&
              restored_dialog->findChild<QCheckBox*>("edictMonitorClipboard")->isChecked(),
          "Clipboard monitoring preference did not survive a restart");
  check("edictMonitorClipboard", false);
  restored_dialog->findChild<QCheckBox*>("edictMonitorClipboard")->click();
  restored_dialog->set_query(U"cat");
  require(restored_dialog->search() && restored_dialog->report().results.size() == 1 &&
              restored_dialog->findChild<QCheckBox*>(QStringLiteral("edictFullAscii"))->isChecked(),
          "Saved dictionary policies did not change the fresh window's actual search");

  require(count(U"cat") == 1, "Cannot prepare live dictionary settings update");
  history_query = dialog->findChild<QLineEdit*>(QStringLiteral("edictQuery"));
  history_query->setCursorPosition(history_query->text().size());
  QKeyEvent pending(QEvent::KeyPress, Qt::Key_K, Qt::NoModifier, QStringLiteral("k"));
  QApplication::sendEvent(history_query, &pending);
  results = dialog->findChild<QTextEdit*>(QStringLiteral("edictResults"));
  const QPointer<QTextDocument> preserved_results = results->document();
  const int preserved_position = results->textCursor().position();
  const int preserved_anchor = results->textCursor().anchor();
  struct ChangeOnFont : QObject {
    QCheckBox* box = nullptr;
    bool changed = false;
    bool eventFilter(QObject*, QEvent* event) override {
      if (event->type() == QEvent::FontChange && !changed) {
        changed = true;
        box->setChecked(true);
      }
      return false;
    }
  } listener;
  listener.box = dialog->findChild<QCheckBox*>(QStringLiteral("edictEnd"));
  history_query->installEventFilter(&listener);
  auto preferences = window.application_settings();
  preferences.dictionary.require_beginning = false;
  preferences.dictionary.require_end = false;
  preferences.dictionary.full_ascii = false;
  preferences.dictionary_extra_exclusions = 0x80000000U;
  preferences.fonts[static_cast<std::size_t>(qt::JapaneseFontRole::kSystem)].size = 17;
  require(window.apply_application_settings(preferences) && listener.changed &&
              window.application_settings().dictionary.require_end && listener.box->isChecked() &&
              history_query->text() == QStringLiteral("cat") && preserved_results &&
              results->document() == preserved_results &&
              results->textCursor().position() == preserved_position &&
              results->textCursor().anchor() == preserved_anchor && !window.document_modified(),
          "Settings lost a newer control change, pending input or existing dictionary results");
  history_query->removeEventFilter(&listener);
  QKeyEvent finish(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, QStringLiteral("a"));
  QApplication::sendEvent(history_query, &finish);
  require(history_query->text() == QStringLiteral("cat\u304b"),
          "Settings discarded the pending dictionary composer");
  check("edictEnd", false);
  require(count(U"cat") == 5, "Accepted dictionary options did not reach the next search");
  const auto accepted_preferences = qt::write_application_settings(window.application_settings());
  const QPointer<QTextDocument> before_options_results(results->document());
  bool options_seen = false;
  QTimer::singleShot(0, [&] {
    auto* popup = qobject_cast<QDialog*>(QApplication::activeModalWidget());
    if (!popup) return;
    auto* begin = popup->findChild<QCheckBox*>(QStringLiteral("settingsDictionaryBegin"));
    options_seen = begin != nullptr;
    if (begin) begin->setChecked(true);
    popup->findChild<QCheckBox*>("settingsDictionaryAutomatic")->setChecked(false);
    popup->findChild<QCheckBox*>("settingsDictionaryCompact")->setChecked(true);
    popup->findChild<QCheckBox*>("settingsDictionaryLinkNames")->setChecked(true);
    popup->findChild<QCheckBox*>("settingsDictionaryClipboard")->setChecked(true);
    popup->reject();
  });
  find_action(window, "applicationOptionsAction")->trigger();
  require(options_seen && qt::write_application_settings(window.application_settings()) == accepted_preferences,
          "Cancelling dictionary Options applied staged controls");
  QTimer::singleShot(0, [&] {
    auto* popup = qobject_cast<QDialog*>(QApplication::activeModalWidget());
    if (!popup) return;
    popup->findChild<QCheckBox*>(QStringLiteral("settingsDictionaryBegin"))->setChecked(true);
    require(popup->findChild<QTabWidget*>()->currentIndex() == 2,
            "Lookup Options did not open the Dictionary tab");
    auto* scroll = popup->findChild<QScrollArea*>("settingsDictionaryScroll");
    auto* note = popup->findChild<QLabel*>("settingsDictionaryNote");
    QApplication::processEvents();
    require(scroll && note && note->height() >= note->heightForWidth(note->width()),
            "Dictionary settings clipped their wrapped explanation");
    scroll->ensureWidgetVisible(note);
    require(scroll->viewport()->rect().contains(note->mapTo(scroll->viewport(), note->rect().bottomRight())),
            "Dictionary settings explanation could not be scrolled into view");
    scroll->verticalScrollBar()->setValue(0);
    options_seen = popup->grab().save(QDir::current().filePath(QStringLiteral("application-options-dictionary.png")));
    popup->findChild<QCheckBox*>("settingsDictionaryAutomatic")->setChecked(false);
    popup->findChild<QCheckBox*>("settingsDictionaryCompact")->setChecked(true);
    popup->findChild<QCheckBox*>("settingsDictionaryLinkNames")->setChecked(true);
    popup->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
  });
  dialog->findChild<QToolButton*>("edictOptions")->click();
  require(options_seen && window.application_settings().dictionary.require_beginning &&
               !window.application_settings().dictionary.automatic_search &&
                window.application_settings().dictionary.compact && window.application_settings().dictionary.link_advanced_names && before_options_results &&
               results->document() == before_options_results &&
              dialog->findChild<QCheckBox*>(QStringLiteral("edictBeginning"))->isChecked() &&
              dialog->report().results.size() == 5 && count(U"cat") == 4,
          "Accepted dictionary Options searched prematurely or failed to update live controls");
  require(results->document()->blockCount() == static_cast<int>(dialog->report().results.size()),
           "Accepted Compact preference did not reach the next real dictionary search");
  check("edictAdvanced", true);
  check("edictPersonalNames", true);
  require(!window.application_settings().dictionary.advanced && window.application_settings().dictionary.personal_names &&
              dialog->report().results.size() == 4 && window.save_application_settings(),
          "Linked name toggle did not update saved preferences without searching");
  qt::MainWindow linked;
  require(linked.load_application_settings(preferences_path) && linked.application_settings().dictionary.link_advanced_names &&
              !linked.application_settings().dictionary.advanced && linked.application_settings().dictionary.personal_names,
          "Linked dictionary policy did not survive a new application owner");
  QTimer::singleShot(0, [] {
    auto* popup = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    if (popup) popup->button(QMessageBox::Yes)->click();
  });
  find_action(window, "defaultSettingsAction")->trigger();
  require(window.application_settings().dictionary.require_beginning &&
               window.application_settings().dictionary.automatic_search &&
                !window.application_settings().dictionary.compact &&
                !window.application_settings().dictionary.link_advanced_names &&
              !window.application_settings().dictionary.require_end &&
              !window.application_settings().dictionary.full_ascii &&
              window.application_settings().dictionary_extra_exclusions == 0x80000000U &&
              window.application_settings().unapplied.join(QLatin1Char('\n')).contains(QStringLiteral("0x80000000")),
          "Default Settings lost unsupported dictionary bits or failed to reset supported flags");
  const auto before_bad_import = qt::write_application_settings(window.application_settings());
  const auto import_path = base + QStringLiteral("/import.cfg");
  write_bytes(import_path, "Dict_AdvancedSearches=bad\nDict_AdvancedSearches=true\n");
  require(!window.import_application_settings(import_path) &&
               qt::write_application_settings(window.application_settings()) == before_bad_import,
           "An invalid earlier dictionary setting changed live preferences");
  QPointer<qt::MainWindow> doomed = new qt::MainWindow;
  require(doomed->load_edict_configuration(registry_path), "Could not prepare Options owner deletion");
  find_action(*doomed, "edictLookupAction")->trigger();
  auto* doomed_lookup = doomed->findChild<QDialog*>(QStringLiteral("edictLookupDialog"));
  bool owner_deleted = false;
  QTimer::singleShot(0, [&] { owner_deleted = true; delete doomed.data(); });
  doomed_lookup->findChild<QToolButton*>("edictOptions")->click();
  require(owner_deleted && !doomed, "Options did not tolerate deletion of its owning workspace");
}

void test_edict_user_dictionary_integration(const QString& directory) {
  const QString case_directory = directory + QStringLiteral("/edict-user");
  require(QDir().mkpath(case_directory),
          "Could not create user dictionary integration directory");
  const QString registry_path = case_directory + QStringLiteral("/dict.cfg");
  const QString user_path = case_directory + QStringLiteral("/user.dct");

  jwpqt::core::EdictRegistry registry;
  jwpqt::core::EdictRegistryEntry main;
  main.label = u"Main";
  main.path = u"main";
  main.encoding = jwpqt::core::EdictRegistryEncoding::kUtf8;
  main.searched = true;
  main.keep = true;
  registry.entries.push_back(main);
  jwpqt::core::EdictRegistryEntry user;
  user.label = u"User";
  user.path = u"user.dct";
  user.encoding = jwpqt::core::EdictRegistryEncoding::kMixed;
  user.names = jwpqt::core::EdictRegistryNames::kNames;
  user.special = jwpqt::core::EdictRegistrySpecial::kUser;
  user.searched = true;
  user.keep = true;
  user.quiet = true;
  registry.entries.push_back(user);
  write_bytes(case_directory + QStringLiteral("/main"),
              QByteArray("cat /main/\n"));
  jwpqt::qt::write_edict_registry_file(registry_path, registry);

  const jwpqt::core::EdictUserEntry initial =
      jwpqt::core::make_edict_user_entry(
          jwpqt::core::encode_jwp_text(U"\u306d\u3053"),
          jwpqt::core::encode_jwp_text(U"\u732b"), U"cat");
  jwpqt::qt::write_edict_user_dictionary_file(
      user_path,
      jwpqt::core::EdictUserDictionary::from_entries({initial}),
      jwpqt::core::LegacyCodePage::k1252);

  jwpqt::qt::MainWindow window;
  require(window.load_edict_configuration(
              registry_path, jwpqt::qt::OpenMode::kNonInteractive) &&
              window.edict_user_dictionary() != nullptr &&
              window.edict_user_dictionary()->entries() ==
                  std::vector<jwpqt::core::EdictUserEntry>{initial},
          "Native user dictionary did not load from the registry");
  QAction* action = find_action(window, "edictUserDictionaryAction");
  require(action != nullptr && action->isEnabled(),
          "Native user dictionary action is unavailable");
  action->trigger();
  QApplication::processEvents();
  auto* dialog = dynamic_cast<jwpqt::qt::EdictUserDictionaryDialog*>(
      window.findChild<QDialog*>(
          QStringLiteral("edictUserDictionaryDialog")));
  require(dialog != nullptr && dialog->entries().size() == 1,
           "Native user dictionary dialog did not open its working copy");
  find_action(window, "edictLookupAction")->trigger();
  auto* lookup = window.findChild<QDialog*>(QStringLiteral("edictLookupDialog"));
  require(lookup && lookup->findChild<QToolButton*>("edictUserDictionary")->isEnabled(),
          "Lookup did not expose loaded user dictionary management");
  lookup->findChild<QToolButton*>("edictUserDictionary")->click();
  require(window.findChild<QDialog*>(QStringLiteral("edictUserDictionaryDialog")) == dialog,
          "Lookup created a second user dictionary instead of reusing its working copy");
  lookup->close();
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  require(window.findChild<QDialog*>(QStringLiteral("edictUserDictionaryDialog")) == dialog,
          "Closing lookup destroyed the independent user dictionary window");

  const jwpqt::core::EdictUserEntry replacement =
      jwpqt::core::make_edict_user_entry(
          jwpqt::core::encode_jwp_text(U"\u3044\u306c"),
          jwpqt::core::encode_jwp_text(U"\u72ac"), U"dog");
  const auto replacement_dictionary =
      jwpqt::core::EdictUserDictionary::from_entries({replacement});
  require(window.set_edict_user_dictionary(
              replacement_dictionary,
              jwpqt::qt::OpenMode::kNonInteractive) &&
              jwpqt::qt::read_edict_user_dictionary_file(
                  user_path, jwpqt::core::LegacyCodePage::k1252)
                      ->entries() ==
                  std::vector<jwpqt::core::EdictUserEntry>{replacement},
          "Native user dictionary update was not persisted");
  const auto loaded_user = std::find_if(
      window.edict_resources()->resources.begin(),
      window.edict_resources()->resources.end(), [](const auto& resource) {
        return resource.entry.special ==
               jwpqt::core::EdictRegistrySpecial::kUser;
      });
  require(loaded_user != window.edict_resources()->resources.end() &&
              loaded_user->dictionary.records().size() == 1 &&
              loaded_user->dictionary.records()[0].definitions ==
                  std::vector<std::u32string>{U"dog"},
          "Updated user dictionary did not replace its search resource");

  jwpqt::core::JwpDocument document;
  document.paragraphs = {paragraph(U"source")};
  const QString document_path = case_directory + QStringLiteral("/entry.jwp");
  jwpqt::qt::write_jwp_file(document_path, document);
  require(window.open_jwp_path(document_path),
          "Could not open user dictionary insertion fixture");
  QTextEdit* editor = window.findChild<QTextEdit*>();
  require(editor != nullptr, "User dictionary window has no editor");
  editor->selectAll();
  require(window.insert_edict_user_entry(replacement) &&
              editor->toPlainText() ==
                  QStringLiteral("\u72ac [\u3044\u306c]\tdog\n"),
          "User dictionary row was not inserted into JWP");
  QAction* undo = find_action(window, "undoAction");
  require(undo != nullptr && undo->isEnabled(),
          "User dictionary insertion did not create portable history");
  undo->trigger();
  require(*window.current_jwp_document() == document,
          "User dictionary insertion undo did not restore the document");

  require(window.load_edict_configuration(
              registry_path, jwpqt::qt::OpenMode::kNonInteractive) &&
              window.findChild<QDialog*>(
                  QStringLiteral("edictUserDictionaryDialog")) == nullptr,
          "Configuration reload retained a stale user dictionary dialog");

  const QString absent_registry =
      case_directory + QStringLiteral("/default/dict.cfg");
  require(QDir().mkpath(QFileInfo(absent_registry).absolutePath()),
          "Could not create default user dictionary directory");
  jwpqt::qt::MainWindow defaults;
  require(defaults.load_edict_configuration(
              absent_registry, jwpqt::qt::OpenMode::kNonInteractive) &&
              defaults.edict_user_dictionary() != nullptr &&
              defaults.edict_user_dictionary()->entries().empty() &&
              find_action(defaults, "edictUserDictionaryAction")->isEnabled() &&
              !find_action(defaults, "edictLookupAction")->isEnabled(),
          "Absent registry did not synthesize an empty user dictionary");

  const QString blocked_registry =
      case_directory + QStringLiteral("/missing-parent/dict.cfg");
  jwpqt::qt::MainWindow blocked;
  require(blocked.load_edict_configuration(
              blocked_registry, jwpqt::qt::OpenMode::kNonInteractive) &&
              !blocked.set_edict_user_dictionary(
                  replacement_dictionary,
                  jwpqt::qt::OpenMode::kNonInteractive) &&
              blocked.edict_user_dictionary()->entries().empty() &&
              !QFileInfo::exists(case_directory +
                                 QStringLiteral("/missing-parent/user.dct")),
          "Failed user dictionary save published candidate state");

  const QString disabled_directory =
      case_directory + QStringLiteral("/disabled");
  require(QDir().mkpath(disabled_directory),
          "Could not create disabled user dictionary directory");
  registry.entries = {user};
  registry.entries[0].searched = false;
  const QString disabled_registry =
      disabled_directory + QStringLiteral("/dict.cfg");
  jwpqt::qt::write_edict_registry_file(disabled_registry, registry);
  jwpqt::qt::MainWindow disabled;
  require(disabled.load_edict_configuration(
              disabled_registry, jwpqt::qt::OpenMode::kNonInteractive) &&
              disabled.set_edict_user_dictionary(
                  replacement_dictionary,
                  jwpqt::qt::OpenMode::kNonInteractive) &&
              disabled.edict_resources()->resources.empty() &&
              !find_action(disabled, "edictLookupAction")->isEnabled(),
          "Saving a disabled user dictionary made it searchable");
}

void test_jwp_wnn_preference_write_failure(const QString& directory) {
  const WnnFixture fixture = write_wnn_fixture(directory);
  const QString blocked_path = directory + QStringLiteral("/blocked-user.sel");
  jwpqt::core::JwpDocument source;
  source.paragraphs = {jwpqt::core::JwpParagraph{}};
  source.paragraphs[0].text = {0x2422};
  const QString source_path = directory + QStringLiteral("/blocked-save.jwp");
  jwpqt::qt::write_jwp_file(source_path, source);

  jwpqt::qt::MainWindow window;
  require(window.load_wnn_resources(fixture.index_path, fixture.data_path,
                                    blocked_path) &&
              window.open_jwp_path(source_path),
          "Could not prepare WNN preference failure fixture");
  require(QDir().mkpath(blocked_path),
          "Could not block the WNN preference output path");
  QTextEdit* editor = window.findChild<QTextEdit*>();
  require(editor != nullptr, "WNN preference failure window has no editor");
  editor->selectAll();
  require(window.convert_selection() && window.cycle_conversion() &&
              window.accept_conversion(),
          "Preference write failure prevented candidate acceptance");
  require(!window.conversion_active() && !editor->isReadOnly() &&
              window.current_jwp_document()->paragraphs[0].text ==
                  jwpqt::core::JwpText{0x3022},
          "Preference write failure rolled back accepted document state");
  QAction* undo = find_action(window, "undoAction");
  require(undo != nullptr && undo->isEnabled(),
          "Preference write failure discarded conversion history");
}

void send_text_key(QTextEdit* editor, int key, const QString& text,
                   Qt::KeyboardModifiers modifiers) {
  QKeyEvent event(QEvent::KeyPress, key, modifiers, text);
  QApplication::sendEvent(editor, &event);
}

void test_jwp_kana_input_mode(const QString& directory) {
  jwpqt::core::JwpDocument source;
  source.paragraphs = {jwpqt::core::JwpParagraph{}};
  const QString source_path = directory + QStringLiteral("/kana-input.jwp");
  jwpqt::qt::write_jwp_file(source_path, source);

  jwpqt::qt::MainWindow window;
  require(window.open_jwp_path(source_path),
          "Could not open kana-input fixture");
  QTextEdit* editor = window.findChild<QTextEdit*>();
  QAction* kana = find_action(window, "kanaInputAction");
  QToolButton* input_mode =
      window.findChild<QToolButton*>(QStringLiteral("inputMode"));
  require(editor != nullptr && kana != nullptr && input_mode != nullptr &&
              kana->isEnabled() && window.kana_input_enabled() &&
              input_mode->text() == QStringLiteral("Kanji"),
          "Input controls did not start in Kanji mode");

  kana->trigger();
  require(window.kana_input_enabled() && kana->isChecked() &&
              input_mode->text() == QStringLiteral("Kanji"),
          "Kana-input action did not enable composition");
  send_text_key(editor, Qt::Key_K, QStringLiteral("k"));
  send_text_key(editor, Qt::Key_A, QStringLiteral("a"));
  require(editor->toPlainText() == QStringLiteral("\u304b") &&
              window.current_jwp_document()->paragraphs[0].text ==
                  jwpqt::core::JwpText{0x242b},
          "Romaji input did not insert synchronized hiragana");

  send_text_key(editor, Qt::Key_K, QStringLiteral("k"));
  send_text_key(editor, Qt::Key_Backspace, QString());
  require(editor->toPlainText() == QStringLiteral("\u304b"),
          "Backspace mutated the document instead of discarding pending kana");
  send_text_key(editor, Qt::Key_A, QStringLiteral("a"));
  require(editor->toPlainText() == QStringLiteral("\u304b\u3042"),
          "Discarded composition leaked into the next kana input");

  send_text_key(editor, Qt::Key_K, QStringLiteral("K"), Qt::ShiftModifier);
  send_text_key(editor, Qt::Key_A, QStringLiteral("A"), Qt::ShiftModifier);
  require(editor->toPlainText() == QStringLiteral("\u304b\u3042\u30ab"),
          "Uppercase romaji did not insert katakana");

  find_action(window, "asciiInputAction")->trigger();
  send_text_key(editor, Qt::Key_K, QStringLiteral("k"));
  send_text_key(editor, Qt::Key_A, QStringLiteral("a"));
  require(!window.kana_input_enabled() &&
              editor->toPlainText() == QStringLiteral("\u304b\u3042\u30abka"),
          "Direct mode unexpectedly composed romaji");

  kana->trigger();
  send_text_key(editor, Qt::Key_N, QStringLiteral("n"));
  const QString saved_path = directory + QStringLiteral("/kana-pending.jwp");
  require(window.save_path(saved_path),
          "Could not save pending kana fixture");
  const std::u32string saved_text = jwpqt::core::decode_jwp_text(
      jwpqt::qt::read_jwp_file(saved_path).paragraphs[0].text);
  require(editor->toPlainText().endsWith(QStringLiteral("\u3093")) &&
              !saved_text.empty() && saved_text.back() == U'\u3093',
          "Save did not commit pending kana before writing");

  send_text_key(editor, Qt::Key_K, QStringLiteral("k"));
  QAction* undo = find_action(window, "undoAction");
  require(undo != nullptr && undo->isEnabled(),
          "Kana input did not create portable history");
  undo->trigger();
  send_text_key(editor, Qt::Key_A, QStringLiteral("a"));
  require(editor->toPlainText().endsWith(QStringLiteral("\u3042")),
          "Undo action left pre-command kana composition pending");

  const QString text_path = directory + QStringLiteral("/plain-kana.txt");
  write_bytes(text_path, QByteArray("plain"));
  require(window.open_path(text_path, jwpqt::core::TextEncoding::kUtf8) &&
              kana->isEnabled() && kana->isChecked() &&
              window.kana_input_enabled() && !window.uses_jwp_format(),
          "Representable text did not retain native kana input");
  auto invalid = source;
  invalid.paragraphs.front().text = {0x2921};
  const QString invalid_path = directory + QStringLiteral("/unmapped-kana.jwp");
  jwpqt::qt::write_jwp_file(invalid_path, invalid);
  editor->moveCursor(QTextCursor::End);
  send_text_key(editor, Qt::Key_K, QStringLiteral("k"));
  require(!window.set_japanese_editing(false, true) &&
              !find_action(window, "japaneseEditingAction")->isEnabled(),
          "Editing-mode switch discarded pending kana input");
  require(!window.open_jwp_path(invalid_path, jwpqt::core::kDefaultLegacyCodePage,
                                jwpqt::qt::OpenMode::kNonInteractive) &&
              window.current_path() == text_path,
          "Unmapped JWP document replaced the active text file");
  send_text_key(editor, Qt::Key_A, QStringLiteral("a"));
  require(editor->toPlainText() == QStringLiteral("plain\u304b"),
          "Failed JWP open discarded the previous pending composition");
}

void test_input_mode_workflow(const QString& directory) {
  jwpqt::qt::MainWindow window;
  QTextEdit* editor = window.findChild<QTextEdit*>();
  auto* mode = window.findChild<QToolButton*>(QStringLiteral("inputMode"));
  require(editor != nullptr && mode != nullptr && mode->isEnabled() &&
              mode->text() == QStringLiteral("Kanji") &&
              find_action(window, "kanaInputAction")->isChecked(),
          "Fresh editor did not expose its default input mode");
  std::vector<QKeySequence> shortcuts;
  for (const QAction* action : window.findChildren<QAction*>()) {
    for (const QKeySequence& shortcut : action->shortcuts()) {
      if (shortcut.isEmpty()) continue;
      require(std::find(shortcuts.begin(), shortcuts.end(), shortcut) == shortcuts.end(),
              "Duplicate native shortcut " + shortcut.toString().toStdString() +
                  " on " + action->text().toStdString());
      shortcuts.push_back(shortcut);
    }
  }
  window.show();
  editor->setFocus();
  QApplication::processEvents();
  send_text_key(editor, Qt::Key_N, QStringLiteral("n"));
  send_text_key(editor, Qt::Key_F4, {});
  require(mode->text() == QStringLiteral("ASCII") &&
              editor->toPlainText() == QStringLiteral("\u3093") &&
              find_action(window, "asciiInputAction")->isChecked(),
          "F4 did not commit pending kana and switch to ASCII");
  send_text_key(editor, Qt::Key_A, QStringLiteral("a"));
  require(editor->toPlainText() == QStringLiteral("\u3093a"),
          "ASCII mode composed input instead of inserting it directly");
  mode->click();
  require(mode->text() == QStringLiteral("JASCII") &&
              find_action(window, "jasciiInputAction")->isChecked(),
          "Status button did not cycle ASCII to JASCII");
  for (const char value : std::string("Ax9 ,.-")) {
    send_text_key(editor, Qt::Key_unknown, QString(QChar::fromLatin1(value)));
  }
  const jwpqt::core::JwpText expected{
      0x2473, 'a', 0x2341, 0x2378, 0x2339, 0x2121, 0x2124, 0x2125, 0x213d};
  require(window.current_jwp_document()->paragraphs[0].text == expected,
          "JASCII input did not use recovered full-width letters and punctuation");
  const QString path = directory + QStringLiteral("/input-modes.jwp");
  require(window.save_path(path) &&
              jwpqt::qt::read_jwp_file(path).paragraphs[0].text == expected,
          "JASCII input did not survive a native save");
  send_text_key(editor, Qt::Key_F4, {});
  require(mode->text() == QStringLiteral("Kanji"),
          "F4 from JASCII must switch to Kanji rather than ASCII");
  mode->click();
  require(mode->text() == QStringLiteral("ASCII"),
          "Status button did not cycle Kanji to ASCII");
  mode->click();
  mode->click();
  require(mode->text() == QStringLiteral("Kanji"),
          "Status button did not cycle JASCII to Kanji");

  const WnnFixture fixture = write_wnn_fixture(directory);
  require(window.load_wnn_resources(fixture.index_path, fixture.data_path,
                                   directory + QStringLiteral("/input-mode-user.sel")),
          "Could not load mode-switch conversion fixture");
  find_action(window, "newDocumentAction")->trigger();
  editor = window.active_editor();
  editor->insertPlainText(QStringLiteral("\u3042"));
  editor->selectAll();
  require(window.convert_selection(), "Could not start mode-switch conversion");
  const auto candidate = window.current_jwp_document()->paragraphs[0].text;
  require(candidate != jwpqt::core::JwpText{0x2422},
          "Mode-switch fixture did not choose a distinct candidate");
  send_text_key(editor, Qt::Key_F4, {});
  require(!window.conversion_active() && !editor->isReadOnly() &&
              window.current_jwp_document()->paragraphs[0].text == candidate &&
              mode->text() == QStringLiteral("ASCII"),
          "Mode switch did not accept the displayed conversion candidate");
  find_action(window, "undoAction")->trigger();
  require(editor->toPlainText() == QStringLiteral("\u3042"),
          "Conversion accepted by a mode switch lost its undo boundary: " +
              editor->toPlainText().toUtf8().toHex().toStdString());
}

void test_overwrite_mode(const QString& directory) {
  using namespace jwpqt;
  core::JwpDocument source;
  source.paragraphs.resize(2);
  source.paragraphs[0].text = {'A', 'B', 'C'};
  source.paragraphs[0].left_indent = 1;
  source.paragraphs[1].text = {'D'};
  source.summary[0] = {'M'};
  const QString path = directory + QStringLiteral("/overwrite.jwp");
  qt::write_jwp_file(path, source);
  qt::MainWindow window;
  require(window.open_jwp_path(path), "Could not load overwrite fixture");
  auto* editor = window.active_editor();
  auto* overwrite = find_action(window, "overwriteModeAction");
  auto* status = window.findChild<QToolButton*>(QStringLiteral("overwriteMode"));
  auto* undo = find_action(window, "undoAction");
  auto* redo = find_action(window, "redoAction");
  require(overwrite && status && !overwrite->isChecked() && !editor->overwriteMode() &&
              status->text() == QStringLiteral("INS") && status->focusPolicy() == Qt::NoFocus,
          "Document insert mode did not start with consistent controls");
  window.show();
  editor->setFocus();
  QApplication::processEvents();
  send_text_key(editor, Qt::Key_Insert, {});
  require(overwrite->isChecked() && editor->overwriteMode() && status->isChecked() &&
              status->isVisible() && status->text() == QStringLiteral("OVR") && !window.document_modified() &&
              !undo->isEnabled(), "Insert did not toggle mode without changing the document");
  require(window.grab().save(QCoreApplication::applicationDirPath() +
                            QStringLiteral("/overwrite-mode.png")),
          "Could not render the overwrite status control");
  const auto select = [&](int first, int last) {
    QTextCursor cursor(editor->document());
    cursor.setPosition(first);
    cursor.setPosition(last, QTextCursor::KeepAnchor);
    editor->setTextCursor(cursor);
  };
  struct Mode { const char* action; const char* text; const char16_t* inserted; };
  for (const auto& mode : {Mode{"asciiInputAction", "X", u"X"},
                           Mode{"jasciiInputAction", "X", u"\uff38"},
                           Mode{"kanaInputAction", "kya", u"\u304d\u3083"}}) {
    for (bool selected : {false, true}) {
      require(window.open_jwp_path(path), "Could not reset overwrite fixture");
      find_action(window, mode.action)->trigger();
      select(1, selected ? 2 : 1);
      for (const char* letter = mode.text; *letter; ++letter)
        send_text_key(editor, Qt::Key_unknown, QString(QChar::fromLatin1(*letter)));
      const QString inserted = QString::fromUtf16(mode.inserted);
      const QString expected = QStringLiteral("A") + inserted +
          ((selected || inserted.size() == 1) ? QStringLiteral("C\nD") : QStringLiteral("\nD"));
      require(editor->toPlainText() == expected && window.document_modified() &&
                  window.current_jwp_document()->paragraphs[0].left_indent == 1 &&
                  window.current_jwp_document()->summary[0] == source.summary[0],
              "Native typing did not honor overwrite/selection or damaged metadata");
      undo->trigger();
      require(editor->toPlainText() == QStringLiteral("ABC\nD") && !window.document_modified(),
              std::string("Native overwrite lost its single undo transaction or saved baseline: ") +
                  mode.action + (selected ? " selected " : " unselected ") +
                  editor->toPlainText().toUtf8().toHex().toStdString());
      redo->trigger();
      require(editor->toPlainText() == expected, "Native overwrite redo changed text");
    }
  }
  const QString saved = directory + QStringLiteral("/overwritten.jwp");
  require(window.save_as_path(saved, std::nullopt) &&
              core::decode_jwp_text(qt::read_jwp_file(saved).paragraphs[0].text) == U"A\u304d\u3083C",
          "Overwritten native text did not survive saving");

  require(window.open_jwp_path(path), "Could not reset clipboard fixture");
  find_action(window, "asciiInputAction")->trigger();
  select(1, 2);
  send_text_key(editor, Qt::Key_Insert, {}, Qt::ControlModifier);
  require(QApplication::clipboard()->text() == QStringLiteral("B") && overwrite->isChecked() &&
              !window.document_modified(), "Ctrl+Insert changed mode instead of copying");
  select(0, 0);
  QApplication::clipboard()->setText(QStringLiteral("xy"));
  send_text_key(editor, Qt::Key_Insert, {}, Qt::ShiftModifier);
  require(editor->toPlainText() == QStringLiteral("xyABC\nD") && overwrite->isChecked(),
          "Shift+Insert overwrote following text or toggled the mode");
  undo->trigger();
  require(!window.document_modified(), "Clipboard insertion lost the saved undo baseline");
  select(0, 0);
  send_text_key(editor, Qt::Key_Insert, {}, Qt::ControlModifier | Qt::ShiftModifier);
  require(editor->toPlainText() == QStringLiteral("xyABC\nD"),
          "Ctrl+Shift+Insert did not retain legacy paste precedence");
  undo->trigger();
  select(3, 3);
  find_action(window, "kanaInputAction")->trigger();
  send_text_key(editor, Qt::Key_A, QStringLiteral("a"));
  require(editor->toPlainText() == QStringLiteral("ABC\u3042\nD"),
          "Kana overwrite consumed a paragraph boundary");
  undo->trigger();
  select(1, 1);
  send_text_key(editor, Qt::Key_K, QStringLiteral("k"));
  status->click();
  require(!overwrite->isChecked() && editor->toPlainText() == QStringLiteral("ABC\nD") &&
              !window.document_modified(), "Mode toggle flushed pending kana");
  send_text_key(editor, Qt::Key_Insert, {});
  send_text_key(editor, Qt::Key_Insert, {});
  require(!overwrite->isChecked() && editor->toPlainText() == QStringLiteral("ABC\nD") &&
              !window.document_modified(), "Insert shortcut flushed pending kana");
  send_text_key(editor, Qt::Key_A, QStringLiteral("a"));
  require(editor->toPlainText() == QStringLiteral("A\u304bBC\nD"),
          "Mode toggle lost pending kana or ignored insert mode");
  undo->trigger();
  status->click();
  editor->setReadOnly(true);
  send_text_key(editor, Qt::Key_A, QStringLiteral("a"));
  find_action(window, "jasciiInputAction")->trigger();
  send_text_key(editor, Qt::Key_X, QStringLiteral("X"));
  require(editor->toPlainText() == QStringLiteral("ABC\nD") && !window.document_modified(),
          "Composed overwrite bypassed a read-only editor");
  editor->setReadOnly(false);
  select(1, 1);
  send_text_key(editor, Qt::Key_unknown, QStringLiteral("\U0001f600"));
  require(editor->toPlainText() == QStringLiteral("ABC\nD") && !window.document_modified() &&
              !undo->isEnabled(), "Unmapped native overwrite deleted the original character");
  select(1, 1);
  QInputMethodEvent commit;
  commit.setCommitString(QStringLiteral("\u65e5"));
  QApplication::sendEvent(editor, &commit);
  require(editor->toPlainText() == QStringLiteral("A\u65e5C\nD"),
          "Native input method commit ignored overwrite mode");
  undo->trigger();
  require(editor->toPlainText() == QStringLiteral("ABC\nD") && !window.document_modified(),
          "Native input method overwrite lost its history baseline");

  const QString unicode_path = directory + QStringLiteral("/overwrite-unicode.txt");
  qt::write_text_file(unicode_path, {U"A\U0001f600BC\nD", core::TextEncoding::kUtf16Be, true});
  require(window.open_path(unicode_path, core::TextEncoding::kUtf16Be, qt::OpenMode::kNonInteractive, true),
          "Could not open a Unicode overwrite tab");
  editor = window.active_editor();
  require(!window.is_jwp_document() && editor->overwriteMode() && overwrite->isChecked(),
          "New Unicode tab did not inherit the runtime mode");
  select(1, 1);
  send_text_key(editor, Qt::Key_X, QStringLiteral("X"));
  require(editor->toPlainText() == QStringLiteral("AXBC\nD"),
          "Unicode overwrite split a supplementary character");
  undo->trigger();
  require(editor->toPlainText() == QStringLiteral("A\U0001f600BC\nD") && !window.document_modified(),
          "Unicode overwrite lost its Qt undo baseline");
  select(1, 3);
  send_text_key(editor, Qt::Key_unknown, QStringLiteral("xy"));
  require(editor->toPlainText() == QStringLiteral("AxyBC\nD"),
          "Unicode overwrite deleted outside the selection");
  require(window.save_path(window.current_path()) && window.text_encoding() == core::TextEncoding::kUtf16Be &&
              qt::read_text_file(unicode_path, core::TextEncoding::kUtf16Be).text == U"AxyBC\nD",
          "Unicode overwrite changed the saved format or content");
  status->click();
  require(window.activate_document(0) && !window.active_editor()->overwriteMode(),
          "Runtime mode was not shared with the inactive native tab");
  qt::MainWindow separate;
  require(!find_action(separate, "overwriteModeAction")->isChecked(),
          "Runtime overwrite mode leaked into a new application window");
}

void test_forced_wnn_conversion(const QString& directory) {
  const auto fixture = write_automatic_wnn_fixture(directory);
  jwpqt::core::JwpDocument blank;
  blank.paragraphs = {jwpqt::core::JwpParagraph{}};
  const QString blank_path = directory + QStringLiteral("/forced-blank.jwp");
  jwpqt::qt::write_jwp_file(blank_path, blank);
  jwpqt::qt::MainWindow window;
  require(window.load_wnn_resources(fixture.index_path, fixture.data_path,
              directory + QStringLiteral("/forced-range.sel")),
          "Could not load forced-conversion fixture");
  auto* editor = window.findChild<QTextEdit*>();
  auto* convert = find_action(window, "convertSelectionAction");
  auto* candidates = window.findChild<QListWidget*>(QStringLiteral("conversionCandidates"));
  auto* convert_button = qobject_cast<QToolButton*>(
      window.findChild<QToolBar*>(QStringLiteral("mainToolBar"))->widgetForAction(convert));
  require(convert_button != nullptr, "Forced conversion toolbar button is missing");
  window.show();
  editor->setFocus();
  QApplication::processEvents();
  const auto type_ka = [&] {
    send_text_key(editor, Qt::Key_K, QStringLiteral("K"), Qt::ShiftModifier);
    send_text_key(editor, Qt::Key_A, QStringLiteral("a"));
  };
  type_ka();
  require(!window.conversion_active() && !editor->textCursor().hasSelection() &&
              editor->extraSelections().size() == 1 && convert->isEnabled(),
          "Convert is disabled for a waiting automatic kana range");
  convert_button->click();
  require(window.conversion_active() && candidates->isVisible() && candidates->count() == 3 &&
              editor->toPlainText() == QStringLiteral("\u4e9c"),
          "Forced conversion did not leave candidates open");
  send_text_key(editor, Qt::Key_Escape, QString());
  find_action(window, "undoAction")->trigger();
  require(editor->toPlainText() == QStringLiteral("\u304b"),
          "Forced conversion undo did not restore the reading");

  require(window.open_jwp_path(blank_path), "Could not reset forced conversion");
  type_ka();
  send_text_key(editor, Qt::Key_N, QStringLiteral("n"));
  convert->trigger();
  require(window.conversion_active() &&
              editor->toPlainText() == QStringLiteral("\u4e9c\u3093"),
          "Forced pending n lost the conversion suffix");
  window.accept_conversion();
  find_action(window, "undoAction")->trigger();
  require(editor->toPlainText() == QStringLiteral("\u304b\u3093"),
          "Pending-n conversion did not retain one conversion undo step");

  require(window.open_jwp_path(blank_path), "Could not reset selection conversion");
  editor->insertPlainText(QStringLiteral("\u3042"));
  type_ka();
  editor->selectAll();
  convert->trigger();
  require(!window.conversion_active() &&
              editor->toPlainText() == QStringLiteral("\u3042\u304b"),
          "An automatic range took precedence over the explicit selection");

  const auto single = write_wnn_fixture(directory);
  jwpqt::qt::MainWindow vowel;
  require(vowel.load_wnn_resources(single.index_path, single.data_path,
              directory + QStringLiteral("/forced-vowel.sel")),
          "Could not load forced-vowel fixture");
  auto* vowel_edit = vowel.findChild<QTextEdit*>();
  auto* vowel_convert = find_action(vowel, "convertSelectionAction");
  auto* vowel_button = qobject_cast<QToolButton*>(
      vowel.findChild<QToolBar*>(QStringLiteral("mainToolBar"))->widgetForAction(vowel_convert));
  require(vowel_button != nullptr, "Forced-vowel toolbar button is missing");
  vowel.show();
  vowel_edit->setFocus();
  send_text_key(vowel_edit, Qt::Key_A, QStringLiteral("A"), Qt::ShiftModifier);
  require(vowel_edit->toPlainText().isEmpty() && vowel_convert->isEnabled(),
          "A pending uppercase vowel cannot be forced from Convert");
  vowel_button->click();
  require(vowel.conversion_active() && vowel_edit->toPlainText() == QStringLiteral("\u4e9c"),
          "Forcing A emitted katakana or accepted the candidate prematurely");
  vowel.accept_conversion();
  find_action(vowel, "undoAction")->trigger();
  require(vowel_edit->toPlainText() == QStringLiteral("\u3042"),
          "Forced-vowel conversion lost its kana undo state");
  require(vowel.open_jwp_path(blank_path), "Could not reset forced vowel");
  send_text_key(vowel_edit, Qt::Key_U, QStringLiteral("U"), Qt::ShiftModifier);
  vowel_convert->trigger();
  require(!vowel.conversion_active() && !vowel_edit->isReadOnly() &&
              vowel_edit->toPlainText() == QStringLiteral("\u3046"),
          "A failed forced conversion lost the composed vowel");
}

void test_jwp_automatic_wnn_conversion(const QString& directory) {
  const WnnFixture fixture = write_automatic_wnn_fixture(directory);
  jwpqt::core::JwpDocument source;
  source.paragraphs = {jwpqt::core::JwpParagraph{}};
  const QString source_path = directory + QStringLiteral("/automatic.jwp");
  jwpqt::qt::write_jwp_file(source_path, source);

  jwpqt::qt::MainWindow window;
  require(window.load_wnn_resources(fixture.index_path, fixture.data_path,
                                    fixture.preferences_path) &&
              window.open_jwp_path(source_path),
          "Could not prepare automatic WNN fixture");
  QTextEdit* editor = window.findChild<QTextEdit*>();
  QAction* kana = find_action(window, "kanaInputAction");
  QAction* undo = find_action(window, "undoAction");
  require(editor != nullptr && kana != nullptr && undo != nullptr,
          "Automatic WNN controls were not created");
  kana->trigger();
  window.show();
  editor->setFocus();
  QApplication::processEvents();

  send_text_key(editor, Qt::Key_K, QStringLiteral("K"), Qt::ShiftModifier);
  send_text_key(editor, Qt::Key_A, QStringLiteral("a"));
  require(!window.conversion_active() &&
              window.current_jwp_document()->paragraphs[0].text ==
                  jwpqt::core::JwpText{0x242b} &&
              editor->extraSelections().size() == 1,
          "Extendable automatic WNN key did not remain pending");

  send_text_key(editor, Qt::Key_K, QStringLiteral("k"));
  send_text_key(editor, Qt::Key_I, QStringLiteral("i"));
  require(window.conversion_active() && editor->isReadOnly() &&
              window.current_jwp_document()->paragraphs[0].text ==
                  jwpqt::core::JwpText{0x3023},
          "Terminal automatic WNN key did not display its candidate");
  send_text_key(editor, Qt::Key_Escape, QString());
  require(!window.conversion_active() && !editor->isReadOnly() &&
              editor->extraSelections().empty(),
          "Escape did not accept automatic WNN conversion");
  undo->trigger();
  require(window.current_jwp_document()->paragraphs[0].text ==
              jwpqt::core::JwpText({0x242b, 0x242d}),
          "Automatic conversion undo did not restore composed kana");

  jwpqt::qt::MainWindow backed_off;
  const QString automatic_color_settings =
      directory + QStringLiteral("/automatic-kanji-colors.ini");
  const QString automatic_color_list_path =
      directory + QStringLiteral("/automatic-colkanji.lst");
  {
    QSettings settings(automatic_color_settings, QSettings::IniFormat);
    jwpqt::qt::write_kanji_color_policy(settings, {});
  }
  jwpqt::core::KanjiColorList automatic_color_list;
  require(automatic_color_list.add(0x3021) &&
              automatic_color_list.add(0x3022),
          "Could not prepare automatic-conversion color list");
  jwpqt::qt::write_kanji_color_list_file(automatic_color_list_path,
                                         automatic_color_list);
  require(backed_off.load_wnn_resources(
              fixture.index_path, fixture.data_path,
              directory + QStringLiteral("/automatic-backoff.sel")) &&
              backed_off.load_kanji_color_configuration(
                  automatic_color_settings, automatic_color_list_path,
                  jwpqt::qt::OpenMode::kNonInteractive) &&
              backed_off.open_jwp_path(source_path),
          "Could not prepare automatic WNN backoff fixture");
  QTextEdit* backoff_editor =
      backed_off.findChild<QTextEdit*>();
  QAction* backoff_kana = find_action(backed_off, "kanaInputAction");
  QAction* backoff_undo = find_action(backed_off, "undoAction");
  require(backoff_editor != nullptr && backoff_kana != nullptr &&
              backoff_undo != nullptr,
          "Automatic WNN backoff controls were not created");
  backoff_kana->trigger();
  backed_off.show();
  backoff_editor->setFocus();
  QApplication::processEvents();
  send_text_key(backoff_editor, Qt::Key_K, QStringLiteral("K"),
                Qt::ShiftModifier);
  send_text_key(backoff_editor, Qt::Key_A, QStringLiteral("a"));
  send_text_key(backoff_editor, Qt::Key_K, QStringLiteral("k"));
  send_text_key(backoff_editor, Qt::Key_U, QStringLiteral("u"));
  require(backed_off.conversion_active() && backoff_editor->isReadOnly() &&
              !backoff_editor->textCursor().hasSelection() &&
              backoff_editor->textCursor().position() == 2 &&
              backoff_editor->extraSelections().size() == 1 &&
               backed_off.current_jwp_document()->paragraphs[0].text ==
                   jwpqt::core::JwpText({0x3021, 0x242f}),
           "Automatic WNN backoff did not preserve its suffix and caret");

  jwpqt::core::KanjiColorPolicy automatic_policy;
  automatic_policy.list_mode = jwpqt::core::KanjiListColorMode::kMatch;
  automatic_policy.list_color = {12, 34, 56};
  require(backed_off.set_kanji_color_policy(
              automatic_policy,
              jwpqt::qt::OpenMode::kNonInteractive) &&
              backed_off.conversion_active() && backoff_editor->isReadOnly() &&
              backoff_editor->textCursor().position() == 2 &&
              backoff_editor->extraSelections().size() == 2 &&
              backoff_editor->extraSelections()[0].format.foreground().color() ==
                  QColor(12, 34, 56) &&
              backoff_editor->extraSelections()[1]
                      .format.background()
                      .style() != Qt::NoBrush,
          "Changing kanji colors discarded a transient WNN overlay");

  require(QFile::remove(automatic_color_settings) &&
              QDir().mkpath(automatic_color_settings),
          "Could not block automatic-conversion color settings");
  jwpqt::core::KanjiColorPolicy rejected_automatic_policy = automatic_policy;
  rejected_automatic_policy.list_color = {65, 43, 21};
  require(!backed_off.set_kanji_color_policy(
               rejected_automatic_policy,
               jwpqt::qt::OpenMode::kNonInteractive) &&
              backed_off.kanji_color_policy().list_mode ==
                  automatic_policy.list_mode &&
              backed_off.kanji_color_policy().list_color ==
                  automatic_policy.list_color &&
              backed_off.kanji_color_policy().colorize_uncommon ==
                  automatic_policy.colorize_uncommon &&
              backed_off.kanji_color_policy().uncommon_color ==
                  automatic_policy.uncommon_color &&
              backed_off.conversion_active() && backoff_editor->isReadOnly() &&
              backoff_editor->textCursor().position() == 2 &&
              backoff_editor->extraSelections().size() == 2 &&
              backoff_editor->extraSelections()[0].format.foreground().color() ==
                  QColor(12, 34, 56) &&
              backoff_editor->extraSelections()[1]
                      .format.background()
                      .style() != Qt::NoBrush,
          "Failed kanji-color persistence discarded a transient WNN overlay");
  send_text_key(backoff_editor, Qt::Key_Space, QStringLiteral(" "));
  require(backed_off.current_jwp_document()->paragraphs[0].text ==
                  jwpqt::core::JwpText({0x3022, 0x242f}) &&
              backoff_editor->textCursor().position() == 2,
          "Automatic WNN cycling did not preserve the unmatched suffix");
  send_text_key(backoff_editor, Qt::Key_Escape, QString());
  backoff_undo->trigger();
  require(backed_off.current_jwp_document()->paragraphs[0].text ==
                  jwpqt::core::JwpText({0x242b, 0x242f}),
          "Automatic WNN backoff undo did not restore the full kana input");

  jwpqt::qt::MainWindow externally_edited;
  require(externally_edited.open_jwp_path(
              source_path, jwpqt::core::LegacyCodePage::k1252,
              jwpqt::qt::OpenMode::kNonInteractive) &&
              externally_edited.load_wnn_resources(
                  fixture.index_path, fixture.data_path,
                  fixture.preferences_path,
                  jwpqt::qt::OpenMode::kNonInteractive),
          "Could not prepare external-edit automatic WNN test");
  QTextEdit* external_editor =
      externally_edited.findChild<QTextEdit*>();
  QAction* external_kana =
      externally_edited.findChild<QAction*>(QStringLiteral("kanaInputAction"));
  require(external_editor != nullptr && external_kana != nullptr,
          "External-edit automatic WNN controls were not created");
  externally_edited.show();
  external_editor->setFocus();
  QCoreApplication::processEvents();
  external_kana->trigger();
  send_text_key(external_editor, Qt::Key_K, QStringLiteral("K"));
  send_text_key(external_editor, Qt::Key_A, QStringLiteral("a"));
  require(external_editor->extraSelections().size() == 1,
          "External-edit test did not create a waiting WNN range");

  external_editor->insertPlainText(QStringLiteral("x"));
  QCoreApplication::processEvents();
  require(external_editor->extraSelections().empty() &&
              !externally_edited.conversion_active() &&
              externally_edited.current_jwp_document()->paragraphs[0].text ==
                  jwpqt::core::JwpText({0x242b, 0x0078}),
          "External document edit did not invalidate waiting WNN conversion");
  require(externally_edited.save_path(directory + QStringLiteral("/external.jwp")) &&
              externally_edited.current_jwp_document()->paragraphs[0].text ==
                  jwpqt::core::JwpText({0x242b, 0x0078}),
          "Saving after an external edit converted a stale WNN range");
}

void test_kanji_color_configuration(const QString& directory) {
  const QString settings_path =
      directory + QStringLiteral("/kanji-colors.ini");
  const QString list_path = directory + QStringLiteral("/colkanji.lst");
  jwpqt::core::KanjiColorPolicy policy;
  policy.list_mode = jwpqt::core::KanjiListColorMode::kMatch;
  policy.list_color = {1, 2, 3};
  policy.colorize_uncommon = true;
  policy.uncommon_color = {4, 5, 6};
  {
    QSettings settings(settings_path, QSettings::IniFormat);
    jwpqt::qt::write_kanji_color_policy(settings, policy);
  }
  jwpqt::core::KanjiColorList list;
  require(list.add(0x3021), "Could not prepare kanji color list fixture");
  jwpqt::qt::write_kanji_color_list_file(list_path, list);

  jwpqt::qt::MainWindow window;
  require(window.load_kanji_color_configuration(
              settings_path, list_path,
              jwpqt::qt::OpenMode::kNonInteractive) &&
              window.kanji_color_policy().list_mode ==
                  jwpqt::core::KanjiListColorMode::kMatch &&
              window.kanji_color_policy().list_color ==
                  jwpqt::core::RgbColor{1, 2, 3} &&
              window.kanji_color_list().contains(0x3021),
          "Kanji color configuration did not load atomically");

  jwpqt::core::JwpDocument source;
  source.paragraphs = {jwpqt::core::JwpParagraph{}};
  source.paragraphs[0].text = {0x3021};
  const QString jwp_path = directory + QStringLiteral("/kanji-color.jwp");
  jwpqt::qt::write_jwp_file(jwp_path, source);
  require(window.open_jwp_path(jwp_path),
          "Could not open kanji color configuration fixture");
  auto* editor =
      dynamic_cast<jwpqt::qt::JwpEditor*>(window.findChild<QTextEdit*>());
  require(editor != nullptr && editor->extraSelections().size() == 1 &&
              editor->extraSelections().front().cursor.selectionStart() == 0 &&
              editor->extraSelections().front().cursor.selectionEnd() == 1 &&
              editor->extraSelections().front().format.foreground().color() ==
                  QColor(1, 2, 3),
          "Loaded kanji color policy was not applied to the JWP document");

  QTextCursor rejected = editor->textCursor();
  rejected.setPosition(0);
  rejected.setPosition(1, QTextCursor::KeepAnchor);
  rejected.insertText(QString::fromUcs4(U"\U0001f600"));
  require(editor->toPlainText() != QString::fromUcs4(U"\U0001f600") &&
              editor->extraSelections().size() == 1 &&
              editor->extraSelections().front().cursor.selectionStart() == 0 &&
              editor->extraSelections().front().cursor.selectionEnd() == 1,
          "Rejected JWP edit did not restore the kanji color overlay");

  QTextCursor removal = editor->textCursor();
  removal.setPosition(0);
  removal.setPosition(1, QTextCursor::KeepAnchor);
  removal.removeSelectedText();
  require(editor->extraSelections().empty(),
          "Kanji color overlay was stale after a native edit");
  QAction* undo = find_action(window, "undoAction");
  require(undo != nullptr && undo->isEnabled(),
          "Kanji color edit did not create a portable undo entry");
  undo->trigger();
  require(editor->extraSelections().size() == 1,
          "Undo did not restore the kanji color overlay");

  const QString malformed_settings =
      directory + QStringLiteral("/malformed-kanji-colors.ini");
  {
    QSettings settings(malformed_settings, QSettings::IniFormat);
    settings.setValue(QStringLiteral("kanjiColor/policy"),
                      QStringLiteral("v1;invalid;010203;1;040506"));
    settings.sync();
  }
  require(!window.load_kanji_color_configuration(
              malformed_settings, list_path,
              jwpqt::qt::OpenMode::kNonInteractive) &&
              window.kanji_color_policy().list_mode ==
                  jwpqt::core::KanjiListColorMode::kMatch &&
              window.kanji_color_list().contains(0x3021) &&
              editor->extraSelections().size() == 1,
          "Failed kanji color reload replaced the working configuration");

  const QString malformed_list =
      directory + QStringLiteral("/malformed-colkanji.lst");
  QFile malformed_list_file(malformed_list);
  require(malformed_list_file.open(QIODevice::WriteOnly) &&
              malformed_list_file.write("\xb0", 1) == 1,
          "Could not create malformed kanji color list fixture");
  malformed_list_file.close();
  require(!window.load_kanji_color_configuration(
              settings_path, malformed_list,
              jwpqt::qt::OpenMode::kNonInteractive) &&
              window.kanji_color_policy().list_mode ==
                  jwpqt::core::KanjiListColorMode::kMatch &&
              window.kanji_color_list().contains(0x3021) &&
              editor->extraSelections().size() == 1,
          "Failed kanji color list reload replaced the working configuration");

  const QString text_path = directory + QStringLiteral("/plain-after-color.txt");
  jwpqt::qt::write_text_file(
      text_path, jwpqt::core::TextFile{U"plain", jwpqt::core::TextEncoding::kUtf8,
                                      false});
  require(window.open_path(text_path, jwpqt::core::TextEncoding::kUtf8) &&
              editor->extraSelections().empty(),
          "Plain-text load retained JWP kanji color overlays");

  jwpqt::qt::MainWindow defaults;
  require(defaults.load_kanji_color_configuration(
              directory + QStringLiteral("/missing-settings.ini"),
              directory + QStringLiteral("/missing-colkanji.lst"),
              jwpqt::qt::OpenMode::kNonInteractive) &&
              defaults.kanji_color_policy().list_mode ==
                  jwpqt::core::KanjiListColorMode::kOff &&
              defaults.kanji_color_list().empty(),
          "Missing kanji color files did not load as defaults");
}

void test_kanji_color_options(const QString& directory) {
  const QString settings_path =
      directory + QStringLiteral("/kanji-options.ini");
  const QString list_path = directory + QStringLiteral("/kanji-options.lst");
  jwpqt::core::KanjiColorPolicy initial;
  initial.list_mode = jwpqt::core::KanjiListColorMode::kMatch;
  initial.list_color = {1, 2, 3};
  {
    QSettings settings(settings_path, QSettings::IniFormat);
    jwpqt::qt::write_kanji_color_policy(settings, initial);
  }
  jwpqt::core::KanjiColorList list;
  require(list.add(0x3021), "Could not prepare kanji options list");
  jwpqt::qt::write_kanji_color_list_file(list_path, list);

  jwpqt::core::JwpDocument source;
  source.paragraphs = {jwpqt::core::JwpParagraph{}};
  source.paragraphs[0].text = {0x3021, 0x5021};
  const QString jwp_path = directory + QStringLiteral("/kanji-options.jwp");
  jwpqt::qt::write_jwp_file(jwp_path, source);

  PromptingWindow window;
  require(window.load_kanji_color_configuration(
              settings_path, list_path,
              jwpqt::qt::OpenMode::kNonInteractive) &&
              window.open_jwp_path(jwp_path),
          "Could not prepare native kanji color options fixture");
  QAction* options = find_action(window, "kanjiColorOptionsAction");
  auto* editor =
      dynamic_cast<jwpqt::qt::JwpEditor*>(window.findChild<QTextEdit*>());
  require(options != nullptr && editor != nullptr,
          "Kanji color options controls were not created");

  jwpqt::core::KanjiColorPolicy updated;
  updated.list_mode = jwpqt::core::KanjiListColorMode::kNoMatch;
  updated.list_color = {10, 20, 30};
  updated.colorize_uncommon = true;
  updated.uncommon_color = {40, 50, 60};
  window.next_kanji_color_policy = updated;
  options->trigger();
  require(window.kanji_color_prompt_count == 1 &&
              window.offered_kanji_color_policy.has_value() &&
              window.offered_kanji_color_policy->list_mode ==
                  jwpqt::core::KanjiListColorMode::kMatch &&
              window.kanji_color_policy().list_mode ==
                  jwpqt::core::KanjiListColorMode::kNoMatch &&
              window.kanji_color_policy().list_color ==
                  jwpqt::core::RgbColor{10, 20, 30} &&
              window.kanji_color_policy().colorize_uncommon &&
              window.kanji_color_policy().uncommon_color ==
                  jwpqt::core::RgbColor{40, 50, 60},
          "Kanji color options dialog did not publish the selected policy");
  require(editor->extraSelections().size() == 1 &&
              editor->extraSelections().front().cursor.selectionStart() == 1 &&
              editor->extraSelections().front().cursor.selectionEnd() == 2 &&
              editor->extraSelections().front().format.foreground().color() ==
                  QColor(10, 20, 30),
          "Kanji color options did not refresh native display precedence");
  {
    QSettings settings(settings_path, QSettings::IniFormat);
    const jwpqt::core::KanjiColorPolicy persisted =
        jwpqt::qt::read_kanji_color_policy(settings);
    require(persisted.list_mode ==
                    jwpqt::core::KanjiListColorMode::kNoMatch &&
                persisted.list_color == jwpqt::core::RgbColor{10, 20, 30} &&
                persisted.colorize_uncommon &&
                persisted.uncommon_color ==
                    jwpqt::core::RgbColor{40, 50, 60},
            "Kanji color options were not persisted");
  }

  window.next_kanji_color_policy.reset();
  options->trigger();
  require(window.kanji_color_prompt_count == 2 &&
              window.kanji_color_policy().list_mode ==
                  jwpqt::core::KanjiListColorMode::kNoMatch,
          "Cancelling kanji color options changed the working policy");

  jwpqt::core::KanjiColorPolicy invalid = updated;
  invalid.list_mode = static_cast<jwpqt::core::KanjiListColorMode>(99);
  require(!window.set_kanji_color_policy(
              invalid, jwpqt::qt::OpenMode::kNonInteractive) &&
              window.kanji_color_policy().list_mode ==
                  jwpqt::core::KanjiListColorMode::kNoMatch &&
              editor->extraSelections().size() == 1,
          "Invalid kanji color options changed the working state");

  const QString blocked_root =
      directory + QStringLiteral("/blocked-kanji-options");
  require(QDir().mkpath(blocked_root),
          "Could not create blocked kanji options fixture");
  PromptingWindow blocked;
  require(blocked.load_kanji_color_configuration(
              blocked_root + QStringLiteral("/settings.ini"),
              blocked_root + QStringLiteral("/colkanji.lst"),
              jwpqt::qt::OpenMode::kNonInteractive) &&
              blocked.open_jwp_path(jwp_path),
          "Could not load blocked kanji options fixture");
  require(QDir(blocked_root).removeRecursively(),
          "Could not remove blocked kanji options directory");
  QFile blocker(blocked_root);
  require(blocker.open(QIODevice::WriteOnly) && blocker.write("x", 1) == 1,
          "Could not block kanji options output directory");
  blocker.close();
  require(!blocked.set_kanji_color_policy(
              updated, jwpqt::qt::OpenMode::kNonInteractive) &&
              blocked.kanji_color_policy().list_mode ==
                  jwpqt::core::KanjiListColorMode::kOff,
          "Failed kanji color persistence published the candidate policy");
  auto* blocked_editor =
      dynamic_cast<jwpqt::qt::JwpEditor*>(blocked.findChild<QTextEdit*>());
  require(blocked_editor != nullptr && blocked_editor->extraSelections().empty(),
          "Failed kanji color persistence retained the candidate overlay");
}

void test_kanji_color_list_commands(const QString& directory) {
  const QString settings_path =
      directory + QStringLiteral("/kanji-list-commands.ini");
  const QString list_path =
      directory + QStringLiteral("/kanji-list-commands.lst");
  jwpqt::core::KanjiColorPolicy policy;
  policy.list_mode = jwpqt::core::KanjiListColorMode::kMatch;
  policy.list_color = {4, 5, 6};
  {
    QSettings settings(settings_path, QSettings::IniFormat);
    jwpqt::qt::write_kanji_color_policy(settings, policy);
  }
  jwpqt::core::KanjiColorList initial_list;
  require(initial_list.add(0x3021),
          "Could not prepare initial kanji command list");
  jwpqt::qt::write_kanji_color_list_file(list_path, initial_list);

  jwpqt::core::JwpDocument source;
  source.paragraphs = {jwpqt::core::JwpParagraph{}};
  source.paragraphs[0].text = {0x3022, 0x2422, 0x3023};
  const QString source_path =
      directory + QStringLiteral("/kanji-list-commands.jwp");
  jwpqt::qt::write_jwp_file(source_path, source);

  {
    const QString dialog_settings =
        directory + QStringLiteral("/kanji-list-dialog.ini");
    const QString dialog_list =
        directory + QStringLiteral("/kanji-list-dialog.lst");
    {
      QSettings settings(dialog_settings, QSettings::IniFormat);
      jwpqt::qt::write_kanji_color_policy(settings, policy);
    }
    jwpqt::qt::write_kanji_color_list_file(dialog_list, initial_list);

    jwpqt::qt::MainWindow dialog_window;
    require(dialog_window.load_kanji_color_configuration(
                dialog_settings, dialog_list,
                jwpqt::qt::OpenMode::kNonInteractive),
            "Could not prepare native kanji-list prompt");
    QAction* dialog_edit =
        find_action(dialog_window, "editKanjiColorListAction");
    QAction* overwrite = find_action(dialog_window, "overwriteModeAction");
    require(dialog_edit != nullptr && dialog_edit->isEnabled() &&
                overwrite != nullptr,
            "Native kanji-list prompt actions were unavailable");
    overwrite->setChecked(true);

    QString final_text;
    QTimer::singleShot(0, [&] {
      auto* dialog =
          qobject_cast<QDialog*>(QApplication::activeModalWidget());
      require(dialog != nullptr,
              "Native kanji-list prompt was not shown");
      auto* text =
          dialog->findChild<QLineEdit*>(QStringLiteral("kanjiColorListText"));
      auto* mode = dialog->findChild<QToolButton*>(
          QStringLiteral("kanjiColorListTextMode"));
      auto* buttons = dialog->findChild<QDialogButtonBox*>();
      require(text != nullptr && mode != nullptr && buttons != nullptr &&
                  mode->text() == QStringLiteral("K"),
              "Native kanji-list prompt did not expose Japanese input");
      QObject::connect(text, &QLineEdit::textChanged,
                       [&](const QString& value) { final_text = value; });
      text->setText(QStringLiteral("\u65e5\u6708"));
      text->setCursorPosition(1);
      QInputMethodEvent commit;
      commit.setCommitString(QStringLiteral("\u672c"));
      QApplication::sendEvent(text, &commit);
      QKeyEvent pending_n(QEvent::KeyPress, Qt::Key_N, Qt::NoModifier,
                          QStringLiteral("n"));
      QApplication::sendEvent(text, &pending_n);
      buttons->button(QDialogButtonBox::Ok)->click();
    });
    dialog_edit->trigger();
    const jwpqt::core::JwpText prompt_codes =
        jwpqt::core::encode_jwp_text(U"\u65e5\u672c\u6708");
    require(final_text == QStringLiteral("\u65e5\u672c\u3093") &&
                dialog_window.kanji_color_list().contains(prompt_codes[0]) &&
                dialog_window.kanji_color_list().contains(prompt_codes[1]) &&
                !dialog_window.kanji_color_list().contains(prompt_codes[2]),
            "Native kanji-list prompt lost overwrite or pending Japanese input");
  }

  PromptingWindow window;
  require(window.load_kanji_color_configuration(
              settings_path, list_path,
              jwpqt::qt::OpenMode::kNonInteractive) &&
              window.open_jwp_path(source_path),
          "Could not prepare kanji list command fixture");
  QAction* make = find_action(window, "makeKanjiColorListAction");
  QAction* append = find_action(window, "appendKanjiColorListAction");
  QAction* edit = find_action(window, "editKanjiColorListAction");
  QAction* view = find_action(window, "viewKanjiColorListAction");
  QAction* clear = find_action(window, "clearKanjiColorListAction");
  auto* editor =
      dynamic_cast<jwpqt::qt::JwpEditor*>(window.findChild<QTextEdit*>());
  require(make != nullptr && append != nullptr && edit != nullptr &&
              view != nullptr && clear != nullptr && editor != nullptr &&
              make->isEnabled() && append->isEnabled() && edit->isEnabled() &&
              view->isEnabled() && clear->isEnabled(),
          "Kanji list commands were not enabled for a JWP document");

  append->trigger();
  require(window.kanji_color_list().size() == 3 &&
              window.kanji_color_list().contains(0x3021) &&
              window.kanji_color_list().contains(0x3022) &&
              window.kanji_color_list().contains(0x3023),
          "Append did not merge document kanji into the color list");
  const std::optional<jwpqt::core::KanjiColorList> appended =
      jwpqt::qt::read_kanji_color_list_file(list_path);
  require(appended.has_value() && appended->size() == 3,
          "Append did not persist the complete kanji color list");

  make->trigger();
  require(window.kanji_color_list().size() == 2 &&
              !window.kanji_color_list().contains(0x3021) &&
              window.kanji_color_list().contains(0x3022) &&
              window.kanji_color_list().contains(0x3023),
          "Make did not replace the list with document kanji");

  const std::u32string remove_text =
      jwpqt::core::decode_jwp_text(jwpqt::core::JwpText{0x3022});
  window.next_kanji_color_list_edit =
      jwpqt::qt::KanjiColorListEditRequest{
          QString::fromUcs4(remove_text.data(),
                            static_cast<qsizetype>(remove_text.size())),
          false};
  edit->trigger();
  require(window.kanji_color_list_prompt_count == 1 &&
              window.kanji_color_list().size() == 1 &&
              !window.kanji_color_list().contains(0x3022) &&
              window.kanji_color_list().contains(0x3023),
          "Remove dialog did not remove encoded kanji from the list");

  const std::u32string add_text =
      jwpqt::core::decode_jwp_text(jwpqt::core::JwpText{0x3021});
  window.next_kanji_color_list_edit =
      jwpqt::qt::KanjiColorListEditRequest{
          QString::fromUcs4(add_text.data(),
                            static_cast<qsizetype>(add_text.size())),
          true};
  edit->trigger();
  require(window.kanji_color_list_prompt_count == 2 &&
              window.kanji_color_list().size() == 2 &&
              window.kanji_color_list().contains(0x3021),
          "Add dialog did not add encoded kanji to the list");
  window.next_kanji_color_list_edit.reset();
  edit->trigger();
  require(window.kanji_color_list_prompt_count == 3 &&
              window.kanji_color_list().size() == 2,
          "Cancelling kanji list editing changed the list");
  require(!window.edit_kanji_color_list(
              QString::fromUtf8("\xf0\x9f\x98\x80"), true,
              jwpqt::qt::OpenMode::kNonInteractive) &&
              window.kanji_color_list().size() == 2,
          "Unrepresentable list input changed the working list");

  view->trigger();
  require(window.is_jwp_document() &&
              window.current_jwp_document()->paragraphs.size() == 1 &&
              window.current_jwp_document()->paragraphs[0].text ==
                  jwpqt::core::JwpText({0x3021, 0x3023}) &&
              !editor->document()->isModified(),
          "View did not create an unmodified sorted JWP list document");

  constexpr jwpqt::core::JisCode kUnmappedListCode = 0x745b;
  require(!jwpqt::core::jis_x0208_to_unicode(kUnmappedListCode).has_value(),
          "Kanji list view fixture unexpectedly has a Unicode mapping");
  jwpqt::core::KanjiColorList unmapped_list;
  require(unmapped_list.add(kUnmappedListCode) &&
              window.set_kanji_color_list(
                  unmapped_list,
                  jwpqt::qt::OpenMode::kNonInteractive),
          "Could not prepare unmapped kanji list view fixture");
  const jwpqt::core::JwpDocument before_failed_view =
      *window.current_jwp_document();
  QTextCursor before_failed_view_cursor = editor->textCursor();
  before_failed_view_cursor.setPosition(1);
  before_failed_view_cursor.setPosition(0, QTextCursor::KeepAnchor);
  editor->setTextCursor(before_failed_view_cursor);
  const QString before_failed_view_text = editor->toPlainText();
  const QString before_failed_view_title = window.windowTitle();
  const bool before_failed_view_modified = editor->document()->isModified();
  const bool before_failed_view_undo_enabled =
      find_action(window, "undoAction")->isEnabled();
  const auto before_failed_view_selections = editor->extraSelections();
  require(!window.view_kanji_color_list(
              jwpqt::qt::OpenMode::kNonInteractive) &&
              *window.current_jwp_document() == before_failed_view &&
              editor->toPlainText() == before_failed_view_text &&
              editor->textCursor().position() ==
                  before_failed_view_cursor.position() &&
              editor->textCursor().anchor() ==
                  before_failed_view_cursor.anchor() &&
              editor->document()->isModified() ==
                  before_failed_view_modified &&
              window.windowTitle() == before_failed_view_title &&
              window.jwp_code_page() == jwpqt::core::kDefaultLegacyCodePage &&
              find_action(window, "undoAction")->isEnabled() ==
                  before_failed_view_undo_enabled &&
              editor->extraSelections().size() ==
                  before_failed_view_selections.size(),
          "Unrenderable kanji list view replaced the current document");
  require_jwp_layout(editor, before_failed_view);

  clear->trigger();
  const std::optional<jwpqt::core::KanjiColorList> cleared =
      jwpqt::qt::read_kanji_color_list_file(list_path);
  require(window.kanji_color_list().empty() && cleared.has_value() &&
              cleared->empty() && !view->isEnabled() && !clear->isEnabled(),
          "Clear did not persist and publish an empty kanji color list");

  const QString blocked_root =
      directory + QStringLiteral("/blocked-kanji-list-commands");
  require(QDir().mkpath(blocked_root),
          "Could not create blocked kanji list command fixture");
  const QString blocked_settings =
      blocked_root + QStringLiteral("/settings.ini");
  const QString blocked_list = blocked_root + QStringLiteral("/colkanji.lst");
  {
    QSettings settings(blocked_settings, QSettings::IniFormat);
    jwpqt::qt::write_kanji_color_policy(settings, policy);
  }
  jwpqt::core::KanjiColorList blocked_initial_list;
  require(blocked_initial_list.add(0x3022),
          "Could not prepare blocked kanji command list");
  jwpqt::qt::write_kanji_color_list_file(blocked_list,
                                         blocked_initial_list);
  PromptingWindow blocked;
  require(blocked.load_kanji_color_configuration(
              blocked_settings, blocked_list,
              jwpqt::qt::OpenMode::kNonInteractive) &&
              blocked.open_jwp_path(source_path),
          "Could not load blocked kanji list command fixture");
  require(QDir(blocked_root).removeRecursively(),
          "Could not remove blocked kanji list directory");
  QFile blocker(blocked_root);
  require(blocker.open(QIODevice::WriteOnly) && blocker.write("x", 1) == 1,
          "Could not block kanji list output directory");
  blocker.close();
  require(!blocked.append_kanji_color_list(
              jwpqt::qt::OpenMode::kNonInteractive) &&
              blocked.kanji_color_list().size() == 1 &&
              blocked.kanji_color_list().contains(0x3022),
          "Failed list persistence published the appended candidate");
  auto* blocked_editor =
      dynamic_cast<jwpqt::qt::JwpEditor*>(blocked.findChild<QTextEdit*>());
  require(blocked_editor != nullptr &&
              blocked_editor->extraSelections().size() == 1 &&
              blocked_editor->extraSelections()[0]
                      .format.foreground()
                      .color() == QColor(4, 5, 6),
          "Failed list persistence changed the prior overlay");
}

void test_jwp_page_layout(const QString& directory) {
  const jwpqt::core::JwpDocument source = sample_jwp_document();
  const QString source_path = directory + QStringLiteral("/page-layout.jwp");
  jwpqt::qt::write_jwp_file(source_path, source);

  PromptingWindow window;
  require(window.open_jwp_path(source_path),
          "Could not open page-layout fixture");
  QAction* action = find_action(window, "pageLayoutAction");
  require(action != nullptr && action->isEnabled(),
          "Page Layout action was not enabled for a JWP document");

  jwpqt::core::JwpDocument changed = source;
  changed.margins = {0.5F, 0.75F, 1.25F, 1.5F};
  changed.landscape = false;
  changed.vertical = true;
  changed.suppress_first_page_headers = true;
  changed.separate_left_right_headers = true;
  changed.headers[1][2] = jwpqt::core::encode_jwp_text(U"Even right");
  changed.summary[3] = jwpqt::core::encode_jwp_text(U"Revision note");
  window.next_page_layout = changed;
  action->trigger();
  require(window.page_layout_prompt_count == 1 &&
              window.offered_page_layout == source &&
              *window.current_jwp_document() == changed &&
              find_action(window, "undoAction")->isEnabled(),
          "Page Layout action did not publish one complete metadata change");

  find_action(window, "undoAction")->trigger();
  require(*window.current_jwp_document() == source,
          "Undo did not restore the original page layout");
  find_action(window, "redoAction")->trigger();
  require(*window.current_jwp_document() == changed,
          "Redo did not restore the edited page layout");

  const QString saved_path = directory + QStringLiteral("/page-layout-saved.jwp");
  require(window.save_path(saved_path) &&
              jwpqt::qt::read_jwp_file(saved_path) == changed,
          "Edited page layout did not survive a JWP save round trip");

  const QString text_path = directory + QStringLiteral("/page-layout.txt");
  jwpqt::qt::write_text_file(
      text_path, {U"plain \U0001f600", jwpqt::core::TextEncoding::kUtf8, false});
  require(window.open_path(text_path, jwpqt::core::TextEncoding::kUtf8) &&
              !action->isEnabled(),
          "Page Layout action stayed enabled for unrestricted Unicode");
}

void test_native_print_commands(const QString& directory) {
  const QString text_path = directory + QStringLiteral("/print.txt");
  jwpqt::qt::write_text_file(
      text_path, {U"Printable \u65e5\u672c", jwpqt::core::TextEncoding::kUtf8,
                  false});
  PromptingWindow window;
  require(window.open_path(text_path, jwpqt::core::TextEncoding::kUtf8),
          "Could not open native-print fixture");
  QAction* print = find_action(window, "printAction");
  QAction* setup = find_action(window, "printerSetupAction");
  require(print != nullptr && print->isEnabled() && setup != nullptr &&
              setup->isEnabled(),
          "Native Print or Printer Setup action was not available");

  window.print_output_path = directory + QStringLiteral("/plain-print.pdf");
  print->trigger();
  QFile plain_output(window.print_output_path);
  require(window.print_prompt_count == 1 && plain_output.open(QIODevice::ReadOnly) &&
              plain_output.read(4) == QByteArray("%PDF", 4),
          "Native Print action did not produce the requested PDF");

  setup->trigger();
  require(window.printer_setup_prompt_count == 1,
          "Printer Setup action did not use its native prompt boundary");

  const QString cancelled_path = directory + QStringLiteral("/cancelled.pdf");
  window.print_output_path = cancelled_path;
  window.accept_print_prompt = false;
  print->trigger();
  require(window.print_prompt_count == 2 && !QFile::exists(cancelled_path),
          "Cancelled Print action created output");

  jwpqt::core::JwpDocument vertical = sample_jwp_document();
  vertical.landscape = false;
  vertical.vertical = true;
  const QString vertical_path = directory + QStringLiteral("/vertical.jwp");
  jwpqt::qt::write_jwp_file(vertical_path, vertical);
  require(window.open_jwp_path(vertical_path),
          "Could not open vertical-print fixture");
  window.printer_setup_landscape = true;
  setup->trigger();
  require(window.printer_setup_prompt_count == 2 &&
              window.current_jwp_document()->landscape &&
              find_action(window, "undoAction")->isEnabled(),
          "Printer Setup did not publish landscape as document metadata");
  find_action(window, "undoAction")->trigger();
  require(!window.current_jwp_document()->landscape,
          "Undo did not restore the prior printer orientation");
  const int prompts_before_vertical = window.print_prompt_count;
  window.accept_print_prompt = true;
  window.print_output_path = directory + QStringLiteral("/vertical-print.pdf");
  print->trigger();
  require(window.print_prompt_count == prompts_before_vertical + 1 && QFile::exists(window.print_output_path),
          "Vertical printing did not reach native output");
}

void test_jis_table_integration(const QString& directory) {
  jwpqt::core::JwpDocument source;
  source.paragraphs = {paragraph(U"\u3042")};
  const QString path = directory + QStringLiteral("/jis-table.jwp");
  jwpqt::qt::write_jwp_file(path, source);

  jwpqt::qt::MainWindow window;
  require(window.open_jwp_path(path), "Could not open JIS table fixture");
  QTextEdit* editor = window.findChild<QTextEdit*>();
  QAction* action = find_action(window, "jisTableAction");
  require(editor != nullptr && action != nullptr && action->isEnabled() &&
              action->shortcut() == QKeySequence(QStringLiteral("Ctrl+T")),
          "Native JIS table action or shortcut is wrong");
  QTextCursor end = editor->textCursor();
  end.movePosition(QTextCursor::End);
  editor->setTextCursor(end);
  action->trigger();
  auto* dialog = dynamic_cast<jwpqt::qt::JisTableDialog*>(
      window.findChild<QDialog*>(QStringLiteral("jisTableDialog")));
  require(dialog != nullptr && dialog->current().has_value() &&
              dialog->current()->jis == 0x2422U,
          "Native JIS table did not seed from the current character");
  require(dialog->set_jis(0x2424U),
          "Could not select JIS table insertion character");
  dialog->findChild<QPushButton*>(QStringLiteral("jisTableInsert"))->click();
  require(window.current_jwp_document()->paragraphs[0].text ==
              jwpqt::core::JwpText{0x2422U, 0x2424U} &&
              find_action(window, "undoAction")->isEnabled(),
          "JIS table insertion did not use portable document history");
  find_action(window, "undoAction")->trigger();
  require(*window.current_jwp_document() == source,
          "Undo did not restore JIS table insertion");
  action->trigger();
  require(window.findChildren<QDialog*>(QStringLiteral("jisTableDialog"))
                  .size() == 1,
          "JIS table action did not reuse its modeless dialog");

  const QString text_path = directory + QStringLiteral("/jis-table.txt");
  jwpqt::qt::write_text_file(
      text_path, {U"plain \U0001f600\u3042", jwpqt::core::TextEncoding::kUtf8, false});
  require(window.open_path(text_path, jwpqt::core::TextEncoding::kUtf8),
          "Could not switch JIS table fixture to plain text");
  require(action->isEnabled(), "Unicode document disabled the JIS table");
  window.active_editor()->moveCursor(QTextCursor::End);
  action->trigger();
  require(dialog->current()->jis == 0x2422U && dialog->set_jis(0x2424U),
          "Unicode character did not seed the JIS table");
  dialog->findChild<QPushButton*>(QStringLiteral("jisTableInsert"))->click();
  require(!window.is_jwp_document() && window.active_editor()->toPlainText() ==
              QString::fromStdU32String(U"plain \U0001f600\u3042\u3044"),
          "JIS table insertion changed the Unicode editing engine or content");
  find_action(window, "undoAction")->trigger();
  require(!window.document_modified(), "Unicode JIS insertion lost its undo baseline");
}

void test_unicode_lookup_insertion(const QString& directory) {
  using namespace jwpqt;
  const QString path = directory + QStringLiteral("/unicode-insertion.txt");
  const std::u32string original = U"\ufeff\U0001f600X\u00a0tail";
  qt::write_text_file(path, {original, core::TextEncoding::kUtf16Be, true});
  qt::MainWindow window;
  require(window.open_path(path, core::TextEncoding::kUtf16Be) && !window.is_jwp_document(),
          "Could not load Unicode insertion fixture");
  auto* editor = window.active_editor();
  require(qt::from_qstring(qt::document_plain_text(*editor->document())) == original,
          "Unicode insertion fixture lost its literal signature");
  QTextCursor selection = editor->textCursor();
  selection.setPosition(1);
  selection.setPosition(4, QTextCursor::KeepAnchor);
  editor->setTextCursor(selection);
  const std::u32string inserted = U"\ufeff\u00a0\U0001f680\n\u65e5";
  for (const char32_t invalid : {char32_t(0xd800), char32_t(0x110000)}) {
    require(!window.insert_edict_text(std::u32string{invalid}) &&
                qt::from_qstring(qt::document_plain_text(*editor->document())) == original &&
                editor->textCursor().position() == 4 && editor->textCursor().anchor() == 1 &&
                !window.document_modified() && !find_action(window, "undoAction")->isEnabled(),
            "Invalid Unicode insertion changed content, selection or history");
  }
  for (const auto range : {std::pair<int, int>{2, 4}, {1, 2}}) {
    QTextCursor split = editor->textCursor();
    split.setPosition(range.first); split.setPosition(range.second, QTextCursor::KeepAnchor);
    editor->setTextCursor(split);
    require(!window.insert_edict_text(inserted) && !window.document_modified() &&
                qt::from_qstring(qt::document_plain_text(*editor->document())) == original,
            "Lookup insertion split an existing Unicode surrogate pair");
  }
  editor->setTextCursor(selection);
  editor->setReadOnly(true);
  require(!window.insert_edict_text(inserted) && !window.document_modified(),
          "Lookup insertion bypassed the read-only target");
  editor->setReadOnly(false);
  const auto expected = std::u32string(U"\ufeff") + inserted + U"\u00a0tail";
  require(window.insert_edict_text(inserted) && !window.is_jwp_document() &&
              !window.uses_jwp_format() && window.current_path() == path &&
              window.text_encoding() == core::TextEncoding::kUtf16Be &&
              qt::from_qstring(qt::document_plain_text(*editor->document())) == expected &&
              editor->textCursor().position() == 7 && !editor->textCursor().hasSelection(),
          "Unicode lookup replacement lost scalars, selection, caret or storage policy");
  find_action(window, "undoAction")->trigger();
  require(qt::from_qstring(qt::document_plain_text(*editor->document())) == original &&
              !window.document_modified() && !find_action(window, "undoAction")->isEnabled(),
          "Unicode lookup replacement was not one undo transaction");
  find_action(window, "redoAction")->trigger();
  require(qt::from_qstring(qt::document_plain_text(*editor->document())) == expected &&
              window.save_path(path) && qt::read_text_file(path, core::TextEncoding::kUtf16Be).text == expected,
          "Unicode lookup redo/save did not preserve content");
  require(!window.insert_edict_text(U"") && !window.document_modified(),
          "Empty lookup insertion changed the saved baseline");
}

void test_document_format_separation(const QString& directory) {
  using jwpqt::core::TextEncoding;
  const QString text_path = directory + QStringLiteral("/native-text.txt");
  const QString destination = directory + QStringLiteral("/transfer-output");
  jwpqt::qt::write_text_file(text_path, {U"\u3042", TextEncoding::kUtf8, false});
  PromptingWindow window;
  require(window.open_path(text_path, TextEncoding::kUtf8) &&
              window.is_jwp_document() && !window.uses_jwp_format() &&
              find_action(window, "kanaInputAction")->isEnabled() &&
              find_action(window, "jisTableAction")->isEnabled(),
          "Representable text did not enable native Japanese editing");
  auto* editor = window.findChild<QTextEdit*>();
  find_encoding_action(window, QStringLiteral("EUC-JP"))->trigger();
  editor->insertPlainText(QStringLiteral("X"));
  find_action(window, "undoAction")->trigger();
  require(editor->toPlainText() == QStringLiteral("\u3042") &&
              window.document_modified() && window.isWindowModified(),
          "Native undo lost an unsaved text-encoding change");
  find_encoding_action(window, QStringLiteral("UTF-8"))->trigger();
  require(!window.document_modified() && !window.isWindowModified(),
          "Restoring the original text encoding retained a false dirty flag");
  const auto fixture = write_wnn_fixture(directory);
  require(window.load_wnn_resources(fixture.index_path, fixture.data_path,
                                    fixture.preferences_path),
          "Could not load native-text conversion resources");
  editor->selectAll();
  require(window.convert_selection() && window.accept_conversion(),
          "Imported text could not use kana-to-kanji conversion");
  const auto converted = editor->toPlainText();
  require(window.save_path(text_path) &&
              jwpqt::qt::read_text_file(text_path, TextEncoding::kUtf8).text ==
                  converted.toStdU32String() && !window.uses_jwp_format(),
          "Japanese editing caused a text file to be saved as a JWP container");
  require(window.save_as_path(destination, std::nullopt) &&
              window.uses_jwp_format() && window.is_jwp_document() &&
              jwpqt::qt::read_jwp_file(destination) == *window.current_jwp_document(),
          "Save As could not turn native text into a JWP file");
  const auto source = sample_jwp_document();
  const QString rich_path = directory + QStringLiteral("/transfer-source.jwp");
  jwpqt::qt::write_jwp_file(rich_path, source);
  require(window.open_jwp_path(rich_path), "Could not open rich transfer fixture");
  editor->insertPlainText(QStringLiteral("Z"));
  const auto before = *window.current_jwp_document();
  const auto text_before = editor->toPlainText();
  write_bytes(destination, QByteArray("keep"));
  require(!window.save_as_path(destination, TextEncoding::kUtf8) &&
              read_bytes(destination) == QByteArray("keep") &&
              window.current_path() == rich_path && window.uses_jwp_format() &&
              window.document_modified() && *window.current_jwp_document() == before,
          "Rejected lossy export damaged the source or destination");
  require(window.save_as_path(destination, TextEncoding::kUtf8, true, true) &&
              jwpqt::qt::read_text_file(destination, TextEncoding::kUtf8).text ==
                  text_before.toStdU32String() && window.current_path() == rich_path &&
              window.uses_jwp_format() && window.document_modified() &&
              *window.current_jwp_document() == before &&
              find_action(window, "undoAction")->isEnabled(),
          "Export Copy changed the source storage, content, baseline or history");
  require(!window.save_as_path(rich_path, TextEncoding::kUtf8, true, true) &&
              jwpqt::qt::read_jwp_file(rich_path) == source,
          "Export Copy overwrote its own source file");
  find_action(window, "undoAction")->trigger();
  require(*window.current_jwp_document() == source && !window.document_modified(),
          "Export Copy disturbed native undo or the saved baseline");
  for (const bool approve : {false, true}) {
    const QString copy_path = directory +
        (approve ? QStringLiteral("/approved-copy.txt") : QStringLiteral("/cancelled-copy.txt"));
    bool offered = false;
    bool warned = false;
    QTimer warning_timer;
    warning_timer.setInterval(1);
    QObject::connect(&warning_timer, &QTimer::timeout, &window, [&] {
      if (auto* warning = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
        warned = warning->text().contains(QStringLiteral("metadata"));
        warning->button(approve ? QMessageBox::Yes : QMessageBox::Cancel)->click();
        warning_timer.stop();
      }
    });
    QTimer::singleShot(0, &window, [&] {
      if (auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget())) {
        const QString filter = QStringLiteral("UTF-16BE text (*.txt *.utf16)");
        offered = dialog->nameFilters().contains(filter);
        dialog->selectNameFilter(filter);
        dialog->selectFile(copy_path);
        warning_timer.start();
        QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
      }
    });
    find_action(window, "exportCopyAction")->trigger();
    require(offered, "Export Copy did not offer the UTF-16BE filter");
    require(warned, "Export Copy did not ask about metadata loss");
    require(QFile::exists(copy_path) == approve,
            "Export Copy did not honor its approval result");
    require(window.current_path() == rich_path && window.uses_jwp_format() &&
                !window.document_modified() && *window.current_jwp_document() == source,
            "Export Copy dialog lost format choice, consent or source state");
    if (approve) {
      const auto copy = jwpqt::qt::read_text_file(copy_path, TextEncoding::kUtf16Be);
      require(copy.has_byte_order_mark && copy.text == editor->toPlainText().toStdU32String(),
              "Approved export copy did not preserve text and UTF-16 BOM");
    }
  }
  const QString unicode = QString::fromStdU32String(U"\u65e5\U0001f600");
  jwpqt::qt::write_text_file(text_path, {unicode.toStdU32String(), TextEncoding::kUtf8, false});
  require(window.open_path(text_path, TextEncoding::kUtf8) &&
              !window.is_jwp_document() && !window.uses_jwp_format() &&
              editor->toPlainText() == unicode,
          "Unrepresentable Unicode did not remain intact and editable");
  const auto previous_bytes = read_bytes(destination);
  require(!window.save_as_path(destination, std::nullopt) &&
              !window.save_as_path(destination, TextEncoding::kShiftJis) &&
              read_bytes(destination) == previous_bytes &&
              editor->toPlainText() == unicode && window.current_path() == text_path &&
              !window.document_modified() && window.text_encoding() == TextEncoding::kUtf8,
          "Unrepresentable Save As mutated live format or existing disk content");
  find_action(window, "newTextDocumentAction")->trigger();
  editor = window.active_editor();
  const std::u32string signature_text = U"\ufeff\U0001f600";
  const QString signature = QString(QChar(0xfeff)) +
                            QString::fromStdU32String(U"\U0001f600");
  editor->insertPlainText(signature);
  require(editor->toPlainText() == signature, "Editor lost the explicit signature character");
  require(window.save_as_path(destination, TextEncoding::kUtf8) &&
              jwpqt::qt::read_text_file(destination, TextEncoding::kUtf8).text == signature_text &&
              jwpqt::qt::read_text_file(destination, TextEncoding::kUtf8).has_byte_order_mark,
          "Leading Unicode signature character disappeared on save/reopen");
  require(window.open_path(destination, TextEncoding::kUtf8) &&
              editor->toPlainText() == signature,
          "Text import lost a leading Unicode signature character");
  for (const auto& text : {std::u32string(U"A\u00a0B"),
                           std::u32string(U"A\u00a0B\U0001f600")}) {
    jwpqt::qt::write_text_file(destination, {text, TextEncoding::kUtf8, false});
    require(window.open_path(destination, TextEncoding::kUtf8),
            "Could not import nonbreaking spaces");
    editor->moveCursor(QTextCursor::End);
    editor->insertPlainText(QStringLiteral("!"));
    require(window.save_as_path(destination, TextEncoding::kUtf8) &&
                jwpqt::qt::read_text_file(destination, TextEncoding::kUtf8).text == text + U"!",
            "Text editing changed a nonbreaking space into a regular space");
  }
  find_encoding_action(window, QStringLiteral("UTF-16LE"))->trigger();
  require(window.document_modified() &&
              window.delete_current_document(jwpqt::qt::OpenMode::kNonInteractive) &&
              window.current_path().isEmpty() && !window.document_modified() &&
              editor->toPlainText().isEmpty() && !QFile::exists(destination),
          "Delete did not clear an unrestricted document with an encoding change");
}

void test_editing_mode_switch(const QString& directory) {
  using jwpqt::core::TextEncoding;
  const QString path = directory + QStringLiteral("/editing-mode.txt");
  jwpqt::qt::write_text_file(path, {U"abc", TextEncoding::kUtf8, false});
  PromptingWindow window;
  require(window.open_path(path, TextEncoding::kUtf8), "Could not open editing-mode fixture");
  auto* editor = window.findChild<QTextEdit*>();
  auto* mode = find_action(window, "japaneseEditingAction");
  QTextCursor cursor(editor->document());
  cursor.setPosition(1);
  cursor.setPosition(2, QTextCursor::KeepAnchor);
  editor->setTextCursor(cursor);
  require(mode->isChecked() && window.set_japanese_editing(false) &&
              !mode->isChecked() && !window.is_jwp_document() &&
              !window.uses_jwp_format() && window.current_path() == path &&
              !window.document_modified() && editor->textCursor().anchor() == 1 &&
              editor->textCursor().position() == 2,
          "Clean Unicode switch changed storage, selection or saved state");
  editor->moveCursor(QTextCursor::End);
  editor->insertPlainText(QString::fromStdU32String(U"\U0001f600"));
  require(!window.set_japanese_editing(true, true) && !mode->isChecked() &&
              editor->toPlainText() == QString::fromStdU32String(U"abc\U0001f600") &&
              window.document_modified() && find_action(window, "undoAction")->isEnabled(),
          "Rejected Japanese editing damaged unrestricted Unicode or undo");
  find_action(window, "undoAction")->trigger();
  require(!window.set_japanese_editing(true) && !mode->isChecked(),
          "Editing switch silently discarded redo history");
  require(window.set_japanese_editing(true, true) && mode->isChecked() &&
              !window.document_modified() && !find_action(window, "redoAction")->isEnabled(),
          "Approved Japanese switch lost the original saved baseline");
  editor->insertPlainText(QStringLiteral("X"));
  find_action(window, "undoAction")->trigger();
  require(!window.document_modified(), "Native undo after switching lost clean state");
  require(window.set_japanese_editing(false, true), "Could not return to Unicode editing");
  editor->insertPlainText(QStringLiteral("Y"));
  require(window.save_as_path(path, TextEncoding::kUtf8) &&
              window.set_japanese_editing(true, true),
          "Could not save and resume Japanese editing");
  editor->insertPlainText(QStringLiteral("Z"));
  find_action(window, "undoAction")->trigger();
  require(!window.document_modified() &&
              editor->toPlainText().toStdU32String() ==
                  jwpqt::qt::read_text_file(path, TextEncoding::kUtf8).text,
          "Saved Unicode baseline was not adopted by Japanese undo");

  const QString rich_path = directory + QStringLiteral("/editing-mode.jwp");
  const auto original = sample_jwp_document();
  jwpqt::qt::write_jwp_file(rich_path, original);
  require(window.open_jwp_path(rich_path) && !window.set_japanese_editing(false) &&
              *window.current_jwp_document() == original && !window.document_modified(),
          "Unapproved mode switch discarded JWP metadata");
  for (const bool approve : {false, true}) {
    bool warned = false;
    QTimer::singleShot(0, &window, [&] {
      if (auto* warning = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
        warned = warning->text().contains(QStringLiteral("Undo/Redo"));
        warning->button(approve ? QMessageBox::Yes : QMessageBox::Cancel)->click();
      }
    });
    mode->trigger();
    require(warned && window.is_jwp_document() != approve &&
                mode->isChecked() != approve && window.document_modified() == approve &&
                window.uses_jwp_format() && window.current_path() == rich_path &&
                jwpqt::qt::read_jwp_file(rich_path) == original,
            "Editing-mode dialog ignored consent or changed the saved file");
  }
  require(window.save_as_path(rich_path, std::nullopt) &&
              window.set_japanese_editing(true, true) && !window.document_modified(),
          "Unicode-to-JWP save did not establish a clean Japanese baseline");
  editor->insertPlainText(QStringLiteral("X"));
  find_action(window, "undoAction")->trigger();
  require(!window.document_modified(), "Unicode-to-JWP baseline retained discarded metadata");
}

}  // namespace

int main(int argc, char* argv[]) {
  QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
  QApplication application(argc, argv);
  try {
    QTemporaryDir directory(QDir::tempPath() +
                            QStringLiteral("/jwpqt-window-test-XXXXXX"));
    require(directory.isValid(), "Could not create temporary test directory");
    test_new_document_workflow(directory.path());
    test_duplicate_open_policy(directory.path());
    test_document_line_width_policy(directory.path());
    test_document_tabs(directory.path());
    test_workspace_kanji_count();
    test_tab_conversion_lifetimes(directory.path());
    test_recent_file_workflow(directory.path());
    test_recent_file_failures(directory.path());
    test_document_path_identity(directory.path());
    test_explicit_open_and_encoding_action(directory.path());
    test_jfc_open_save_and_revert(directory.path());
    test_utf16_workflow(directory.path());
    test_document_format_separation(directory.path());
    test_editing_mode_switch(directory.path());
    test_jfc_file_dialogs(directory.path());
    test_local_file_lifecycle_actions(directory.path());
    test_startup_close_policies(directory.path());
    test_backup_policy(directory.path());
    test_leaving_utf8_drops_bom(directory.path());
    test_detected_open(directory.path());
    test_detected_bom_is_preserved(directory.path());
    test_detection_prompt_and_cancellation(directory.path());
    test_ascii_and_unknown_prompts(directory.path());
    test_plain_text_find_actions(directory.path());
    test_plain_text_find_actions(directory.path(), true);
    test_jwp_find_uses_legacy_comparison(directory.path());
    test_plain_text_replace_actions(directory.path());
    test_plain_text_replace_actions(directory.path(), true);
    test_jwp_replace_preserves_structure(directory.path());
    test_jwp_open_edit_and_save(directory.path());
    test_jwp_clipboard_changes(directory.path());
    test_application_settings_workflow(directory.path());
    test_application_settings_preview(directory.path());
    test_application_settings_exit(directory.path());
    test_jwp_code_page_switch(directory.path());
    test_jwp_code_page_can_be_selected_before_open(directory.path());
    test_zero_paragraph_jwp_save_is_not_normalized(directory.path());
    test_jwp_rejects_lossy_edits(directory.path());
    test_jwp_rejects_non_bmp_edit(directory.path());
    test_jwp_history_actions(directory.path());
    test_jwp_paragraph_formatting(directory.path());
    test_jwp_page_break_insertion(directory.path());
    test_katakana_policy(directory.path());
    test_control_arrow_conversion(directory.path());
    test_undo_depth_policy(directory.path());
    test_conversion_choice_policy(directory.path());
    test_selection_autoscroll_policy(directory.path());
    test_result_list_insertion_policy(directory.path());
    test_selected_romaji(directory.path());
    test_jwp_wnn_conversion(directory.path());
    test_jwp_wnn_conversion_boundaries(directory.path());
    test_jwp_wnn_user_dictionary(directory.path());
    test_jwp_wnn_user_dictionary_dialog(directory.path());
    test_edict_lookup_integration(directory.path());
    test_edict_search_controls(directory.path());
    test_edict_automatic_search(directory.path());
    test_unicode_lookup_insertion(directory.path());
    test_edict_user_dictionary_integration(directory.path());
    test_jwp_wnn_preference_write_failure(directory.path());
    test_jwp_kana_input_mode(directory.path());
    test_input_mode_workflow(directory.path());
    test_overwrite_mode(directory.path());
    test_jwp_automatic_wnn_conversion(directory.path());
    test_forced_wnn_conversion(directory.path());
    test_kanji_color_configuration(directory.path());
    test_kanji_color_options(directory.path());
    test_kanji_color_list_commands(directory.path());
    test_jwp_page_layout(directory.path());
    test_native_print_commands(directory.path());
    test_jis_table_integration(directory.path());
    std::cout << "All main window tests passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
