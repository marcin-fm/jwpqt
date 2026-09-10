// SPDX-License-Identifier: GPL-2.0-or-later

#include "wnn_user_dictionary_dialog.h"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include <QApplication>
#include <QAction>
#include <QComboBox>
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
#include <QListWidget>
#include <QMessageBox>
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

using jwpqt::core::JwpText;
using jwpqt::core::WnnUserDictionary;
using jwpqt::core::WnnUserDictionaryError;
using jwpqt::core::WnnUserEntry;
using jwpqt::qt::WnnUserDictionaryDialog;

class PromptTestDialog : public WnnUserDictionaryDialog {
 public:
  using WnnUserDictionaryDialog::WnnUserDictionaryDialog;

  std::optional<WnnUserEntry> prompt(
      const std::optional<WnnUserEntry>& initial) {
    return prompt_for_entry(initial);
  }
};

void require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

WnnUserEntry entry(JwpText reading, JwpText candidate) {
  return WnnUserEntry{std::move(reading), '*', {std::move(candidate)}};
}

void type_key(QLineEdit& edit, int key, const QString& text) {
  QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier, text);
  QApplication::sendEvent(&edit, &press);
  QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier, text);
  QApplication::sendEvent(&edit, &release);
}

void test_editing_and_save() {
  const WnnUserEntry first = entry({0x242b}, {0x3021});
  const WnnUserEntry second = entry({0x2422}, {0x3022});
  const WnnUserEntry replacement = entry({0x2424}, {0x3023});
  WnnUserDictionary saved;
  int saves = 0;
  WnnUserDictionaryDialog dialog(
      WnnUserDictionary::from_entries({first}),
      [&](WnnUserDictionary dictionary) {
        ++saves;
        saved = std::move(dictionary);
        return true;
      });

  require(dialog.add_entry(second) == 1 && dialog.entries().size() == 2,
          "Dialog did not append a user entry");
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
  const WnnUserEntry first = entry({0x2422}, {0x3021});
  const WnnUserEntry imported = entry({0x242b}, {0x3022});
  std::optional<WnnUserEntry> inserted;
  WnnUserDictionaryDialog dialog(
      WnnUserDictionary::from_entries({first}),
      [](WnnUserDictionary) { return false; },
      [&](const WnnUserEntry& value) { inserted = value; });
  dialog.append_dictionary(WnnUserDictionary::from_entries({imported}));
  require(dialog.entries().size() == 2,
          "Dialog did not append an imported dictionary");

  QListWidget* list =
      dialog.findChild<QListWidget*>(QStringLiteral("wnnUserEntries"));
  require(list != nullptr && list->count() == 2,
          "Dialog did not render imported entries");
  list->setCurrentRow(1);
  require(dialog.insert_selected() && inserted == imported,
          "Dialog did not insert the selected user entry");

  const std::vector<WnnUserEntry> before = dialog.entries();
  require(!dialog.save_changes() && dialog.entries() == before,
          "Failed dictionary save changed the working entries");
}

