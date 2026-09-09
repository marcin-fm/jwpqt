// SPDX-License-Identifier: GPL-2.0-or-later

#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPointer>
#include <QProcess>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>
#include <QToolBar>

#include "file_io.h"
#include "jwp_editor.h"
#include "jwpqt/core/jwp_text_codec.h"
#include "main_window.h"
#include "text_bridge.h"

namespace {
using namespace jwpqt;
constexpr auto ni = qt::OpenMode::kNonInteractive;

void require(bool condition, std::string_view message) {
  if (!condition) throw std::runtime_error(std::string(message));
}

void write(const QString& path, const QByteArray& bytes) {
  QFile file(path);
  require(file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size(), "Could not write fixture");
}

void answer(QMessageBox::StandardButton choice) {
  QTimer::singleShot(0, [choice] {
    auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    require(dialog && dialog->button(choice), "Expected a project/save confirmation");
    dialog->button(choice)->click();
  });
}

void mixed_workspace(const QString& directory) {
  const QString native_path = directory + "/native.jwp";
  const QString unicode_path = directory + "/unicode.txt";
  const QString legacy_path = directory + "/ascii.old";
  const QString project_path = directory + "/workspace.jpr";
  core::JwpDocument native;
  native.margins = {1, 1, 1, 1};
  native.summary[0] = {'T'};
  native.paragraphs.push_back({core::encode_jwp_text(U"\u0402\u65e5", core::LegacyCodePage::k1251)});
  qt::write_jwp_file(native_path, native);
  qt::write_text_file(unicode_path, {U"A\ufeff\u00a0\U0001f600", core::TextEncoding::kUtf16Be, false});
  qt::write_text_file(legacy_path, {U"ASCII", core::TextEncoding::kOldJis, false});
  qt::MainWindow original;
  require(original.load_recent_file_configuration(directory + "/project-history.json"), "Could not set project history");
  require(original.open_jwp_path(native_path, core::LegacyCodePage::k1251, ni) &&
          original.open_path(unicode_path, core::TextEncoding::kUtf16Be, ni, true) &&
          original.open_path(legacy_path, core::TextEncoding::kOldJis, ni, true), "Could not open mixed workspace");
  auto settings = original.application_settings();
  settings.fonts[static_cast<int>(qt::JapaneseFontRole::kSystem)].size = 21;
  settings.show_toolbar = false;
  settings.dictionary.monitor_clipboard = true;
  settings.fonts[static_cast<int>(qt::JapaneseFontRole::kBitmap)] = {{}, 28, false};
  settings.omit_clipboard_bitmap = true;
  settings.vertical_clipboard_bitmap = true;
  settings.color_clipboard_bitmap = true;
  settings.ascii_font.family = QStringLiteral("DejaVu Sans Mono");
  require(original.apply_application_settings(settings) && original.activate_document(1), "Could not prepare project settings");
  auto cursor = original.active_editor()->textCursor(); cursor.setPosition(1); cursor.setPosition(3, QTextCursor::KeepAnchor);
  original.active_editor()->setTextCursor(cursor);
  require(original.save_project_path(project_path, false), "Could not save mixed project");
  require(original.recent_documents().front().project &&
          qt::read_recent_documents(directory + "/project-history.json").front().project,
          "Project kind was not persisted in recent files");
  auto invalid_history = original.recent_documents();
  invalid_history.front().encoding = core::TextEncoding::kUtf8;
  const auto history_bytes = qt::read_file_bytes(directory + "/project-history.json");
  bool rejected = false;
  try { qt::write_recent_documents(directory + "/project-history.json", invalid_history); }
  catch (const qt::RecentFilesError&) { rejected = true; }
  require(rejected && qt::read_file_bytes(directory + "/project-history.json") == history_bytes,
          "Invalid project history changed the existing store");
  require(original.current_document_index() == 1 && original.active_editor()->textCursor().selectionStart() == 1 &&
          original.active_editor()->textCursor().selectionEnd() == 3, "Reference save changed active editor/selection");
  require(qt::read_jwp_project_file(project_path).paths.back() == qt::from_qstring(unicode_path), "Active file not saved last");

  qt::MainWindow restored;
  QPointer<qt::JwpEditor> retired = restored.active_editor();
  require(restored.load_recent_file_configuration(directory + "/project-history.json") &&
          restored.open_recent_document(0), restored.project_warning().toStdString());
  require(retired.isNull() && restored.document_count() == 3 && restored.current_document_index() == 1 &&
          restored.current_path() == unicode_path && !restored.is_jwp_document() &&
          restored.text_encoding() == core::TextEncoding::kUtf16Be, "Mixed project selection/Unicode format failed");
  require(restored.active_editor()->font().pixelSize() == 21 && !restored.application_settings().show_toolbar &&
              restored.application_settings().dictionary.monitor_clipboard &&
              restored.application_settings().omit_clipboard_bitmap &&
              restored.application_settings().vertical_clipboard_bitmap &&
              restored.application_settings().color_clipboard_bitmap &&
              restored.application_settings().ascii_font.family == QStringLiteral("DejaVu Sans Mono") &&
              restored.application_settings().fonts[static_cast<int>(qt::JapaneseFontRole::kBitmap)].size == 28,
          "Project did not apply supported settings");
  require(qt::document_plain_text(*restored.active_editor()->document()) == qt::to_qstring(U"A\ufeff\u00a0\U0001f600"),
          "Project changed Unicode content");
  restored.active_editor()->moveCursor(QTextCursor::End);
  restored.active_editor()->insertPlainText("x");
  restored.findChild<QAction*>("undoAction")->trigger();
  require(!restored.document_modified(), "Transferred Unicode editor lost its undo baseline");
  require(restored.activate_document(0) && restored.jwp_code_page() == core::LegacyCodePage::k1251 &&
          *restored.current_jwp_document() == native, "Project lost JWP metadata/code page");
  restored.active_editor()->moveCursor(QTextCursor::End);
  restored.active_editor()->insertPlainText("x");
  restored.findChild<QAction*>("undoAction")->trigger();
  require(!restored.document_modified() && *restored.current_jwp_document() == native,
          "Transferred Japanese editor lost history/model synchronization");
  require(restored.activate_document(2) && restored.text_encoding() == core::TextEncoding::kOldJis,
          "ASCII-only project document was redetected as another encoding");
  require(restored.save_project_path(project_path, true), "Save project with document saves failed");
  require(!qt::read_text_file(unicode_path, core::TextEncoding::kUtf16Be).has_byte_order_mark,
          "Project changed an unmarked UTF-16 file");
  require(!restored.save_project_path(unicode_path, false), "Project overwrote an open document");

  restored.active_editor()->moveCursor(QTextCursor::End); restored.active_editor()->insertPlainText("!");
  QPointer<qt::JwpEditor> held = restored.active_editor();
  require(!restored.open_project_path(project_path) && held == restored.active_editor() && restored.document_modified(),
          "Noninteractive replacement discarded a modified buffer");
  write(directory + "/bad.jpr", "bad");
  require(!restored.open_project_path(directory + "/bad.jpr") && held == restored.active_editor() && restored.document_count() == 3,
          "Malformed project changed workspace");
  answer(QMessageBox::Cancel);
  require(!restored.open_project_path(project_path, {}, qt::OpenMode::kInteractive) &&
          held == restored.active_editor() && restored.document_modified(), "Cancelled replacement discarded a buffer");
  qt::ProjectOpenOptions append; append.append = true;
  require(restored.open_project_path(project_path, append) && restored.document_count() == 3 &&
          restored.active_editor() == held && restored.document_modified(), "Append reloaded an existing modified file");
  restored.findChild<QAction*>("undoAction")->trigger();
  require(!restored.document_modified(), "Append damaged existing history");

  auto broken = qt::read_jwp_project_file(project_path);
  broken.paths[0] = U"missing-file.txt";
  qt::write_jwp_project_file(directory + "/missing.jpr", broken);
  require(!restored.open_project_path(directory + "/missing.jpr") && restored.document_count() == 3 && held,
          "Missing project document discarded open editors");
  restored.new_document_tab(false); restored.active_editor()->insertPlainText("unnamed");
  const auto prior = qt::read_file_bytes(project_path);
  require(!restored.save_project_path(project_path, false) && qt::read_file_bytes(project_path) == prior,
          "Unnamed content was silently omitted or project overwritten on failure");
}

void mapping_and_presave(const QString& directory) {
  const auto text_path = directory + "/mapped.txt";
  qt::write_text_file(text_path, {U"\u65e5\u672c", core::TextEncoding::kUtf8, false});
  const auto legacy_path = directory + "/legacy-project.jpr";
  qt::write_jwp_project_file(legacy_path, {"TranslationCodePage = 1251\n", U"C:\\Old", {U"C:\\Old\\mapped.txt"}});
  qt::MainWindow window;
  const auto before = window.active_editor();
  require(!window.open_project_path(legacy_path) && window.active_editor() == before, "Windows paths were guessed");
  qt::ProjectOpenOptions mapped; mapped.path_mappings = {{"C:/Old", directory}};
  require(window.open_project_path(legacy_path, mapped) && window.current_path() == text_path,
          window.project_warning().toStdString());
  const auto native_project = directory + "/presave.jpr";
  require(window.save_project_path(native_project, false), "Cannot save pre-save fixture");
  for (auto* action : window.findChildren<QAction*>())
    if (action->text() == QStringLiteral("Old JIS")) action->trigger();
  require(window.document_modified() && window.text_encoding() == core::TextEncoding::kOldJis, "Cannot change unsaved codec");
  answer(QMessageBox::Save);
  require(window.open_project_path(native_project, {}, qt::OpenMode::kInteractive), window.project_warning().toStdString());
  require(window.text_encoding() == core::TextEncoding::kOldJis &&
          qt::document_plain_text(*window.active_editor()->document()) == qt::to_qstring(U"\u65e5\u672c") &&
          !window.document_modified(), "Project restored stale bytes/codec from before save consent");

  qt::ProjectWorkspace extra; extra.detect_formats = false;
  extra.settings = qt::read_application_settings("Future_Option = opaque\n");
  extra.documents = {{text_path, core::TextEncoding::kOldJis}};
  qt::write_jwp_project_file(directory + "/future.jpr", qt::encode_project_workspace(extra));
  auto* preserved = window.active_editor();
  require(!window.open_project_path(directory + "/future.jpr") && window.active_editor() == preserved,
          "Unapplied project settings did not require consent");
  answer(QMessageBox::Cancel);
  require(!window.open_project_path(directory + "/future.jpr", {}, qt::OpenMode::kInteractive) &&
          window.active_editor() == preserved, "Cancelled settings consent changed the workspace");
  QTimer::singleShot(0, [] {
    auto* warning = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    require(warning && warning->detailedText().contains("Future_Option"), "Unsupported settings lack bounded dialog details");
    warning->button(QMessageBox::Yes)->click();
  });
  require(window.open_project_path(directory + "/future.jpr", {}, qt::OpenMode::kInteractive), "Settings approval was ignored");
  qt::ProjectOpenOptions consent; consent.allow_unapplied_settings = true;
  require(window.open_project_path(directory + "/future.jpr", consent) &&
          window.resource_report().contains("Future_Option"), "Unapplied project settings were not disclosed/preserved");
  qt::MainWindow empty;
  require(empty.save_project_path(directory + "/empty.jpr", false) && empty.open_project_path(directory + "/empty.jpr") &&
          empty.document_count() == 1 && empty.current_path().isEmpty() && !empty.document_modified(), "Empty project failed");
  require(empty.new_document_tab() == 1 && empty.close_document(0, ni), "Restored workspace lifecycle is broken");
}

void menu_workflow(const QString& directory) {
  qt::MainWindow window;
  require(window.open_path(directory + "/ascii.old", core::TextEncoding::kOldJis, ni), "Cannot open menu fixture");
  const auto project_path = directory + "/menu-workspace.jpr";
  QTimer::singleShot(0, [&] {
    auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
    require(dialog, "Expected Save Project file chooser");
    dialog->selectFile(project_path);
    answer(QMessageBox::No);
    QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
  });
  window.findChild<QAction*>("saveProjectAction")->trigger();
  require(window.current_project_path() == project_path && QFile::exists(project_path), "Save Project command failed");
  const auto bytes = qt::read_file_bytes(project_path);
  QTimer::singleShot(0, [] {
    auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
    require(dialog, "Expected cancelled project file chooser"); dialog->reject();
  });
  window.findChild<QAction*>("openProjectAction")->trigger();
  require(window.document_count() == 1 && qt::read_file_bytes(project_path) == bytes, "Cancelled project chooser changed data");
  QPointer<qt::JwpEditor> old = window.active_editor();
  QTimer::singleShot(0, [&] {
    auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
    require(dialog, "Expected Open Project file chooser");
    dialog->selectFile(project_path);
    answer(QMessageBox::Yes);
    QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
  });
  window.findChild<QAction*>("openProjectAction")->trigger();
  require(old.isNull() && window.document_count() == 1 && window.text_encoding() == core::TextEncoding::kOldJis,
          "Open Project command did not replace with the saved format");
  QPointer<QAction> recent = window.findChild<QAction*>("recentFile1Action");
  old = window.active_editor();
  answer(QMessageBox::No);
  recent->trigger();
  require(recent && old == window.active_editor() && window.document_count() == 1,
          "Recent Project command lost its action or reloaded an appended buffer");
  qt::MainWindow detected;
  require(detected.open_path(directory + "/unicode.txt", core::TextEncoding::kUtf16Be, ni), "Cannot open detection fixture");
  old = detected.active_editor();
  require(detected.open_path_detected(project_path, ni, true) && detected.document_count() == 2 && old,
          "Detected project ignored the new-tab preservation contract");
  const auto magic_path = directory + "/workspace.binary";
  write(magic_path, QByteArray(bytes.data(), static_cast<qsizetype>(bytes.size())));
  require(detected.activate_document(0), "Cannot activate preserved detection buffer");
  old->moveCursor(QTextCursor::End); old->insertPlainText("dirty");
  require(detected.open_path_detected(magic_path, ni, true) && detected.document_count() == 2 && old && old->document()->isModified(),
          "Magic-based project opening replaced a dirty workspace");
  const auto explicit_text = directory + "/text-with-project-suffix.jpr";
  write(explicit_text, "text");
  require(detected.open_path(explicit_text, core::TextEncoding::kUtf8, ni, true), "Cannot open explicit-format fixture");
  old = detected.active_editor(); old->moveCursor(QTextCursor::End); old->insertPlainText("!");
  require(detected.open_path_detected(explicit_text, ni, true) && old == detected.active_editor() &&
          detected.document_modified() && detected.text_encoding() == core::TextEncoding::kUtf8,
          "Project suffix overrode an already-open explicit text format");

  qt::MainWindow mapping;
  QTimer::singleShot(0, [&] {
    auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
    require(dialog, "Expected Windows project directory chooser");
    dialog->selectFile(directory);
    QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
  });
  require(mapping.open_project_path(directory + "/legacy-project.jpr", {}, qt::OpenMode::kInteractive),
          "Project directory chooser did not supply the mapping");

  const auto ambiguous = directory + "/ambiguous-project.jpr";
  qt::write_jwp_project_file(ambiguous, {"", qt::from_qstring(directory), {U"ascii.old"}});
  qt::MainWindow encoding;
  old = encoding.active_editor();
  QTimer::singleShot(0, [] {
    auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
    require(dialog, "Expected project encoding chooser"); dialog->reject();
  });
  require(!encoding.open_project_path(ambiguous, {}, qt::OpenMode::kInteractive) && old == encoding.active_editor(),
          "Cancelled project encoding discarded an editor");
  QTimer::singleShot(0, [] {
    auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
    require(dialog, "Expected project encoding chooser"); dialog->setTextValue("Old JIS"); dialog->accept();
  });
  require(encoding.open_project_path(ambiguous, {}, qt::OpenMode::kInteractive) &&
          encoding.text_encoding() == core::TextEncoding::kOldJis, "Project encoding choice was ignored");
}

void conversion_and_transfer(const QString& directory) {
  write(directory + "/wnn.dix", QByteArray::fromHex("a280807700000000"));
  write(directory + "/wnn.dat", QByteArray::fromHex("a22ab0a12fb0a20a"));
  qt::MainWindow window;
  require(window.load_wnn_resources(directory + "/wnn.dix", directory + "/wnn.dat", directory + "/user.sel", ni),
          "Cannot load project conversion fixture");
  window.active_editor()->insertPlainText(qt::to_qstring(U"\u3042"));
  const auto origin_path = directory + "/conversion-origin.jwp";
  require(window.save_as_path(origin_path, std::nullopt), "Cannot save conversion origin");
  const auto incoming_path = directory + "/incoming.txt";
  qt::write_text_file(incoming_path, {U"X\U0001f600", core::TextEncoding::kUtf8, false});
  qt::ProjectWorkspace workspace;
  workspace.detect_formats = false;
  workspace.documents = {{origin_path, {}}, {incoming_path, core::TextEncoding::kUtf8, core::LegacyCodePage::k1252, false}};
  const auto path = directory + "/conversion-project.jpr";
  qt::write_jwp_project_file(path, qt::encode_project_workspace(workspace));
  window.active_editor()->selectAll();
  require(window.convert_selection(), "Cannot start project preview");
  QPointer<qt::JwpEditor> origin = window.active_editor();
  const auto glyph = origin->toPlainText();
  auto* overwrite = window.findChild<QAction*>("overwriteModeAction");
  overwrite->trigger();
  require(window.conversion_active() && origin->toPlainText() == glyph && origin->overwriteMode(),
          "Overwrite toggle accepted or changed a conversion preview");
  auto invalid = workspace;
  invalid.documents[1].encoding = core::TextEncoding::kUtf16Le;
  write(directory + "/invalid-project-doc.txt", QByteArray::fromHex("fffe00d8"));
  invalid.documents[1].path = directory + "/invalid-project-doc.txt";
  qt::write_jwp_project_file(directory + "/invalid-reference.jpr", qt::encode_project_workspace(invalid));
  require(!window.open_project_path(directory + "/invalid-reference.jpr") && window.conversion_active() &&
          window.active_editor() == origin && origin->toPlainText() == glyph, "Invalid reference disturbed a preview");
  require(window.save_project_path(directory + "/preview-references.jpr", false) && window.conversion_active(),
          "Reference-only project saving accepted a preview");
  window.findChild<QAction*>("kanjiInfoAction")->trigger();
  QPointer<QDialog> information = window.findChild<QDialog*>("kanjiInfoDialog");
  require(information, "Cannot inspect project preview");
  qt::ProjectOpenOptions append; append.append = true;
  require(window.open_project_path(path, append) && window.document_count() == 2 &&
          window.active_editor() == origin && window.conversion_active() && origin->toPlainText() == glyph,
          "Appending a new editor displaced the existing conversion");
  require(window.activate_document(1) && !window.conversion_active(), "Cannot activate the transferred editor");
  require(window.active_editor()->overwriteMode(), "Appended editor did not inherit overwrite mode");
  window.active_editor()->moveCursor(QTextCursor::End); window.active_editor()->insertPlainText("!");
  window.findChild<QAction*>("undoAction")->trigger();
  require(!window.document_modified(), "Appended Unicode editor lost its undo baseline");
  require(window.activate_document(0), "Cannot return to the existing editor");
  window.findChild<QAction*>("undoAction")->trigger();
  require(!window.document_modified() && origin->toPlainText() == qt::to_qstring(U"\u3042"), "Existing conversion history was lost");
  require(window.open_project_path(path) && origin.isNull() && information && window.document_count() == 2,
          "Workspace replacement lost an independent character viewer");
  auto* character = information->findChild<QLabel*>("kanjiInfoCharacter");
  require(character, "Information viewer lost its glyph");
  const auto center = character->rect().center();
  for (const int index : {0, 1}) {
    require(window.activate_document(index), "Cannot select the information insertion target");
    auto* target = window.active_editor(); target->moveCursor(QTextCursor::End);
    require(target->overwriteMode(), "Restored editor lost the window's overwrite mode");
    const auto before = target->toPlainText();
    QMouseEvent insert(QEvent::MouseButtonDblClick, QPointF(center), QPointF(character->mapToGlobal(center)),
                       Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(character, &insert);
    require(target->toPlainText() == before + glyph && window.is_jwp_document() == (index == 0),
            "Information insertion used a retired workspace or changed the editing engine");
    window.findChild<QAction*>("undoAction")->trigger();
    require(target->toPlainText() == before && !window.document_modified(), "Information insertion lost transferred undo");
  }
  require(window.activate_document(0), "Cannot return to restored Japanese editor");
  window.active_editor()->selectAll();
  require(window.convert_selection(), "Restored Japanese editor lost access to shared WNN resources");
}

void bounded_io_and_cli(const QString& directory, const QString& executable) {
  const auto bounded = directory + "/bounded.bin";
  write(bounded, "1234");
  require(qt::read_file_bytes(bounded, 4) == "1234", "Exact bounded input failed");
  bool rejected = false;
  try { qt::read_file_bytes(bounded, 3); } catch (const std::runtime_error&) { rejected = true; }
  require(rejected, "Oversized input was not rejected");
  rejected = false;
  try { qt::read_file_bytes(directory, 4); } catch (const std::runtime_error&) { rejected = true; }
  require(rejected, "Directory input was not rejected");
  rejected = false;
  try { qt::read_file_bytes(bounded, std::numeric_limits<std::size_t>::max()); }
  catch (const std::runtime_error&) { rejected = true; }
  require(rejected, "Overflowing read budget was not rejected");
  write(bounded, {});
  require(qt::read_file_bytes(bounded, 0).empty(), "Empty file exceeded a zero remaining budget");
  for (const bool legacy : {false, true}) {
    QProcess process;
    QStringList arguments{"--resource-report", "--config-dir", directory + "/cli-config", "--user-data-dir", directory + "/cli-user"};
    if (legacy) arguments << "--project" << "--encoding" << "old-jis";
    const auto project = directory + (legacy ? "/ambiguous-project.jpr" : "/menu-workspace.jpr");
    arguments << project;
    process.start(executable, arguments);
    require(process.waitForFinished(10000) && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0,
            "Command-line project opening failed");
    require(process.readAllStandardOutput().contains(project.toUtf8()), "CLI did not report the loaded project");
  }
}
}  // namespace

int main(int argc, char** argv) {
  QApplication application(argc, argv);
  application.setQuitOnLastWindowClosed(false);
  try {
    QTemporaryDir directory;
    require(directory.isValid(), "Cannot create project-window fixtures");
    mixed_workspace(directory.path()); mapping_and_presave(directory.path());
    menu_workflow(directory.path()); conversion_and_transfer(directory.path());
    require(argc == 2, "Expected the native application path");
    bounded_io_and_cli(directory.path(), QString::fromLocal8Bit(argv[1]));
    return 0;
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
