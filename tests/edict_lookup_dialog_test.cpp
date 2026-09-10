// SPDX-License-Identifier: GPL-2.0-or-later

#include "edict_lookup_dialog.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDialogButtonBox>
#include <QLabel>
#include <QInputMethodEvent>
#include <QEventLoop>
#include <QIntValidator>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>

#include "jwpqt/core/jwp_text_codec.h"
#include "jwp_editor.h"
#include "kana_input_field.h"
#include "text_bridge.h"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

jwpqt::qt::EdictResourceSearchResult result(
    std::size_t registry_index, QString label, std::u32string headword,
    std::vector<std::u32string> readings,
    std::vector<std::u32string> definitions) {
  jwpqt::core::EdictRecord record;
  record.headword = std::move(headword);
  record.readings = std::move(readings);
  record.definitions = std::move(definitions);
  jwpqt::core::EdictSearchResult found;
  found.record = std::move(record);
  return {registry_index, std::move(label), std::move(found)};
}

void test_names_and_clipboard() {
  namespace qt = jwpqt::qt;
  auto options = std::make_shared<qt::EdictLookupOptions>();
  options->advanced = options->contingent = true;
  options->link_advanced_names = true;
  int calls = 0, changed = 0;
  qt::EdictLookupOptions request;
  bool forced = true;
  qt::EdictLookupDialog dialog([&](const auto&, const auto& settings, bool force) {
    ++calls; request = settings; forced = force;
    qt::EdictResourceSearchReport report;
    report.results = {result(0, QStringLiteral("Main"), U"cat", {}, {U"feline"})};
    return report;
  }, {}, nullptr, {}, options);
  dialog.set_options_changed_handler([&](const auto&) { ++changed; });
  dialog.set_query(U"cat");
  dialog.findChild<QPushButton*>("edictNamesSearch")->click();
  require(calls == 1 && request.personal_names && request.place_names && !request.advanced &&
              !request.contingent && !forced && options->advanced && options->contingent &&
              !options->personal_names && !options->place_names && changed == 0,
          "One-shot Names changed saved options or retained adaptive/contingent retries");
  auto* no_names = dialog.findChild<QCheckBox*>("edictNoNames");
  require(no_names->checkState() == Qt::Checked, "No Names did not represent both exclusions");
  no_names->click();
  require(options->personal_names && options->place_names && !options->advanced && changed == 1 && calls == 1,
          "Combined No Names did not update both categories and Advanced without searching");
  options->place_names = false; dialog.set_options(*options);
  require(no_names->checkState() == Qt::PartiallyChecked, "Mixed names settings were silently normalized");
  no_names->click();
  require(options->personal_names && options->place_names && no_names->checkState() == Qt::Unchecked,
          "Clicking mixed No Names failed to include both categories");
  no_names->click();
  require(!options->personal_names && !options->place_names, "No Names did not exclude both categories");

  QApplication::clipboard()->setText(QStringLiteral("dog\nignored"));
  require(dialog.search_clipboard() && dialog.query() == U"dog" && calls == 2,
          "Explicit clipboard search did not use the first paragraph");
  auto* results = dialog.findChild<QTextEdit*>("edictResults");
  const QPointer<QTextDocument> previous(results->document());
  for (const QString& invalid : {QString(), QStringLiteral("ca"), QStringLiteral("a\u3042"), QString(101, QLatin1Char('a')),
       qt::to_qstring(U"\U0001f600"), QString(QChar(0xd800))}) {
    QApplication::clipboard()->setText(invalid);
    require(!dialog.search_clipboard() && dialog.query() == U"dog" && calls == 2 &&
                results->document() == previous, "Invalid clipboard text erased the query/results");
  }
  auto wait = [] {
    QEventLoop loop;
    QTimer::singleShot(220, &loop, &QEventLoop::quit);
    loop.exec();
  };
  QApplication::clipboard()->setText(QStringLiteral("existing"));
  dialog.show(); QApplication::processEvents();
  auto* monitor = dialog.findChild<QCheckBox*>("edictMonitorClipboard");
  monitor->click(); wait();
  require(calls == 2 && dialog.query() == U"dog", "Enabling monitoring consumed the existing clipboard");
  QApplication::clipboard()->setText(QStringLiteral("first"));
  QApplication::clipboard()->setText(QStringLiteral("latest"));
  wait();
  require(calls == 3 && dialog.query() == U"latest", "Clipboard changes did not coalesce to the latest query");
  dialog.copy_selected(); wait();
  require(calls == 3 && dialog.query() == U"latest", "Lookup's own Copy triggered clipboard monitoring");
  auto* query = dialog.findChild<QLineEdit*>("edictQuery");
  query->setCursorPosition(query->text().size());
  QApplication::clipboard()->setText(QStringLiteral("before typing"));
  QKeyEvent pending(QEvent::KeyPress, Qt::Key_K, Qt::NoModifier, QStringLiteral("k"));
  QApplication::sendEvent(query, &pending); wait();
  require(calls == 3 && dialog.query() == U"latest", "Clipboard monitoring discarded pending kana input");
  QKeyEvent finish(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, QStringLiteral("a"));
  QApplication::sendEvent(query, &finish);
  require(dialog.query() == U"latest\u304b", "Cancelling clipboard search lost the kana composer");
  QApplication::clipboard()->setText(QStringLiteral("obsolete"));
  dialog.set_query(U"edited"); wait();
  require(calls == 3 && dialog.query() == U"edited", "Queued clipboard replaced a newer user edit");
  QApplication::clipboard()->setText(QStringLiteral("disabled"));
  monitor->click(); wait();
  require(calls == 3 && dialog.query() == U"edited", "Disabling monitor failed to cancel its queued search");
  monitor->click();
  QApplication::clipboard()->setText(QStringLiteral("hidden"));
  dialog.hide(); wait();
  require(calls == 3, "Hidden lookup searched queued clipboard content");
  dialog.show(); wait();
  require(calls == 3, "Reopening lookup consumed old clipboard content");
  QApplication::clipboard()->setText(QStringLiteral("hide and reopen"));
  dialog.hide(); dialog.show(); wait();
  require(calls == 3, "Briefly hiding lookup retained a stale clipboard request");
  QDialog modal(&dialog); modal.setModal(true); modal.show(); QApplication::processEvents();
  QApplication::clipboard()->setText(QStringLiteral("modal")); wait(); modal.hide();
  require(calls == 3, "Clipboard monitoring interrupted modal interaction");
  dialog.close();

  QPointer<qt::EdictLookupDialog> doomed;
  doomed = new qt::EdictLookupDialog([&](const auto&, const auto&, bool) {
    delete doomed.data();
    return qt::EdictResourceSearchReport{};
  });
  QApplication::clipboard()->setText(QStringLiteral("delete"));
  require(!doomed->search_clipboard() && !doomed, "Clipboard search retained a deleted owner");
}

void test_management_commands() {
  namespace qt = jwpqt::qt;
  QAction options(nullptr), user(nullptr);
  int options_calls = 0, user_calls = 0;
  QObject::connect(&options, &QAction::triggered, [&] { ++options_calls; });
  QObject::connect(&user, &QAction::triggered, [&] { ++user_calls; });
  qt::EdictLookupDialog* current = nullptr;
  qt::EdictLookupDialog dialog([&](const auto&, const auto&, bool) {
    current->findChild<QToolButton*>("edictOptions")->click();
    current->findChild<QToolButton*>("edictUserDictionary")->click();
    qt::EdictResourceSearchReport report;
    report.results = {result(0, QStringLiteral("Main"), U"cat", {}, {U"feline"})};
    return report;
  });
  current = &dialog;
  auto* options_button = dialog.findChild<QToolButton*>("edictOptions");
  auto* user_button = dialog.findChild<QToolButton*>("edictUserDictionary");
  require(options_button && user_button && !options_button->isEnabled() && !user_button->isEnabled(),
          "Standalone lookup offered unavailable management commands");
  dialog.set_management_actions(&options, &user);
  options_button->click(); user_button->click();
  require(options_calls == 1 && user_calls == 1, "Lookup management did not use supplied actions");
  user.setEnabled(false); user_button->click();
  require(!user_button->isEnabled() && user_calls == 1, "Lookup ignored disabled user dictionary action");
  user.setEnabled(true);
  dialog.set_query(U"cat");
  require(dialog.search() && options_calls == 1 && user_calls == 1,
          "Management command ran inside an active search");
  auto* results = dialog.findChild<QTextEdit*>("edictResults");
  const QPointer<QTextDocument> document(results->document());
  const int position = results->textCursor().position(), anchor = results->textCursor().anchor();
  options_button->click(); user_button->click();
  require(options_calls == 2 && user_calls == 2 && document && results->document() == document &&
              results->textCursor().position() == position && results->textCursor().anchor() == anchor &&
              dialog.query() == U"cat", "Management altered current query or result selection");
  auto* temporary = new QAction(nullptr);
  dialog.set_management_actions(temporary, &user);
  delete temporary;
  require(!options_button->isEnabled(), "Deleted management action left its command enabled");
  dialog.set_management_actions(&options, &user);
  options_button->click();
  require(options_calls == 3, "Rebinding management actions retained old callbacks");
  QAction close_owner(nullptr);
  QPointer<qt::EdictLookupDialog> disposable = new qt::EdictLookupDialog({});
  QObject::connect(&close_owner, &QAction::triggered, [&] { delete disposable.data(); });
  disposable->set_management_actions(&close_owner, nullptr);
  disposable->findChild<QToolButton*>("edictOptions")->click();
  require(!disposable, "Management callback could not safely dispose its lookup");
}

