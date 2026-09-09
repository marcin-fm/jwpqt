// SPDX-License-Identifier: GPL-2.0-or-later
#include "main_window.h"
#include "jwp_editor.h"
#include "file_io.h"
#include "session_io.h"
#include "application_settings_dialog.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QLockFile>
#include <QKeyEvent>
#include <QMessageBox>
#include <QPushButton>
#include <QProcess>
#include <QPointer>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>
#include <stdexcept>

using namespace jwpqt;
using namespace jwpqt::qt;
namespace {
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void put(const QString& path, const QByteArray& bytes) {
  QFile file(path); require(file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size(), "Write fixture failed");
}
QByteArray get(const QString& path) { QFile file(path); require(file.open(QIODevice::ReadOnly), "Read fixture failed"); return file.readAll(); }
template<class F> void rejects(F&& call) {
  bool failed = false; try { call(); } catch (const std::exception&) { failed = true; }
  require(failed, "Invalid/stale session accepted");
}
void answer(QMessageBox::StandardButton button) {
  QTimer::singleShot(0, [button] {
    for (auto* widget : QApplication::topLevelWidgets()) {
      if (auto* box = qobject_cast<QMessageBox*>(widget); box && box->isVisible()) {
        box->button(button)->click(); return;
      }
    }
    throw std::runtime_error("Expected session confirmation");
  });
}
}

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  app.setQuitOnLastWindowClosed(false);
  try {
    QTemporaryDir directory; require(directory.isValid(), "Temporary directory failed");
    const auto session = directory.filePath("last-session.jpr");
    const auto first = directory.filePath(QString::fromUtf8("first-愛.txt"));
    const auto second = directory.filePath("second.txt");
    const auto bad = directory.filePath("invalid.txt");
    write_text_file(first, {U"first", core::TextEncoding::kUtf16Be, false});
    write_text_file(second, {U"second", core::TextEncoding::kUtf8, false});
    put(bad, QByteArray(1, '\xff'));
    auto settings = read_application_settings("reload_files=true\nSaveSettingsOnExit=false\nSave_Histories=false\n");
    require(settings.reload_previous_files && !ApplicationSettings{}.reload_previous_files, "Reload defaults/alias wrong");
    require(read_application_settings(write_application_settings(settings)).reload_previous_files, "Reload serialization lost");
    rejects([] { (void)read_application_settings("ReloadPreviousFiles=bad\nreload_files=true\n"); });
    ApplicationSettingsDialog options(settings);
    options.findChild<QCheckBox*>("settingsReloadFiles")->click(); options.reject();
    require(settings.reload_previous_files, "Cancel mutated settings");
    options.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
    require(!options.settings().reload_previous_files, "Options did not accept the reload control");
    MainWindow project_settings;
    require(project_settings.apply_application_settings(settings) &&
        project_settings.save_project_path(directory.filePath("preferences.jpr"), false), "Project preferences save failed");
    MainWindow project_reload;
    require(project_reload.open_project_path(directory.filePath("preferences.jpr")) &&
        project_reload.application_settings().reload_previous_files && project_reload.current_path().isEmpty(),
        "Project lost reload setting or triggered startup restoration");

    MainWindow source;
    require(source.apply_application_settings(settings) && source.load_previous_session(session), "Missing session failed");
    require(!QFile::exists(session), "Read created session");
    require(source.open_path(first, core::TextEncoding::kUtf16Be, OpenMode::kNonInteractive), "First file failed");
    require(source.open_path(second, core::TextEncoding::kUtf8, OpenMode::kNonInteractive, true), "Second file failed");
    source.activate_document(0);
    source.active_editor()->insertPlainText("UNSAVED");
    const auto dirty = source.active_editor()->toPlainText();
    source.new_document_tab(false); source.active_editor()->insertPlainText("unnamed");
    source.activate_document(0);
    require(source.save_previous_session(), "Explicit snapshot failed");
    require(source.document_modified() && source.active_editor()->toPlainText() == dirty, "Snapshot changed dirty buffer");
    const auto original = read_session_source(session);
    const auto conflict = directory.filePath("conflict.jpr");
    ProjectWorkspace empty;
    empty.detect_formats = false;
    const auto created = write_session_source(conflict, empty, {});
    rejects([&] { write_session_source(conflict, empty, {}); });
    require(get(conflict) == QByteArray::fromStdString(created), "Concurrent creation overwritten");
    require(QFile::remove(conflict), "Delete fixture failed");
    rejects([&] { write_session_source(conflict, empty, created); });
    require(!QFile::exists(conflict), "Concurrent deletion recreated");
    rejects([&] { read_session_source(directory.path()); });
    rejects([&] { read_session_source(QString("invalid") + QChar::Null + "path"); });
    const auto alias = directory.filePath("session-link.jpr");
    require(QFile::link(session, alias), "Session alias fixture failed");
    QLockFile alias_lock(session + ".lock");
    require(alias_lock.tryLock(), "Alias lock fixture failed");
    rejects([&] { write_session_source(alias, empty, original); });
    alias_lock.unlock();
    require(read_session_source(alias) == original, "Alias conflict changed the archive");
    require(!source.save_application_settings(session) && !source.save_project_path(session, false) &&
        read_session_source(session) == original, "Auxiliary save overwrote session");
    auto workspace = decode_project_workspace(core::parse_jwp_project(*original), session, {}, {}, false);
    require(workspace.documents.size() == 2 && workspace.current_document == 0 &&
        workspace.documents[0].encoding == core::TextEncoding::kUtf16Be &&
        original->find("UNSAVED") == std::string::npos && original->find("Show_Toolbar") == std::string::npos,
        "Snapshot stored buffers/settings or lost format/order");

    MainWindow restored;
    settings.show_toolbar = false;
    require(restored.apply_application_settings(settings) && restored.load_previous_session(session), "Restore failed");
    require(restored.document_count() == 2 && restored.current_document_index() == 0 &&
        restored.current_path() == first && restored.text_encoding() == core::TextEncoding::kUtf16Be &&
        restored.active_editor()->toPlainText() == "first" && !restored.document_modified() &&
        !restored.application_settings().show_toolbar && restored.current_project_path().isEmpty(),
        "Restore changed current preferences, data, active document or project");
    for (const auto& entry : restored.recent_documents()) require(!entry.project, "Session leaked into project MRU");
    auto foreign = core::parse_jwp_project(*original);
    foreign.configuration += "\nReloadPreviousFiles=invalid\nUnknown_Preference=opaque\n";
    const auto foreign_path = directory.filePath("foreign-session.jpr");
    put(foreign_path, QByteArray::fromStdString(core::serialize_jwp_project(foreign)));
    MainWindow current_preferences;
    require(current_preferences.apply_application_settings(settings) && current_preferences.load_previous_session(foreign_path) &&
        current_preferences.application_settings().reload_previous_files && !current_preferences.application_settings().show_toolbar,
        "Embedded session settings replaced current preferences");
    MainWindow disabled;
    require(disabled.load_previous_session(session) && disabled.document_count() == 1 && disabled.current_path().isEmpty(),
        "Disabled restore loaded documents");
    restored.active_editor()->insertPlainText("kept");
    require(restored.load_previous_session(session) && restored.active_editor()->toPlainText().contains("kept"),
        "Reload replaced existing dirty buffer");
    restored.findChild<QAction*>("undoAction")->trigger();
    require(restored.active_editor()->toPlainText() == "first", "Restored editor undo lost");

    workspace.documents[1].code_page = core::LegacyCodePage::k1253;
    workspace.documents[1].japanese_editing = false;
    workspace.documents.insert(workspace.documents.begin() + 1,
        {directory.filePath("missing.txt"), core::TextEncoding::kUtf8, core::kDefaultLegacyCodePage, true});
    workspace.documents.push_back({bad, core::TextEncoding::kUtf8, core::kDefaultLegacyCodePage, true});
    workspace.current_document = 3;
    const auto partial = write_session_source(session, workspace, original);
    MainWindow missing;
    require(missing.apply_application_settings(settings) && missing.load_previous_session(session), "Partial restore failed");
    require(missing.document_count() == 2 && missing.current_path() == second && !missing.is_jwp_document() &&
        missing.jwp_code_page() == core::LegacyCodePage::k1253 &&
        missing.session_warning().contains("missing.txt") && missing.session_warning().contains("invalid.txt"),
        "Partial restore lost usable files/current fallback/diagnostics");
    ProjectWorkspace empty_recovery;
    empty_recovery.detect_formats = false;
    empty_recovery.documents.push_back({directory.filePath("gone.txt"), core::TextEncoding::kUtf8,
                                        core::kDefaultLegacyCodePage, true});
    const auto empty_path = directory.filePath("missing-session.jpr");
    write_session_source(empty_path, empty_recovery, {});
    MainWindow pending; pending.apply_application_settings(settings);
    QKeyEvent n(QEvent::KeyPress, Qt::Key_N, Qt::NoModifier, "n");
    QApplication::sendEvent(pending.active_editor(), &n);
    require(pending.load_previous_session(empty_path), "Missing-only archive failed");
    QKeyEvent a(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, "a");
    QApplication::sendEvent(pending.active_editor(), &a);
    require(pending.active_editor()->toPlainText() == QString::fromUtf8("な"), "Missing-only restore flushed pending kana");
    rejects([&] { write_session_source(session, workspace, original); });
    QLockFile lock(session + ".lock"); require(lock.tryLock(), "Lock fixture failed");
    rejects([&] { write_session_source(session, workspace, partial); }); lock.unlock();
    put(session, "corrupt");
    require(!missing.save_previous_session() && get(session) == "corrupt", "Save overwrote corrupt session");
    MainWindow corrupt; require(corrupt.apply_application_settings(settings), "Apply failed");
    require(!corrupt.load_previous_session(session) && !corrupt.save_previous_session() && get(session) == "corrupt",
        "Unknown session source was overwritten");
    corrupt.show(); answer(QMessageBox::Cancel);
    require(!corrupt.close_application() && corrupt.isVisible(), "Failed session save closed workspace");
    answer(QMessageBox::Discard); require(corrupt.close_application() && get(session) == "corrupt", "Discard changed archive");
    auto* doomed = new MainWindow;
    doomed->apply_application_settings(settings); doomed->load_previous_session(session); doomed->show();
    QPointer<MainWindow> weak(doomed);
    QTimer::singleShot(0, [doomed] { delete doomed; });
    doomed->close_application();
    require(!weak, "Session failure prompt retained deleted owner");
    MainWindow changed;
    changed.apply_application_settings(settings); changed.load_previous_session(session); changed.show();
    QTimer::singleShot(0, [&changed] {
      changed.active_editor()->insertPlainText("newer");
      for (auto* widget : QApplication::topLevelWidgets())
        if (auto* box = qobject_cast<QMessageBox*>(widget); box && box->isVisible()) box->button(QMessageBox::Discard)->click();
    });
    require(!changed.close_application() && changed.isVisible() && changed.active_editor()->toPlainText() == "newer",
        "Stale failed-exit consent discarded newer input");
    put(session, QByteArray::fromStdString(partial));
    MainWindow report; require(report.apply_application_settings(settings) && report.load_previous_session(session, false) &&
        report.document_count() == 1 && report.current_path().isEmpty() && get(session) == QByteArray::fromStdString(partial),
        "Read-only configuration restored/wrote files");

    // Real close/discard saves only the on-disk version and preserves unrelated preferences.
    const auto exit_session = directory.filePath("exit.jpr");
    MainWindow closing; require(closing.apply_application_settings(settings) && closing.load_previous_session(exit_session), "Exit setup");
    require(closing.open_path(second, core::TextEncoding::kUtf8, OpenMode::kNonInteractive), "Exit open");
    closing.active_editor()->insertPlainText("discard"); closing.show(); answer(QMessageBox::Discard);
    require(closing.close_application() && QFile::exists(exit_session), "Exit did not save references");
    MainWindow next; require(next.apply_application_settings(settings) && next.load_previous_session(exit_session) &&
        next.active_editor()->toPlainText() == "second", "Discarded changes serialized");

    if (argc > 1) {
      const auto config = directory.filePath("config"); require(QDir().mkpath(config), "Config directory failed");
      write_application_settings_file(config + "/jwpqt.cfg", settings);
      write_session_source(config + "/last-session.jpr", workspace, {});
      QProcess process;
      process.start(QString::fromLocal8Bit(argv[1]), {"--smoke-test", "--config-dir", config,
          "--user-data-dir", directory.filePath("user"), "--encoding", "utf-16be", first});
      require(process.waitForFinished(30000) && process.exitCode() == 0, "CLI/session smoke failed");
      const auto after = read_session_source(config + "/last-session.jpr");
      const auto result = decode_project_workspace(core::parse_jwp_project(*after), config + "/last-session.jpr", {}, {}, false);
      if (result.documents.size() != 2 || result.documents[result.current_document].path != first) {
        std::cerr << process.readAllStandardError().toStdString() << "count=" << result.documents.size()
                  << " active=" << result.current_document << '\n';
        for (const auto& entry : result.documents) std::cerr << entry.path.toStdString() << '\n';
      }
      require(result.documents.size() == 2 && result.documents[result.current_document].path == first,
          "CLI did not follow restoration/activate explicit file");
    }
    std::cout << "Session restoration, storage, lifecycle and CLI tests passed\n";
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
