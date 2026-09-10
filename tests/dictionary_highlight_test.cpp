// SPDX-License-Identifier: GPL-2.0-or-later
#include "edict_lookup_dialog.h"
#include "edict_results_window.h"
#include "source_highlight.h"
#include "text_bridge.h"
#include "jwp_editor.h"
#include "main_window.h"
#include "file_io.h"
#include <QAction>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include <QApplication>
#include <QClipboard>
#include <QListWidget>
#include <QLabel>
#include <QKeyEvent>
#include <QPushButton>
#include <QTextEdit>
#include <iostream>
#include <stdexcept>

namespace qt = jwpqt::qt;
namespace core = jwpqt::core;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  try {
    qt::EdictResourceSearchReport report;
    for (const auto& name : {U"normal", U"special", U"adaptive"}) {
      qt::EdictResourceSearchResult entry;
      entry.label = QStringLiteral("source");
      entry.result.record.headword = name;
      entry.result.record.definitions = {U"meaning\u00a0\ufeff"};
      entry.highlighted = report.results.size() == 1;
      report.results.push_back(entry);
    }
    report.results[0].result.priority = true;
    report.sections = {{core::EdictSearchStage::kDirect, 0, true},
                       {core::EdictSearchStage::kContingent, 2}, {core::EdictSearchStage::kAdaptive, 2}};
    bool fail = false;
    std::u32string inserted;
    qt::EdictLookupDialog dialog([&](const auto&, const auto&, bool) {
      if (fail) throw std::runtime_error("expected search failure");
      return report;
    }, [&](const auto& text) { inserted = text; return true; });
    dialog.set_highlight_color(QColor("#0000cc"));
    dialog.set_query(U"cat");
    if (!dialog.search()) throw std::runtime_error(dialog.findChild<QLabel*>(QStringLiteral("edictStatus"))->text().toStdString());
    auto* text = dialog.findChild<QTextEdit*>(QStringLiteral("edictResults"));
    require(text && !text->extraSelections().empty(), "Highlights missing");
    QString highlighted;
    for (const auto& selection : text->extraSelections()) {
      require(selection.format.foreground().color() == QColor("#0000cc"), "Configured highlight missing");
      highlighted += selection.cursor.selectedText();
    }
    require(highlighted.contains("special") && highlighted.contains("Advanced") &&
            !highlighted.contains("normal") && !highlighted.contains("adaptive") &&
            !highlighted.contains("Priority") && !highlighted.contains("No Exact"), "Wrong entries or labels highlighted");
    QTextCursor selected(text->document());
    const auto position = text->toPlainText().indexOf("special");
    selected.setPosition(position); selected.setPosition(position + 7, QTextCursor::KeepAnchor);
    text->setTextCursor(selected);
    const auto original = qt::document_plain_text(*text->document());
    const int revision = text->document()->revision();
    auto* document = text->document();
    dialog.set_highlight_color(QColor("#006000"));
    require(text->textCursor().selectedText() == "special" && text->document() == document &&
            document->revision() == revision && qt::document_plain_text(*document) == original, "Live color changed content or selection");
    require(dialog.insert_selected() && inserted.find(U"special") != std::u32string::npos &&
            inserted.find(U'\u00a0') != std::u32string::npos, "Highlight broke canonical insertion");
    dialog.copy_selected();
    require(QApplication::clipboard()->text() == "special", "Highlight broke Copy");
    fail = true;
    require(!dialog.search() && text->document() == document && text->textCursor().selectedText() == "special", "Failure discarded selection");
    fail = false;
    require(dialog.sort_results(), "Sort failed");
    highlighted.clear();
    for (const auto& selection : text->extraSelections()) highlighted += selection.cursor.selectedText();
    require(highlighted.contains("special") && !highlighted.contains("Advanced") && !highlighted.contains("normal"), "Sort lost provenance or retained labels");

    QPalette dark = dialog.palette();
    dark.setColor(QPalette::Base, QColor("#202325")); dark.setColor(QPalette::Text, Qt::white);
    dialog.setPalette(dark); QApplication::processEvents();
    const auto expected = qt::source_highlight_color(text->palette(), QColor("#006000"));
    for (const auto& selection : text->extraSelections())
      require(selection.format.foreground().color() == expected, "Dark theme stale highlight");
    dialog.show(); QApplication::processEvents();
    require(dialog.grab().save(QDir::current().filePath("dictionary-highlights.png")), "Highlight capture failed");

