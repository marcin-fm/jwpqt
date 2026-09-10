// SPDX-License-Identifier: GPL-2.0-or-later

#include "edict_user_dictionary_dialog.h"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include <QApplication>
#include <QAction>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QListWidget>
#include <QMenu>
#include <QMimeData>
#include <QPointer>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QUrl>

#include "file_io.h"
#include "jwpqt/core/jwp_text_codec.h"

namespace {

using jwpqt::core::EdictUserDictionary;
using jwpqt::core::EdictUserDictionaryError;
using jwpqt::core::EdictUserEntry;
using jwpqt::core::LegacyCodePage;
using jwpqt::core::JisCode;
using jwpqt::qt::EdictUserDictionaryDialog;

class PromptTestDialog : public EdictUserDictionaryDialog {
 public:
  using EdictUserDictionaryDialog::EdictUserDictionaryDialog;

  std::optional<EdictUserEntry> prompt(
      const std::optional<EdictUserEntry>& initial) {
    return prompt_for_entry(initial);
  }
};

class ShortcutTestDialog : public EdictUserDictionaryDialog {
 public:
  using EdictUserDictionaryDialog::EdictUserDictionaryDialog;

  int prompt_count = 0;
  std::optional<EdictUserEntry> last_initial;

 protected:
  std::optional<EdictUserEntry> prompt_for_entry(
      const std::optional<EdictUserEntry>& initial) override {
    ++prompt_count;
    last_initial = initial;
    return std::nullopt;
  }
};

void require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

EdictUserEntry entry(jwpqt::core::JwpText reading, std::u32string meaning,
                     jwpqt::core::JwpText headword = {}) {
  return jwpqt::core::make_edict_user_entry(
      std::move(reading), std::move(headword), std::move(meaning));
}

void type_key(QLineEdit& edit, int key, const QString& text) {
  QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier, text);
  QApplication::sendEvent(&edit, &press);
  QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier, text);
  QApplication::sendEvent(&edit, &release);
}

void send_list_key(QListWidget* list, int key,
                   Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
  QKeyEvent shortcut(QEvent::ShortcutOverride, key, modifiers);
  QApplication::sendEvent(list, &shortcut);
  QKeyEvent press(QEvent::KeyPress, key, modifiers);
  QApplication::sendEvent(list, &press);
  QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier);
  QApplication::sendEvent(list, &release);
}

void test_list_shortcuts() {
  const EdictUserEntry first = entry({0x2422}, U"first");
  const EdictUserEntry second = entry({0x2424}, U"second");
  const EdictUserEntry third = entry({0x2426}, U"third");
  int saves = 0;
  ShortcutTestDialog dialog(
      EdictUserDictionary::from_entries({first, second, third}),
      LegacyCodePage::k1252,
      [&](EdictUserDictionary) {
        ++saves;
        return true;
      });
  auto* list =
      dialog.findChild<QListWidget*>(QStringLiteral("edictUserEntries"));
  require(list != nullptr, "EDICT shortcut list is missing");

  list->setCurrentRow(0);
  send_list_key(list, Qt::Key_Up, Qt::ControlModifier);
  require(dialog.entries()[0] == first,
          "EDICT Ctrl+Up changed the first entry");
  list->setCurrentRow(2);
  send_list_key(list, Qt::Key_Down, Qt::ControlModifier);
  require(dialog.entries()[2] == third,
          "EDICT Ctrl+Down changed the last entry");

  list->setCurrentRow(1);
  send_list_key(list, Qt::Key_Space);
  require(dialog.prompt_count == 1 && dialog.last_initial == second,
          "EDICT Space did not invoke Edit without changing the model");
  send_list_key(list, Qt::Key_Insert);
  require(dialog.prompt_count == 2 && !dialog.last_initial.has_value(),
          "EDICT Insert did not invoke Add without changing the model");

  const QString rendered = list->currentItem()->text();
  QApplication::clipboard()->clear();
  send_list_key(list, Qt::Key_C, Qt::ControlModifier);
  require(QApplication::clipboard()->text() == rendered &&
              dialog.entries() ==
                  std::vector<EdictUserEntry>{first, second, third},
          "EDICT Ctrl+C did not copy the rendered current entry safely");
  QApplication::clipboard()->clear();
  send_list_key(list, Qt::Key_Insert, Qt::ControlModifier);
  require(QApplication::clipboard()->text() == rendered,
          "EDICT Ctrl+Insert did not copy the rendered current entry");

  send_list_key(list, Qt::Key_Up, Qt::ControlModifier);
  require(list->currentRow() == 0 && dialog.entries()[0] == second &&
              dialog.entries()[1] == first,
          "EDICT Ctrl+Up did not reorder and retain the current entry");
  send_list_key(list, Qt::Key_Down, Qt::ControlModifier);
  require(list->currentRow() == 1 && dialog.entries()[0] == first &&
              dialog.entries()[1] == second,
          "EDICT Ctrl+Down did not restore the current entry order");
  send_list_key(list, Qt::Key_Delete);
  require(dialog.entries() == std::vector<EdictUserEntry>{first, third} &&
              saves == 0,
          "EDICT Delete did not remove the current entry");
}