void test_result_keyboard_commands() {
  namespace qt = jwpqt::qt;
  int searches = 0, inserts = 0;
  bool reject_insert = false;
  std::u32string inserted;
  qt::EdictLookupDialog dialog([&](const auto&, const auto&, bool) {
    ++searches;
    qt::EdictResourceSearchReport report;
    report.results = {
        result(0, QStringLiteral("Main"), U"cat", {}, {U"feline"}),
        result(0, QStringLiteral("Main"), U"\u72ac", {U"\u3044\u306c"},
               {U"dog"})};
    return report;
  }, [&](const std::u32string& text) {
    ++inserts; inserted = text; return !reject_insert;
  });
  dialog.set_query(U"cat");
  dialog.show();
  require(dialog.search(), "Could not prepare result keyboard commands");
  auto* results = dialog.findChild<QTextEdit*>("edictResults");
  auto* query = dialog.findChild<QLineEdit*>("edictQuery");
  auto* field = dynamic_cast<qt::KanaInputField*>(query->parentWidget());
  require(field, "Result typing has no local input field");
  const QPointer<QTextDocument> document(results->document());
  const int position = results->textCursor().position(), anchor = results->textCursor().anchor();
  auto key = [&](int code, Qt::KeyboardModifiers modifiers, const QString& text = {}) {
    QKeyEvent event(QEvent::KeyPress, code, modifiers, text);
    QApplication::sendEvent(results, &event);
  };
  QKeyEvent override(QEvent::ShortcutOverride, Qt::Key_Return, Qt::NoModifier);
  override.ignore();
  QApplication::sendEvent(results, &override);
  require(override.isAccepted(), "Result Return did not own its shortcut");
  key(Qt::Key_Return, Qt::NoModifier);
  require(inserts == 1 && searches == 1 && inserted == U"cat /feline/",
          "Result Return searched or inserted display text instead of the canonical entry");
  key(Qt::Key_Enter, Qt::ShiftModifier);
  require(inserts == 2 && searches == 1, "Keypad/Shift Enter did not insert from results");
  reject_insert = true;
  key(Qt::Key_Return, Qt::NoModifier);
  require(inserts == 3 && searches == 1 && results->document() == document &&
              results->textCursor().position() == position && results->textCursor().anchor() == anchor,
          "Failed keyboard insertion damaged result selection or searched again");
  QTextCursor empty = results->textCursor(); empty.clearSelection(); results->setTextCursor(empty);
  key(Qt::Key_Return, Qt::NoModifier);
  require(inserts == 3 && searches == 1 && dialog.focusWidget() == query,
          "Unselected result Enter did not return to the query without searching");
  QTextCursor selected(results->document()); selected.select(QTextCursor::Document); results->setTextCursor(selected);
  query->setText(QStringLiteral("draft")); query->setCursorPosition(5);
  results->setFocus();
  key(Qt::Key_K, Qt::NoModifier, QStringLiteral("k"));
  require(query->text() == QStringLiteral("draft") && dialog.focusWidget() == query,
          "Result typing failed to focus the query or prematurely flushed kana");
  key(Qt::Key_A, Qt::NoModifier, QStringLiteral("a"));
  require(query->text() == QStringLiteral("draft\u304b") && searches == 1 && results->document() == document,
          "Result typing did not use local kana composition");
  field->set_input_mode(qt::InputMode::kAscii);
  query->setText(QStringLiteral("ab")); query->setSelection(0, 1);
  key(Qt::Key_X, Qt::ShiftModifier, QStringLiteral("X"));
  require(query->text() == QStringLiteral("Xb"), "Result typing ignored local ASCII selection replacement");
  query->undo();
  require(query->text() == QStringLiteral("ab"), "Forwarded typing did not retain local query undo");
  field->set_input_mode(qt::InputMode::kJascii);
  query->clear();
  key(Qt::Key_A, Qt::NoModifier, QStringLiteral("a"));
  require(query->text() == QStringLiteral("\uff41"), "Result typing ignored local JASCII mode");
  query->setReadOnly(true);
  key(Qt::Key_X, Qt::NoModifier, QStringLiteral("x"));
  require(query->text() == QStringLiteral("\uff41"), "Result typing changed a read-only query");
  query->setReadOnly(false); field->set_input_mode(qt::InputMode::kAscii);
  query->setText(QStringLiteral("kept"));
  results->selectAll();
  key(Qt::Key_C, Qt::ControlModifier, QStringLiteral("c"));
  require(query->text() == QStringLiteral("kept") && !QApplication::clipboard()->text().isEmpty(),
          "Result typing stole native Copy");
  QTextCursor dog(results->document());
  dog.setPosition(results->toPlainText().indexOf(QStringLiteral("\u72ac")));
  results->setTextCursor(dog);
  key(Qt::Key_C, Qt::ControlModifier);
  require(QApplication::clipboard()->text() == QStringLiteral("\u72ac [\u3044\u306c] /dog/") &&
              !results->textCursor().hasSelection() &&
              QApplication::clipboard()->mimeData()->property("jwpqtInternalCopy").toBool(),
          "No-selection Ctrl+C did not copy the current canonical result row");
  QApplication::clipboard()->setText(QStringLiteral("changed"));
  key(Qt::Key_Insert, Qt::ControlModifier);
  require(QApplication::clipboard()->text() == QStringLiteral("\u72ac [\u3044\u306c] /dog/"),
          "Ctrl+Insert did not copy the current canonical result row");
  key(Qt::Key_J, Qt::ControlModifier);
  require(field->input_mode() == qt::InputMode::kJascii,
          "Ctrl+J did not select local JASCII input mode");
  key(Qt::Key_K, Qt::ControlModifier);
  require(field->input_mode() == qt::InputMode::kKanji,
          "Ctrl+K did not select local Kanji input mode");
  key(Qt::Key_6, Qt::ControlModifier);
  require(field->input_mode() == qt::InputMode::kAscii,
          "Ctrl+6 did not toggle local Kanji/ASCII input mode");
  key(Qt::Key_K, Qt::ControlModifier);
  key(Qt::Key_A, Qt::ControlModifier);
  dialog.copy_selected();
  require(QApplication::clipboard()->text() ==
              QStringLiteral("cat /feline/\n\u72ac [\u3044\u306c] /dog/"),
          "Ctrl+A did not select all canonical result rows");
  QTextCursor clear_rows = dog;
  clear_rows.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
  results->setTextCursor(clear_rows);
  results->setTextCursor(dog);
  QKeyEvent field_override(QEvent::ShortcutOverride, Qt::Key_E,
                           Qt::ControlModifier);
  field_override.ignore();
  QApplication::sendEvent(results, &field_override);
  require(field_override.isAccepted(),
          "Dictionary field Copy did not own its shortcut");
  key(Qt::Key_E, Qt::ControlModifier);
  require(QApplication::clipboard()->text() == QStringLiteral("\u72ac"),
          "Ctrl+E did not copy the current headword");
  require(QApplication::clipboard()->mimeData()->property("jwpqtInternalCopy").toBool(),
          "Dictionary field Copy was not marked as an internal clipboard change");
  require(!results->textCursor().hasSelection(),
          "Ctrl+E changed the result selection");
  key(Qt::Key_R, Qt::ControlModifier);
  require(QApplication::clipboard()->text() == QStringLiteral("\u3044\u306c"),
          "Ctrl+R did not copy the current reading");
  key(Qt::Key_W, Qt::ControlModifier);
  require(results->textCursor().selectedText() == QStringLiteral("\u72ac"),
          "Ctrl+W did not select the current headword");
  key(Qt::Key_W, Qt::ControlModifier | Qt::ShiftModifier);
  dialog.copy_selected();
  require(QApplication::clipboard()->text() ==
              QStringLiteral("\u72ac [\u3044\u306c]\ndog"),
          "Ctrl+Shift+W did not select the complete current result");
  reject_insert = false;
  key(Qt::Key_Return, Qt::NoModifier);
  require(inserted == U"\u72ac [\u3044\u306c] /dog/",
          "Field selection changed canonical result insertion");
  QTextCursor cat(results->document());
  cat.setPosition(results->toPlainText().indexOf(QStringLiteral("cat")));
  results->setTextCursor(cat);
  key(Qt::Key_R, Qt::ControlModifier);
  require(QApplication::clipboard()->text() == QStringLiteral("cat"),
          "Reading Copy did not fall back to a reading-only headword");

  key(Qt::Key_Space, Qt::NoModifier);
  results->setTextCursor(dog);
  key(Qt::Key_Space, Qt::ControlModifier);
  dialog.copy_selected();
  require(QApplication::clipboard()->text() ==
              QStringLiteral("cat /feline/\n\u72ac [\u3044\u306c] /dog/"),
          "Disjoint result rows were not copied in display order");
  require(QApplication::clipboard()->mimeData()->property("jwpqtInternalCopy").toBool(),
          "Disjoint result Copy was not marked as an internal clipboard change");
  require(results->extraSelections().size() >= 2,
          "Disjoint result rows were not visibly selected");
  const QPointer<QTextDocument> selected_document(results->document());
  QEvent palette_change(QEvent::PaletteChange);
  QApplication::sendEvent(results, &palette_change);
  require(selected_document && results->document() == selected_document &&
              results->extraSelections().size() >= 2,
          "Palette refresh discarded the disjoint result selection");
  key(Qt::Key_Return, Qt::NoModifier);
  require(inserted == U"cat /feline/\n\u72ac [\u3044\u306c] /dog/",
          "Disjoint row insertion lost canonical result identity");

  key(Qt::Key_Space, Qt::ControlModifier);
  dialog.copy_selected();
  require(QApplication::clipboard()->text() == QStringLiteral("cat /feline/"),
          "Ctrl+Space did not toggle the current result row");
  const QPoint dog_point = results->cursorRect(dog).center();
  QMouseEvent dog_click(QEvent::MouseButtonPress, QPointF(dog_point),
      QPointF(results->viewport()->mapToGlobal(dog_point)), Qt::LeftButton,
      Qt::LeftButton, Qt::ControlModifier);
  QApplication::sendEvent(results->viewport(), &dog_click);
  dialog.copy_selected();
  require(dog_click.isAccepted() && QApplication::clipboard()->text() ==
              QStringLiteral("cat /feline/\n\u72ac [\u3044\u306c] /dog/"),
          "Ctrl+left did not toggle a disjoint result row");
  key(Qt::Key_W, Qt::ControlModifier);
  dialog.copy_selected();
  require(QApplication::clipboard()->text() == QStringLiteral("\u72ac"),
          "Ordinary within-result selection did not clear disjoint rows");

  key(Qt::Key_unknown, Qt::NoModifier);
  const auto before = query->text();
  key(Qt::Key_Left, Qt::NoModifier);
  require(query->text() == before && searches == 1, "Result navigation changed or searched the query");
  auto* insert_button = dialog.findChild<QPushButton*>("edictInsert");
  QTextCursor row_cursor(results->document());
  row_cursor.setPosition(results->toPlainText().indexOf(QStringLiteral("cat")));
  results->setTextCursor(row_cursor);
  key(Qt::Key_Space, Qt::NoModifier);
  require(insert_button->isEnabled(), "Row selection did not enable canonical insertion");
  require(dialog.sort_results(), "Could not sort a row-selected result set");
  QTextCursor collapsed = results->textCursor();
  collapsed.clearSelection();
  results->setTextCursor(collapsed);
  require(!insert_button->isEnabled(), "Sorting retained stale selected result rows");
  key(Qt::Key_Space, Qt::NoModifier);
  require(dialog.search() && searches == 2, "Could not replace row-selected search results");
  collapsed = results->textCursor();
  collapsed.clearSelection();
  results->setTextCursor(collapsed);
  require(!insert_button->isEnabled(), "A new search retained stale selected result rows");
  QPointer<qt::EdictLookupDialog> disposable;
  disposable = new qt::EdictLookupDialog([](const auto&, const auto&, bool) {
    qt::EdictResourceSearchReport report;
    report.results = {result(0, {}, U"cat", {}, {U"feline"})}; return report;
  }, [&](const auto&) { delete disposable.data(); return false; });
  disposable->set_query(U"cat");
  require(disposable->search(), "Could not prepare deleting insertion callback");
  QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
  QApplication::sendEvent(disposable->findChild<QTextEdit*>("edictResults"), &enter);
  require(!disposable, "Result insertion callback did not safely delete its owner");

  qt::EdictLookupDialog empty_dialog([](const auto&, const auto&, bool) {
    return qt::EdictResourceSearchReport{};
  });
  auto* empty_results = empty_dialog.findChild<QTextEdit*>("edictResults");
  auto* empty_query = empty_dialog.findChild<QLineEdit*>("edictQuery");
  auto* empty_field = empty_query == nullptr ? nullptr
      : dynamic_cast<qt::KanaInputField*>(empty_query->parentWidget());
  require(empty_results && empty_field, "Empty result input fixture is incomplete");
  QKeyEvent ascii_empty(QEvent::KeyPress, Qt::Key_A, Qt::ControlModifier);
  QApplication::sendEvent(empty_results, &ascii_empty);
  require(ascii_empty.isAccepted() && empty_field->input_mode() == qt::InputMode::kAscii,
          "Ctrl+A did not select ASCII input mode for an empty result list");
}

