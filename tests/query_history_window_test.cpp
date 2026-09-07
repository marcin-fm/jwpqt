// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimer>

#include "edict_lookup_dialog.h"
#include "file_io.h"
#include "jwp_editor.h"
#include "main_window.h"
#include "query_history_io.h"
#include "jwpqt/core/byte_io.h"
#include "text_bridge.h"

namespace {
using namespace jwpqt;
constexpr auto ni = qt::OpenMode::kNonInteractive;
constexpr auto interactive = qt::OpenMode::kInteractive;

void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

void put(const QString& path, std::string_view bytes) {
  QFile file(path);
  require(file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
          file.write(bytes.data(), static_cast<qint64>(bytes.size())) == static_cast<qint64>(bytes.size()),
          "Could not write history fixture");
}

std::string bytes_at(const QString& path) {
  QFile file(path);
  require(file.open(QIODevice::ReadOnly), "Could not read history fixture");
  return file.readAll().toStdString();
}

QAction* action(qt::MainWindow& window, const char* name) {
  auto* result = window.findChild<QAction*>(QString::fromLatin1(name));
  require(result != nullptr, "History command is unavailable");
  return result;
}

core::QueryHistories sample() {
  core::QueryHistories result;
  result.dictionary.remember(U"older");
  result.dictionary.remember(U"cat");
  result.search.remember(U"\ufeff\u00a0\U0001f600\t");
  result.replace.remember(U"replacement");
  return result;
}

void key(QLineEdit* edit, int code, const QString& text = {}) {
  QKeyEvent event(QEvent::KeyPress, code, Qt::NoModifier, text);
  QApplication::sendEvent(edit, &event);
}

void answer(QMessageBox::StandardButton button, bool& seen) {
  QTimer::singleShot(0, [&seen, button] {
    auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    seen = box && box->button(button);
    if (seen) box->button(button)->click();
  });
}

void storage(const QString& root) {
  const QString path = root + "/history.bin";
  const auto initial = sample();
  const auto original = core::encode_query_history_file(initial);
  put(path, original);
  qt::MainWindow window;
  require(window.query_histories().dictionary.entries().empty() &&
          window.resource_report().contains("Query history: memory only"), "Constructor performed history I/O");
  require(window.load_query_history(path) &&
          core::encode_query_history_file(window.query_histories()) == original, "Not all history kinds loaded");
  const auto* owner = &window.query_histories();
  auto imported = initial;
  imported.dictionary.remember(U"dog");
  const QString source = root + "/import.bin";
  put(source, core::encode_query_history_file(imported));
  require(window.import_query_history(source) && &window.query_histories() == owner && bytes_at(path) == original,
          "Import changed history ownership, its destination or the file prematurely");
  action(window, "saveQueryHistoryAction")->trigger();
  require(qt::read_query_histories(path).histories.dictionary.entries() == imported.dictionary.entries() &&
          qt::read_query_histories(path).histories.search.entries() == initial.search.entries(),
          "Native Save lost imported dictionary or retained search history");
  const auto saved = bytes_at(path);
  put(path, original);
  require(!window.save_query_history() && bytes_at(path) == original &&
          !window.clear_query_history() && &window.query_histories() == owner &&
          window.query_histories().dictionary.entries() == imported.dictionary.entries(),
          "A stale save/clear overwrote an external archive or current histories");
  require(window.load_query_history(path), "Could not reload changed history");
  put(path, "corrupt");
  require(!window.load_query_history(path) && !window.save_query_history() && !window.clear_query_history() &&
          core::encode_query_history_file(window.query_histories()) == original && bytes_at(path) == "corrupt",
          "Corrupt history was replaced or discarded live entries");
  const QString recovery = root + "/recovery.bin";
  require(window.save_query_history(recovery) && bytes_at(recovery) == original && bytes_at(path) == "corrupt",
          "Save As could not recover without rewriting corrupt input");
  require(QFile::remove(recovery) && !window.save_query_history() && !QFile::exists(recovery),
          "Saving silently recreated an externally deleted archive");
  require(window.load_query_history(recovery), "Missing archive was not optional");
  put(recovery, saved);
  require(!window.save_query_history() && bytes_at(recovery) == saved,
          "An external creation was overwritten from a missing snapshot");
  require(window.load_query_history(recovery), "Could not load newly created archive");
  bool prompt = false;
  answer(QMessageBox::Cancel, prompt);
  action(window, "clearQueryHistoryAction")->trigger();
  require(prompt && bytes_at(recovery) == saved && !window.query_histories().dictionary.entries().empty(),
          "Cancelling Clear changed memory or the archive");
  auto settings = window.application_settings();
  settings.save_histories = false;
  require(window.apply_application_settings(settings), "Could not disable automatic history saving");
  prompt = false;
  answer(QMessageBox::Yes, prompt);
  action(window, "clearQueryHistoryAction")->trigger();
  require(prompt && window.query_histories().dictionary.entries().empty() &&
          qt::read_query_histories(recovery).histories.search.entries().empty() && bytes_at(source) == saved,
          "Explicit Clear did not clear all kinds while preserving import source");
  require(!window.import_query_history(root + "/absent.bin"), "Missing native import erased history silently");
  qt::MainWindow memory;
  require(memory.import_query_history(source) && memory.clear_query_history() &&
          memory.query_histories().dictionary.entries().empty() && memory.query_histories().search.entries().empty() &&
          memory.query_histories().replace.entries().empty() && bytes_at(source) == saved,
          "Memory-only Clear changed its import source or retained a history kind");
}

void pruning_and_legacy(const QString& root) {
  core::QueryHistories large(600);
  large.dictionary.remember(std::u32string(400, U'a'));
  large.search.remember(U"find");
  const QString path = root + "/large.bin";
  const auto original = core::encode_query_history_file(large);
  put(path, original);
  qt::MainWindow window;
  require(window.load_query_history(path) && window.query_history_warning().contains("paused") &&
          window.query_histories().dictionary.entries().empty() && !window.save_query_history() && bytes_at(path) == original,
          "Load-time pruning silently replaced the fuller archive");
  bool consent = false;
  answer(QMessageBox::Cancel, consent);
  require(!window.save_query_history({}, interactive) && consent && bytes_at(path) == original,
          "Cancelled save of pruned history replaced original entries");
  consent = false;
  answer(QMessageBox::Yes, consent);
  require(window.save_query_history({}, interactive) && consent &&
          qt::read_query_histories(path).histories.dictionary.entries().empty(),
          "Explicit consent could not save the smaller histories");
  put(path, original);
  require(window.load_query_history(path), "Could not restore pruning fixture");
  auto settings = window.application_settings();
  settings.history_size = 600;
  require(window.apply_application_settings(settings) && !window.save_query_history() && bytes_at(path) == original,
          "Increasing capacity incorrectly recovered omitted entries or unpaused saving");
  require(window.load_query_history(path) && window.query_histories().dictionary.entries() == large.dictionary.entries() &&
          window.save_query_history() && bytes_at(path) == original, "Larger reload did not recover original history");
  const QString smaller = root + "/smaller.cfg";
  put(smaller, "HistoryBuffers_NumChars=300\n");
  require(window.import_application_settings(smaller) && !window.save_query_history() && bytes_at(path) == original &&
          window.query_history_warning().contains("paused"),
          "Imported settings allowed an automatic save to erase the fuller history archive");
  const auto before = core::encode_query_history_file(window.query_histories());
  require(!window.import_query_history(path) && core::encode_query_history_file(window.query_histories()) == before,
          "Noninteractive import silently truncated histories");
  bool confirmed = false;
  answer(QMessageBox::Yes, confirmed);
  require(window.import_query_history(path, {}, interactive) && confirmed && bytes_at(path) == original,
          "Confirmed import loss changed its source file");

  core::ByteWriter legacy;
  legacy.write_u32_le(core::kLegacyQueryHistoryMagic);
  for (const auto& text : {core::JwpText{0x80}, core::JwpText{'a', 'b', 'c'}, core::JwpText{'x', 'y', 'z'}}) {
    legacy.write_u32_le(1);
    std::vector<std::uint16_t> cells(300, 0xffff);
    cells[0] = 0; cells[1] = static_cast<std::uint16_t>(text.size());
    std::copy(text.begin(), text.end(), cells.begin() + 32);
    for (auto cell : cells) legacy.write_u16_le(cell);
  }
  legacy.write_bytes("opaque path tail");
  const QString old = root + "/JWPxp.his";
  const auto legacy_bytes = legacy.take_bytes();
  put(old, legacy_bytes);
  require(!window.import_query_history(old) &&
          !window.import_query_history(old, qt::LegacyHistoryOptions{300, core::LegacyCodePage::k1252}),
          "Legacy input or its code page was guessed");
  require(window.import_query_history(old, qt::LegacyHistoryOptions{300, core::LegacyCodePage::k1251}) &&
          window.query_histories().dictionary.entries() == std::vector<std::u32string>{U"\u0402"} &&
          window.query_histories().search.entries() == std::vector<std::u32string>{U"abc"} &&
          window.query_histories().replace.entries() == std::vector<std::u32string>{U"xyz"} &&
          bytes_at(old) == legacy_bytes, "Explicit legacy import lost kinds/codepage or rewrote its source");
}

void collisions_and_exit(const QString& root) {
  const QString path = root + "/exit.bin";
  put(path, core::encode_query_history_file(sample()));
  qt::MainWindow window;
  require(window.load_query_history(path), "Could not prepare exit history");
  const QString document = root + "/document.txt";
  put(document, "unchanged");
  require(window.open_path(document, core::TextEncoding::kUtf8, ni), "Could not prepare protected document");
  require(!window.save_query_history(document) && window.query_history_warning().contains("open document") &&
          bytes_at(document) == "unchanged", "History Save overwrote an open document");
  const QString alias = root + "/alias";
  require(QFile::link(root, alias) && QFile::remove(document), "Could not prepare missing path alias");
  require(!window.save_query_history(alias + "/document.txt") &&
          window.query_history_warning().contains("open document") && !QFile::exists(document),
          "Prospective directory alias bypassed open-document protection");
  require(QFile::remove(alias), "Could not remove fixture link");
  const QString settings_path = root + "/preferences.cfg";
  require(window.load_application_settings(settings_path) && !window.save_query_history(settings_path) &&
          !QFile::exists(settings_path), "History Save used the settings destination");
  auto settings = window.application_settings();
  settings.save_settings_on_exit = false;
  require(window.apply_application_settings(settings), "Could not isolate exit settings");
  const auto memory = core::encode_query_history_file(window.query_histories());
  auto changed = sample(); changed.dictionary.remember(U"changed");
  const auto external = core::encode_query_history_file(changed);
  put(path, external);
  window.show();
  bool prompt = false;
  answer(QMessageBox::Cancel, prompt);
  require(!window.close() && prompt && window.isVisible() && window.document_count() == 1 &&
          core::encode_query_history_file(window.query_histories()) == memory && bytes_at(path) == external,
          "Cancelling failed exit save discarded documents, histories or external data");
  prompt = false;
  answer(QMessageBox::Discard, prompt);
  require(window.close() && prompt && bytes_at(path) == external, "Exit Discard overwrote external history");
  require(window.load_query_history(path), "Could not recover exit archive");
  const QString incoming = root + "/exit-import.bin";
  put(incoming, memory);
  require(window.import_query_history(incoming), "Could not stage exit import");
  settings.save_histories = false;
  require(window.apply_application_settings(settings), "Could not disable exit persistence");
  window.show();
  require(window.close() && bytes_at(path) == external, "Disabling history saving erased or modified the previous file");
  settings.save_histories = true;
  require(window.apply_application_settings(settings), "Could not enable exit persistence");
  window.show();
  require(window.close() && bytes_at(path) == memory, "Successful exit did not persist all history kinds");
}

void dialogs(const QString& root) {
  qt::MainWindow window;
  const QString path = root + "/dialogs.bin";
  const auto original = core::encode_query_history_file(sample());
  put(path, original);
  require(window.load_query_history(path), "Could not prepare history command dialogs");
  bool cancelled = false;
  QTimer::singleShot(0, [&] {
    auto* file = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
    cancelled = file != nullptr;
    if (file) file->reject();
  });
  action(window, "importQueryHistoryAction")->trigger();
  require(cancelled && core::encode_query_history_file(window.query_histories()) == original,
          "Cancelled Import file picker changed histories");

  QTimer timer;
  bool selected = false, parameters = false, confirmed = false;
  QObject::connect(&timer, &QTimer::timeout, [&] {
    auto* modal = QApplication::activeModalWidget();
    if (auto* file = qobject_cast<QFileDialog*>(modal); file && !selected) {
      selected = true;
      file->selectNameFilter("JWPxp history (*.his)");
      file->selectFile(root + "/JWPxp.his");
      QMetaObject::invokeMethod(file, "accept", Qt::DirectConnection);
    } else if (modal && modal->objectName() == "legacyHistoryImportDialog" && !parameters) {
      auto* size = modal->findChild<QSpinBox*>("legacyHistorySize");
      auto* page = modal->findChild<QComboBox*>("legacyHistoryCodePage");
      parameters = size && page;
      if (parameters) {
        size->setValue(300); page->setCurrentIndex(page->findData(1251));
        modal->grab().save("legacy-history-import.png");
        modal->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
      }
    } else if (auto* question = qobject_cast<QMessageBox*>(modal); question && !confirmed) {
      confirmed = question->button(QMessageBox::Yes) != nullptr;
      if (confirmed) question->button(QMessageBox::Yes)->click();
      else question->accept();
    }
  });
  timer.start(1);
  action(window, "importQueryHistoryAction")->trigger();
  timer.stop();
  require(selected && parameters && confirmed && window.query_histories().dictionary.entries().front() == U"\u0402" &&
          bytes_at(path) == original, "Legacy Import did not use confirmed source parameters or preserve its target");
  const auto imported = core::encode_query_history_file(window.query_histories());
  const QString exported = root + "/dialogs-export.bin";
  bool saved_as = false;
  QTimer::singleShot(0, [&] {
    auto* file = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
    saved_as = file != nullptr;
    if (file) {
      file->selectFile(exported);
      QMetaObject::invokeMethod(file, "accept", Qt::DirectConnection);
    }
  });
  action(window, "saveQueryHistoryAsAction")->trigger();
  require(saved_as && bytes_at(exported) == imported && bytes_at(path) == original,
          "Save As command did not preserve the previous archive");
  put(exported, original);
  cancelled = false;
  answer(QMessageBox::Cancel, cancelled);
  action(window, "reloadQueryHistoryAction")->trigger();
  require(cancelled && core::encode_query_history_file(window.query_histories()) == imported,
          "Cancelled Reload changed live histories");
  confirmed = false;
  answer(QMessageBox::Yes, confirmed);
  action(window, "reloadQueryHistoryAction")->trigger();
  require(confirmed && core::encode_query_history_file(window.query_histories()) == original,
          "Reload command did not reload the selected save target");

  const QString existing = root + "/confirmation-target.bin";
  put(existing, original);
  auto changed = sample(); changed.dictionary.remember(U"external");
  const auto external = core::encode_query_history_file(changed);
  bool replaced = false, failure = false;
  QTimer conflict;
  QObject::connect(&conflict, &QTimer::timeout, [&] {
    auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    if (!box) return;
    if (!replaced && box->button(QMessageBox::Yes)) {
      replaced = true;
      put(existing, external);
      box->button(QMessageBox::Yes)->click();
    } else if (box->button(QMessageBox::Ok)) {
      failure = true;
      box->button(QMessageBox::Ok)->click();
    }
  });
  conflict.start(1);
  require(!window.save_query_history(existing, interactive), "Confirmed Save As ignored an external change");
  conflict.stop();
  require(replaced && failure && bytes_at(existing) == external && window.save_query_history() &&
          bytes_at(exported) == original, "Failed Save As changed its configured target or external bytes");
}

class FontObserver : public QObject {
 public:
  std::function<void()> callback;
  bool eventFilter(QObject*, QEvent* event) override {
    if (event->type() == QEvent::FontChange && callback) {
      auto invoke = std::move(callback);
      invoke();
    }
    return false;
  }
};

void lookup_and_settings(const QString& root) {
  put(root + "/edict", "\xe3\x81\x82 /cat/\n\xe3\x81\x84 /dog/\n");
  core::EdictRegistry registry;
  core::EdictRegistryEntry resource;
  resource.label = u"History"; resource.path = u"edict";
  resource.encoding = core::EdictRegistryEncoding::kUtf8;
  resource.searched = true; resource.keep = true;
  registry.entries.push_back(resource);
  qt::write_edict_registry_file(root + "/dict.cfg", registry);
  const QString path = root + "/lookup.bin";
  const auto initial = sample();
  put(path, core::encode_query_history_file(initial));
  qt::MainWindow window;
  require(window.load_query_history(path) && window.load_edict_configuration(root + "/dict.cfg", ni),
          "Could not load native history/dictionary fixture");
  window.active_editor()->insertPlainText("body");
  const auto document = qt::document_plain_text(*window.active_editor()->document());
  action(window, "edictLookupAction")->trigger();
  auto* dialog = dynamic_cast<qt::EdictLookupDialog*>(window.findChild<QDialog*>("edictLookupDialog"));
  require(dialog != nullptr, "Dictionary lookup is missing");
  auto* query = dialog->findChild<QLineEdit*>("edictQuery");
  auto* results = dialog->findChild<QTextEdit*>("edictResults");
  dialog->set_query(U"cat");
  require(dialog->search(), "Could not search with restored history");
  const QPointer<QTextDocument> displayed = results->document();
  const int position = results->textCursor().position(), anchor = results->textCursor().anchor();
  query->setText("cat"); query->setCursorPosition(3);
  key(query, Qt::Key_K, "k");
  auto settings = window.application_settings();
  settings.history_size = 12;
  require(window.apply_application_settings(settings) && window.query_histories().dictionary.storage_cells() == 12 &&
          results->document() == displayed && results->textCursor().position() == position &&
          results->textCursor().anchor() == anchor && query->text() == "cat" &&
          qt::document_plain_text(*window.active_editor()->document()) == document && window.document_modified(),
          "History resize altered query, results, selection or document history");
  key(query, Qt::Key_A, "a");
  require(query->text() == QStringLiteral("cat\u304b"), "History resize flushed pending kana");
  require(window.query_histories().replace.entries().empty(), "All history kinds were not resized");
  auto smaller = sample();
  smaller.replace.remove(0);
  smaller.dictionary.remove(1);
  const QString source = root + "/small.bin";
  put(source, core::encode_query_history_file(smaller));
  require(window.import_query_history(source), "Could not replace an existing dictionary alias");
  key(query, Qt::Key_Up);
  require(query->text() == "cat" && results->document() == displayed,
          "Existing lookup did not see replacement history or submitted a recall");

  FontObserver observer;
  bool searched = false;
  observer.callback = [&] { dialog->set_query(U"dog"); searched = dialog->search(); };
  window.active_editor()->installEventFilter(&observer);
  settings.history_size = 32;
  auto& font = settings.fonts[static_cast<std::size_t>(qt::JapaneseFontRole::kFile)];
  font.automatic = false; font.size = 21;
  require(window.apply_application_settings(settings) && searched &&
          window.query_histories().dictionary.entries().front() == U"dog" &&
          window.query_histories().dictionary.storage_cells() == 32,
          "Settings publication overwrote a reentrant history addition");
  window.active_editor()->removeEventFilter(&observer);
  const auto before = core::encode_query_history_file(window.query_histories());
  bool shown = false;
  QTimer::singleShot(0, [&] {
    auto* options = QApplication::activeModalWidget();
    auto* size = options ? options->findChild<QSpinBox*>("settingsHistorySize") : nullptr;
    auto* save = options ? options->findChild<QCheckBox*>("settingsSaveHistories") : nullptr;
    auto* buttons = options ? options->findChild<QDialogButtonBox*>() : nullptr;
    shown = size && save && buttons;
    if (shown) {
      size->setValue(0); save->setChecked(false);
      options->findChild<QTabWidget*>()->setCurrentIndex(3);
      options->grab().save("application-options-history.png");
      buttons->button(QDialogButtonBox::Cancel)->click();
    }
  });
  action(window, "applicationOptionsAction")->trigger();
  require(shown && core::encode_query_history_file(window.query_histories()) == before &&
          window.application_settings().save_histories, "Cancelled History options changed persistence or entries");
  bool accepted = false;
  QTimer::singleShot(0, [&] {
    auto* options = QApplication::activeModalWidget();
    auto* size = options ? options->findChild<QSpinBox*>("settingsHistorySize") : nullptr;
    auto* save = options ? options->findChild<QCheckBox*>("settingsSaveHistories") : nullptr;
    accepted = size && save;
    if (accepted) {
      size->setValue(64); save->setChecked(false);
      options->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
    }
  });
  action(window, "applicationOptionsAction")->trigger();
  require(accepted && window.query_histories().dictionary.storage_cells() == 64 &&
           window.query_histories().search.storage_cells() == 64 && !window.application_settings().save_histories,
           "Accepted History options did not apply to the live three-history owner");
  require(!window.save_query_history() && bytes_at(path) == core::encode_query_history_file(initial),
          "Resizing or increasing capacity lost protection of the original archive");
  bool saved_smaller = false;
  answer(QMessageBox::Yes, saved_smaller);
  require(window.save_query_history({}, interactive) && saved_smaller,
          "Explicit consent could not save resized history with automatic saving disabled");

  bool changed = false, nested_rejected = false, error_seen = false;
  QTimer reentrant;
  QObject::connect(&reentrant, &QTimer::timeout, [&] {
    auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    if (!box) return;
    if (!changed && box->button(QMessageBox::Yes)) {
      auto resize = window.application_settings(); resize.history_size = 65;
      nested_rejected = !window.clear_query_history() && !window.apply_application_settings(resize);
      dialog->set_query(U"bird"); changed = dialog->search();
      box->button(QMessageBox::Yes)->click();
    } else if (box->button(QMessageBox::Ok)) {
      error_seen = true;
      box->button(QMessageBox::Ok)->click();
    }
  });
  reentrant.start(1);
  require(!window.load_query_history(path, interactive), "Reload overwrote a reentrant query addition");
  reentrant.stop();
  require(changed && nested_rejected && error_seen && window.query_histories().dictionary.entries().front() == U"bird" &&
          window.application_settings().history_size == 64 && window.save_query_history(),
          "Rejected reentrant Reload changed capacity, entries or the known save snapshot");
  settings.history_size = 0;
  require(window.apply_application_settings(settings), "Could not disable history retention");
  dialog->set_query(U"cat");
  require(dialog->search() && window.query_histories().dictionary.entries().empty(),
          "Disabled shared history continued retaining queries");
  action(window, "undoAction")->trigger();
  require(!window.document_modified() && window.active_editor()->toPlainText().isEmpty(),
          "History operations damaged native document undo");
}

}  // namespace

int main(int argc, char** argv) {
  QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
  QApplication application(argc, argv);
  QApplication::setQuitOnLastWindowClosed(false);
  try {
    QTemporaryDir directory(QDir::currentPath() + "/history-window-XXXXXX");
    require(directory.isValid(), "Could not create history window fixture directory");
    storage(directory.path());
    pruning_and_legacy(directory.path());
    collisions_and_exit(directory.path());
    dialogs(directory.path());
    lookup_and_settings(directory.path());
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
