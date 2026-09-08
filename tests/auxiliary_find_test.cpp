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
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
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
AuxiliaryFind* finder(QWidget* target) {
  for (auto* child : target->children()) if (auto* value = dynamic_cast<AuxiliaryFind*>(child)) return value;
  throw std::runtime_error("Missing auxiliary finder");
}
void key(QWidget* widget, int code, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
  QKeyEvent event(QEvent::KeyPress, code, modifiers); QApplication::sendEvent(widget, &event);
}
FindReplaceRequest request(QString text, bool wrap = true, bool back = false) {
  FindReplaceRequest value; value.text = std::move(text); value.options.wrap = wrap;
  value.options.direction = back ? core::JwpSearchDirection::kBackward : core::JwpSearchDirection::kForward;
  return value;
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
  MainWindow window; auto* list = new QListWidget(&window); auto* find = new AuxiliaryFind(list);
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
  find->open(); dialog = window.findChild<QDialog*>("auxiliaryFindDialog");
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
  try { test_logical_navigation(); test_dialog_and_workspace(); test_real_result_ownership(); test_reentrancy_and_bounds();
    std::cout << "Auxiliary Find tests passed\n"; return 0;
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