void test_linked_names() {
  namespace qt = jwpqt::qt;
  int searches = 0, notifications = 0;
  qt::EdictLookupOptions seen;
  auto history = std::make_shared<jwpqt::core::QueryHistory>();
  qt::EdictLookupDialog dialog([&](const auto&, const auto& options, bool) {
    ++searches; seen = options;
    qt::EdictResourceSearchReport report;
    report.results = {result(0, {}, U"cat", {}, {U"feline"})}; return report;
  }, {}, nullptr, {}, {}, history);
  dialog.set_options_changed_handler([&](const auto& options) { ++notifications; seen = options; });
  dialog.set_query(U"cat");
  require(dialog.search(), "Could not prepare linked-name policy fixture");
  auto* results = dialog.findChild<QTextEdit*>("edictResults");
  const QPointer<QTextDocument> document(results->document());
  const int anchor = results->textCursor().anchor(), position = results->textCursor().position();
  auto* query = dialog.findChild<QLineEdit*>("edictQuery");
  auto* advanced = dialog.findChild<QCheckBox*>("edictAdvanced");
  auto* personal = dialog.findChild<QCheckBox*>("edictPersonalNames");
  auto* places = dialog.findChild<QCheckBox*>("edictPlaceNames");
  auto* always = dialog.findChild<QCheckBox*>("edictAdvancedAlways");
  auto options = seen;
  options.link_advanced_names = true;
  options.advanced = true; options.personal_names = true; options.place_names = true;
  dialog.set_options(options);
  require(advanced->isChecked() && personal->isChecked() && places->isChecked() && notifications == 0,
          "Imported linked-name settings were silently normalized");
  advanced->setChecked(false);
  query->setCursorPosition(query->text().size());
  QKeyEvent pending(QEvent::KeyPress, Qt::Key_K, Qt::NoModifier, QStringLiteral("k"));
  QApplication::sendEvent(query, &pending);
  advanced->setChecked(true);
  require(notifications == 2 && seen.advanced && !seen.personal_names && !seen.place_names &&
              !personal->isChecked() && !places->isChecked() && always->parentWidget()->isEnabled(),
          "Enabling linked Advanced did not atomically exclude both name categories");
  personal->setChecked(true);
  require(notifications == 3 && !seen.advanced && seen.personal_names && !seen.place_names &&
              !advanced->isChecked() && !always->parentWidget()->isEnabled(),
          "Including personal names did not disable linked Advanced");
  advanced->setChecked(true); places->setChecked(true);
  require(notifications == 5 && !seen.advanced && !seen.personal_names && seen.place_names,
          "Including place names did not disable linked Advanced");
  require(searches == 1 && document && results->document() == document &&
              results->textCursor().anchor() == anchor && results->textCursor().position() == position &&
              history->entries() == std::vector<std::u32string>{U"cat"} && query->text() == QStringLiteral("cat"),
          "Linked controls changed results, history or pending input");
  QKeyEvent finish(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, QStringLiteral("a"));
  QApplication::sendEvent(query, &finish);
  require(query->text() == QStringLiteral("cat\u304b"), "Linked controls discarded pending kana");
  options = seen; options.link_advanced_names = false; options.personal_names = true; options.place_names = true;
  dialog.set_options(options); advanced->setChecked(true);
  require(seen.advanced && seen.personal_names && seen.place_names,
          "Disabled linkage still coupled Advanced and name controls");
  dialog.set_options_changed_handler([&](const auto& changed) {
    auto replacement = changed; replacement.advanced = false; dialog.set_options(replacement);
  });
  advanced->setChecked(false); advanced->setChecked(true);
  require(!advanced->isChecked() && !always->parentWidget()->isEnabled(),
          "A stale toggle overwrote newer reentrant advanced-control state");
}

void test_search_render_status_copy_and_insert() {
  jwpqt::core::JwpText received_query;
  jwpqt::qt::EdictLookupOptions received_options;
  std::u32string inserted;
  jwpqt::qt::EdictLookupDialog dialog(
      [&](const jwpqt::core::JwpText& query,
          const jwpqt::qt::EdictLookupOptions& options, bool) {
        received_query = query;
        received_options = options;
        jwpqt::qt::EdictResourceSearchReport report;
        report.results = {
            result(0, QStringLiteral("Main"), U"\u3042", {U"\u3044"},
                   {U"cat", U"feline"}),
            result(1, QStringLiteral("Names"), U"Alice", {}, {U"name"})};
        report.rejected = 3;
        report.failures = {
            {2, QStringLiteral("quiet"), QStringLiteral("hidden"), true},
            {3, QStringLiteral("broken"), QStringLiteral("unavailable"),
             false}};
        return report;
      },
      [&](const std::u32string& row) {
        inserted = row;
        return true;
      });

  dialog.set_query(U"\u3042");
  auto* personal =
      dialog.findChild<QCheckBox*>(QStringLiteral("edictPersonalNames"));
  auto* place = dialog.findChild<QCheckBox*>(QStringLiteral("edictPlaceNames"));
  auto* classical =
      dialog.findChild<QCheckBox*>(QStringLiteral("edictClassical"));
  require(personal != nullptr && place != nullptr && classical != nullptr,
          "Dictionary lookup options were not created");
  personal->setChecked(true);
  classical->setChecked(true);
  require(dialog.search() && received_query == jwpqt::core::JwpText{0x2422} &&
              received_options.personal_names && !received_options.place_names &&
              received_options.classical,
          "Dictionary search did not validate its query or options");

  auto* list = dialog.findChild<QTextEdit*>(QStringLiteral("edictResults"));
  auto* status = dialog.findChild<QLabel*>(QStringLiteral("edictStatus"));
  require(list != nullptr && status != nullptr && list->isReadOnly() &&
              list->font().pixelSize() == 16 && list->toPlainText() ==
                  QStringLiteral("\u3042 [\u3044]\ncat; feline\nAlice\nname"),
          "Dictionary results were not rendered in search order");
  require(status->text().contains(QStringLiteral("2 matches")) &&
              status->text().contains(QStringLiteral("3 rejected")) &&
              status->text().contains(QStringLiteral("broken: unavailable")) &&
              !status->text().contains(QStringLiteral("hidden")),
          "Dictionary status did not expose the bounded visible diagnostics");

  list->selectAll();
  dialog.copy_selected();
  require(QApplication::clipboard()->text() ==
              list->toPlainText(),
          "Dictionary result copy did not preserve selected text");
  require(dialog.insert_selected() &&
              inserted == U"\u3042 [\u3044] /cat/feline/\nAlice /name/",
          "Dictionary insertion callback did not receive every selected row");
}

