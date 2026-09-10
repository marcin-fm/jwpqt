// SPDX-License-Identifier: GPL-2.0-or-later
#include "auxiliary_find.h"
#include "edict_lookup_dialog.h"
#include "edict_results_window.h"
#include "main_window.h"
#include "text_bridge.h"
#include "jwp_editor.h"
#include "jwpqt/core/unicode_search.h"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <QTextEdit>
#include <iostream>
#include <stdexcept>
using namespace jwpqt;
using namespace jwpqt::qt;
namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
class CloseGuard final : public QWidget {
 public:
  bool allow = false;
  int attempts = 0;

 protected:
  void closeEvent(QCloseEvent* event) override {
    ++attempts;
    if (allow) event->accept();
    else event->ignore();
  }
};
AuxiliaryFind* finder(QWidget* target) {
  for (auto* child : target->children()) if (auto* value = dynamic_cast<AuxiliaryFind*>(child)) return value;
  throw std::runtime_error("Missing auxiliary finder");
}
void key(QWidget* widget, int code, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
  QKeyEvent event(QEvent::KeyPress, code, modifiers); QApplication::sendEvent(widget, &event);
}
void shortcut_key(QWidget* widget, int code,
                  Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
  QKeyEvent shortcut(QEvent::ShortcutOverride, code, modifiers);
  QApplication::sendEvent(widget, &shortcut);
  require(shortcut.isAccepted(), "Auxiliary Find did not own its shortcut alias");
  key(widget, code, modifiers);
}
FindReplaceRequest request(QString text, bool wrap = true, bool back = false) {
  FindReplaceRequest value; value.text = std::move(text); value.options.wrap = wrap;
  value.options.direction = back ? core::JwpSearchDirection::kBackward : core::JwpSearchDirection::kForward;
  return value;
}
QAction* result_action(QWidget* target, const char* name) {
  auto* action = target->findChild<QAction*>(QString::fromLatin1(name));
  if (!action) throw std::runtime_error("Missing result insertion action");
  return action;
}
void select_text(JwpEditor* editor, int anchor, int position) {
  QTextCursor cursor(editor->document());
  cursor.setPosition(anchor);
  cursor.setPosition(position, QTextCursor::KeepAnchor);
  editor->setTextCursor(cursor);
}
void test_logical_navigation() {
  QWidget owner; auto* list = new QListWidget(&owner); auto* find = new AuxiliaryFind(list);
  list->addItems({"cat zero", "dog", "CAT cat two", "cat three"});
  list->setCurrentRow(0);
  require(find->find(request("cat")).valid && list->currentRow() == 2, "Find must skip current and tolerate repeated matches");
  require(find->find(request("cat")).valid && list->currentRow() == 3, "Find next entry");
  require(find->find(request("cat", false)).valid && list->currentRow() == 3, "No-wrap preserves selection");
  require(find->find(request("cat")).valid && list->currentRow() == 0, "Wrap to first entry");
  require(find->find(request("cat", true, true)).valid && list->currentRow() == 3, "Backward wraps to last");
  list->item(2)->setHidden(true);
  require(find->find(request("cat", true, true)).valid && list->currentRow() == 0, "Hidden entries skipped");
  list->clear(); list->addItem("only match"); list->setCurrentRow(0);
  require(find->find(request("match")).message.contains("No other") && list->currentRow() == 0, "Never match current alone");
  list->clear(); list->addItems({"alpha", "beta"}); list->setCurrentRow(-1);
  require(find->find(request("alpha beta")).message.contains("No other"), "Do not match across independent entries");
  const QString literal = to_qstring(U"\ufeff\ufffeA\u00a0\U0001f600");
  list->addItem(literal); list->setCurrentRow(0);
  require(find->find(request(literal)).valid && list->currentRow() == 2 && list->currentItem()->text() == literal,
          "Preserve signatures NBSP and supplementary text");
  const int before = list->currentRow();
  require(!find->find(request(QString(QChar(0xd800)))).valid && list->currentRow() == before, "Invalid Unicode preserves selection");
  auto invalid = request("alpha"); invalid.all_files = true;
  require(!find->find(invalid).valid && list->currentRow() == before, "Reject document scope");
  std::size_t work = 1;
  require(core::find_unicode_text(U"aaaa", U"a", {}, 1, &work, false, true).size() == 1 && work == 0,
          "Existence search must stop after first match without overspending budget");
}
void test_dialog_and_workspace() {
  MainWindow window; auto* list = new QListWidget(&window); new AuxiliaryFind(list);
  list->addItems({"skip", QString::fromUtf8("かな cat"), "last cat"}); list->setCurrentRow(0);
  const auto document = *window.current_jwp_document();
  window.show(); list->show();
  key(list, Qt::Key_F, Qt::ControlModifier);
  auto* dialog = window.findChild<QDialog*>("auxiliaryFindDialog");
  require(dialog && dialog->isVisible(), "Ctrl-F opens auxiliary Find");
  require(dialog->findChild<QCheckBox*>("searchAllFiles")->isHidden() &&
          !dialog->findChild<QPushButton*>("searchReplace"), "No document/replacement controls in auxiliary Find");
  auto* input = dialog->findChild<QLineEdit*>("findText");
  require(input, "Native Japanese search input");
  input->setText("cat");
  dialog->findChild<QCheckBox*>("searchKeepOpen")->setChecked(true);
  dialog->findChild<QPushButton*>("searchFind")->click();
  require(list->currentRow() == 1 && window.query_histories().search.find(U"cat").has_value(), "Search selects whole entry and shares history");
  require(*window.current_jwp_document() == document, "Auxiliary Find must not edit the document");
  key(list, Qt::Key_F3); require(list->currentRow() == 2, "F3 repeats auxiliary Find");
  key(list, Qt::Key_F3, Qt::ShiftModifier); require(list->currentRow() == 1, "Shift F3 searches backward");
  dialog->close(); QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  shortcut_key(list, Qt::Key_S, Qt::ControlModifier);
  dialog = window.findChild<QDialog*>("auxiliaryFindDialog");
  require(dialog && dialog->isVisible(), "Ctrl+S did not open auxiliary Find");
  dialog->close(); QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  shortcut_key(list, Qt::Key_F8);
  dialog = window.findChild<QDialog*>("auxiliaryFindDialog");
  require(dialog && dialog->findChild<QLineEdit*>("findText")->text() == "cat", "Reopened auxiliary query retained");
  auto* second = new QListWidget(&window); auto* another = new AuxiliaryFind(second);
  another->open();
  bool shared = false;
  for (auto* open : window.findChildren<QDialog*>("auxiliaryFindDialog"))
    shared |= open->findChild<QLineEdit*>("findText")->text() == "cat";
  require(shared, "Other list uses shared search history");
  require(another->find(request("last")).valid, "Another list can publish the shared search term");
  list->setCurrentRow(0); key(list, Qt::Key_F3);
  require(list->currentRow() == 2, "Repeat uses current workspace search, not a stale list-local term");
  list->setCurrentRow(0); shortcut_key(list, Qt::Key_N, Qt::ControlModifier);
  require(list->currentRow() == 2, "Ctrl+N did not repeat auxiliary Find");
  list->setCurrentRow(0); shortcut_key(list, Qt::Key_F9);
  require(list->currentRow() == 2, "F9 did not repeat auxiliary Find");
  auto* query = dialog->findChild<QLineEdit*>("findText"); query->clear();
  for (const auto ch : QStringLiteral("kana")) {
    QKeyEvent typed(QEvent::KeyPress, ch.toUpper().unicode(), Qt::NoModifier, QString(ch));
    QApplication::sendEvent(query, &typed);
  }
  dialog->findChild<QCheckBox*>("searchWrap")->setChecked(true);
  dialog->findChild<QPushButton*>("searchFind")->click();
  require(query->text() == QString::fromUtf8("かな") && list->currentRow() == 1,
          "Japanese romaji input finds the next logical entry");
  QTemporaryDir temp;
  require(temp.isValid() && window.save_query_history(temp.filePath("history.bin")), "Save auxiliary shared history");
  MainWindow reopened;
  require(reopened.load_query_history(temp.filePath("history.bin")) && reopened.query_histories().search.find(U"かな"),
          "Auxiliary search history survives restart");
}
void test_shared_window_commands() {
  CloseGuard owner;
  auto* list = new QListWidget(&owner);
  new AuxiliaryFind(list);
  list->addItems({"first", "second"});
  list->setCurrentRow(1);
  owner.show(); list->show(); QApplication::processEvents();

  bool list_popup = false;
  QTimer::singleShot(0, [&] {
    auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
    if (!menu) return;
    for (auto* action : menu->actions()) {
      list_popup |= action && action->objectName() == QStringLiteral("auxiliaryFind");
    }
    menu->close();
  });
  shortcut_key(list, Qt::Key_F23);
  require(list_popup, "F23 did not open the shared list popup");
  require(list->currentRow() == 1, "F23 popup changed the current list entry");

  shortcut_key(list, Qt::Key_F4, Qt::ControlModifier);
  require(owner.attempts == 1 && owner.isVisible(),
          "Ctrl+F4 bypassed an ignored close event");
  owner.allow = true;
  shortcut_key(list, Qt::Key_F4, Qt::ControlModifier);
  require(owner.attempts == 2 && !owner.isVisible(),
          "Ctrl+F4 did not close through the normal window event");

}
EdictResourceSearchReport report() {
  EdictResourceSearchReport result;
  for (const auto& word : {U"zero", U"cat", U"dog"}) {
    EdictResourceSearchResult entry; entry.label = "Fixture";
    entry.result.record.headword = word; entry.result.record.readings = {U"reading"};
    entry.result.record.definitions = {U"meaning cat"}; result.results.push_back(std::move(entry));
  }
  return result;
}
void test_real_result_ownership() {
  std::u32string inserted;
  EdictLookupDialog dictionary([](const core::JwpText&, const EdictLookupOptions&, bool) { return report(); },
      [&](const std::u32string& value) { inserted = value; return true; });
  dictionary.set_query(U"cat"); require(dictionary.search(), "Dictionary fixture search");
  auto* text = dictionary.findChild<QTextEdit*>("edictResults");
  require(text, "Result text widget");
  dictionary.show(); text->show(); QApplication::processEvents();
  bool text_popup = false;
  QTimer::singleShot(0, [&] {
    auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
    if (menu) {
      text_popup = menu->objectName() == QStringLiteral("characterContextMenu");
      menu->close();
    }
  });
  shortcut_key(text, Qt::Key_F23);
  require(text_popup, "F23 did not open the text-result popup");
  auto* find = finder(text);
  const auto* original = text->document();
  require(find->find(request("cat")).valid && text->textCursor().selectedText().startsWith("cat [reading]"),
          "Dictionary Find selects whole logical entry including wrapped definitions");
  dictionary.findChild<QPushButton*>("edictInsert")->click();
  require(inserted == U"cat [reading] /meaning cat/" && text->document() == original, "Canonical insertion ownership preserved");
  require(dictionary.sort_results(), "Sorting fixture");
  require(find->find(request("zero")).valid && text->textCursor().selectedText().startsWith("zero"), "Find follows new display ranges after sort");
  EdictResultsWindow accumulated; accumulated.append_report(report());
  auto* list = accumulated.findChild<QListWidget*>("edictResultsList"); list->setCurrentRow(0);
  accumulated.set_insert_handler([&](const std::u32string& value) { inserted = value; return true; });
  require(finder(list)->find(request("dog")).valid && list->currentRow() == 2, "Accumulated results Find");
  accumulated.findChild<QPushButton*>("edictResultsInsert")->click();
  require(inserted == U"dog [reading] /meaning cat/", "Accumulated canonical insertion retained");
  EdictResourceSearchReport unicode; EdictResourceSearchResult entry;
  entry.result.record.headword = U"\ufeff\ufffe"; entry.result.record.definitions = {U"\u00a0\U0001f600"};
  unicode.results.push_back(entry); accumulated.append_report(std::move(unicode));
  require(list->item(3)->text().startsWith(to_qstring(U"\ufeff\ufffe")), "Accumulated display must not consume a signature");
  auto broken = report(); broken.results.back().result.record.headword = std::u32string(1, 0xd800);
  bool rejected = false;
  try { accumulated.append_report(std::move(broken)); } catch (const std::exception&) { rejected = true; }
  require(rejected && accumulated.result_count() == 4 && list->count() == 4, "Invalid incoming Unicode cannot partially append results");
  QPointer<EdictResultsWindow> disposed = new EdictResultsWindow;
  disposed->append_report(report());
  auto* disposed_list = disposed->findChild<QListWidget*>("edictResultsList"); disposed_list->setCurrentRow(0);
  require(finder(disposed_list)->find(request("dog")).valid, "Find before owner-deleting insertion");
  disposed->set_insert_handler([&](const std::u32string&) -> bool { delete disposed.data(); throw std::runtime_error("closed"); });
  disposed->findChild<QPushButton*>("edictResultsInsert")->click();
  require(!disposed, "Found-entry insertion may destroy the owning window");
}
void test_result_insert_destinations() {
  MainWindow window;
  require(window.insert_edict_text(U"AB"), "Prepare native destination text");
  select_text(window.active_editor(), 1, 2);

  auto* list = new QListWidget(&window);
  list->addItem("canonical result");
  list->setCurrentRow(0);
  auto* button = new QPushButton("Insert", list);
  bool allow_insert = true;
  bool redirect_insert = false;
  bool redirect_rejected = false;
  QObject::connect(button, &QPushButton::clicked, list, [&] {
    if (redirect_insert) {
      window.activate_document(1);
      redirect_rejected = !window.insert_edict_text(U"X");
      return;
    }
    if (allow_insert) window.insert_edict_text(U"X");
  });
  auto* auxiliary = new AuxiliaryFind(list);
  auxiliary->set_result_insertion(button);
  auto* current = result_action(list, "resultInsertCurrent");
  auto* replace = result_action(list, "resultReplaceCurrent");
  auto* create = result_action(list, "resultInsertNew");
  auto* any = result_action(list, "resultInsertAny");
  auto* last = result_action(list, "resultInsertLast");
  auto* undo = window.findChild<QAction*>("undoAction");
  require(current->isEnabled() && replace->isEnabled() && create->isEnabled() &&
              any->isEnabled() && !last->isEnabled() && undo,
          "Initial result destination availability");

  replace->trigger();
  require(window.active_editor()->toPlainText() == "AX",
          "Replace destination deletes the current selection");
  undo->trigger();
  require(window.active_editor()->toPlainText() == "AB",
          "Replace destination is one undo transaction");
  select_text(window.active_editor(), 1, 2);
  current->trigger();
  require(window.active_editor()->toPlainText() == "ABX" &&
              window.active_editor()->textCursor().position() == 3,
          "Standard destination preserves selection text and inserts at its endpoint");
  undo->trigger();
  require(window.active_editor()->toPlainText() == "AB",
          "Standard destination is one undo transaction");

  allow_insert = false;
  create->trigger();
  require(window.document_count() == 1 && window.current_document_index() == 0,
          "Failed insertion removes its unused new destination");
  allow_insert = true;
  create->trigger();
  require(window.document_count() == 2 && window.current_document_index() == 0 &&
              last->isEnabled() && last->text().contains("Untitled"),
          "New destination restores the source and becomes Last");
  require(window.activate_document(1) && window.uses_jwp_format() &&
              window.active_editor()->toPlainText() == "X",
          "New destination is an independent Japanese document");
  require(window.activate_document(0), "Restore source after checking New destination");
  last->trigger();
  require(window.current_document_index() == 0 && window.activate_document(1) &&
              window.active_editor()->toPlainText() == "XX",
          "Last routes to the remembered live destination and restores the source");
  require(window.activate_document(0), "Restore source before reentrant routing check");
  redirect_insert = true;
  current->trigger();
  redirect_insert = false;
  require(redirect_rejected && window.current_document_index() == 0 &&
              window.active_editor()->toPlainText() == "AB" &&
              window.activate_document(1) &&
              window.active_editor()->toPlainText() == "XX",
          "Reentrant tab changes cannot redirect a result insertion");

  const int unicode = window.new_document_tab(false);
  require(unicode == 2 && window.insert_edict_text(U"U"),
          "Prepare unrestricted Unicode destination");
  require(window.activate_document(0), "Restore source before Any cancellation");
  QTimer::singleShot(0, [&window] {
    if (auto* dialog = window.findChild<QInputDialog*>("resultInsertFileDialog"))
      dialog->reject();
  });
  any->trigger();
  require(window.current_document_index() == 0 && window.activate_document(unicode) &&
              window.active_editor()->toPlainText() == "U",
          "Cancelling Any preserves every destination");
  require(window.activate_document(0), "Restore source before Any insertion");
  QTimer::singleShot(0, [&window] {
    if (auto* dialog = window.findChild<QInputDialog*>("resultInsertFileDialog")) {
      dialog->setTextValue("3. Untitled");
      dialog->accept();
    }
  });
  any->trigger();
  require(window.current_document_index() == 0 && window.activate_document(unicode) &&
              window.active_editor()->toPlainText() == "UX",
          "Any inserts into an unrestricted Unicode target and restores the source");
  require(window.activate_document(0), "Restore source before updated Last insertion");
  last->trigger();
  require(window.current_document_index() == 0 && window.activate_document(unicode) &&
              window.active_editor()->toPlainText() == "UXX",
          "Any updates the remembered Last destination");
  undo->trigger();
  undo->trigger();
  undo->trigger();
  require(window.active_editor()->toPlainText().isEmpty() &&
              !window.document_modified() &&
              window.close_document(unicode, OpenMode::kNonInteractive),
          "Remembered Unicode destination can close cleanly");
  require(window.activate_document(0), "Restore source after closing Last destination");
  last->trigger();
  require(!last->isEnabled() && window.active_editor()->toPlainText() == "AB",
          "Last disables safely after its destination closes");
  require(list->currentRow() == 0 && list->currentItem()->text() == "canonical result",
          "Destination insertion preserves the source result selection");
}
void test_reentrancy_and_bounds() {
  QPointer<QWidget> owner = new QWidget; auto* list = new QListWidget(owner);
  auto* find = new AuxiliaryFind(list); list->addItems({"a", "b"}); list->setCurrentRow(0);
  QPointer<QItemSelectionModel> selection_model = list->selectionModel();
  bool model_survived_notification = false;
  QObject::connect(list, &QListWidget::itemSelectionChanged, list, [&] {
    delete owner.data();
    model_survived_notification = !selection_model.isNull();
  });
  find->find(request("b")); require(!owner, "Selection may delete owner");
  require(model_survived_notification, "Qt selection model must outlive its remaining notifications");
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  require(!selection_model, "Detached selection model must not leak");
  QWidget second; auto* text = new QTextEdit(&second); text->setPlainText("first\nlabel\nsecond");
  auto* bad = new AuxiliaryFind(text, [] { return std::vector<std::pair<int,int>>{{0, 100}}; });
  const auto cursor = text->textCursor();
  require(!bad->find(request("second")).valid && text->textCursor().position() == cursor.position(), "Bad range preserves selection");
  auto* changing = new QTextEdit(&second); changing->setPlainText("first\nsecond");
  auto* replace = new AuxiliaryFind(changing, [] { return std::vector<std::pair<int,int>>{{0,5},{6,12}}; });
  QObject::connect(changing, &QTextEdit::selectionChanged, changing, [changing] { changing->setPlainText("new results"); });
  require(!replace->find(request("second")).valid && changing->toPlainText() == "new results",
          "Selection callbacks that replace results win over older snapshots");
  auto* redirected = new QListWidget(&second); auto* redirect_find = new AuxiliaryFind(redirected);
  redirected->addItems({"first", "second", "third"}); redirected->setCurrentRow(0);
  QObject::connect(redirected, &QListWidget::currentRowChanged, redirected, [redirected](int row) {
    if (row == 1) redirected->setCurrentRow(2);
  });
  require(!redirect_find->find(request("second")).valid && redirected->currentRow() == 2,
          "A newer list selection must not be reported as the found entry");
  auto* redirected_text = new QTextEdit(&second); redirected_text->setPlainText("first\nsecond");
  auto* redirect_text_find = new AuxiliaryFind(redirected_text, [] {
    return std::vector<std::pair<int,int>>{{0,5},{6,12}};
  });
  QObject::connect(redirected_text, &QTextEdit::selectionChanged, redirected_text, [redirected_text] {
    if (redirected_text->textCursor().selectionStart() == 6) {
      QTextCursor newer(redirected_text->document()); newer.setPosition(2);
      redirected_text->setTextCursor(newer);
    }
  });
  require(!redirect_text_find->find(request("second")).valid &&
              redirected_text->textCursor().position() == 2 && !redirected_text->textCursor().hasSelection(),
          "A newer text selection must win without a false matching-entry status");
  auto* small = new QListWidget(&second); auto* bounded = new AuxiliaryFind(small);
  small->addItem(QString(32 * 1024 * 1024 + 1, 'x'));
  require(!bounded->find(request("x")).valid, "Bounded result snapshot");
  QPointer<QWidget> disposable = new QWidget; auto* target = new QListWidget(disposable);
  auto* opened = new AuxiliaryFind(target); opened->open();
  delete disposable.data(); QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  require(!disposable, "Find dialog closes safely with owner");
}
}
int main(int argc, char** argv) {
  QApplication app(argc, argv);
  try { test_logical_navigation(); test_dialog_and_workspace(); test_shared_window_commands();
    test_real_result_ownership();
    test_result_insert_destinations(); test_reentrancy_and_bounds();
    std::cout << "Auxiliary Find tests passed\n"; return 0;
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
