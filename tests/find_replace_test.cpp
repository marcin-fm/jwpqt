// SPDX-License-Identifier: GPL-2.0-or-later
#include "main_window.h"
#include "find_replace_dialog.h"
#include "jwp_editor.h"
#include "kana_input_field.h"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QDir>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>
#include <stdexcept>
namespace qt = jwpqt::qt;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template <class T> T* child(QObject& parent, const char* name) {
  auto* object = parent.findChild<T*>(QString::fromLatin1(name));
  require(object != nullptr, name); return object;
}
QString text(qt::MainWindow& window) { return qt::document_plain_text(*window.active_editor()->document()); }
void set_text(qt::MainWindow& window, const QString& value, int position = 0) {
  window.active_editor()->selectAll(); window.active_editor()->insertPlainText(value);
  QTextCursor cursor(window.active_editor()->document()); cursor.setPosition(position);
  window.active_editor()->setTextCursor(cursor);
}
void click(QObject& parent, const char* name) { child<QPushButton>(parent, name)->click(); }
void key(QWidget* widget, int value, const QString& content = {}) {
  QKeyEvent event(QEvent::KeyPress, value, Qt::NoModifier, content); QApplication::sendEvent(widget, &event);
}
QDialog* open(qt::MainWindow& window, bool replace) {
  child<QAction>(window, replace ? "replaceAction" : "findAction")->trigger();
  return child<QDialog>(window, replace ? "replaceDialog" : "findDialog");
}
void answer(QMessageBox::StandardButton value, std::function<void()> before = {}) {
  QTimer::singleShot(0, [value, before] {
    auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    require(box, "Replacement confirmation missing");
    if (before) before();
    box->button(value)->click();
  });
}
void workflows() {
  QTemporaryDir directory(QDir::currentPath() + QStringLiteral("/find-replace-XXXXXX"));
  require(directory.isValid(), "Temporary directory failed");
  qt::MainWindow window; window.show();
  set_text(window, QStringLiteral("cat CAT cat"));
  require(window.format_paragraphs({3, 0, 0, 120}), "Native formatting fixture failed");
  const auto original_native = *window.current_jwp_document();
  auto* native = window.active_editor();
  window.new_document_tab(false);
  set_text(window, QString::fromUtf8("cat \xef\xbc\xa3\xef\xbc\xa1\xef\xbc\xb4 \xf0\x9f\x98\x80 cat"));
  auto* unicode = window.active_editor();
  window.activate_document(0);
  auto* find = open(window, false);
  require(!find->isModal(), "Find is not modeless");
  auto* query = child<QLineEdit>(*find, "findText"); query->setText(QStringLiteral("cat"));
  child<QCheckBox>(*find, "searchAllFiles")->setChecked(true);
  require(!child<QCheckBox>(*find, "searchWrap")->isEnabled(), "All files did not supersede wrap");
  click(*find, "searchFind"); require(native->textCursor().selectionStart() == 4, "Forward case search failed");
  child<QAction>(window, "findNextAction")->trigger();
  require(native->textCursor().selectionStart() == 8, "Menu repeat search failed");
  click(*find, "searchFind"); require(window.current_document_index() == 1 && unicode->textCursor().selectionStart() == 0,
                                       "All-files traversal skipped boundary match");
  click(*find, "searchFind"); require(unicode->textCursor().selectedText() == QString::fromUtf8("\xef\xbc\xa3\xef\xbc\xa1\xef\xbc\xb4"), "Width matching failed in Unicode document");
  child<QCheckBox>(*find, "searchBackward")->setChecked(true);
  click(*find, "searchFind"); require(unicode->textCursor().selectionStart() == 0, "Backward traversal failed");
  click(*find, "searchFind"); require(window.current_document_index() == 0 && native->textCursor().selectionStart() == 8, "Backward file boundary failed");
  query->clear(); key(query, Qt::Key_K, QStringLiteral("k")); key(query, Qt::Key_A, QStringLiteral("a"));
  require(query->text() == QString::fromUtf8("\xe3\x81\x8b"), "Find lacks local Japanese input");
  query->clear(); key(query, Qt::Key_Up); require(query->text() == QStringLiteral("cat"), "Search history recall failed");
  const auto selection = native->textCursor().selectionStart();
  QTimer::singleShot(0, [] {
    auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget()); require(dialog, "History chooser missing");
    child<QDialogButtonBox>(*dialog, "")->button(QDialogButtonBox::Cancel)->click();
  });
  click(*find, "searchHistory"); require(native->textCursor().selectionStart() == selection, "History recall searched");
  auto* replace = open(window, true);
  auto* pattern = child<QLineEdit>(*replace, "replaceFindText");
  auto* replacement = child<QLineEdit>(*replace, "replacementText");
  pattern->setText(QStringLiteral("cat")); replacement->setText(QStringLiteral("dog"));
  child<QCheckBox>(*replace, "searchAllFiles")->setChecked(true);
  click(*replace, "searchReplaceAll");
  require(qt::document_plain_text(*native->document()) == QStringLiteral("dog dog dog"), "Native bulk replacement failed");
  require(text(window) == QString::fromUtf8("dog dog \xf0\x9f\x98\x80 dog"), "Unicode bulk replacement failed");
  for (int i = 0; i < 3; ++i) child<QAction>(window, "undoAction")->trigger();
  require(text(window).startsWith(QStringLiteral("cat")), "Unicode replacement undo failed");
  window.activate_document(0);
  for (int i = 0; i < 3; ++i) child<QAction>(window, "undoAction")->trigger();
  require(text(window) == QStringLiteral("cat CAT cat"), "Native replacement undo failed");
  require(*window.current_jwp_document() == original_native, "Replacement/undo lost native metadata");
  require(window.query_histories().search.find(U"cat").has_value() && window.query_histories().replace.find(U"dog").has_value(), "Both histories were not remembered");
  require(window.save_query_history(directory.filePath(QStringLiteral("history.bin"))), "Search histories did not save");
  require(window.save_application_settings(directory.filePath(QStringLiteral("settings.cfg"))), "Search settings did not save");
  qt::MainWindow restored;
  require(restored.load_query_history(directory.filePath(QStringLiteral("history.bin"))) &&
          restored.query_histories().replace.find(U"dog").has_value(), "Replacement history did not reload");
  require(restored.load_application_settings(directory.filePath(QStringLiteral("settings.cfg"))) && restored.application_settings().search_all_files,
          "Search policies did not reload");
  replace->close(); QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  query->setText(QStringLiteral("missing")); click(*find, "searchFind");
  replace = open(window, true);
  pattern = child<QLineEdit>(*replace, "replaceFindText");
  replacement = child<QLineEdit>(*replace, "replacementText");
  require(replacement->text() == QStringLiteral("dog"), "An ordinary Find erased the last replacement");
  pattern->setText(QStringLiteral("cat"));
  replacement->clear(); key(replacement, Qt::Key_Up);
  require(replacement->text() == QStringLiteral("dog"), "Replacement history did not recall independently");
  key(replacement, Qt::Key_Down); require(replacement->text().isEmpty(), "Newer history did not return to a blank draft");
  replacement->setText(QString(400, QLatin1Char('x'))); key(replacement, Qt::Key_Up);
  require(replacement->text().size() == 400, "History navigation discarded an oversized draft");
  replacement->setText(QStringLiteral("dog"));
  QTimer::singleShot(0, [] {
    auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget()); require(dialog, "Replacement history missing");
    auto* list = child<QListWidget>(*dialog, ""); list->setCurrentRow(0);
    for (auto* button : dialog->findChildren<QPushButton*>())
      if (button->text() == QStringLiteral("Copy")) button->click();
    require(QApplication::clipboard()->text() == QStringLiteral("dog"), "History Copy changed replacement text");
    for (auto* button : dialog->findChildren<QPushButton*>())
      if (button->text() == QStringLiteral("Delete")) button->click();
    child<QDialogButtonBox>(*dialog, "")->button(QDialogButtonBox::Cancel)->click();
  });
  click(*replace, "replacementHistory");
  require(window.query_histories().replace.entries().empty() && replacement->text() == QStringLiteral("dog"),
          "History Delete/Cancel did not preserve draft and immediate deletion semantics");
  child<QCheckBox>(*replace, "searchAllFiles")->setChecked(false);
  set_text(window, QStringLiteral("cat cat cat"));
  answer(QMessageBox::Cancel); click(*replace, "searchReview");
  require(text(window) == QStringLiteral("cat cat cat"), "Cancelled review changed the document");
  set_text(window, QStringLiteral("cat cat cat"));
  answer(QMessageBox::No, [] { answer(QMessageBox::Yes); });
  child<QCheckBox>(*replace, "searchWrap")->setChecked(true);
  click(*replace, "searchReview");
  require(text(window) == QStringLiteral("cat cat dog"), "Reviewed Skip/Yes/circular stop replaced wrong match");
  child<QAction>(window, "undoAction")->trigger();
  set_text(window, QStringLiteral("cat cat cat"));
  answer(QMessageBox::YesToAll); click(*replace, "searchReview");
  require(text(window) == QStringLiteral("cat dog dog"), "Yes to All failed or matched the excluded starting position");
  set_text(window, QStringLiteral("cat cat"));
  answer(QMessageBox::Yes, [&] { window.active_editor()->setTextCursor(QTextCursor(window.active_editor()->document())); });
  click(*replace, "searchReplace");
  require(text(window) == QStringLiteral("cat cat"), "Confirmation replaced a changed selection");
  replacement->setText(QString::fromUtf8("\xf0\x9f\x98\x80"));
  click(*replace, "searchReplaceAll");
  require(text(window) == QStringLiteral("cat cat"), "Unrepresentable native replacement changed source");
  replacement->clear(); click(*replace, "searchReplaceAll"); require(text(window) == QStringLiteral(" "), "Empty replacement failed");
  child<QAction>(window, "undoAction")->trigger(); require(text(window).contains(QStringLiteral("cat")), "Deletion replacement was not undoable");
  find->grab().save(QStringLiteral("find-dialog.png")); replace->grab().save(QStringLiteral("replace-dialog.png"));
  child<QCheckBox>(*find, "searchKeepOpen")->setChecked(false);
  query->setText(QStringLiteral("absent")); QPointer<QDialog> closing(find); click(*find, "searchFind");
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  require(!closing, "Keep-open policy did not close modeless Find");
  child<QAction>(window, "findNextAction")->trigger();
  require(!window.application_settings().search_keep_open, "Find Next changed the keep-open preference");
}
void failure_and_lifetime() {
  int calls = 0;
  qt::FindReplaceDialog guarded(false, {}, std::make_shared<jwpqt::core::QueryHistories>(),
      [&](const qt::FindReplaceRequest&) { ++calls; return qt::FindReplaceResult{true, QStringLiteral("Found")}; });
  auto* composing = child<QLineEdit>(guarded, "findText");
  key(composing, Qt::Key_N, QStringLiteral("n"));
  QObject::connect(composing, &QLineEdit::textChanged, &guarded, [&] { click(guarded, "searchFind"); });
  click(guarded, "searchFind");
  require(calls == 1, "Finishing pending kana recursively submitted the search");
  qt::MainWindow window;
  window.new_document_tab(false);
  set_text(window, QStringLiteral("aaa"), 2);
  auto* find = open(window, false);
  child<QLineEdit>(*find, "findText")->setText(QStringLiteral("aa"));
  child<QCheckBox>(*find, "searchBackward")->setChecked(true);
  click(*find, "searchFind");
  require(window.active_editor()->textCursor().selectionStart() == 1, "Backward search lost overlapping matches");
  auto* replace = open(window, true);
  child<QLineEdit>(*replace, "replaceFindText")->setText(QStringLiteral("a"));
  child<QLineEdit>(*replace, "replacementText")->setText(QStringLiteral("b"));
  window.active_editor()->setReadOnly(true);
  click(*replace, "searchReplaceAll"); require(text(window) == QStringLiteral("aaa"), "Read-only replacement mutated data");
  window.active_editor()->setReadOnly(false);
  set_text(window, QStringLiteral("a a"));
  answer(QMessageBox::Yes, [&] { window.active_editor()->insertPlainText(QStringLiteral("EXTERNAL")); });
  click(*replace, "searchReplace");
  require(text(window).contains(QStringLiteral("EXTERNAL")) && !text(window).contains(QLatin1Char('b')),
          "Reentrant edit was overwritten after confirmation");
  set_text(window, QString::fromUtf8("\xf0\x9f\x98\x80 / \xf0\x9f\x98\x80"));
  const QString unicode_before = text(window);
  child<QLineEdit>(*replace, "replaceFindText")->setText(QString::fromUtf8("\xf0\x9f\x98\x80"));
  child<QLineEdit>(*replace, "replacementText")->setText(QString(QChar(0xfeff)) + QChar(0xa0));
  click(*replace, "searchReplaceAll");
  const QString literal = QString(QChar(0xfeff)) + QChar(0xa0);
  require(text(window) == literal + QStringLiteral(" / ") + literal, "Unicode replacement normalized literal characters");
  child<QAction>(window, "undoAction")->trigger(); child<QAction>(window, "undoAction")->trigger();
  require(text(window) == unicode_before, "Supplementary replacement undo changed Unicode");
  auto settings = window.application_settings();
  const auto encoded = qt::write_application_settings(settings);
  bool invalid = false;
  try { qt::read_application_settings("Search_AllFiles = bad\nsearch_all = true\n"); }
  catch (const std::exception&) { invalid = true; }
  require(invalid, "Invalid earlier search setting was accepted");
  auto restored = qt::read_application_settings("search_all = true\nsearch_nocase = false\nsearch_jascii = false\nsearch_wrap = true\nkeep_find = false\n");
  require(restored.search_all_files && !restored.search_ignore_case && !restored.search_ignore_width &&
          restored.search_wrap && !restored.search_keep_open, "Legacy search aliases did not round trip");
  restored = qt::read_application_settings(qt::write_application_settings(restored));
  require(restored.search_wrap && restored.search_all_files, "Search policy serialization failed");
  require(qt::write_application_settings(window.application_settings()) == encoded, "Staged settings read altered live options");

  qt::MainWindow japanese;
  const QString source = QString::fromUtf8("\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e \xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e");
  set_text(japanese, source);
  auto* japanese_replace = open(japanese, true);
  child<QLineEdit>(*japanese_replace, "replaceFindText")->setText(source.section(QLatin1Char(' '), 0, 0));
  const auto translated = QString::fromUtf8("\xe5\x9b\xbd\xe8\xaa\x9e");
  child<QLineEdit>(*japanese_replace, "replacementText")->setText(translated);
  click(*japanese_replace, "searchReplaceAll");
  require(text(japanese) == translated + QLatin1Char(' ') + translated, "Japanese native replacement failed");
  child<QAction>(japanese, "undoAction")->trigger(); child<QAction>(japanese, "undoAction")->trigger();
  require(text(japanese) == source, "Japanese native replacement undo failed");

  auto* owner = new qt::MainWindow;
  owner->new_document_tab(false); set_text(*owner, QStringLiteral("cat cat"));
  auto* dialog = open(*owner, true);
  child<QLineEdit>(*dialog, "replaceFindText")->setText(QStringLiteral("cat"));
  child<QLineEdit>(*dialog, "replacementText")->setText(QStringLiteral("dog"));
  QPointer<qt::MainWindow> weak(owner);
  QTimer::singleShot(0, [owner] { delete owner; });
  click(*dialog, "searchReview");
  require(!weak, "Search owner did not close during confirmation");

  owner = new qt::MainWindow;
  owner->new_document_tab(false); set_text(*owner, QStringLiteral("cat cat"));
  dialog = open(*owner, false);
  child<QLineEdit>(*dialog, "findText")->setText(QStringLiteral("cat")); click(*dialog, "searchFind");
  weak = owner;
  QTimer::singleShot(0, [owner] { delete owner; });
  click(*dialog, "searchHistory"); require(!weak, "History owner did not close safely");
}
int main(int argc, char** argv) {
  QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
  QApplication app(argc, argv); app.setQuitOnLastWindowClosed(false);
  try { workflows(); failure_and_lifetime(); return 0; }
  catch (const std::exception& error) { std::cerr << "find_replace_test: " << error.what() << '\n'; return 1; }
}
