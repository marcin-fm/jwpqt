// SPDX-License-Identifier: GPL-2.0-or-later
#include "edict_registry_dialog.h"
#include "edict_lookup_dialog.h"
#include "edict_resources.h"
#include "file_io.h"
#include "main_window.h"
#include "jwp_editor.h"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QLabel>
#include <QMimeData>
#include <QLineEdit>
#include <QListWidget>
#include <QLockFile>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <iostream>
#include <algorithm>
#include <stdexcept>

using namespace jwpqt;
using Mode = qt::OpenMode;
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
template<class T> T* child(QObject& parent, const char* name) {
  auto* value = parent.findChild<T*>(QString::fromLatin1(name)); require(value, name); return value;
}
template<class F> void rejects(F operation) {
  bool failed = false; try { operation(); } catch (const std::exception&) { failed = true; }
  require(failed, "Invalid registry operation succeeded");
}
void write(const QString& path, const QByteArray& bytes) {
  QFile file(path); require(file.open(QIODevice::WriteOnly), "Open fixture");
  require(file.write(bytes) == bytes.size(), "Write fixture");
}
core::EdictRegistry fixture() {
  core::EdictRegistry value;
  core::EdictRegistryEntry entry;
  entry.label = u"First"; entry.path = u"first"; entry.encoding = core::EdictRegistryEncoding::kUtf8;
  entry.searched = entry.keep = true;
  value.entries.push_back(entry);
  entry.label = u"Second"; entry.path = u"second"; value.entries.push_back(entry);
  entry.label = u"User"; entry.path = u"user.dct";
  entry.special = core::EdictRegistrySpecial::kUser; entry.encoding = core::EdictRegistryEncoding::kMixed;
  entry.quiet = true; value.entries.push_back(entry);
  return value;
}
void storage(const QString& root) {
  const auto path = root + "/dict.cfg";
  auto value = fixture();
  const auto missing = qt::read_edict_registry_snapshot(path);
  require(!missing.source, "Missing registry was not optional");
  qt::write_edict_registry_checked(path, value, missing.source);
  auto saved = qt::read_edict_registry_snapshot(path);
  require(saved.registry == value, "Registry round trip");
  rejects([&] { qt::write_edict_registry_checked(path, value, missing.source); });
  auto invalid = value; invalid.entries[0].label = {char16_t(0xd800)};
  rejects([&] { qt::write_edict_registry_checked(path, invalid, saved.source); });
  require(qt::read_edict_registry_snapshot(path).source == saved.source, "Invalid registry changed file");
  QLockFile lock(path + ".lock"); require(lock.tryLock(), "Lock fixture");
  rejects([&] { qt::write_edict_registry_checked(path, value, saved.source); });
  lock.unlock();
  const auto alias = root + "/alias.cfg"; require(QFile::link(path, alias), "Symlink fixture");
  require(lock.tryLock(), "Relock fixture");
  rejects([&] { qt::write_edict_registry_checked(alias, value, saved.source); });
  lock.unlock();
  require(QFile::remove(path), "Remove fixture");
  rejects([&] { qt::write_edict_registry_checked(path, value, saved.source); });
  rejects([&] { qt::read_edict_registry_snapshot(alias); });
  write(path, "corrupt");
  rejects([&] { qt::write_edict_registry_checked(path, value, saved.source); });
  require(qt::read_file_bytes(path) == "corrupt", "Corrupt registry overwritten");
  rejects([&] { qt::read_edict_registry_snapshot(root); });
  rejects([&] { qt::read_edict_registry_snapshot(QString()); });
  rejects([&] { qt::read_edict_registry_snapshot(path + QChar::Null + "suffix"); });
  qt::write_edict_registry_file(path, value);
}
void sample_import(const QString& root) {
  const auto utf8 = root + "/dropped.utf";
  const auto mixed = root + "/mixed.dic";
  write(utf8, QByteArray::fromHex("e78cab202fe8be9ee69bb82f0a"));
  write(root + "/dropped.jdx", "index");
  write(mixed, QByteArray::fromHex("c7ad202f80206e616d652f0a"));

  auto value = fixture();
  qt::EdictRegistryDialog detected(value, root, core::LegacyCodePage::k1251);
  auto* list = child<QListWidget>(detected, "registryEntries");
  child<QLineEdit>(detected, "registryPath")->setText(utf8);
  QTimer::singleShot(0, &detected, [&] {
    auto* prompt = child<QMessageBox>(detected, "replaceDetectedDictionaryNamePrompt");
    require(prompt->defaultButton() == prompt->button(QMessageBox::No),
            "Detect name replacement did not use the safe default");
    prompt->button(QMessageBox::No)->click();
  });
  child<QPushButton>(detected, "registryDetect")->click();
  require(detected.registry().entries[0].path == utf8.toStdU16String() &&
              detected.registry().entries[0].encoding == core::EdictRegistryEncoding::kUtf8 &&
              detected.registry().entries[0].indexed &&
              detected.registry().entries[0].label == u"First",
          "Declining Detect name replacement lost inferred fields or the staged name");
  QTimer::singleShot(0, &detected, [&] {
    child<QMessageBox>(detected, "replaceDetectedDictionaryNamePrompt")
        ->button(QMessageBox::Yes)->click();
  });
  child<QPushButton>(detected, "registryDetect")->click();
  require(detected.registry().entries[0].path == utf8.toStdU16String() &&
              detected.registry().entries[0].encoding == core::EdictRegistryEncoding::kUtf8 &&
              detected.registry().entries[0].indexed &&
              detected.registry().entries[0].label == u"\u8f9e\u66f8",
          "Detect did not apply source sample, companion index, path, and description");
  const auto after_detect = detected.registry();
  child<QLineEdit>(detected, "registryPath")->setText(root + "/missing");
  child<QPushButton>(detected, "registryDetect")->click();
  require(detected.registry().entries[0].encoding == after_detect.entries[0].encoding &&
              detected.registry().entries[0].label == after_detect.entries[0].label &&
              child<QLabel>(detected, "registryStatus")->text().contains("regular file"),
          "Failed Detect changed inferred fields or concealed its error");
  QTimer::singleShot(0, &detected, [&] {
    auto* chooser = detected.findChild<QFileDialog*>();
    require(chooser, "Dictionary Browse dialog");
    chooser->selectFile(mixed);
    static_cast<QDialog*>(chooser)->accept();
  });
  child<QPushButton>(detected, "registryBrowse")->click();
  require(detected.registry().entries[0].path == mixed.toStdU16String() &&
              detected.registry().entries[0].encoding == core::EdictRegistryEncoding::kMixed &&
              detected.registry().entries[0].label == u"\u0402 name" &&
              !detected.registry().entries[0].indexed,
          "Browse did not run the bounded dictionary inference workflow");

  list->setCurrentRow(2);
  child<QLineEdit>(detected, "registryPath")->setText(mixed);
  QTimer::singleShot(0, &detected, [&] {
    child<QMessageBox>(detected, "replaceDetectedDictionaryNamePrompt")
        ->button(QMessageBox::Yes)->click();
  });
  child<QPushButton>(detected, "registryDetect")->click();
  require(detected.registry().entries[2].path == mixed.toStdU16String() &&
              detected.registry().entries[2].encoding == core::EdictRegistryEncoding::kMixed &&
              detected.registry().entries[2].special == core::EdictRegistrySpecial::kUser &&
              !detected.registry().entries[2].indexed &&
              detected.registry().entries[2].label == u"\u0402 name",
          "Detect changed the protected user dictionary contract");

  list->setCurrentRow(0);
  QMimeData mime;
  mime.setUrls({QUrl::fromLocalFile(mixed), QUrl::fromLocalFile(utf8)});
  QDragEnterEvent enter(QPoint(2, 2), Qt::CopyAction, &mime,
                        Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(&detected, &enter);
  require(enter.isAccepted(), "Local dictionary drag was not accepted");
  QDropEvent drop(QPointF(2, 2), Qt::CopyAction, &mime,
                  Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(&detected, &drop);
  require(drop.isAccepted() && detected.registry().entries.size() == value.entries.size() + 2 &&
              detected.registry().entries[1].path == mixed.toStdU16String() &&
              detected.registry().entries[1].encoding == core::EdictRegistryEncoding::kMixed &&
              detected.registry().entries[1].label == u"\u0402 name" &&
              detected.registry().entries[2].path == utf8.toStdU16String(),
          "Multi-file drop did not retain source order or inferred fields");
  require(child<QLabel>(detected, "registryStatus")->text().contains("2 dictionary"),
          "Multi-file drop did not disclose staged additions");

  auto* deleted = new qt::EdictRegistryDialog(
      fixture(), root, core::LegacyCodePage::k1251);
  QPointer<qt::EdictRegistryDialog> deleted_guard(deleted);
  child<QLineEdit>(*deleted, "registryPath")->setText(utf8);
  QTimer::singleShot(0, deleted, [deleted_guard] {
    require(deleted_guard, "Detect dialog disappeared before its confirmation");
    auto* prompt = child<QMessageBox>(
        *deleted_guard, "replaceDetectedDictionaryNamePrompt");
    require(prompt->defaultButton() == prompt->button(QMessageBox::No),
            "Owner-deletion Detect prompt lost its safe default");
    delete deleted_guard.data();
  });
  child<QPushButton>(*deleted, "registryDetect")->click();
  require(!deleted_guard,
          "Deleting the registry during Detect used the stale staged owner");

  const auto count = detected.registry().entries.size();
  QMimeData remote;
  remote.setUrls({QUrl(QStringLiteral("https://example.invalid/dict"))});
  QDragEnterEvent remote_enter(QPoint(2, 2), Qt::CopyAction, &remote,
                               Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(&detected, &remote_enter);
  require(!remote_enter.isAccepted() && detected.registry().entries.size() == count,
          "Nonlocal dictionary drag was accepted or changed the registry");

  auto full = fixture();
  full.entries.resize(core::EdictRegistryLimits{}.entries, full.entries.front());
  full.entries.back() = fixture().entries.back();
  qt::EdictRegistryDialog bounded(full, root, core::kDefaultLegacyCodePage);
  QMimeData one;
  one.setUrls({QUrl::fromLocalFile(utf8)});
  QDragEnterEvent full_enter(QPoint(2, 2), Qt::CopyAction, &one,
                             Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(&bounded, &full_enter);
  QDropEvent full_drop(QPointF(2, 2), Qt::CopyAction, &one,
                       Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(&bounded, &full_drop);
  require(bounded.registry().entries.size() == core::EdictRegistryLimits{}.entries &&
              child<QLabel>(bounded, "registryStatus")->text().contains("entry limit"),
          "Drop exceeded or concealed the registry entry bound");
}
void controls(const QString& root) {
  auto value = fixture();
  for (const char16_t signature : {u'\ufeff', u'\ufffe'}) {
    auto literal = value;
    literal.entries.erase(literal.entries.begin() + 1);
    literal.entries[0].label = std::u16string(1, signature) + u"Literal";
    literal.entries[0].path = std::u16string(1, signature) + u"first";
    literal.entries[1].path = std::u16string(1, signature) + u"user.dct";
    const QString prefix{QChar(signature)};
    write(root + "/" + prefix + "first", "cat /literal/\n");
    write(root + "/" + prefix + "user.dct", "\xa4\xa2 /personal/\n");
    require(qt::read_edict_user_dictionary_file(root + "/" + prefix + "user.dct", core::kDefaultLegacyCodePage)
                .value().entries().size() == 1, "Literal user file fixture");
    qt::EdictRegistryDialog exact(literal, root, core::kDefaultLegacyCodePage);
    require(child<QLineEdit>(exact, "registryName")->text() == prefix + "Literal" &&
                child<QLineEdit>(exact, "registryPath")->text() == prefix + "first",
            "UTF-16 signature in registry text was interpreted as a BOM");
    child<QCheckBox>(exact, "registryQuiet")->setChecked(true);
    require(exact.registry().entries[0].path == literal.entries[0].path &&
                exact.registry().entries[0].label == literal.entries[0].label,
            "Editing a flag changed literal registry text");
    child<QPushButton>(exact, "registryInspect")->click();
    require(child<QLabel>(exact, "registryStatus")->text().contains("1 records"), "Literal path inspection failed");
    const auto configuration = root + "/literal.cfg";
    qt::write_edict_registry_file(configuration, literal);
    qt::MainWindow owner;
    require(owner.load_edict_configuration(configuration, Mode::kNonInteractive) && owner.edict_user_dictionary() &&
                owner.edict_user_dictionary()->entries().size() == 1 && owner.edict_resources()->failures.empty(),
            "Literal dictionary/user path was changed on loading");
  }
  qt::MainWindow font_owner;
  auto settings = font_owner.application_settings();
  settings.fonts[static_cast<std::size_t>(qt::JapaneseFontRole::kList)] = {{}, 20, false};
  settings.fonts[static_cast<std::size_t>(qt::JapaneseFontRole::kEdit)] = {{}, 22, false};
  require(font_owner.apply_application_settings(settings), "Font fixture");
  qt::EdictRegistryDialog dialog(value, root, core::kDefaultLegacyCodePage, &font_owner);
  dialog.show(); QApplication::processEvents();
  auto* list = child<QListWidget>(dialog, "registryEntries");
  require(list->font().pixelSize() == 20 && child<QLineEdit>(dialog, "registryPath")->font().pixelSize() == 22,
          "Registry fields ignored Japanese font roles");
  require(font_owner.apply_application_settings(qt::ApplicationSettings{}) && list->font().pixelSize() == 16 &&
              child<QLineEdit>(dialog, "registryName")->font().pixelSize() == 16 && dialog.registry() == value,
          "Live font reset did not preserve the staged registry");
  list->item(0)->setCheckState(Qt::Unchecked);
  require(!dialog.registry().entries[0].searched && !child<QCheckBox>(dialog, "registrySearched")->isChecked(),
          "List search toggle did not update entry");
  child<QCheckBox>(dialog, "registrySearched")->setChecked(true);
  require(list->item(0)->checkState() == Qt::Checked, "Property search toggle did not update list");
  child<QPushButton>(dialog, "registryDown")->click();
  require(dialog.registry().entries[1].label == u"First", "Move down");
  child<QPushButton>(dialog, "registryUp")->click();
  child<QLineEdit>(dialog, "registryName")->setText(QString::fromUtf8("\xe6\x97\xa5\xe6\x9c\xac"));
  child<QComboBox>(dialog, "registryEncoding")->setCurrentIndex(2);
  child<QComboBox>(dialog, "registryNames")->setCurrentIndex(2);
  child<QComboBox>(dialog, "registryRole")->setCurrentIndex(1);
  for (const char* name : {"registryIndexed", "registryBuffered", "registryQuiet"})
    child<QCheckBox>(dialog, name)->setChecked(true);
  child<QCheckBox>(dialog, "registryKeep")->setChecked(false);
  const auto& changed = dialog.registry().entries[0];
  require(changed.label == u"\u65e5\u672c" && changed.encoding == core::EdictRegistryEncoding::kMixed &&
      changed.names == core::EdictRegistryNames::kNamesOnly && changed.special == core::EdictRegistrySpecial::kClassical &&
      changed.indexed && changed.buffered && changed.quiet && !changed.keep, "Property editing lost flags");
  list->setCurrentRow(2);
  require(!child<QPushButton>(dialog, "registryRemove")->isEnabled() &&
      !child<QComboBox>(dialog, "registryEncoding")->isEnabled(), "User protection missing");
  child<QPushButton>(dialog, "registryAdd")->click();
  dialog.accept(); require(dialog.result() != QDialog::Accepted, "Empty added path accepted");
  child<QLineEdit>(dialog, "registryPath")->setText("second");
  child<QComboBox>(dialog, "registryEncoding")->setCurrentIndex(1);
  child<QPushButton>(dialog, "registryInspect")->click();
  require(child<QLabel>(dialog, "registryStatus")->text().contains("1 records"), "Resource inspection failed");
  child<QPushButton>(dialog, "registryRemove")->click();
  auto* box = dialog.findChild<QDialogButtonBox*>(); require(box, "Dialog buttons");
  box->button(QDialogButtonBox::RestoreDefaults)->click();
  require(dialog.registry().entries.size() == 4 && dialog.registry().entries[1].path == u"edict" &&
      dialog.registry().entries.back() == value.entries.back(), "Defaults lost user settings");
  dialog.reject(); require(value == fixture(), "Cancel modified input");
  write(root + "/partial", "cat /valid/\ninvalid\n");
  auto partial = value;
  partial.entries[0].path = u"partial";
  partial.entries[0].encoding = core::EdictRegistryEncoding::kEucJp;
  partial.entries[0].special = core::EdictRegistrySpecial::kClassical;
  qt::EdictRegistryDialog inspect_partial(partial, root, core::kDefaultLegacyCodePage);
  child<QPushButton>(inspect_partial, "registryInspect")->click();
  require(child<QLabel>(inspect_partial, "registryStatus")->text().contains("1 invalid records skipped"),
          "Inspection concealed bounded classical record recovery");
  auto dark = dialog.palette();
  dark.setColor(QPalette::Window, QColor("#202325")); dark.setColor(QPalette::Base, QColor("#17191b"));
  dark.setColor(QPalette::Text, Qt::white); dark.setColor(QPalette::WindowText, Qt::white);
  dark.setColor(QPalette::Button, QColor("#303438")); dark.setColor(QPalette::ButtonText, Qt::white);
  dialog.setPalette(dark); dialog.show(); QApplication::processEvents();
  require(dialog.grab().save("dictionary-manager-dark.png"), "Manager screenshot");
}
void integration(const QString& root) {
  const auto path = root + "/dict.cfg";
  auto initial = qt::read_edict_registry_snapshot(path);
  qt::MainWindow window;
  require(window.load_edict_configuration(path, Mode::kNonInteractive), "Load registry");
  window.active_editor()->insertPlainText("safe");
  const auto document = core::encode_jwp_document(*window.current_jwp_document());
  child<QAction>(window, "edictLookupAction")->trigger();
  auto* lookup = dynamic_cast<qt::EdictLookupDialog*>(window.findChild<QDialog*>("edictLookupDialog"));
  require(lookup, "Lookup dialog");
  lookup->set_query(U"cat"); require(lookup->search() && lookup->report().results.size() == 2, "Initial search");
  require(lookup->report().results.front().label == "First", "Initial order");
  const auto history = window.query_histories().dictionary.entries();
  lookup->set_query(U"draft");
  auto next = initial.registry; std::swap(next.entries[0], next.entries[1]);
  require(window.save_edict_configuration(path, next, initial.source), "Save registry");
  require(lookup->query() == U"draft" && lookup->report().results.front().label == "First" &&
      window.query_histories().dictionary.entries() == history, "Management changed query/results/history");
  lookup->set_query(U"cat"); require(lookup->search() && lookup->report().results.front().label == "Second", "Saved order not used");
  auto saved = qt::read_edict_registry_snapshot(path);
  require(!window.save_edict_configuration(path, initial.registry, initial.source), "Stale save accepted");
  auto bad = next; bad.entries[0].path = u"missing";
  require(!window.save_edict_configuration(path, bad, saved.source), "Missing resource accepted silently");
  require(qt::read_edict_registry_snapshot(path).source == saved.source &&
      window.edict_resources()->registry == next, "Failed save changed live or disk state");
  require(window.save_edict_configuration(path, bad, saved.source, true), "Explicit unavailable save rejected");
  require(!window.edict_resources()->failures.empty(), "Unavailable diagnostics lost");
  saved = qt::read_edict_registry_snapshot(path);
  require(window.save_edict_configuration(path, next, saved.source), "Restore resources");
  child<QAction>(window, "edictUserDictionaryAction")->trigger();
  auto* user = window.findChild<QDialog*>("edictUserDictionaryDialog"); require(user, "User editor");
  saved = qt::read_edict_registry_snapshot(path);
  require(!window.save_edict_configuration(path, next, saved.source), "Open user working copy discarded");
  user->close(); QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  QTimer::singleShot(0, &window, [&] {
    auto* manager = dynamic_cast<qt::EdictRegistryDialog*>(window.findChild<QDialog*>("edictRegistryDialog"));
    require(manager, "Manager action");
    child<QLineEdit>(*manager, "registryName")->setText("Cancelled"); manager->reject();
  });
  child<QToolButton>(*lookup, "edictRegistry")->click();
  require(qt::read_edict_registry_snapshot(path).source == saved.source, "Cancel wrote registry");
  QTimer::singleShot(0, &window, [&] {
    auto* manager = dynamic_cast<qt::EdictRegistryDialog*>(window.findChild<QDialog*>("edictRegistryDialog"));
    require(manager, "Manager save action");
    child<QLineEdit>(*manager, "registryName")->setText("Renamed"); manager->accept();
  });
  child<QAction>(window, "edictRegistryAction")->trigger();
  require(qt::read_edict_registry_snapshot(path).registry.entries[0].label == u"Renamed", "Actual Save command failed");
  require(core::encode_jwp_document(*window.current_jwp_document()) == document, "Registry modified document");
  child<QAction>(window, "undoAction")->trigger();
  require(window.active_editor()->toPlainText().isEmpty(), "Registry destroyed native undo");
  qt::MainWindow restarted;
  require(restarted.load_edict_configuration(path, Mode::kNonInteractive) &&
      restarted.edict_resources()->registry.entries[0].label == u"Renamed", "Restart lost configuration");
  require(window.new_document_tab(false) >= 0, "Could not create unrestricted Unicode registry fixture");
  const auto unicode = QStringLiteral("Unicode \U0001f642 text");
  window.active_editor()->insertPlainText(unicode);
  const auto undo_steps = window.active_editor()->document()->availableUndoSteps();
  saved = qt::read_edict_registry_snapshot(path);
  require(window.save_edict_configuration(path, saved.registry, saved.source), "Save in Unicode document");
  require(window.active_editor()->toPlainText() == unicode &&
      window.active_editor()->document()->availableUndoSteps() == undo_steps, "Registry changed Unicode history");
  child<QAction>(window, "undoAction")->trigger();
  require(window.active_editor()->toPlainText().isEmpty(), "Unicode undo lost");
  QPointer<qt::MainWindow> owner = new qt::MainWindow;
  require(owner->load_edict_configuration(path, Mode::kNonInteractive), "Owner fixture");
  QTimer::singleShot(0, [&] { delete owner; });
  child<QAction>(*owner, "edictRegistryAction")->trigger();
  require(!owner, "Modal owner retained");
}
void migration_and_paths(const QString& root) {
  auto registry = fixture();
  registry.wire_encoding = core::EdictRegistryWireEncoding::kAnsiBytes;
  registry.entries[0].label = {char16_t(0x80)};
  const auto path = root + "/ansi.cfg";
  qt::write_edict_registry_file(path, registry);
  const auto original = qt::read_file_bytes(path);
  qt::MainWindow window;
  require(window.load_edict_configuration(path, Mode::kNonInteractive) &&
      std::any_of(window.edict_resources()->failures.begin(), window.edict_resources()->failures.end(),
          [](const auto& failure) { return failure.message.contains("undefined"); }),
      "Undefined CP1252 label was not diagnosed");
  QTimer::singleShot(0, &window, [&] {
    auto* prompt = window.findChild<QInputDialog*>(); require(prompt, "ANSI page prompt");
    prompt->setTextValue("1251");
    QTimer::singleShot(0, &window, [&] {
      auto* manager = dynamic_cast<qt::EdictRegistryDialog*>(window.findChild<QDialog*>("edictRegistryDialog"));
      require(manager && manager->registry().entries[0].label == u"\u0402", "ANSI migration guessed or lost bytes");
      require(qt::read_file_bytes(path) == original, "Migration wrote before Save");
      manager->accept();
    });
    prompt->accept();
  });
  child<QAction>(window, "edictRegistryAction")->trigger();
  const auto converted = qt::read_edict_registry_snapshot(path);
  require(converted.registry.wire_encoding == core::EdictRegistryWireEncoding::kUtf16Le &&
      converted.registry.entries[0].label == u"\u0402", "Unicode registry not persisted");
  const auto before = converted.source;
  auto duplicate_user = converted.registry; duplicate_user.entries.push_back(duplicate_user.entries.back());
  require(!window.save_edict_configuration(path, duplicate_user, before), "Duplicate editable users accepted");
  auto self_reference = converted.registry; self_reference.entries[0].path = u"ansi.cfg";
  require(!window.save_edict_configuration(path, self_reference, before, true), "Registry overwrote a dictionary source");
  require(qt::read_edict_registry_snapshot(path).source == before, "Rejected registry changed bytes");
  require(QDir().mkpath(root + "/physical/child"), "Physical fixture directory");
  require(QFile::link(root + "/physical/child", root + "/jump"), "Directory symlink");
  write(root + "/physical/first", "\xe7\x8c\xab /physical cat/\n");
  auto physical = fixture(); physical.entries.erase(physical.entries.begin() + 1);
  const auto indirect = root + "/jump/../physical.cfg";
  qt::write_edict_registry_file(indirect, physical);
  require(window.load_edict_configuration(indirect, Mode::kNonInteractive) &&
      window.edict_resources()->resources.front().dictionary.records().front().definitions.front() == U"physical cat",
      "Registry directory symlink parent was lexically cleaned");
  require(qt::edict_index_path(root + "/jump/../first") == root + "/jump/../first.jdx",
      "Index path changed symlink semantics");
  auto settings = window.application_settings(); settings.translation_code_page = 1251;
  require(window.apply_application_settings(settings, Mode::kNonInteractive), "Mixed code page settings");
  physical.entries[0].encoding = core::EdictRegistryEncoding::kMixed;
  physical.entries[0].keep = false;
  write(root + "/physical/first", "\xc7\xad /\x80 cat/\n");
  qt::write_edict_registry_file(indirect, physical);
  require(window.load_edict_configuration(indirect, Mode::kNonInteractive), "Load CP1251 mixed dictionary");
  settings.translation_code_page = 1252;
  require(window.apply_application_settings(settings, Mode::kNonInteractive), "Change next-load default");
  child<QAction>(window, "edictLookupAction")->trigger();
  auto* lookup = dynamic_cast<qt::EdictLookupDialog*>(window.findChild<QDialog*>("edictLookupDialog"));
  require(lookup, "Mixed lookup");
  lookup->set_query(U"cat");
  require(lookup->search() && lookup->report().results.size() == 1 &&
      lookup->report().results.front().result.record.definitions.front() == U"\u0402 cat",
      "Non-keep reload lost the captured mixed code page");
}
int main(int argc, char** argv) {
  QApplication app(argc, argv); QApplication::setStyle("Fusion");
  try {
    QTemporaryDir directory; require(directory.isValid(), "Temporary directory");
    const auto root = directory.path();
    write(root + "/first", "\xe7\x8c\xab /cat first/\n");
    write(root + "/second", "\xe7\x8a\xac /cat second/\n");
    storage(root); sample_import(root); controls(root); integration(root); migration_and_paths(root);
    std::cout << "All dictionary registry manager tests passed\n";
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