void test_priority_presentation() {
  namespace qt = jwpqt::qt;
  namespace core = jwpqt::core;
  auto options = std::make_shared<qt::EdictLookupOptions>();
  options->compact = true;
  auto history = std::make_shared<core::QueryHistory>();
  bool malformed = false;
  std::u32string inserted;
  qt::EdictLookupDialog dialog([&](const auto&, const auto&, bool) {
    qt::EdictResourceSearchReport report;
    report.results = {result(0, "D0", U"d0", {}, {U"ordinary"}),
                      result(1, "P1", U"p1", {}, {U"priority", U"(P)"}),
                      result(2, "A2", U"a2", {}, {U"ordinary"}),
                      result(3, "P3", U"p3", {}, {U"priority", U"(P)"})};
    report.results[1].result.priority = report.results[3].result.priority = true;
    report.sections = {{core::EdictSearchStage::kDirect, 0, true},
                       {core::EdictSearchStage::kAdaptive, malformed ? 99U : 2U}};
    return report;
  }, [&](const auto& text) { inserted = text; return true; }, nullptr, {}, options, history);
  dialog.set_query(U"cat");
  dialog.show();
  require(dialog.search(), "Priority presentation search failed");
  auto* view = dialog.findChild<QTextEdit*>("edictResults");
  auto* query = dialog.findChild<QLineEdit*>("edictQuery");
  require(view->document()->blockCount() == 7 && view->textCursor().selectedText().startsWith("p1") &&
          view->textCursor().charFormat().toolTip() == "P1" &&
          dialog.report().results[0].result.record.headword == U"d0",
          "Presentation lost first visible entry, provenance or original report order");
  view->selectAll();
  require(dialog.insert_selected() && inserted == U"p1 /priority/(P)/\nd0 /ordinary/\np3 /priority/(P)/\na2 /ordinary/",
          "Labels became insertable or entries used backend rather than visible order");
  view->setTextCursor(view->document()->find("End of Priority Entries"));
  const auto before = inserted;
  require(!dialog.insert_selected() && inserted == before,
          "A label-only selection inserted an entry");
  dialog.copy_selected();
  require(QApplication::clipboard()->text() == "End of Priority Entries", "Label Copy differs from displayed text");
  const auto control_click = [&](const QString& target) {
    const auto found = view->document()->find(target);
    QTextCursor position(view->document());
    position.setPosition(found.selectionStart() + 1);
    view->setTextCursor(position);
    view->ensureCursorVisible();
    const QPoint point = view->cursorRect(position).center();
    QMouseEvent event(QEvent::MouseButtonPress, QPointF(point),
        QPointF(view->viewport()->mapToGlobal(point)), Qt::LeftButton,
        Qt::LeftButton, Qt::ControlModifier);
    QApplication::sendEvent(view->viewport(), &event);
    require(event.isAccepted(), "Ctrl+left result selection was not handled");
  };
  control_click("p1");
  control_click("a2");
  control_click("End of Priority Entries");
  dialog.copy_selected();
  require(QApplication::clipboard()->text() ==
              QStringLiteral("p1 /priority/(P)/\na2 /ordinary/"),
          "Presentation labels entered a disjoint result selection");
  bool popup_copy = false;
  QTimer::singleShot(0, [&] {
    auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
    if (!menu) return;
    for (QAction* action : menu->actions()) {
      if (action->objectName() != QStringLiteral("edictCopy")) continue;
      popup_copy = true;
      action->trigger();
      break;
    }
    menu->close();
  });
  const auto p1 = view->document()->find(QStringLiteral("p1"));
  QTextCursor popup_cursor(view->document());
  popup_cursor.setPosition(p1.selectionStart() + 1);
  const QPoint popup_point = view->cursorRect(popup_cursor).center();
  QContextMenuEvent popup(QContextMenuEvent::Mouse, popup_point,
      view->viewport()->mapToGlobal(popup_point));
  QApplication::sendEvent(view->viewport(), &popup);
  require(popup_copy && QApplication::clipboard()->text() ==
              QStringLiteral("p1 /priority/(P)/\na2 /ordinary/"),
          "Result popup omitted disjoint row Copy");
  require(dialog.insert_selected() && inserted == U"p1 /priority/(P)/\na2 /ordinary/",
          "Disjoint presentation selection lost canonical visible order");
  const auto double_click = [&](const QString& target, bool interfere = false) {
    const auto found = view->document()->find(target);
    QTextCursor position(view->document());
    position.setPosition(found.selectionStart() + 1);
    view->setTextCursor(position);
    view->ensureCursorVisible();
    const QPoint point = view->cursorRect(position).center();
    bool armed = interfere;
    const auto connection = QObject::connect(view, &QTextEdit::selectionChanged, view, [&] {
      if (!armed) return;
      armed = false;
      view->setTextCursor(QTextCursor(view->document()));
    });
    QMouseEvent event(QEvent::MouseButtonDblClick, QPointF(point),
        QPointF(view->viewport()->mapToGlobal(point)), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(view->viewport(), &event);
    QObject::disconnect(connection);
  };
  double_click("a2");
  require(inserted == U"a2 /ordinary/", "Double-click used old selection or backend index instead of clicked entry");
  double_click("End of Priority Entries");
  require(inserted == U"a2 /ordinary/", "Double-click on a presentation label inserted an entry");
  double_click("p1", true);
  require(inserted == U"a2 /ordinary/", "Double-click ignored a reentrant selection change");
  view->setTextCursor(view->document()->find("End of Priority Entries"));
  const QPointer<QTextDocument> document = view->document();
  const int start = view->textCursor().selectionStart(), end = view->textCursor().selectionEnd();
  const auto remembered = history->entries();
  malformed = true;
  require(!dialog.search() && view->document() == document && view->textCursor().selectionStart() == start &&
          view->textCursor().selectionEnd() == end && history->entries() == remembered,
          "Malformed presentation destroyed completed results or history");
  malformed = false;
  auto* marker = dialog.findChild<QCheckBox*>("edictAdvancedSeparator");
  marker->setChecked(false);
  require(view->document() == document && query->text() == "cat", "Marker preference reformatted old results");
  require(dialog.search() && view->document()->blockCount() == 5 &&
          !qt::document_plain_text(*view->document()).contains("Advanced"), "Unmarked adaptive layout failed");
  view->selectAll();
  require(dialog.insert_selected() && inserted == U"p1 /priority/(P)/\np3 /priority/(P)/\nd0 /ordinary/\na2 /ordinary/",
          "Shared priority boundary lost visible insertion ordering");
  require(dialog.sort_results() && view->document()->blockCount() == 4 &&
          !qt::document_plain_text(*view->document()).contains("Priority") && dialog.report().sections.empty(),
          "Sort retained presentation labels or invalid phase indices");
  const QPointer<QTextDocument> sorted = view->document();
  options->advanced_separator = true;
  require(dialog.sort_results() && view->document()->blockCount() == 4 && !sorted,
          "Later Sort recreated priority presentation");
  dialog.findChild<QCheckBox*>("edictPriority")->setChecked(false);
  require(dialog.search() && view->textCursor().selectedText().startsWith("d0") &&
          view->document()->blockCount() == 5, "Disabling priority also disabled advanced headings");
  dialog.grab().save(QStringLiteral("dictionary-priority.png"));
}

void test_compact_presentation() {
  using namespace jwpqt;
  qt::EdictLookupDialog* owner = nullptr;
  bool fail = false;
  bool change_policy = false;
  std::u32string inserted;
  qt::EdictLookupDialog dialog(
      [&](const core::JwpText&, const qt::EdictLookupOptions& options, bool) {
        if (fail) throw std::runtime_error("presentation search failed");
        if (change_policy) {
          auto next = options;
          next.compact = false;
          owner->set_options(next);
        }
        qt::EdictResourceSearchReport report;
        report.results = {
            result(4, QStringLiteral("Main source"), U"zeta", {U"\u3042", U"\u3044"},
                   {U"first", U"\ufeff\u00a0\U0001f600", U"(P)", U"EntL123"}),
            result(7, QStringLiteral("Second source"), U"alpha", {}, {U"other"})};
        return report;
      }, [&](const std::u32string& value) { inserted = value; return true; });
  owner = &dialog;
  auto* results = dialog.findChild<QTextEdit*>("edictResults");
  dialog.set_query(U"cat");
  qt::EdictLookupOptions options;
  options.compact = true;
  dialog.set_options(options);
  change_policy = true;
  require(dialog.search(), "Compact search failed");
  const QString expected = QStringLiteral("zeta [\u3042; \u3044] first, \ufeff\u00a0\U0001f600, (P), EntL123\nalpha other");
  require(qt::document_plain_text(*results->document()) == expected && results->document()->blockCount() == 2,
          "Compact results lost scalar content, metadata or inline layout");
  require(results->textCursor().selectedText() == expected.section(QLatin1Char('\n'), 0, 0) &&
              dialog.insert_selected() && inserted == U"zeta [\u3042; \u3044] /first/\ufeff\u00a0\U0001f600/(P)/EntL123/",
          "Compact selection inserted display formatting instead of the full canonical entry");
  results->selectAll();
  QTextEdit reference_copy;
  reference_copy.setPlainText(expected);
  reference_copy.selectAll();
  reference_copy.copy();
  auto clipboard_formats = QApplication::clipboard()->mimeData()->formats();
  clipboard_formats.sort();
  dialog.copy_selected();
  require(QApplication::clipboard()->text() == expected,
          "Compact Copy changed exact Unicode display text");
  auto copied_formats = QApplication::clipboard()->mimeData()->formats();
  copied_formats.sort();
  require(copied_formats == clipboard_formats,
          "Result Copy dropped a native Qt clipboard format");
  QApplication::clipboard()->setText(QStringLiteral("sentinel"));
  QKeyEvent copy_key(QEvent::KeyPress, Qt::Key_C, Qt::ControlModifier, QStringLiteral("c"));
  QApplication::sendEvent(results, &copy_key);
  QKeyEvent release_copy(QEvent::KeyRelease, Qt::Key_C, Qt::NoModifier);
  QApplication::sendEvent(results, &release_copy);
  // Synthetic releases do not update QApplication's cached modifier state.
  QKeyEvent reset_modifiers(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier);
  QApplication::sendEvent(results, &reset_modifiers);
  require(QApplication::keyboardModifiers() == Qt::NoModifier, "Copy fixture left keyboard modifiers active");
  require(QApplication::clipboard()->text() == expected,
          "Native result Copy bypassed exact Unicode clipboard content");
  require(QApplication::clipboard()->mimeData()->hasHtml(), "Result Copy lost its rich-text representation");
  QTextDocument rich_copy;
  rich_copy.setHtml(QApplication::clipboard()->mimeData()->html());
  require(qt::document_plain_text(rich_copy) == expected, "Rich result Copy changed Unicode content");
  require(dialog.insert_selected() &&
              inserted == U"zeta [\u3042; \u3044] /first/\ufeff\u00a0\U0001f600/(P)/EntL123/\nalpha /other/",
          "Compact multi-entry insertion changed canonical content");
  require(dialog.sort_results() && results->document()->blockCount() == 2 && dialog.query() == U"cat",
          "Sort applied a newer layout preference or changed the query");
  auto selected = results->document()->find(QStringLiteral("zeta"));
  results->setTextCursor(selected);
  require(selected.charFormat().toolTip() == QStringLiteral("Main source") && dialog.insert_selected() &&
              inserted == U"zeta [\u3042; \u3044] /first/\ufeff\u00a0\U0001f600/(P)/EntL123/",
          "Compact sorting lost provenance or canonical insertion ownership");
  dialog.resize(780, 560);
  dialog.show();
  QApplication::processEvents();
  require(dialog.grab().save(QStringLiteral("dictionary-compact.png")), "Could not capture compact results");
  const QPointer<QTextDocument> previous(results->document());
  const int position = results->textCursor().position();
  const int anchor = results->textCursor().anchor();
  core::EdictSortLimits limits;
  limits.comparisons = 0;
  fail = true;
  require(!dialog.search() && !dialog.sort_results(Qt::NoModifier, limits) && previous &&
              results->document() == previous && results->textCursor().position() == position &&
              results->textCursor().anchor() == anchor,
          "Failed compact search/sort changed the completed view or selection");
  fail = false;
  change_policy = false;
  require(dialog.search() && results->document()->blockCount() == 4 && previous.isNull(),
          "Next search failed to use the new expanded preference or retained an obsolete document");
  options.compact = true;
  dialog.set_options(options);
  require(dialog.sort_results() && results->document()->blockCount() == 4,
          "Expanded sorting reformatted an already completed search");
  results->selectAll();
  dialog.copy_selected();
  require(QApplication::clipboard()->text() == qt::document_plain_text(*results->document()),
          "Expanded result Copy changed exact Unicode content");
}

void test_empty_invalid_and_failed_search_are_contained() {
  int searches = 0;
  jwpqt::qt::EdictLookupDialog dialog(
      [&](const jwpqt::core::JwpText&,
          const jwpqt::qt::EdictLookupOptions&, bool)
          -> jwpqt::qt::EdictResourceSearchReport {
        ++searches;
        throw std::runtime_error("lookup exploded");
      });
  auto* query = dialog.findChild<QLineEdit*>(QStringLiteral("edictQuery"));
  auto* status = dialog.findChild<QLabel*>(QStringLiteral("edictStatus"));
  require(query != nullptr && status != nullptr && !dialog.search() &&
              searches == 0 &&
              status->text() == QStringLiteral("Enter a search term."),
          "Empty dictionary query reached the search callback");

  query->setText(QString::fromUtf8("\xf0\x9f\x98\x80"));
  require(!dialog.search() && searches == 0 &&
              status->text().startsWith(QStringLiteral("Search failed:")),
          "Unrepresentable dictionary query was not contained before search");

  query->setText(QStringLiteral("cat"));
  require(!dialog.search() && searches == 1 &&
              status->text().contains(QStringLiteral("lookup exploded")),
          "Dictionary search callback failure escaped the dialog");

  jwpqt::qt::EdictLookupDialog unknown_search(
      [](const jwpqt::core::JwpText&,
          const jwpqt::qt::EdictLookupOptions&, bool)
          -> jwpqt::qt::EdictResourceSearchReport { throw 7; });
  auto* unknown_query =
      unknown_search.findChild<QLineEdit*>(QStringLiteral("edictQuery"));
  auto* unknown_status =
      unknown_search.findChild<QLabel*>(QStringLiteral("edictStatus"));
  unknown_query->setText(QStringLiteral("cat"));
  require(!unknown_search.search() &&
              unknown_status->text().contains(QStringLiteral("unknown error")),
          "Unknown dictionary search failure escaped the dialog");

  jwpqt::qt::EdictLookupDialog unknown_insert(
      [](const jwpqt::core::JwpText&,
          const jwpqt::qt::EdictLookupOptions&, bool) {
        jwpqt::qt::EdictResourceSearchReport report;
        report.results = {result(0, QStringLiteral("Main"), U"cat", {},
                                 {U"feline"})};
        return report;
      },
      [](const std::u32string&) -> bool { throw 9; });
  auto* insert_query =
      unknown_insert.findChild<QLineEdit*>(QStringLiteral("edictQuery"));
  auto* insert_status =
      unknown_insert.findChild<QLabel*>(QStringLiteral("edictStatus"));
  insert_query->setText(QStringLiteral("cat"));
  require(unknown_insert.search() && !unknown_insert.insert_selected() &&
              insert_status->text().contains(QStringLiteral("unknown error")),
          "Unknown dictionary insertion failure escaped the dialog");
}

void test_result_sorting() {
  using namespace jwpqt;
  auto history = std::make_shared<core::QueryHistory>();
  int searches = 0;
  bool fail = false;
  bool empty = false;
  std::u32string inserted;
  const auto handler = [&](const core::JwpText&, const qt::EdictLookupOptions&, bool) {
    ++searches;
    if (fail) throw std::runtime_error("failed replacement");
    qt::EdictResourceSearchReport report;
    if (!empty) {
      report.results = {
          result(10, QStringLiteral("Z source"), U"Z", {U"\u3044\u3044"}, {U"alpha"}),
          result(11, QStringLiteral("First B"), U"B", {U"\u3042"}, {U"zulu"}),
          result(12, QStringLiteral("AA source"), U"AA", {U"\u3044"}, {U"beta"}),
          result(13, QStringLiteral("Duplicate B"), U"B", {U"\u3042"}, {U"zulu"})};
      report.results[1].result.record.byte_offset = 9;
      report.results[3].result.record.byte_offset = 77;
    }
    report.rejected = 7;
    report.queries = 4;
    report.failures = {{8, QStringLiteral("source"), QStringLiteral("diagnostic"), false}};
    return report;
  };
  qt::EdictLookupDialog dialog(handler, [&](const std::u32string& text) {
    inserted = text;
    return true;
  }, nullptr, {}, {}, history);
  auto* sort = dialog.findChild<QPushButton*>(QStringLiteral("edictSort"));
  auto* results = dialog.findChild<QTextEdit*>(QStringLiteral("edictResults"));
  auto* query = dialog.findChild<QLineEdit*>(QStringLiteral("edictQuery"));
  auto* status = dialog.findChild<QLabel*>(QStringLiteral("edictStatus"));
  const auto order = [&] {
    std::vector<std::size_t> indices;
    for (const auto& row : dialog.report().results) indices.push_back(row.registry_index);
    return indices;
  };
  require(sort && !sort->isEnabled() && !dialog.sort_results(),
          "Empty dictionary results could be sorted");
  dialog.set_query(U"cat");
  dialog.show();
  require(dialog.search() && sort->isEnabled(), "Sort fixture search failed");
  const auto retained_history = history->entries();
  const QPointer<QTextDocument> previous = results->document();
  sort->click();
  require(previous.isNull() && order() == std::vector<std::size_t>{11, 12, 10} &&
              dialog.report().results.front().result.record.byte_offset == 9 &&
              dialog.report().results.front().label == QStringLiteral("First B") &&
              dialog.report().rejected == 7 && dialog.report().queries == 4 &&
              dialog.report().failures.size() == 1 &&
              status->text().contains(QStringLiteral("Reading order")) &&
              searches == 1 && history->entries() == retained_history,
          "Sort did not deduplicate in reading order while preserving first provenance");
  dialog.copy_selected();
  require(QApplication::clipboard()->text() == QStringLiteral("B [\u3042]\nzulu") &&
              dialog.insert_selected() && inserted == U"B [\u3042] /zulu/" &&
              results->textCursor().charFormat().toolTip() == QStringLiteral("First B"),
          "Sorted selection copied or inserted the wrong record");
  QApplication::processEvents();
  require(dialog.grab().save(QCoreApplication::applicationDirPath() +
                             QStringLiteral("/dictionary-sorted.png")),
          "Could not render sorted dictionary results");
  query->setText(QStringLiteral("\u4e9c"));
  require(dialog.sort_results() && order() == std::vector<std::size_t>{10, 12, 11} &&
              status->text().contains(QStringLiteral("Length order")),
          "Length sorting used an edited query instead of the completed search");
  require(dialog.sort_results() && order() == std::vector<std::size_t>{12, 11, 10} &&
              dialog.sort_results() && order() == std::vector<std::size_t>{10, 12, 11},
          "Sort did not cycle through Entry and Definition");
  require(dialog.sort_results(Qt::ControlModifier) &&
              order() == std::vector<std::size_t>{11, 12, 10} &&
              status->text().contains(QStringLiteral("Definition order (reversed)")) &&
              dialog.sort_results(Qt::ShiftModifier) &&
              order() == std::vector<std::size_t>{10, 11, 12} &&
              status->text().contains(QStringLiteral("Entry order (reversed)")) &&
              dialog.sort_results(Qt::ControlModifier | Qt::ShiftModifier) &&
              order() == std::vector<std::size_t>{12, 11, 10} &&
              status->text().contains(QStringLiteral("Entry order")) &&
              !status->text().contains(QStringLiteral("reversed")),
          "Ctrl/Shift sort precedence, direction or reverse retention changed");
  QTextCursor selected(results->document());
  selected.setPosition(1);
  selected.setPosition(4, QTextCursor::KeepAnchor);
  results->setTextCursor(selected);
  const QPointer<QTextDocument> before_failure = results->document();
  core::EdictSortLimits limits;
  limits.comparisons = 0;
  require(!dialog.sort_results(Qt::NoModifier, limits) && results->document() == before_failure &&
              results->textCursor().position() == 4 && results->textCursor().anchor() == 1 &&
              order() == std::vector<std::size_t>{12, 11, 10} &&
              status->text().startsWith(QStringLiteral("Sort failed:")) &&
              history->entries() == retained_history && searches == 1,
          "Failed sorting changed the result document, selection or query history");
  require(dialog.sort_results(Qt::ControlModifier) &&
              order() == std::vector<std::size_t>{10, 11, 12} &&
              status->text().contains(QStringLiteral("Entry order (reversed)")),
          "A failed sort advanced the mode or reverse state");
  require(dialog.search() && !status->text().contains(QStringLiteral("order")) &&
              order() == std::vector<std::size_t>{10, 11, 12, 13},
          "A new search did not restore ranked results and reset sorting");
  query->setText(QStringLiteral("cat"));
  sort->click();
  require(dialog.sort_results() && order() == std::vector<std::size_t>{12, 11, 10},
          "Completed kanji-query length sorting did not use headword length");
  fail = true;
  const QPointer<QTextDocument> sorted_document = results->document();
  require(!dialog.search() && results->document() == sorted_document &&
              dialog.sort_results(Qt::ControlModifier) &&
              status->text().contains(QStringLiteral("Length order (reversed)")),
          "Failed search reset the successful result ordering");
  fail = false;
  empty = true;
  require(dialog.search() && !sort->isEnabled() && !dialog.sort_results(),
          "Empty replacement results left sorting enabled");
  empty = false;
  dialog.set_query(U"cat");
  require(dialog.search(), "Could not prepare pending query sort");
  query->deselect();
  query->setCursorPosition(query->text().size());
  QKeyEvent pending(QEvent::KeyPress, Qt::Key_N, Qt::NoModifier, QStringLiteral("n"));
  QApplication::sendEvent(query, &pending);
  const int before_sort = searches;
  require(query->text() == QStringLiteral("cat") && dialog.sort_results() &&
              query->text() == QStringLiteral("cat") && searches == before_sort,
          "Sorting flushed pending query input or searched again");
  QKeyEvent complete(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, QStringLiteral("a"));
  QApplication::sendEvent(query, &complete);
  require(query->text() == QStringLiteral("cat\u306a"),
          (QStringLiteral("Sorting discarded pending query composition: ") +
           QString::fromLatin1(query->text().toUtf8().toHex())).toStdString().c_str());
  dialog.set_query(U"cat");
  bool reentered = false;
  bool attempted = false;
  QObject::connect(results, &QTextEdit::selectionChanged, &dialog, [&] {
    if (!attempted) {
      attempted = true;
      reentered = dialog.sort_results() || dialog.search();
      history->remember(U"listener");
    }
  });
  require(dialog.search() && attempted && !reentered && history->find(U"listener") &&
              history->find(U"cat"),
          "Result publication clobbered listener history or permitted reentrant commands");
  attempted = false;
  require(dialog.sort_results(Qt::ShiftModifier) && attempted && !reentered &&
              status->text().contains(QStringLiteral("Definition order")),
          "Shift-first sorting or publication command guards failed");
  require(dialog.search(), "Could not reset repeated reverse test");
  for (int press = 0; press < 25; ++press) {
    require(dialog.sort_results(Qt::ControlModifier) &&
                status->text().contains(QStringLiteral("Reading order")) &&
                status->text().contains(QStringLiteral("reversed")) == (press % 2 == 0),
            "Repeated Ctrl sorting locked the reverse control");
  }
}

void test_search_controls() {
  using namespace jwpqt::qt;
  auto settings = std::make_shared<EdictLookupOptions>();
  EdictLookupOptions received;
  bool forced = false;
  int searches = 0;
  const auto search = [&](const jwpqt::core::JwpText&, const EdictLookupOptions& options, bool force) {
    received = options;
    forced = force;
    ++searches;
    return EdictResourceSearchReport{};
  };
  {
    EdictLookupDialog dialog(search, {}, nullptr, {}, settings);
    const auto box = [&](const char* name) {
      auto* found = dialog.findChild<QCheckBox*>(QString::fromLatin1(name));
      require(found != nullptr, "A dictionary search control is missing");
      return found;
    };
    require(box("edictBeginning")->isChecked() && !box("edictEnd")->isChecked() &&
                !box("edictAdvanced")->isChecked() &&
                box("edictAdvancedAlways")->isChecked() && box("edictIAdjectives")->isChecked() &&
                !box("edictAdvancedShowAll")->isChecked() &&
                !box("edictAdvancedAlways")->isEnabled() && !box("edictIAdjectives")->isEnabled(),
            "Dictionary controls do not use the source defaults and advanced dependencies");
    dialog.set_query(U"cat");
    box("edictBeginning")->click();
    box("edictEnd")->click();
    box("edictAdvanced")->click();
    require(searches == 0 && box("edictAdvancedAlways")->isEnabled() &&
                box("edictIAdjectives")->isEnabled(),
            "Changing dictionary policies started a search or left controls disabled");
    box("edictAdvancedAlways")->click();
    box("edictAdvancedShowAll")->click();
    box("edictIAdjectives")->click();
    box("edictFullAscii")->click();
    box("edictJasciiToAscii")->click();
    require(dialog.search() && !received.require_beginning && received.require_end &&
                received.advanced && !received.advanced_always && received.advanced_show_all &&
                !received.i_adjectives && received.full_ascii && received.jascii_to_ascii,
            "Dictionary controls were not forwarded as a complete search snapshot");
    box("edictAdvanced")->click();
    require(!box("edictAdvancedShowAll")->isEnabled() && settings->advanced_show_all &&
                !settings->advanced && searches == 1,
            "Disabling Advanced discarded its options or searched unexpectedly");
    require(dialog.search(true) && forced && !received.contingent && !settings->contingent,
            "Forced contingent search changed the persisted preference");
    dialog.findChild<QPushButton*>(QStringLiteral("edictSearch"))->click();
    require(!forced && !received.contingent, "One-shot forcing leaked into a normal search");
    const int before = searches;
    box("edictContingent")->click();
    require(searches == before && settings->contingent && dialog.search() &&
                received.contingent && !forced,
            "Contingent preference searched immediately or was not forwarded");
  }
  EdictLookupDialog reopened(search, {}, nullptr, {}, settings);
  reopened.set_query(U"cat");
  require(reopened.findChild<QCheckBox*>(QStringLiteral("edictEnd"))->isChecked() &&
              !reopened.findChild<QCheckBox*>(QStringLiteral("edictBeginning"))->isChecked() &&
               reopened.search() && received.contingent && received.full_ascii && received.jascii_to_ascii &&
              received.advanced_show_all && !received.advanced && !received.advanced_always,
          "Closing the dictionary discarded its retained search policies");
  EdictLookupDialog independent(search);
  independent.set_query(U"cat");
  require(independent.search() && received.require_beginning && !received.require_end &&
              !received.full_ascii && !received.jascii_to_ascii,
          "Independent dictionary state inherited another owner's settings");
}

void test_options_updates() {
  using namespace jwpqt;
  auto options = std::make_shared<qt::EdictLookupOptions>();
  auto history = std::make_shared<core::QueryHistory>();
  int searches = 0;
  int notifications = 0;
  std::u32string inserted;
  const auto search = [&](const core::JwpText&, const qt::EdictLookupOptions&, bool) {
    ++searches;
    qt::EdictResourceSearchReport report;
    report.results = {result(0, QStringLiteral("Main"), U"\u3042", {}, {U"cat"})};
    return report;
  };
  qt::EdictLookupDialog dialog(search, [&](const std::u32string& text) {
    inserted = text;
    return true;
  }, nullptr, {}, options, history);
  dialog.set_options_changed_handler([&](const qt::EdictLookupOptions& value) {
    ++notifications;
    require(value.personal_names, "Option callback did not contain the changed value");
  });
  dialog.set_query(U"cat");
  require(dialog.search(), "Cannot seed live dictionary options test");
  auto* query = dialog.findChild<QLineEdit*>(QStringLiteral("edictQuery"));
  auto* results = dialog.findChild<QTextEdit*>(QStringLiteral("edictResults"));
  const auto old_history = history->entries();
  const QPointer<QTextDocument> document = results->document();
  const int position = results->textCursor().position();
  const int anchor = results->textCursor().anchor();
  query->setCursorPosition(query->text().size());
  QKeyEvent pending(QEvent::KeyPress, Qt::Key_K, Qt::NoModifier, QStringLiteral("k"));
  QApplication::sendEvent(query, &pending);
  auto next = *options;
  next.require_beginning = false;
  next.advanced = true;
  next.full_ascii = true;
  dialog.set_options(next);
  require(!dialog.findChild<QCheckBox*>(QStringLiteral("edictBeginning"))->isChecked() &&
              dialog.findChild<QCheckBox*>(QStringLiteral("edictAdvancedAlways"))->isEnabled() &&
              options->full_ascii && searches == 1 && notifications == 0 &&
              history->entries() == old_history && query->text() == QStringLiteral("cat") &&
              document && results->document() == document &&
              results->textCursor().position() == position && results->textCursor().anchor() == anchor,
          "Applying dictionary options searched, flushed input or replaced existing state");
  dialog.findChild<QCheckBox*>(QStringLiteral("edictPersonalNames"))->click();
  require(notifications == 1 && options->personal_names,
          "A manual dictionary policy did not publish exactly once");
  QKeyEvent finish(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, QStringLiteral("a"));
  QApplication::sendEvent(query, &finish);
  require(query->text() == QStringLiteral("cat\u304b") && dialog.insert_selected() &&
              inserted == U"\u3042 /cat/" && searches == 1,
          "Applying options lost pending kana or canonical result ownership");
  auto* dying = new qt::EdictLookupDialog(search);
  QPointer<qt::EdictLookupDialog> guard = dying;
  dying->set_options_changed_handler([dying](const qt::EdictLookupOptions&) { delete dying; });
  dying->findChild<QCheckBox*>(QStringLiteral("edictPersonalNames"))->setChecked(true);
  require(!guard, "An options callback could not destroy its dialog safely");
}

void test_query_input_modes() {
  int searches = 0;
  jwpqt::core::JwpText received;
  jwpqt::qt::EdictLookupDialog dialog(
      [&](const jwpqt::core::JwpText& query, const jwpqt::qt::EdictLookupOptions&, bool) {
        ++searches;
        received = query;
        return jwpqt::qt::EdictResourceSearchReport{};
      });
  auto* query = dialog.findChild<QLineEdit*>(QStringLiteral("edictQuery"));
  auto* mode = dialog.findChild<QToolButton*>(QStringLiteral("edictQueryMode"));
  require(query && mode && mode->text() == QStringLiteral("K") && query->font().pixelSize() == 16,
          "Dictionary query has no local Japanese input mode");
  const auto key = [&](int code, const QString& text = {}, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent event(QEvent::KeyPress, code, modifiers, text);
    QApplication::sendEvent(query, &event);
  };
  const auto type = [&](const QString& text) {
    for (const QChar character : text) key(character.toUpper().unicode(), QString(character));
  };
  dialog.show();
  query->setFocus();
  QApplication::processEvents();
  type(QStringLiteral("ain"));
  require(query->text() == QStringLiteral("\u3042\u3044"), "Lookup romaji was not composed locally");
  key(Qt::Key_Return);
  require(searches == 1 && received == jwpqt::core::JwpText({0x2422, 0x2424, 0x2473}),
          (std::string("Return did not submit pending n exactly once: ") +
           std::to_string(searches) + " " + query->text().toStdString()).c_str());
  query->clear();
  type(QStringLiteral("n"));
  key(Qt::Key_F4);
  require(mode->text() == QStringLiteral("A") && query->text() == QStringLiteral("\u3093"),
          "F4 failed to finish kana and switch this field to ASCII");
  type(QStringLiteral("abc"));
  require(query->text().endsWith(QStringLiteral("abc")), "ASCII field input was converted");
  mode->click();
  query->clear();
  type(QStringLiteral("A,.-"));
  require(mode->text() == QStringLiteral("J") && query->text() == QStringLiteral("\uff21\uff0c\uff0e\u2015"),
          "Lookup JASCII mode did not use the recovered punctuation");
  key(Qt::Key_F4);
  require(mode->text() == QStringLiteral("K"), "F4 did not return JASCII to Kanji");
  query->clear();
  type(QStringLiteral("k"));
  query->setText(QStringLiteral("x"));
  type(QStringLiteral("a"));
  require(query->text() == QStringLiteral("x\u3042"), "External query replacement retained pending kana");
  query->clear();
  type(QStringLiteral("n"));
  key(Qt::Key_Backspace);
  type(QStringLiteral("a"));
  require(query->text() == QStringLiteral("\u3042"), "Backspace did not discard pending composition");
  query->undo();
  require(query->text().isEmpty(), "Composed query did not preserve native undo");
  query->redo();
  require(query->text() == QStringLiteral("\u3042"), "Composed query did not preserve native redo");
  type(QStringLiteral("k"));
  QInputMethodEvent preedit(QStringLiteral("\u611b"), {});
  QApplication::sendEvent(query, &preedit);
  QInputMethodEvent commit;
  commit.setCommitString(QStringLiteral("\u611b"));
  QApplication::sendEvent(query, &commit);
  type(QStringLiteral("a"));
  require(query->text() == QStringLiteral("\u3042\u611b\u3042"), "Native IME input was mixed into romaji composition");
  query->clear();
  bool changed = false;
  const auto connection = QObject::connect(query, &QLineEdit::textChanged, query, [&] {
    if (!changed) { changed = true; query->setText(QStringLiteral("x")); }
  });
  type(QStringLiteral("kka"));
  QObject::disconnect(connection);
  require(query->text() == QStringLiteral("x\u3042"), "Reentrant text replacement retained a pending doubled consonant");
  query->selectAll();
  key(Qt::Key_C, {}, Qt::ControlModifier);
  require(QApplication::clipboard()->text() == query->text(), "Query Copy was captured by the results view");
  query->setReadOnly(true);
  key(Qt::Key_F4);
  require(mode->text() == QStringLiteral("K"), "Read-only field changed input mode");
}

void test_query_overwrite() {
  using namespace jwpqt::qt;
  int searches = 0;
  EdictLookupDialog dialog([&](const auto&, const auto&, bool) {
    ++searches;
    return EdictResourceSearchReport{};
  });
  auto* query = dialog.findChild<QLineEdit*>(QStringLiteral("edictQuery"));
  auto* field = dynamic_cast<KanaInputField*>(query->parentWidget());
  auto action = std::make_unique<QAction>();
  action->setCheckable(true);
  dialog.set_overwrite_action(action.get());
  KanaInputField other(QStringLiteral("other"));
  other.set_overwrite_action(action.get());
  const auto key = [&](int code, const QString& text = {},
                       Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent event(QEvent::KeyPress, code, modifiers, text);
    QApplication::sendEvent(query, &event);
  };
  const auto type = [&](const QString& text) {
    for (const QChar character : text) key(character.toUpper().unicode(), QString(character));
  };
  const auto reset = [&](const QString& text = QStringLiteral("ABC")) {
    query->setText(text);
    query->setCursorPosition(1);
  };
  dialog.show();
  query->setFocus();
  QApplication::processEvents();
  require(field && !field->overwrite_mode(), "Query did not start in insert mode");
  key(Qt::Key_Insert);
  require(action->isChecked() && field->overwrite_mode() && other.overwrite_mode() &&
              query->accessibleDescription().contains(QStringLiteral("Overwrite")),
          "Query Insert did not update the shared mode and accessibility hint");
  for (const auto mode : {InputMode::kAscii, InputMode::kJascii, InputMode::kKanji}) {
    field->set_input_mode(mode);
    for (const bool selected : {false, true}) {
      reset();
      if (selected) query->setSelection(1, 1);
      type(mode == InputMode::kKanji ? QStringLiteral("kya") : QStringLiteral("X"));
      const QString added = mode == InputMode::kKanji ? QStringLiteral("\u304d\u3083") :
                            mode == InputMode::kJascii ? QStringLiteral("\uff38") : QStringLiteral("X");
      require(query->text() == QStringLiteral("A") + added +
                  (selected || mode != InputMode::kKanji ? QStringLiteral("C") : QString{}),
              "Query overwrite changed the wrong range or lost selected-range suffix text");
      query->undo();
      require(query->text() == QStringLiteral("ABC") && !query->isUndoAvailable(),
              "Query overwrite did not undo as one input transaction");
      query->redo();
      require(query->text().startsWith(QStringLiteral("A") + added),
              "Query overwrite redo lost composed input");
    }
  }
  reset();
  type(QStringLiteral("k"));
  key(Qt::Key_Insert);
  key(Qt::Key_Insert);
  require(query->text() == QStringLiteral("ABC"), "Query mode toggle flushed pending kana");
  type(QStringLiteral("a"));
  require(query->text() == QStringLiteral("A\u304bC"), "Query mode toggle discarded pending kana");
  field->set_input_mode(InputMode::kAscii);
  const QString supplementary = QStringLiteral("A\U0001f600BC");
  reset(supplementary);
  key(Qt::Key_unknown, QStringLiteral("\ufeff\u00a0"));
  require(query->text() == QStringLiteral("A\ufeff\u00a0C") && query->cursorPosition() == 3,
          (std::string("Query overwrite split a scalar or normalized signature/NBSP content: ") +
           query->text().toUtf8().toHex().toStdString() + " cursor=" +
           std::to_string(query->cursorPosition())).c_str());
  query->undo();
  require(query->text() == supplementary, "Unicode query overwrite did not undo");
  reset(supplementary);
  query->setCursorPosition(2);
  key(Qt::Key_X, QStringLiteral("X"));
  require(query->text() == supplementary && !query->isUndoAvailable(),
          "Query overwrite accepted a cursor inside a surrogate pair");
  for (const char16_t invalid : {char16_t{0xd800}, char16_t{0xdc00}}) {
    reset();
    key(Qt::Key_unknown, QString(QChar(invalid)));
    require(query->text() == QStringLiteral("ABC") && !query->isUndoAvailable(),
            "Query overwrite accepted malformed Unicode input");
  }
  reset();
  query->setMaxLength(3);
  key(Qt::Key_unknown, QStringLiteral("\U0001f680"));
  require(query->text() == QStringLiteral("ABC") && !query->hasSelectedText() &&
              !query->isUndoAvailable(), "Query maxLength rejection erased or split text");
  query->setMaxLength(32767);
  QIntValidator numbers(0, 999, &dialog);
  reset(QStringLiteral("123"));
  query->setValidator(&numbers);
  key(Qt::Key_X, QStringLiteral("X"));
  require(query->text() == QStringLiteral("123") && !query->hasSelectedText() &&
              !query->isUndoAvailable(), "Query validator rejection changed the selection or text");
  query->setValidator(nullptr);
  reset();
  query->setReadOnly(true);
  QKeyEvent shortcut(QEvent::ShortcutOverride, Qt::Key_Insert,
                     Qt::ControlModifier | Qt::ShiftModifier);
  shortcut.ignore();
  QApplication::sendEvent(query, &shortcut);
  require(shortcut.isAccepted(), "Read-only query leaked its paste shortcut to the document");
  key(Qt::Key_X, QStringLiteral("X"));
  require(query->text() == QStringLiteral("ABC"), "Read-only query was overwritten");
  query->setReadOnly(false);
  query->setSelection(1, 1);
  key(Qt::Key_Insert, {}, Qt::ControlModifier);
  require(QApplication::clipboard()->text() == QStringLiteral("B") && action->isChecked(),
          "Query Ctrl+Insert toggled mode instead of copying");
  for (const auto modifiers : {Qt::KeyboardModifiers(Qt::ShiftModifier),
                              Qt::KeyboardModifiers(Qt::ShiftModifier | Qt::ControlModifier)}) {
    reset();
    QApplication::clipboard()->setText(QStringLiteral("xy"));
    key(Qt::Key_Insert, {}, modifiers);
    require(query->text() == QStringLiteral("AxyBC") && action->isChecked(),
            "Query clipboard paste overwrote following text or changed mode");
    query->undo();
    require(query->text() == QStringLiteral("ABC"), "Query clipboard paste did not undo");
  }
  reset(supplementary);
  QInputMethodEvent preedit(QStringLiteral("n"), {});
  QApplication::sendEvent(query, &preedit);
  require(query->text() == supplementary, "Query IME preedit overwrote committed text");
  QInputMethodEvent commit;
  commit.setCommitString(QStringLiteral("xy"));
  QApplication::sendEvent(query, &commit);
  require(query->text() == QStringLiteral("AxyC"), "Query IME did not overwrite whole scalars");
  query->undo();
  require(query->text() == supplementary, "Query IME overwrite was not one undo transaction");
  reset();
  query->setSelection(1, 1);
  QApplication::sendEvent(query, &commit);
  require(query->text() == QStringLiteral("AxyC"), "Selected IME query erased following text");
  reset();
  query->setCursorPosition(2);
  QInputMethodEvent explicit_replacement;
  explicit_replacement.setCommitString(QStringLiteral("X"), -1, 1);
  QApplication::sendEvent(query, &explicit_replacement);
  require(query->text() == QStringLiteral("AXC"), "Query overwrote an explicit IME range");
  reset(supplementary);
  QInputMethodEvent continuing(QStringLiteral("z"), {});
  continuing.setCommitString(QStringLiteral("x"));
  QApplication::sendEvent(query, &continuing);
  commit.setCommitString(QStringLiteral("y"));
  QApplication::sendEvent(query, &commit);
  require(query->text() == QStringLiteral("AxyC"), "Query overwrite lost continued IME preedit");
  reset();
  bool replaced = false;
  const auto reentrant = QObject::connect(query, &QLineEdit::selectionChanged, query, [&] {
    if (!replaced) { replaced = true; query->setText(QStringLiteral("safe")); }
  });
  key(Qt::Key_X, QStringLiteral("X"));
  QObject::disconnect(reentrant);
  require(query->text() == QStringLiteral("safe"), "Query overwrite clobbered a reentrant replacement");
  field->set_input_mode(InputMode::kKanji);
  reset();
  type(QStringLiteral("n"));
  key(Qt::Key_Insert);
  key(Qt::Key_Return);
  require(!action->isChecked() && !other.overwrite_mode() && searches == 1 &&
              query->text() == QStringLiteral("A\u3093BC"),
          "Query mode toggle or submission lost pending input or submitted twice");
  QAction invalid_action;
  bool rejected = false;
  try { field->set_overwrite_action(&invalid_action); }
  catch (const std::invalid_argument&) { rejected = true; }
  require(rejected, "Query accepted a non-checkable overwrite action");
  action->setChecked(true);
  action.reset();
  require(!field->overwrite_mode() && !other.overwrite_mode(),
          "Query retained a destroyed overwrite action");
  key(Qt::Key_Insert);
  require(field->overwrite_mode() && !other.overwrite_mode(),
          "Standalone query fallback did not retain independent runtime mode");
}

void test_query_history() {
  using namespace jwpqt;
  auto history = std::make_shared<core::QueryHistory>();
  int searches = 0;
  bool fail = false;
  qt::EdictLookupDialog dialog([&](const core::JwpText&, const qt::EdictLookupOptions&, bool) {
    ++searches;
    if (fail) throw std::runtime_error("history search failure");
    qt::EdictResourceSearchReport report;
    report.results = {result(0, QStringLiteral("Main"), U"cat", {}, {U"feline"})};
    return report;
  }, {}, nullptr, {}, {}, history);
  dialog.show();
  auto* query = dialog.findChild<QLineEdit*>(QStringLiteral("edictQuery"));
  auto* field = dynamic_cast<qt::KanaInputField*>(query->parentWidget());
  auto* history_button = dialog.findChild<QPushButton*>(QStringLiteral("edictHistory"));
  auto* results = dialog.findChild<QTextEdit*>(QStringLiteral("edictResults"));
  auto* status = dialog.findChild<QLabel*>(QStringLiteral("edictStatus"));
  const auto key = [&](int code, QString text = {}) {
    QKeyEvent event(QEvent::KeyPress, code, Qt::NoModifier, text);
    QApplication::sendEvent(query, &event);
  };
  for (const auto& text : {U"cat", U"dog", U"bird"}) {
    dialog.set_query(text);
    require(dialog.search(), "History fixture search failed");
  }
  require(history->entries() == std::vector<std::u32string>{U"bird", U"dog", U"cat"},
          "Successful queries were not retained newest-first");
  const QPointer<QTextDocument> report = results->document();
  const int result_position = results->textCursor().position();
  const int result_anchor = results->textCursor().anchor();
  key(Qt::Key_Up);
  require(query->text() == QStringLiteral("dog"), "Up did not skip the current top query");
  key(Qt::Key_Up);
  key(Qt::Key_Up);
  require(query->text() == QStringLiteral("cat"), "Older navigation did not clamp at the oldest entry");
  key(Qt::Key_Down);
  key(Qt::Key_Down);
  require(query->text() == QStringLiteral("bird"), "Newer navigation did not return to the first entry");
  key(Qt::Key_Down);
  require(query->text().isEmpty(), "Newer navigation did not produce an empty draft");
  dialog.set_query(U"fresh");
  key(Qt::Key_Up);
  require(query->text() == QStringLiteral("bird") && history->entries().front() == U"fresh",
          "History navigation lost an edited draft");
  key(Qt::Key_Down);
  require(query->text() == QStringLiteral("fresh"), "The retained draft could not be recalled");
  query->clear();
  key(Qt::Key_N, QStringLiteral("n"));
  key(Qt::Key_Up);
  require(query->text() == QStringLiteral("fresh") && history->entries().front() == U"\u3093" &&
              searches == 3 && results->document() == report &&
              results->textCursor().position() == result_position && results->textCursor().anchor() == result_anchor,
          "History navigation searched, lost pending kana, or changed existing results");
  const auto before_failure = history->entries();
  query->setText(QString(268, QLatin1Char('q')));
  key(Qt::Key_Up);
  require(query->text() == QString(268, QLatin1Char('q')) && history->entries() == before_failure &&
              status->text().contains(QStringLiteral("not replaced")),
          "An oversized edited query was silently lost during history navigation");
  fail = true;
  dialog.set_query(U"fail");
  require(!dialog.search() && history->entries() == before_failure && results->document() == report &&
              results->textCursor().position() == result_position && results->textCursor().anchor() == result_anchor,
          "A failed search replaced history or previous results");
  fail = false;
  dialog.set_query(std::u32string(268, U'q'));
  require(dialog.search() && history->entries() == before_failure &&
              status->text().contains(QStringLiteral("not retained")),
          "A valid oversized query was truncated into history or prevented from searching");

  history->remember(U"long");
  query->setMaxLength(3);
  query->clear();
  const auto bounded = history->entries();
  key(Qt::Key_Up);
  require(query->text().isEmpty() && history->entries() == bounded,
          "History recall bypassed the field length limit");
  query->setMaxLength(32767);
  QIntValidator validator(0, 9, &dialog);
  query->setValidator(&validator);
  key(Qt::Key_Up);
  require(query->text().isEmpty() && history->entries() == bounded,
          "History recall bypassed the field validator");
  query->setValidator(nullptr);
  class EditingValidator : public QValidator {
   public:
    QLineEdit* target = nullptr;
    mutable bool armed = false;
    State validate(QString&, int&) const override {
      if (armed) { armed = false; target->setText(QStringLiteral("safe")); }
      return Acceptable;
    }
  } editing_validator;
  editing_validator.target = query;
  query->setValidator(&editing_validator);
  editing_validator.armed = true;
  key(Qt::Key_Up);
  require(query->text() == QStringLiteral("safe") && history->entries() == bounded,
          "History recall clobbered an edit made during validation");
  query->setValidator(nullptr);
  query->clear();
  const std::u32string special = U"\ufeff\U0001f600\u00a0\tquery";
  history->remember(special);
  key(Qt::Key_Up);
  require(query->text() == qt::to_qstring(special) && !query->isUndoAvailable(),
          "History recall lost Unicode scalars or retained unrelated query undo");
  query->setReadOnly(true);
  QKeyEvent override_event(QEvent::ShortcutOverride, Qt::Key_Up, Qt::NoModifier);
  override_event.ignore();
  QApplication::sendEvent(query, &override_event);
  key(Qt::Key_Up);
  require(override_event.isAccepted() && query->text() == qt::to_qstring(special),
          "Read-only history navigation changed text or leaked a shortcut");
  query->setReadOnly(false);
  query->clear();
  bool replaced = false;
  const auto connection = QObject::connect(query, &QLineEdit::textChanged, &dialog, [&] {
    if (!replaced) { replaced = true; query->setText(QStringLiteral("safe")); }
  });
  key(Qt::Key_Up);
  QObject::disconnect(connection);
  require(query->text() == QStringLiteral("safe"), "History recall clobbered a reentrant edit");

  const int completed_searches = searches;
  query->setText(QStringLiteral("draft"));
  query->setSelection(1, 2);
  bool handled = false;
  QTimer::singleShot(0, &dialog, [&] {
    auto* chooser = qobject_cast<QDialog*>(QApplication::activeModalWidget());
    if (!chooser) return;
    auto* list = chooser->findChild<QListWidget*>(QStringLiteral("edictHistoryList"));
    auto* buttons = chooser->findChild<QDialogButtonBox*>();
    handled = list && list->font().pixelSize() == 16 &&
        list->count() == static_cast<int>(history->entries().size());
    if (handled) {
      handled = chooser->grab().save(QCoreApplication::applicationDirPath() + QStringLiteral("/dictionary-history.png"));
    }
    buttons->button(QDialogButtonBox::Cancel)->click();
  });
  history_button->click();
  require(handled && query->text() == QStringLiteral("draft") && query->selectionStart() == 1 &&
              query->selectedText() == QStringLiteral("ra") && searches == completed_searches,
          "History Cancel changed the query, selection, or search count");
  handled = false;
  const auto deleted_first = history->entries()[0];
  const auto deleted_second = history->entries()[1];
  QTimer::singleShot(0, &dialog, [&] {
    auto* chooser = qobject_cast<QDialog*>(QApplication::activeModalWidget());
    if (!chooser) return;
    auto* list = chooser->findChild<QListWidget*>(QStringLiteral("edictHistoryList"));
    list->setCurrentRow(0);
    list->item(1)->setSelected(true);
    chooser->findChild<QAction*>(QStringLiteral("edictHistoryCopy"))->trigger();
    handled = QApplication::clipboard()->text() == qt::to_qstring(deleted_first) +
        QLatin1Char('\n') + qt::to_qstring(deleted_second);
    chooser->findChild<QPushButton*>(QStringLiteral("edictHistoryDelete"))->click();
    chooser->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Cancel)->click();
  });
  key(Qt::Key_Down);
  require(handled && !history->find(deleted_first) && !history->find(deleted_second) &&
              query->text() == QStringLiteral("draft") && searches == completed_searches,
          "History copy/delete/Cancel did not preserve the query and immediate deletion semantics");
  const auto chosen = history->entries().back();
  QTimer::singleShot(0, &dialog, [&] {
    auto* chooser = qobject_cast<QDialog*>(QApplication::activeModalWidget());
    if (!chooser) return;
    auto* list = chooser->findChild<QListWidget*>(QStringLiteral("edictHistoryList"));
    list->setCurrentRow(list->count() - 1);
    chooser->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
  });
  history_button->click();
  require(query->text() == qt::to_qstring(chosen) && searches == completed_searches &&
              field->input_mode() == qt::InputMode::kKanji,
          "Choosing history searched automatically or changed the local input mode");

  qt::EdictLookupDialog reopened({}, {}, nullptr, {}, {}, history);
  auto* reopened_query = reopened.findChild<QLineEdit*>(QStringLiteral("edictQuery"));
  QKeyEvent older(QEvent::KeyPress, Qt::Key_Up, Qt::NoModifier);
  QApplication::sendEvent(reopened_query, &older);
  require(reopened_query->text() == qt::to_qstring(history->entries().front()),
          "A replacement dialog did not inherit the shared history");
  QPointer<qt::EdictLookupDialog> dying = new qt::EdictLookupDialog({}, {}, nullptr, {}, {}, history);
  QPointer<QDialog> popup;
  QTimer::singleShot(0, &dialog, [&] {
    popup = qobject_cast<QDialog*>(QApplication::activeModalWidget());
    delete dying.data();
  });
  dying->findChild<QPushButton*>(QStringLiteral("edictHistory"))->click();
  require(!dying && !popup, "History chooser retained a deleted resource owner");
  dying = new qt::EdictLookupDialog([&](const core::JwpText&, const qt::EdictLookupOptions&, bool) {
    delete dying.data();
    return qt::EdictResourceSearchReport{};
  }, {}, nullptr, {}, {}, history);
  dying->set_query(U"cat");
  const auto previous_history = history->entries();
  require(!dying->search() && !dying && history->entries() == previous_history,
          "Search used a deleted dialog or published history after owner deletion");

  auto separate_history = std::make_shared<core::QueryHistory>();
  QPointer<qt::EdictLookupDialog> changing;
  int callbacks = 0;
  bool recursive_blocked = false;
  changing = new qt::EdictLookupDialog([&](const core::JwpText&, const qt::EdictLookupOptions&, bool) {
    if (++callbacks == 2) {
      recursive_blocked = !changing->search();
      changing->set_query(U"other");
    }
    return qt::EdictResourceSearchReport{};
  }, {}, nullptr, {}, {}, separate_history);
  changing->set_query(U"cat");
  require(changing->search() && separate_history->entries() == std::vector<std::u32string>{U"cat"},
          "A zero-match query was not retained");
  const QPointer<QTextDocument> old_results = changing->findChild<QTextEdit*>(QStringLiteral("edictResults"))->document();
  changing->set_query(U"dog");
  require(!changing->search() && recursive_blocked && callbacks == 2 &&
              separate_history->entries() == std::vector<std::u32string>{U"cat"} && old_results &&
              changing->findChild<QLineEdit*>(QStringLiteral("edictQuery"))->text() == QStringLiteral("other") &&
              changing->findChild<QTextEdit*>(QStringLiteral("edictResults"))->document() == old_results,
          "A reentrant search or changed query published stale results/history");
  separate_history->set_storage_cells(0);
  require(changing->search() && separate_history->entries().empty(),
          "Disabled history prevented a valid query from completing");
  delete changing.data();
}