void test_title_bar_close() {
  const WnnUserEntry first = entry({0x2422}, {0x3021});

  int discarded_saves = 0;
  WnnUserDictionaryDialog discarded(
      WnnUserDictionary::from_entries({}),
      [&](WnnUserDictionary) {
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
                    QStringLiteral("saveWnnUserDictionaryClosePrompt") &&
                prompt->defaultButton() == prompt->button(QMessageBox::Yes);
    if (prompt) prompt->button(QMessageBox::No)->click();
  });
  require(discarded.close() && no_prompt && !discarded.isVisible() &&
              discarded.isWindowModified() && discarded_saves == 0,
          "Closing user conversions with No saved or retained the window");

  int accepted_saves = 0;
  WnnUserDictionary saved;
  WnnUserDictionaryDialog accepted(
      WnnUserDictionary::from_entries({}),
      [&](WnnUserDictionary dictionary) {
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
              saved.entries() == std::vector<WnnUserEntry>{first} &&
              !accepted.isWindowModified() && !accepted.isVisible(),
          "Closing user conversions with Yes did not save exactly once");

  int failed_saves = 0;
  WnnUserDictionaryDialog failed(
      WnnUserDictionary::from_entries({}),
      [&](WnnUserDictionary) {
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
          "A failed user-conversion save closed or cleared the editor");

  int cancel_saves = 0;
  WnnUserDictionaryDialog cancelled(
      WnnUserDictionary::from_entries({}),
      [&](WnnUserDictionary) {
        ++cancel_saves;
        return true;
      });
  cancelled.add_entry(first);
  cancelled.show();
  cancelled.reject();
  require(!cancelled.isVisible() && cancelled.isWindowModified() &&
              cancel_saves == 0 &&
              cancelled.findChild<QMessageBox*>() == nullptr,
          "Explicit user-conversion Cancel prompted or saved changes");

  auto* deleted = new WnnUserDictionaryDialog(
      WnnUserDictionary::from_entries({}),
      [](WnnUserDictionary) { return true; });
  deleted->add_entry(first);
  deleted->show();
  QPointer<WnnUserDictionaryDialog> deleted_guard(deleted);
  bool deletion_prompt = false;
  QTimer::singleShot(0, [&] {
    auto* prompt =
        qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    deletion_prompt = prompt &&
                      prompt->objectName() ==
                          QStringLiteral("saveWnnUserDictionaryClosePrompt");
    delete deleted;
  });
  (void)deleted->close();
  require(deletion_prompt && deleted_guard.isNull(),
          "Deleting user conversions during its close prompt used stale state");
}

void test_import_command() {
  QTemporaryDir temporary;
  require(temporary.isValid(), "Could not create WNN import fixture directory");
  const QString directory = temporary.filePath(QStringLiteral("dictionary"));
  require(QDir().mkpath(directory), "Could not create WNN import source directory");
  const QString backing_path = directory + QStringLiteral("/user.cnv");
  const QString import_path = directory + QStringLiteral("/import.any");
  const WnnUserEntry imported = entry({0x242b}, {0x3022});
  jwpqt::qt::write_wnn_user_dictionary_file(
      import_path, WnnUserDictionary::from_entries({imported}));

  WnnUserDictionaryDialog dialog(
      WnnUserDictionary::from_entries({}),
      [](WnnUserDictionary) { return true; }, {}, nullptr, backing_path);
  QPushButton* import_button =
      dialog.findChild<QPushButton*>(QStringLiteral("wnnUserImport"));
  require(import_button != nullptr, "WNN Import button is unavailable");

  bool safe_default = false;
  QTimer::singleShot(0, [&] {
    auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    safe_default =
        prompt && prompt->objectName() ==
                      QStringLiteral("confirmWnnUserDictionaryImportPrompt") &&
        prompt->defaultButton() == prompt->button(QMessageBox::No);
    if (prompt) prompt->button(QMessageBox::No)->click();
  });
  import_button->click();
  require(safe_default && dialog.entries().empty() &&
              dialog.findChild<QFileDialog*>() == nullptr,
          "Declining additive WNN Import opened a chooser or changed entries");

  bool chooser_verified = false;
  QTimer::singleShot(0, [&] {
    auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    if (prompt) prompt->button(QMessageBox::Yes)->click();
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
  });
  import_button->click();
  require(chooser_verified && dialog.entries() ==
                                  std::vector<WnnUserEntry>{imported} &&
              dialog.isWindowModified(),
          "WNN Import did not use the configured directory or append entries");

  auto* deleted = new WnnUserDictionaryDialog(
      WnnUserDictionary::from_entries({}),
      [](WnnUserDictionary) { return true; }, {}, nullptr, backing_path);
  QPushButton* deleted_button =
      deleted->findChild<QPushButton*>(QStringLiteral("wnnUserImport"));
  QPointer<WnnUserDictionaryDialog> deleted_guard(deleted);
  QTimer::singleShot(0, [&] { delete deleted; });
  deleted_button->click();
  require(deleted_guard.isNull(),
          "Deleting WNN Import during confirmation used stale dialog state");
}

void test_multi_file_import_drop() {
  QTemporaryDir temporary;
  require(temporary.isValid(), "Could not create WNN drop fixture directory");
  const WnnUserEntry first = entry({0x2422}, {0x3021});
  const WnnUserEntry imported_a = entry({0x2424}, {0x3022});
  const WnnUserEntry imported_b = entry({0x2426}, {0x3023});
  const QString path_a = temporary.filePath(QStringLiteral("first.dat"));
  const QString path_b = temporary.filePath(QStringLiteral("second.dat"));
  const QString malformed = temporary.filePath(QStringLiteral("malformed.dat"));
  jwpqt::qt::write_wnn_user_dictionary_file(
      path_a, WnnUserDictionary::from_entries({imported_a}));
  jwpqt::qt::write_wnn_user_dictionary_file(
      path_b, WnnUserDictionary::from_entries({imported_b}));
  QFile bad(malformed);
  require(bad.open(QIODevice::WriteOnly) && bad.write("\0", 1) == 1,
          "Could not write malformed WNN drop fixture");
  bad.close();

  WnnUserDictionaryDialog dialog(WnnUserDictionary::from_entries({first}),
                                 [](WnnUserDictionary) { return true; });
  QMimeData files;
  files.setUrls({QUrl::fromLocalFile(path_a), QUrl::fromLocalFile(path_b)});
  QDragEnterEvent enter(QPoint(2, 2), Qt::CopyAction, &files,
                        Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(&dialog, &enter);
  require(enter.isAccepted(), "Local WNN dictionary drag was not accepted");
  QDropEvent drop(QPointF(2, 2), Qt::CopyAction, &files,
                  Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(&dialog, &drop);
  require(drop.isAccepted() &&
              dialog.entries() ==
                  std::vector<WnnUserEntry>{first, imported_a, imported_b} &&
              dialog.findChild<QLabel*>(QStringLiteral("wnnUserStatus"))
                  ->text()
                  .contains(QStringLiteral("2 files")),
          "Multi-file WNN drop lost source order or status");

  WnnUserDictionaryDialog atomic(WnnUserDictionary::from_entries({first}),
                                 [](WnnUserDictionary) { return true; });
  QListWidget* list =
      atomic.findChild<QListWidget*>(QStringLiteral("wnnUserEntries"));
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
              atomic.entries() == std::vector<WnnUserEntry>{first} &&
              list->currentRow() == 0 && atomic.isWindowModified() == modified &&
              !atomic.findChild<QLabel*>(QStringLiteral("wnnUserStatus"))
                   ->text()
                   .isEmpty(),
          "Malformed WNN drop partially changed the working copy");

  QMimeData remote;
  remote.setUrls({QUrl(QStringLiteral("https://example.invalid/user.dat"))});
  QDragEnterEvent remote_enter(QPoint(2, 2), Qt::CopyAction, &remote,
                               Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(&atomic, &remote_enter);
  require(!remote_enter.isAccepted() &&
              atomic.entries() == std::vector<WnnUserEntry>{first},
          "Nonlocal WNN dictionary drag was accepted");
}

void test_invalid_edit_is_atomic() {
  const WnnUserEntry first = entry({0x2422}, {0x3021});
  WnnUserDictionaryDialog dialog(WnnUserDictionary::from_entries({first}),
                                 [](WnnUserDictionary) { return true; });
  const std::vector<WnnUserEntry> before = dialog.entries();
  bool rejected = false;
  try {
    dialog.replace_entry(0, WnnUserEntry{{0x2422}, '?', {{0x3022}}});
  } catch (const WnnUserDictionaryError&) {
    rejected = true;
  }
  require(rejected && dialog.entries() == before,
          "Invalid dialog edit partially changed the working dictionary");
}

void test_imported_inflection_round_trip() {
  const WnnUserEntry imported{{0x2422, 0x246b},
                              '1',
                              {{0x3021, 0x246b}, {0x3022}}};
  PromptTestDialog dialog(WnnUserDictionary::from_entries({imported}),
                          [](WnnUserDictionary) { return true; });
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
  const std::optional<WnnUserEntry> edited = dialog.prompt(imported);
  require(edited.has_value() && *edited == imported,
          "Confirming an imported inflected entry changed its wire semantics");
}

void test_japanese_entry_fields() {
  QAction overwrite(nullptr);
  overwrite.setCheckable(true);
  overwrite.setChecked(true);
  const WnnUserEntry initial = entry(
      jwpqt::core::encode_jwp_text(U"かき"),
      jwpqt::core::encode_jwp_text(U"候補"));
  PromptTestDialog dialog(WnnUserDictionary::from_entries({initial}),
                          [](WnnUserDictionary) { return true; });
  dialog.set_overwrite_action(&overwrite);
  bool interacted = false;
  QTimer::singleShot(0, [&] {
    QWidget* modal = QApplication::activeModalWidget();
    auto* reading = modal ? modal->findChild<QLineEdit*>(
                                QStringLiteral("wnnUserReading"))
                          : nullptr;
    auto* candidates = modal ? modal->findChild<QLineEdit*>(
                                   QStringLiteral("wnnUserCandidates"))
                             : nullptr;
    auto* mode = modal ? modal->findChild<QToolButton*>(
                             QStringLiteral("wnnUserReadingMode"))
                       : nullptr;
    auto* buttons = modal ? modal->findChild<QDialogButtonBox*>() : nullptr;
    if (!reading || !candidates || !mode || !buttons) {
      if (modal) modal->close();
      return;
    }
    interacted = mode->text() == QStringLiteral("K") &&
                 reading->toolTip().contains(QStringLiteral("Overwrite"));
    reading->setCursorPosition(0);
    type_key(*reading, Qt::Key_N, QStringLiteral("n"));
    type_key(*reading, Qt::Key_A, QStringLiteral("a"));
    candidates->setCursorPosition(candidates->text().size());
    type_key(*candidates, Qt::Key_N, QStringLiteral("n"));
    buttons->button(QDialogButtonBox::Ok)->click();
  });
  const std::optional<WnnUserEntry> edited = dialog.prompt(initial);
  require(interacted && edited.has_value() &&
              edited->reading == jwpqt::core::encode_jwp_text(U"なき") &&
              edited->candidates == std::vector<JwpText>{
                                        jwpqt::core::encode_jwp_text(U"候補ん")},
          "User conversion fields did not share Japanese overwrite and pending input");

  QAction invalid(nullptr);
  bool rejected = false;
  try {
    dialog.set_overwrite_action(&invalid);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  require(rejected, "User conversion dialog accepted a non-checkable overwrite action");

  PromptTestDialog expired(WnnUserDictionary::from_entries({initial}),
                           [](WnnUserDictionary) { return true; });
  auto* transient = new QAction(nullptr);
  transient->setCheckable(true);
  transient->setChecked(true);
  expired.set_overwrite_action(transient);
  delete transient;
  bool fell_back = false;
  QTimer::singleShot(0, [&] {
    QWidget* modal = QApplication::activeModalWidget();
    auto* reading = modal ? modal->findChild<QLineEdit*>(
                                QStringLiteral("wnnUserReading"))
                          : nullptr;
    auto* buttons = modal ? modal->findChild<QDialogButtonBox*>() : nullptr;
    if (!reading || !buttons) {
      if (modal) modal->close();
      return;
    }
    fell_back = reading->toolTip().contains(QStringLiteral("Insert mode"));
    buttons->button(QDialogButtonBox::Cancel)->click();
  });
  require(!expired.prompt(initial).has_value() && fell_back,
          "Expired overwrite action did not fall back safely or Cancel mutated input");
}

void test_invalid_entry_stays_open() {
  PromptTestDialog dialog(WnnUserDictionary::from_entries({}),
                          [](WnnUserDictionary) { return true; });
  bool empty_error_verified = false;
  bool empty_fields_retained = false;
  QTimer::singleShot(0, [&] {
    QWidget* modal = QApplication::activeModalWidget();
    auto* reading = modal ? modal->findChild<QLineEdit*>(
                                QStringLiteral("wnnUserReading"))
                          : nullptr;
    auto* candidates = modal ? modal->findChild<QLineEdit*>(
                                   QStringLiteral("wnnUserCandidates"))
                             : nullptr;
    auto* buttons = modal ? modal->findChild<QDialogButtonBox*>() : nullptr;
    if (!reading || !candidates || !buttons) {
      if (modal) modal->close();
      return;
    }
    reading->setText(QStringLiteral("かな"));
    candidates->clear();
    QTimer::singleShot(0, [&] {
      auto* error = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
      if (!error ||
          error->objectName() != QStringLiteral("wnnUserEntryError")) {
        if (error) error->reject();
        return;
      }
      empty_error_verified =
          error->text().contains(QStringLiteral("Candidates cannot be empty"));
      auto* entry_dialog = qobject_cast<QDialog*>(error->parentWidget());
      QObject::connect(error, &QDialog::finished, entry_dialog,
                       [&, entry_dialog] {
        auto* retry_reading = entry_dialog->findChild<QLineEdit*>(
            QStringLiteral("wnnUserReading"));
        auto* retry_candidates = entry_dialog->findChild<QLineEdit*>(
            QStringLiteral("wnnUserCandidates"));
        empty_fields_retained =
            retry_reading && retry_candidates &&
            retry_reading->text() == QStringLiteral("かな") &&
            retry_candidates->text().isEmpty();
        entry_dialog->reject();
      }, Qt::QueuedConnection);
      error->button(QMessageBox::Ok)->click();
    });
    buttons->button(QDialogButtonBox::Ok)->click();
  });
  require(!dialog.prompt(std::nullopt).has_value() && empty_error_verified &&
              empty_fields_retained,
          "Empty candidate validation closed the editor or lost fields");

  bool reading_error_verified = false;
  bool reading_fields_retained = false;
  QTimer::singleShot(0, [&] {
    QWidget* modal = QApplication::activeModalWidget();
    auto* reading = modal ? modal->findChild<QLineEdit*>(
                                QStringLiteral("wnnUserReading"))
                          : nullptr;
    auto* candidates = modal ? modal->findChild<QLineEdit*>(
                                   QStringLiteral("wnnUserCandidates"))
                             : nullptr;
    auto* buttons = modal ? modal->findChild<QDialogButtonBox*>() : nullptr;
    if (!reading || !candidates || !buttons) {
      if (modal) modal->close();
      return;
    }
    reading->setText(QStringLiteral("日本"));
    candidates->setText(QStringLiteral("候補"));
    QTimer::singleShot(0, [&] {
      auto* error = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
      if (!error ||
          error->objectName() != QStringLiteral("wnnUserEntryError")) {
        if (error) error->reject();
        return;
      }
      reading_error_verified =
          error->text().contains(QStringLiteral("only hiragana"));
      auto* entry_dialog = qobject_cast<QDialog*>(error->parentWidget());
      QObject::connect(error, &QDialog::finished, entry_dialog,
                       [&, entry_dialog] {
        auto* retry_reading = entry_dialog->findChild<QLineEdit*>(
            QStringLiteral("wnnUserReading"));
        auto* retry_candidates = entry_dialog->findChild<QLineEdit*>(
            QStringLiteral("wnnUserCandidates"));
        reading_fields_retained =
            retry_reading && retry_candidates &&
            retry_reading->text() == QStringLiteral("日本") &&
            retry_candidates->text() == QStringLiteral("候補");
        entry_dialog->reject();
      }, Qt::QueuedConnection);
      error->button(QMessageBox::Ok)->click();
    });
    buttons->button(QDialogButtonBox::Ok)->click();
  });
  require(!dialog.prompt(std::nullopt).has_value() &&
              reading_error_verified && reading_fields_retained,
          "Non-hiragana validation closed the editor or lost fields");

  bool inflection_error_verified = false;
  bool inflection_fields_retained = false;
  QTimer::singleShot(0, [&] {
    QWidget* modal = QApplication::activeModalWidget();
    auto* reading = modal ? modal->findChild<QLineEdit*>(
                                QStringLiteral("wnnUserReading"))
                          : nullptr;
    auto* candidates = modal ? modal->findChild<QLineEdit*>(
                                   QStringLiteral("wnnUserCandidates"))
                             : nullptr;
    auto* inflection = modal ? modal->findChild<QComboBox*>(
                                   QStringLiteral("wnnUserInflection"))
                             : nullptr;
    auto* buttons = modal ? modal->findChild<QDialogButtonBox*>() : nullptr;
    if (!reading || !candidates || !inflection || !buttons) {
      if (modal) modal->close();
      return;
    }
    reading->setText(QStringLiteral("か"));
    candidates->setText(QStringLiteral("候補"));
    inflection->setCurrentIndex(inflection->findData(
        static_cast<int>(jwpqt::core::WnnUserInflection::kGodan)));
    QTimer::singleShot(0, [&] {
      auto* error = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
      if (!error ||
          error->objectName() != QStringLiteral("wnnUserEntryError")) {
        if (error) error->reject();
        return;
      }
      inflection_error_verified = error->text().contains(
          QStringLiteral("at least two kana"));
      auto* entry_dialog = qobject_cast<QDialog*>(error->parentWidget());
      QObject::connect(error, &QDialog::finished, entry_dialog,
                       [&, entry_dialog] {
        auto* retry_reading = entry_dialog->findChild<QLineEdit*>(
            QStringLiteral("wnnUserReading"));
        auto* retry_candidates = entry_dialog->findChild<QLineEdit*>(
            QStringLiteral("wnnUserCandidates"));
        auto* retry_buttons = entry_dialog->findChild<QDialogButtonBox*>();
        inflection_fields_retained =
            retry_reading && retry_candidates && retry_buttons &&
            retry_reading->text() == QStringLiteral("か") &&
            retry_candidates->text() == QStringLiteral("候補");
        if (!inflection_fields_retained) {
          entry_dialog->reject();
          return;
        }
        retry_reading->setText(QStringLiteral("かく"));
        retry_candidates->setText(QStringLiteral("書く"));
        retry_buttons->button(QDialogButtonBox::Ok)->click();
      }, Qt::QueuedConnection);
      error->button(QMessageBox::Ok)->click();
    });
    buttons->button(QDialogButtonBox::Ok)->click();
  });
  const std::optional<WnnUserEntry> accepted = dialog.prompt(std::nullopt);
  require(inflection_error_verified && inflection_fields_retained &&
              accepted.has_value() &&
              accepted->reading == jwpqt::core::encode_jwp_text(U"かく") &&
              accepted->ending == 'k' &&
              accepted->candidates == std::vector<JwpText>{
                                          jwpqt::core::encode_jwp_text(U"書")},
          "Invalid inflection closed the editor or lost fields");
}

void test_insert_exception_is_contained() {
  const WnnUserEntry first = entry({0x2422}, {0x3021});
  WnnUserDictionaryDialog dialog(
      WnnUserDictionary::from_entries({first}),
      [](WnnUserDictionary) { return true; },
      [](const WnnUserEntry&) { throw std::runtime_error("insert exploded"); });
  QListWidget* list =
      dialog.findChild<QListWidget*>(QStringLiteral("wnnUserEntries"));
  QLabel* status =
      dialog.findChild<QLabel*>(QStringLiteral("wnnUserStatus"));
  require(list != nullptr && status != nullptr,
          "Insert failure fixture has no dialog controls");
  list->setCurrentRow(0);
  require(!dialog.insert_selected() &&
              status->text().contains(QStringLiteral("insert exploded")) &&
              dialog.entries() == std::vector<WnnUserEntry>{first},
          "Insert callback failure escaped or changed the working dictionary");
}

}  // namespace

int main(int argc, char** argv) {
  QApplication application(argc, argv);
  try {
    test_editing_and_save();
    test_import_insert_and_failed_save();
    test_title_bar_close();
    test_import_command();
    test_multi_file_import_drop();
    test_invalid_edit_is_atomic();
    test_imported_inflection_round_trip();
    test_japanese_entry_fields();
    test_invalid_entry_stays_open();
    test_insert_exception_is_contained();
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << "wnn_user_dictionary_dialog_test: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