void test_list_lookups() {
  const EdictUserEntry first = entry({0x2422}, U"first", {0x3021});
  const EdictUserEntry second = entry({0x2424}, U"second");
  std::vector<JisCode> information;
  std::vector<JisCode> radicals;
  EdictUserDictionaryDialog dialog(
      EdictUserDictionary::from_entries({first, second}),
      LegacyCodePage::k1252,
      [](EdictUserDictionary) { return true; });
  dialog.set_lookup_handlers(
      [&](JisCode code) { information.push_back(code); },
      [&](JisCode code) { radicals.push_back(code); });

  auto* list =
      dialog.findChild<QListWidget*>(QStringLiteral("edictUserEntries"));
  auto* information_action = dialog.findChild<QAction*>(
      QStringLiteral("edictUserCharacterInformation"));
  auto* radical_action =
      dialog.findChild<QAction*>(QStringLiteral("edictUserRadicalLookup"));
  require(list && information_action && radical_action,
          "EDICT lookup controls are missing");

  list->setCurrentRow(0);
  send_list_key(list, Qt::Key_I, Qt::ControlModifier);
  send_list_key(list, Qt::Key_L, Qt::ControlModifier);
  send_list_key(list, Qt::Key_F5);
  require(information == std::vector<JisCode>{0x3021} &&
              radicals == std::vector<JisCode>({0x3021, 0x3021}),
          "EDICT lookup shortcuts did not prefer the original headword");

  bool saw_popup_actions = false;
  QTimer::singleShot(0, [&] {
    auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
    if (!menu) return;
    saw_popup_actions =
        menu->actions().contains(information_action) &&
        menu->actions().contains(radical_action);
    information_action->trigger();
    menu->close();
  });
  send_list_key(list, Qt::Key_F23);
  require(saw_popup_actions &&
              information == std::vector<JisCode>({0x3021, 0x3021}),
          "EDICT F23 popup omitted row lookup commands");

  list->setCurrentRow(1);
  information_action->trigger();
  radical_action->trigger();
  require(information.back() == 0x2424 && radicals.back() == 0x2424,
          "EDICT lookup did not fall back to the original reading");
  list->setCurrentRow(-1);
  require(!information_action->isEnabled() && !radical_action->isEnabled(),
          "EDICT lookup commands stayed enabled without a row");

  auto* status = dialog.findChild<QLabel*>(QStringLiteral("edictUserStatus"));
  require(status != nullptr, "EDICT lookup status is missing");
  list->setCurrentRow(0);
  dialog.set_lookup_handlers(
      [](JisCode) { throw std::runtime_error("information failed"); },
      [](JisCode) { throw 7; });
  information_action->trigger();
  require(status->text() == QStringLiteral("information failed"),
          "EDICT information failure escaped the list command");
  radical_action->trigger();
  require(status->text() == QStringLiteral("Could not open Radical Lookup"),
          "EDICT unknown radical failure escaped the list command");

  auto* doomed = new EdictUserDictionaryDialog(
      EdictUserDictionary::from_entries({first}),
      LegacyCodePage::k1252,
      [](EdictUserDictionary) { return true; });
  QPointer<EdictUserDictionaryDialog> guard(doomed);
  doomed->set_lookup_handlers([doomed](JisCode) { delete doomed; }, {});
  auto* doomed_list =
      doomed->findChild<QListWidget*>(QStringLiteral("edictUserEntries"));
  auto* doomed_action = doomed->findChild<QAction*>(
      QStringLiteral("edictUserCharacterInformation"));
  require(doomed_list && doomed_action,
          "Disposable EDICT lookup controls are missing");
  doomed_list->setCurrentRow(0);
  doomed_action->trigger();
  require(!guard, "EDICT lookup touched a deleted owning dialog");
}