void test_result_character_navigation() {
  std::vector<char32_t> inspected;
  int searches = 0;
  jwpqt::qt::EdictLookupDialog dialog(
      [&](const jwpqt::core::JwpText&, const jwpqt::qt::EdictLookupOptions&, bool) {
        if (++searches == 2) throw std::runtime_error("failed replacement");
        jwpqt::qt::EdictResourceSearchReport report;
        report.results = {result(0, QStringLiteral("Main"), U"\u611b", {U"\u3042\u3044"},
                                 {U"love <&> \U0001f600"})};
        return report;
      }, {}, nullptr, [&](char32_t character) { inspected.push_back(character); });
  dialog.set_query(U"\u3042\u3044");
  dialog.show();
  require(dialog.search(), "Could not prepare character navigation results");
  auto* results = dialog.findChild<QTextEdit*>(QStringLiteral("edictResults"));
  QApplication::processEvents();
  require(results && results->toPlainText().contains(QStringLiteral("love <&>")),
          "Dictionary text was interpreted as markup");
  const QTextCursor original = results->textCursor();
  const QString original_text = results->toPlainText();
  for (const QString& character : {QStringLiteral("\u611b"), QStringLiteral("\u3042"),
                                  QString::fromUcs4(U"\U0001f600")}) {
    QTextCursor cursor(results->document());
    const int position = original_text.indexOf(character);
    cursor.setPosition(position);
    const QRect first = results->cursorRect(cursor);
    cursor.setPosition(position + character.size());
    const QPoint point((first.left() + results->cursorRect(cursor).left()) / 2, first.center().y());
    bool selected = false;
    QTimer::singleShot(0, [&] {
      auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
      if (!menu) return;
      for (QAction* action : menu->actions()) {
        if (action->objectName() == QStringLiteral("characterInfoContextAction") && action->isEnabled()) {
          selected = true;
          menu->setActiveAction(action);
          QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
          QApplication::sendEvent(menu, &enter);
          return;
        }
      }
      menu->close();
    });
    QContextMenuEvent context(QContextMenuEvent::Mouse, point, results->viewport()->mapToGlobal(point));
    QApplication::sendEvent(results->viewport(), &context);
    require(selected && results->textCursor().position() == original.position() &&
                results->textCursor().anchor() == original.anchor(),
            "Character context inspection changed the result selection");
  }
  require(inspected == std::vector<char32_t>{U'\u611b', U'\u3042', U'\U0001f600'},
          "Dictionary context menu inspected a row instead of the clicked character");
  require(!dialog.search() && results->toPlainText() == original_text &&
              results->textCursor().position() == original.position(),
          "Failed search discarded previous results or selection");
  const QPointer<QTextDocument> previous = results->document();
  require(dialog.search() && previous.isNull(),
          "Repeated search retained obsolete result documents");
}

}  // namespace

int main(int argc, char** argv) {
  QApplication application(argc, argv);
  try {
    test_names_and_clipboard();
    test_management_commands();
    test_result_keyboard_commands();
    test_linked_names();
    test_search_render_status_copy_and_insert();
    test_compact_presentation();
    test_priority_presentation();
    test_empty_invalid_and_failed_search_are_contained();
    test_result_sorting();
    test_search_controls();
    test_options_updates();
    test_query_input_modes();
    test_query_overwrite();
    test_query_history();
    test_result_character_navigation();
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << "edict_lookup_dialog_test: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
