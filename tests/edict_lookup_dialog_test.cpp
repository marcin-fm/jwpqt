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
#include <QLabel>
#include <QInputMethodEvent>
#include <QIntValidator>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QPointer>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>

#include "jwpqt/core/jwp_text_codec.h"
#include "kana_input_field.h"

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

void test_search_render_status_copy_and_insert() {
  jwpqt::core::JwpText received_query;
  jwpqt::qt::EdictLookupOptions received_options;
  std::u32string inserted;
  jwpqt::qt::EdictLookupDialog dialog(
      [&](const jwpqt::core::JwpText& query,
          const jwpqt::qt::EdictLookupOptions& options) {
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

void test_empty_invalid_and_failed_search_are_contained() {
  int searches = 0;
  jwpqt::qt::EdictLookupDialog dialog(
      [&](const jwpqt::core::JwpText&,
          const jwpqt::qt::EdictLookupOptions&)
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
         const jwpqt::qt::EdictLookupOptions&)
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
         const jwpqt::qt::EdictLookupOptions&) {
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

void test_search_controls() {
  using namespace jwpqt::qt;
  auto settings = std::make_shared<EdictLookupOptions>();
  EdictLookupOptions received;
  int searches = 0;
  const auto search = [&](const jwpqt::core::JwpText&, const EdictLookupOptions& options) {
    received = options;
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
  }
  EdictLookupDialog reopened(search, {}, nullptr, {}, settings);
  reopened.set_query(U"cat");
  require(reopened.findChild<QCheckBox*>(QStringLiteral("edictEnd"))->isChecked() &&
              !reopened.findChild<QCheckBox*>(QStringLiteral("edictBeginning"))->isChecked() &&
              reopened.search() && received.full_ascii && received.jascii_to_ascii &&
              received.advanced_show_all && !received.advanced && !received.advanced_always,
          "Closing the dictionary discarded its retained search policies");
  EdictLookupDialog independent(search);
  independent.set_query(U"cat");
  require(independent.search() && received.require_beginning && !received.require_end &&
              !received.full_ascii && !received.jascii_to_ascii,
          "Independent dictionary state inherited another owner's settings");
}

void test_query_input_modes() {
  int searches = 0;
  jwpqt::core::JwpText received;
  jwpqt::qt::EdictLookupDialog dialog(
      [&](const jwpqt::core::JwpText& query, const jwpqt::qt::EdictLookupOptions&) {
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
  EdictLookupDialog dialog([&](const auto&, const auto&) {
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

void test_result_character_navigation() {
  std::vector<char32_t> inspected;
  int searches = 0;
  jwpqt::qt::EdictLookupDialog dialog(
      [&](const jwpqt::core::JwpText&, const jwpqt::qt::EdictLookupOptions&) {
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
    test_search_render_status_copy_and_insert();
    test_empty_invalid_and_failed_search_are_contained();
    test_search_controls();
    test_query_input_modes();
    test_query_overwrite();
    test_result_character_navigation();
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << "edict_lookup_dialog_test: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