void test_editing_and_save() {
  const EdictUserEntry first = entry({0x242b}, U"first");
  const EdictUserEntry second = entry({0x2422}, U"second");
  const EdictUserEntry replacement = entry({0x2424}, U"replacement");
  EdictUserDictionary saved;
  int saves = 0;
  EdictUserDictionaryDialog dialog(
      EdictUserDictionary::from_entries({first}), LegacyCodePage::k1252,
      [&](EdictUserDictionary dictionary) {
        ++saves;
        saved = std::move(dictionary);
        return true;
      });

  require(dialog.add_entry(second) == 1 && dialog.entries().size() == 2,
          "Dialog did not append a user dictionary entry");
  require(dialog.move_entry_up(1) && !dialog.move_entry_up(0) &&
              dialog.entries()[0] == second,
          "Dialog did not preserve explicit move semantics");
  dialog.replace_entry(1, replacement);
  dialog.sort_entries();
  require(dialog.entries().size() == 2,
          "Dialog sort changed the entry count");
  dialog.erase_entry(1);
  require(dialog.entries().size() == 1 && dialog.save_changes() &&
              saves == 1 && saved.entries() == dialog.entries(),
          "Dialog did not publish its complete edited dictionary");
}

void test_import_insert_and_failed_save() {
  const EdictUserEntry first = entry({0x2422}, U"first");
  const EdictUserEntry imported = entry({0x242b}, U"imported", {0x3021});
  std::optional<EdictUserEntry> inserted;
  EdictUserDictionaryDialog dialog(
      EdictUserDictionary::from_entries({first}), LegacyCodePage::k1252,
      [](EdictUserDictionary) { return false; },
      [&](const EdictUserEntry& value) { inserted = value; });
  dialog.append_dictionary(EdictUserDictionary::from_entries({imported}));
  require(dialog.entries().size() == 2,
          "Dialog did not append an imported dictionary");

  QListWidget* list =
      dialog.findChild<QListWidget*>(QStringLiteral("edictUserEntries"));
  require(list != nullptr && list->count() == 2,
          "Dialog did not render imported entries");
  list->setCurrentRow(1);
  require(dialog.insert_selected() && inserted == imported,
          "Dialog did not insert the selected user entry");

  const std::vector<EdictUserEntry> before = dialog.entries();
  require(!dialog.save_changes() && dialog.entries() == before,
          "Failed dictionary save changed the working entries");
}

