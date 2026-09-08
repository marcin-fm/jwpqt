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
#include <QIntValidator>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMimeData>
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
    test_management_commands();
    test_search_render_status_copy_and_insert();
    test_compact_presentation();
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