    qt::EdictResultsWindow accumulated;
    accumulated.set_highlight_color(QColor("#0000cc"));
    accumulated.append_report(report);
    auto* list = accumulated.findChild<QListWidget*>(QStringLiteral("edictResultsList"));
    require(list->count() == 3 && list->item(1)->foreground().color() == QColor("#0000cc") &&
            list->item(0)->foreground().style() == Qt::NoBrush, "Accumulated provenance lost");
    list->setCurrentRow(1);
    const auto canonical = list->item(1)->text();
    accumulated.set_insert_handler([&](const auto& value) { inserted = value; return true; });
    accumulated.set_highlight_color(QColor("#006000"));
    require(list->item(1)->isSelected() && list->item(1)->text() == canonical, "Accumulated recolor changed selection/text");
    accumulated.findChild<QPushButton*>(QStringLiteral("edictResultsInsert"))->click();
    require(qt::to_qstring(inserted) == canonical, "Accumulated insertion changed");
    QKeyEvent copy_headword(QEvent::KeyPress, Qt::Key_E,
                            Qt::ControlModifier);
    QApplication::sendEvent(list, &copy_headword);
    require(QApplication::clipboard()->text() == QStringLiteral("special"),
            "Accumulated Ctrl+E did not copy the structured headword");
    QKeyEvent copy_reading(QEvent::KeyPress, Qt::Key_R,
                           Qt::ControlModifier);
    QApplication::sendEvent(list, &copy_reading);
    require(QApplication::clipboard()->text() == QStringLiteral("special"),
            "Accumulated reading Copy did not fall back to the headword");
    list->clearSelection();
    list->setCurrentItem(nullptr);
    QApplication::clipboard()->setText(QStringLiteral("stale"));
    QApplication::sendEvent(list, &copy_headword);
    require(QApplication::clipboard()->text().isEmpty(),
            "Accumulated field Copy retained stale clipboard text without a row");
    list->setCurrentRow(1);
    QKeyEvent select_row(QEvent::KeyPress, Qt::Key_W,
                         Qt::ControlModifier | Qt::ShiftModifier);
    QApplication::sendEvent(list, &select_row);
    require(list->selectedItems() == QList<QListWidgetItem*>{list->item(1)},
            "Accumulated Ctrl+Shift+W did not select the current result row");
    accumulated.setPalette(dark); QApplication::processEvents();
    require(list->item(1)->foreground().color() == qt::source_highlight_color(list->palette(), QColor("#006000")), "Accumulated theme stale");

    // Identical records retain the first provenance when Sort removes duplicates.
    report.results = {report.results[1], report.results[1]};
    report.results[1].highlighted = false; report.sections.clear();
    require(dialog.search() && dialog.sort_results() && dialog.report().results.size() == 1 &&
            dialog.report().results.front().highlighted && !text->extraSelections().empty(), "Dedup lost first provenance");
    std::swap(report.results[0], report.results[1]);
    require(dialog.search() && dialog.sort_results() && !dialog.report().results.front().highlighted &&
            text->extraSelections().empty(), "Dedup invented special provenance");

    QTemporaryDir directory;
    require(directory.isValid(), "Temporary directory failed");
    QFile dictionary(directory.filePath("words"));
    require(dictionary.open(QIODevice::WriteOnly) && dictionary.write("cat /special/\n") == 14, "Fixture write failed");
    dictionary.close();
    core::EdictRegistry registry;
    core::EdictRegistryEntry resource;
    resource.label = u"Classical"; resource.path = u"words";
    resource.encoding = core::EdictRegistryEncoding::kUtf8;
    resource.special = core::EdictRegistrySpecial::kClassical;
    resource.searched = resource.keep = true;
    registry.entries.push_back(resource);
    qt::write_edict_registry_file(directory.filePath("dict.cfg"), registry);
    qt::MainWindow window;
    auto settings = window.application_settings();
    settings.dictionary.classical = true;
    settings.color_refs[0] = 0x00cc0000;
    require(window.apply_application_settings(settings) && window.load_edict_configuration(directory.filePath("dict.cfg")), "Resource setup failed");
    window.findChild<QAction*>(QStringLiteral("edictLookupAction"))->trigger();
    auto* lookup = dynamic_cast<qt::EdictLookupDialog*>(window.findChild<QDialog*>(QStringLiteral("edictLookupDialog")));
    require(lookup != nullptr, "Lookup missing");
    lookup->set_query(U"cat");
    require(lookup->search(), "Actual special search failed");
    auto* view = lookup->findChild<QTextEdit*>(QStringLiteral("edictResults"));
    auto* aggregate = window.findChild<QListWidget*>(QStringLiteral("edictResultsList"));
    require(lookup->report().results.front().highlighted && !view->extraSelections().empty() &&
            view->extraSelections().front().format.foreground().color() == QColor("#0000cc") &&
            aggregate->item(0)->foreground().color() == QColor("#0000cc"), "Initial workspace policy not used");
    auto* before = view->document(); const auto selected_before = view->textCursor().selectedText();
    settings.color_refs[0] = 0x00006000;
    require(window.apply_application_settings(settings) && view->document() == before &&
            view->textCursor().selectedText() == selected_before &&
            view->extraSelections().front().format.foreground().color() == QColor("#006000") &&
            aggregate->item(0)->foreground().color() == QColor("#006000"), "Live workspace policy lost views or color");
    require(lookup->insert_selected() && !qt::document_plain_text(*window.active_editor()->document()).isEmpty(), "Actual insertion failed");
    window.findChild<QAction*>(QStringLiteral("undoAction"))->trigger();
    require(qt::document_plain_text(*window.active_editor()->document()).isEmpty(), "Actual insertion undo failed");
    std::cout << "Dictionary highlight workflows passed\n";
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