void test_title_bar_close() {
  const EdictUserEntry first = entry({0x2422}, U"first");

  int discarded_saves = 0;
  EdictUserDictionaryDialog discarded(
      EdictUserDictionary::from_entries({}), LegacyCodePage::k1252,
      [&](EdictUserDictionary) {
        ++discarded_saves;
        return true;
      });
  discarded.add_entry(first);
  discarded.show();
  bool no_prompt = false;
  QTimer::singleShot(0, [&] {
    auto* prompt =
        qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    no_prompt = prompt &&
                prompt->objectName() ==
                    QStringLiteral("saveEdictUserDictionaryClosePrompt") &&
                prompt->defaultButton() == prompt->button(QMessageBox::Yes);
    if (prompt) prompt->button(QMessageBox::No)->click();
  });
  send_list_key(discarded.findChild<QListWidget*>(
                    QStringLiteral("edictUserEntries")),
                Qt::Key_F4, Qt::ControlModifier);
  require(no_prompt && !discarded.isVisible() &&
              discarded.isWindowModified() && discarded_saves == 0,
          "Ctrl+F4 did not use the guarded user-dictionary close");

  int accepted_saves = 0;
  EdictUserDictionary saved;
  EdictUserDictionaryDialog accepted(
      EdictUserDictionary::from_entries({}), LegacyCodePage::k1252,
      [&](EdictUserDictionary dictionary) {
        ++accepted_saves;
        saved = std::move(dictionary);
        return true;
      });
  accepted.add_entry(first);
  accepted.show();
  QTimer::singleShot(0, [] {
    auto* prompt =
        qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    if (prompt) prompt->button(QMessageBox::Yes)->click();
  });
  require(accepted.close() && accepted_saves == 1 &&
              saved.entries() == std::vector<EdictUserEntry>{first} &&
              !accepted.isWindowModified() && !accepted.isVisible(),
          "Closing the user dictionary with Yes did not save exactly once");

  int failed_saves = 0;
  EdictUserDictionaryDialog failed(
      EdictUserDictionary::from_entries({}), LegacyCodePage::k1252,
      [&](EdictUserDictionary) {
        ++failed_saves;
        return false;
      });
  failed.add_entry(first);
  failed.show();
  QTimer::singleShot(0, [] {
    auto* prompt =
        qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    if (prompt) prompt->button(QMessageBox::Yes)->click();
  });
  require(!failed.close() && failed_saves == 1 && failed.isVisible() &&
              failed.isWindowModified(),
          "A failed user-dictionary save closed or cleared the editor");

  int cancel_saves = 0;
  EdictUserDictionaryDialog cancelled(
      EdictUserDictionary::from_entries({}), LegacyCodePage::k1252,
      [&](EdictUserDictionary) {
        ++cancel_saves;
        return true;
      });
  cancelled.add_entry(first);
  cancelled.show();
  cancelled.reject();
  require(!cancelled.isVisible() && cancelled.isWindowModified() &&
              cancel_saves == 0 &&
              cancelled.findChild<QMessageBox*>() == nullptr,
          "Explicit user-dictionary Cancel prompted or saved changes");

  auto* deleted = new EdictUserDictionaryDialog(
      EdictUserDictionary::from_entries({}), LegacyCodePage::k1252,
      [](EdictUserDictionary) { return true; });
  deleted->add_entry(first);
  deleted->show();
  QPointer<EdictUserDictionaryDialog> deleted_guard(deleted);
  bool deletion_prompt = false;
  QTimer::singleShot(0, [&] {
    auto* prompt =
        qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    deletion_prompt = prompt &&
                      prompt->objectName() ==
                          QStringLiteral("saveEdictUserDictionaryClosePrompt");
    delete deleted;
  });
  (void)deleted->close();
  require(deletion_prompt && deleted_guard.isNull(),
          "Deleting the user dictionary during close used stale state");
}

void test_import_command() {
  QTemporaryDir temporary;
  require(temporary.isValid(), "Could not create EDICT import fixture directory");
  const QString directory = temporary.filePath(QStringLiteral("dictionary"));
  require(QDir().mkpath(directory),
          "Could not create EDICT import source directory");
  const QString backing_path = directory + QStringLiteral("/user.dct");
  const QString import_path = directory + QStringLiteral("/import.any");
  const EdictUserEntry imported = entry({0x242b}, U"imported", {0x3021});
  jwpqt::qt::write_edict_user_dictionary_file(
      import_path, EdictUserDictionary::from_entries({imported}),
      jwpqt::core::LegacyCodePage::k1252);

  EdictUserDictionaryDialog dialog(
      EdictUserDictionary::from_entries({}),
      jwpqt::core::LegacyCodePage::k1252,
      [](EdictUserDictionary) { return true; }, {}, nullptr, backing_path);
  QPushButton* import_button =
      dialog.findChild<QPushButton*>(QStringLiteral("edictUserImport"));
  require(import_button != nullptr, "EDICT Import button is unavailable");

  bool chooser_verified = false;
  QTimer::singleShot(0, [&] {
    auto* chooser = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
    chooser_verified =
        chooser && chooser->directory().absolutePath() == directory &&
        chooser->nameFilters().contains(QStringLiteral("All files (*)"));
    if (chooser) {
      chooser->selectFile(import_path);
      static_cast<QDialog*>(chooser)->accept();
    }
  });
  import_button->click();
  require(chooser_verified && dialog.entries() ==
                                  std::vector<EdictUserEntry>{imported} &&
              dialog.isWindowModified(),
          "EDICT Import did not use the configured directory or append entries");

  auto* deleted = new EdictUserDictionaryDialog(
      EdictUserDictionary::from_entries({}),
      jwpqt::core::LegacyCodePage::k1252,
      [](EdictUserDictionary) { return true; }, {}, nullptr, backing_path);
  QPushButton* deleted_button =
      deleted->findChild<QPushButton*>(QStringLiteral("edictUserImport"));
  QPointer<EdictUserDictionaryDialog> deleted_guard(deleted);
  QTimer::singleShot(0, [&] { delete deleted; });
  deleted_button->click();
  require(deleted_guard.isNull(),
          "Deleting EDICT Import during its chooser used stale dialog state");
}

void test_multi_file_import_drop() {
  QTemporaryDir temporary;
  require(temporary.isValid(), "Could not create EDICT drop fixture directory");
  const EdictUserEntry first = entry({0x2422}, U"first");
  const EdictUserEntry imported_a = entry({0x2424}, U"second");
  const EdictUserEntry imported_b = entry({0x2426}, U"third", {0x3021});
  const QString path_a = temporary.filePath(QStringLiteral("first.dct"));
  const QString path_b = temporary.filePath(QStringLiteral("second.dct"));
  const QString malformed = temporary.filePath(QStringLiteral("malformed.dct"));
  jwpqt::qt::write_edict_user_dictionary_file(
      path_a, EdictUserDictionary::from_entries({imported_a}),
      LegacyCodePage::k1252);
  jwpqt::qt::write_edict_user_dictionary_file(
      path_b, EdictUserDictionary::from_entries({imported_b}),
      LegacyCodePage::k1252);
  QFile bad(malformed);
  require(bad.open(QIODevice::WriteOnly) &&
              bad.write("word /unterminated\n") == 19,
          "Could not write malformed EDICT drop fixture");
  bad.close();

  EdictUserDictionaryDialog dialog(
      EdictUserDictionary::from_entries({first}), LegacyCodePage::k1252,
      [](EdictUserDictionary) { return true; });
  QMimeData files;
  files.setUrls({QUrl::fromLocalFile(path_a), QUrl::fromLocalFile(path_b)});
  QDragEnterEvent enter(QPoint(2, 2), Qt::CopyAction, &files,
                        Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(&dialog, &enter);
  require(enter.isAccepted(), "Local EDICT dictionary drag was not accepted");
  QDropEvent drop(QPointF(2, 2), Qt::CopyAction, &files,
                  Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(&dialog, &drop);
  require(drop.isAccepted() &&
              dialog.entries() ==
                  std::vector<EdictUserEntry>{first, imported_a, imported_b} &&
              dialog.findChild<QLabel*>(QStringLiteral("edictUserStatus"))
                  ->text()
                  .contains(QStringLiteral("2 files")),
          "Multi-file EDICT drop lost source order or status");

  EdictUserDictionaryDialog atomic(
      EdictUserDictionary::from_entries({first}), LegacyCodePage::k1252,
      [](EdictUserDictionary) { return true; });
  QListWidget* list =
      atomic.findChild<QListWidget*>(QStringLiteral("edictUserEntries"));
  list->setCurrentRow(0);
  const bool modified = atomic.isWindowModified();
  QMimeData mixed;
  mixed.setUrls({QUrl::fromLocalFile(path_a), QUrl::fromLocalFile(malformed)});
  QDragEnterEvent mixed_enter(QPoint(2, 2), Qt::CopyAction, &mixed,
                              Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(&atomic, &mixed_enter);
  QDropEvent mixed_drop(QPointF(2, 2), Qt::CopyAction, &mixed,
                        Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(&atomic, &mixed_drop);
  require(mixed_enter.isAccepted() && mixed_drop.isAccepted() &&
              atomic.entries() == std::vector<EdictUserEntry>{first} &&
              list->currentRow() == 0 && atomic.isWindowModified() == modified &&
              !atomic.findChild<QLabel*>(QStringLiteral("edictUserStatus"))
                   ->text()
                   .isEmpty(),
          "Malformed EDICT drop partially changed the working copy");

  QMimeData remote;
  remote.setUrls({QUrl(QStringLiteral("https://example.invalid/user.dct"))});
  QDragEnterEvent remote_enter(QPoint(2, 2), Qt::CopyAction, &remote,
                               Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(&atomic, &remote_enter);
  require(!remote_enter.isAccepted() &&
              atomic.entries() == std::vector<EdictUserEntry>{first},
          "Nonlocal EDICT dictionary drag was accepted");
}

void test_invalid_edit_is_atomic() {
  const EdictUserEntry first = entry({0x2422}, U"first");
  EdictUserDictionaryDialog dialog(
      EdictUserDictionary::from_entries({first}), LegacyCodePage::k1252,
      [](EdictUserDictionary) { return true; });
  const std::vector<EdictUserEntry> before = dialog.entries();
  bool rejected = false;
  try {
    dialog.replace_entry(0, EdictUserEntry{{}, {}, U"bad"});
  } catch (const EdictUserDictionaryError&) {
    rejected = true;
  }
  require(rejected && dialog.entries() == before,
          "Invalid dialog edit partially changed the working dictionary");
  rejected = false;
  try {
    dialog.add_entry(entry({0x2424}, U"\U0001f600"));
  } catch (const EdictUserDictionaryError&) {
    rejected = true;
  }
  require(rejected && dialog.entries() == before,
          "Unrepresentable dialog entry partially changed the working copy");
}

void test_imported_empty_meaning_round_trip() {
  const EdictUserEntry imported{{}, {0x2422}, {}};
  PromptTestDialog dialog(EdictUserDictionary::from_entries({imported}),
                          LegacyCodePage::k1252,
                          [](EdictUserDictionary) { return true; });
  QTimer::singleShot(0, [] {
    QWidget* modal = QApplication::activeModalWidget();
    if (modal == nullptr) {
      return;
    }
    QDialogButtonBox* buttons = modal->findChild<QDialogButtonBox*>();
    if (buttons != nullptr) {
      buttons->button(QDialogButtonBox::Ok)->click();
    }
  });
  const std::optional<EdictUserEntry> edited = dialog.prompt(imported);
  require(edited.has_value() && *edited == imported,
          "Confirming an imported empty meaning changed its wire semantics");
}

void test_japanese_entry_fields() {
  QAction overwrite(nullptr);
  overwrite.setCheckable(true);
  overwrite.setChecked(true);
  const EdictUserEntry initial = entry(
      jwpqt::core::encode_jwp_text(U"かき"), U"meaning",
      jwpqt::core::encode_jwp_text(U"日本"));
  PromptTestDialog dialog(EdictUserDictionary::from_entries({initial}),
                          LegacyCodePage::k1252,
                          [](EdictUserDictionary) { return true; });
  dialog.set_overwrite_action(&overwrite);
  bool interacted = false;
  QTimer::singleShot(0, [&] {
    QWidget* modal = QApplication::activeModalWidget();
    auto* headword = modal ? modal->findChild<QLineEdit*>(
                                 QStringLiteral("edictUserHeadword"))
                           : nullptr;
    auto* reading = modal ? modal->findChild<QLineEdit*>(
                                QStringLiteral("edictUserReading"))
                          : nullptr;
    auto* meaning = modal ? modal->findChild<QLineEdit*>(
                                QStringLiteral("edictUserMeaning"))
                          : nullptr;
    auto* meaning_mode = modal ? modal->findChild<QToolButton*>(
                                     QStringLiteral("edictUserMeaningMode"))
                               : nullptr;
    auto* buttons = modal ? modal->findChild<QDialogButtonBox*>() : nullptr;
    if (!headword || !reading || !meaning || !meaning_mode || !buttons) {
      if (modal) modal->close();
      return;
    }
    interacted = reading->toolTip().contains(QStringLiteral("Overwrite"));
    reading->setCursorPosition(0);
    type_key(*reading, Qt::Key_N, QStringLiteral("n"));
    type_key(*reading, Qt::Key_A, QStringLiteral("a"));
    headword->setCursorPosition(headword->text().size());
    type_key(*headword, Qt::Key_N, QStringLiteral("n"));
    meaning->clear();
    meaning_mode->click();
    type_key(*meaning, Qt::Key_N, QStringLiteral("n"));
    type_key(*meaning, Qt::Key_E, QStringLiteral("e"));
    type_key(*meaning, Qt::Key_W, QStringLiteral("w"));
    buttons->button(QDialogButtonBox::Ok)->click();
  });
  const std::optional<EdictUserEntry> edited = dialog.prompt(initial);
  require(interacted && edited.has_value() &&
              edited->reading == jwpqt::core::encode_jwp_text(U"なき") &&
              edited->headword == jwpqt::core::encode_jwp_text(U"日本ん") &&
              edited->meaning == U"new",
          "EDICT entry fields did not preserve Japanese and ASCII input modes");
}

void test_non_kana_reading_confirmation() {
  PromptTestDialog dialog(EdictUserDictionary::from_entries({}),
                          LegacyCodePage::k1252,
                          [](EdictUserDictionary) { return true; });
  bool declined = false;
  bool fields_retained = false;
  bool warning_verified = false;
  bool reading_retained = false;
  bool meaning_retained = false;
  QTimer::singleShot(0, [&] {
    QWidget* modal = QApplication::activeModalWidget();
    auto* reading = modal ? modal->findChild<QLineEdit*>(
                                QStringLiteral("edictUserReading"))
                          : nullptr;
    auto* meaning = modal ? modal->findChild<QLineEdit*>(
                                QStringLiteral("edictUserMeaning"))
                          : nullptr;
    auto* buttons = modal ? modal->findChild<QDialogButtonBox*>() : nullptr;
    if (!reading || !meaning || !buttons) {
      if (modal) modal->close();
      return;
    }
    reading->setText(QStringLiteral("日本"));
    meaning->setText(QStringLiteral("Japan"));
    QTimer::singleShot(0, [&] {
      auto* warning = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
      if (!warning ||
          warning->objectName() != QStringLiteral("edictUserNonKanaWarning")) {
        if (warning) warning->reject();
        return;
      }
      warning_verified =
          warning->defaultButton() == warning->button(QMessageBox::No) &&
          warning->text().contains(QStringLiteral("non-kana"));
      auto* entry_dialog = qobject_cast<QDialog*>(warning->parentWidget());
      QObject::connect(warning, &QDialog::finished, entry_dialog, [&, entry_dialog] {
        auto* retry_reading =
            entry_dialog->findChild<QLineEdit*>(QStringLiteral("edictUserReading"));
        auto* retry_meaning =
            entry_dialog->findChild<QLineEdit*>(QStringLiteral("edictUserMeaning"));
        auto* retry_buttons =
            entry_dialog->findChild<QDialogButtonBox*>();
        if (!retry_reading || !retry_meaning || !retry_buttons) {
          entry_dialog->close();
          return;
        }
        reading_retained = retry_reading->text() == QStringLiteral("日本");
        meaning_retained = retry_meaning->text() == QStringLiteral("Japan");
        fields_retained = reading_retained && meaning_retained;
        QTimer::singleShot(0, [&] {
          auto* retry_warning =
              qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
          if (retry_warning &&
              retry_warning->objectName() ==
                  QStringLiteral("edictUserNonKanaWarning")) {
            retry_warning->button(QMessageBox::Yes)->click();
          } else if (retry_warning) {
            retry_warning->reject();
          }
        });
        retry_buttons->button(QDialogButtonBox::Ok)->click();
      }, Qt::QueuedConnection);
      warning->button(QMessageBox::No)->click();
      declined = true;
    });
    buttons->button(QDialogButtonBox::Ok)->click();
  });

  const std::optional<EdictUserEntry> accepted = dialog.prompt(std::nullopt);
  require(declined && fields_retained && warning_verified &&
              accepted.has_value() &&
              accepted->reading ==
                  jwpqt::core::encode_jwp_text(U"日本") &&
              accepted->meaning == U"Japan",
          "Non-kana reading confirmation lost fields or ignored its safe default");
}

void test_invalid_entry_stays_open() {
  PromptTestDialog dialog(EdictUserDictionary::from_entries({}),
                          LegacyCodePage::k1252,
                          [](EdictUserDictionary) { return true; });
  bool error_verified = false;
  bool meaning_error_verified = false;
  bool fields_retained = false;
  QTimer::singleShot(0, [&] {
    QWidget* modal = QApplication::activeModalWidget();
    auto* reading = modal ? modal->findChild<QLineEdit*>(
                                QStringLiteral("edictUserReading"))
                          : nullptr;
    auto* meaning = modal ? modal->findChild<QLineEdit*>(
                                QStringLiteral("edictUserMeaning"))
                          : nullptr;
    auto* buttons = modal ? modal->findChild<QDialogButtonBox*>() : nullptr;
    if (!reading || !meaning || !buttons) {
      if (modal) modal->close();
      return;
    }
    reading->setText(QStringLiteral("か き"));
    meaning->setText(QStringLiteral("oyster"));
    QTimer::singleShot(0, [&] {
      auto* error = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
      if (!error ||
          error->objectName() != QStringLiteral("edictUserEntryError")) {
        if (error) error->reject();
        return;
      }
      error_verified = error->text().contains(QStringLiteral("space"));
      auto* entry_dialog = qobject_cast<QDialog*>(error->parentWidget());
      QObject::connect(error, &QDialog::finished, entry_dialog, [&, entry_dialog] {
        auto* retry_reading =
            entry_dialog->findChild<QLineEdit*>(QStringLiteral("edictUserReading"));
        auto* retry_meaning =
            entry_dialog->findChild<QLineEdit*>(QStringLiteral("edictUserMeaning"));
        auto* retry_buttons =
            entry_dialog->findChild<QDialogButtonBox*>();
        fields_retained = retry_reading && retry_meaning && retry_buttons &&
                          retry_reading->text() == QStringLiteral("か き") &&
                          retry_meaning->text() == QStringLiteral("oyster");
        if (!fields_retained) {
          if (auto* dialog = qobject_cast<QDialog*>(entry_dialog)) {
            dialog->reject();
          }
          return;
        }
        retry_reading->setText(QStringLiteral("かき"));
        retry_meaning->clear();
        QTimer::singleShot(0, [&] {
          auto* meaning_error =
              qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
          if (!meaning_error ||
              meaning_error->objectName() !=
                  QStringLiteral("edictUserEntryError")) {
            if (meaning_error) meaning_error->reject();
            return;
          }
          meaning_error_verified =
              meaning_error->text().contains(QStringLiteral("Meaning"));
          auto* retained_dialog =
              qobject_cast<QDialog*>(meaning_error->parentWidget());
          QObject::connect(meaning_error, &QDialog::finished, retained_dialog,
                           [&, retained_dialog] {
            auto* retained_meaning =
                retained_dialog->findChild<QLineEdit*>(
                    QStringLiteral("edictUserMeaning"));
            meaning_error_verified =
                meaning_error_verified && retained_meaning &&
                retained_meaning->text().isEmpty();
            retained_dialog->reject();
          }, Qt::QueuedConnection);
          meaning_error->button(QMessageBox::Ok)->click();
        });
        retry_buttons->button(QDialogButtonBox::Ok)->click();
      }, Qt::QueuedConnection);
      error->button(QMessageBox::Ok)->click();
    });
    buttons->button(QDialogButtonBox::Ok)->click();
  });
  require(!dialog.prompt(std::nullopt).has_value() && error_verified &&
              meaning_error_verified && fields_retained,
          "Invalid user entry closed its editor or lost typed fields");
}

void test_callback_exceptions_are_contained() {
  const EdictUserEntry first = entry({0x2422}, U"first");
  EdictUserDictionaryDialog dialog(
      EdictUserDictionary::from_entries({first}), LegacyCodePage::k1252,
      [](EdictUserDictionary) -> bool { throw 7; },
      [](const EdictUserEntry&) { throw 9; });
  QListWidget* list =
      dialog.findChild<QListWidget*>(QStringLiteral("edictUserEntries"));
  QLabel* status =
      dialog.findChild<QLabel*>(QStringLiteral("edictUserStatus"));
  require(list != nullptr && status != nullptr,
          "Callback failure fixture has no dialog controls");
  list->setCurrentRow(0);
  require(!dialog.insert_selected() && !status->text().isEmpty(),
          "Nonstandard insert failure escaped the Qt callback boundary");
  require(!dialog.save_changes() && !status->text().isEmpty(),
          "Nonstandard save failure escaped the Qt callback boundary");
}

}  // namespace

int main(int argc, char** argv) {
  QApplication application(argc, argv);
  try {
    test_list_shortcuts();
    test_list_lookups();
    test_editing_and_save();
    test_import_insert_and_failed_save();
    test_title_bar_close();
    test_import_command();
    test_multi_file_import_drop();
    test_invalid_edit_is_atomic();
    test_imported_empty_meaning_round_trip();
    test_japanese_entry_fields();
    test_non_kana_reading_confirmation();
    test_invalid_entry_stays_open();
    test_callback_exceptions_are_contained();
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << "edict_user_dictionary_dialog_test: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
