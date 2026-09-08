// SPDX-License-Identifier: GPL-2.0-or-later

#include <iostream>
#include <stdexcept>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QFile>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QTemporaryDir>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>

#include "application_settings_dialog.h"
#include "edict_lookup_dialog.h"
#include "edict_resource_search.h"
#include "file_io.h"
#include "jwp_editor.h"
#include "jwpqt/core/edict_filter.h"
#include "jwpqt/core/utf8.h"
#include "main_window.h"
#include "text_bridge.h"

namespace {
void require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}

void write(const QString& path, const std::string& bytes) {
  QFile file(path);
  require(file.open(QIODevice::WriteOnly) && file.write(bytes.data(), bytes.size()) ==
      static_cast<qint64>(bytes.size()), "Could not write category fixture");
}

void run(const QString& directory) {
  using namespace jwpqt;
  std::string data;
  for (auto tag : core::kEdictCategoryTags)
    data += core::encode_utf8(U"\u3042 /(" + std::u32string(tag) + U") cat tagged/\n");
  data += core::encode_utf8(U"\u3044 /cat ordinary/\n");
  write(directory + "/edict", data);
  std::string names = "(s) cat person/(p) cat place/";
  for (auto tag : core::kEdictCategoryTags)
    names += "(" + core::encode_utf8(tag) + ") cat classified/";
  write(directory + "/names", core::encode_utf8(U"\u3046 /") + names + "\n");
  core::EdictRegistry registry;
  core::EdictRegistryEntry entry;
  entry.label = u"Categories"; entry.path = u"edict";
  entry.encoding = core::EdictRegistryEncoding::kUtf8;
  entry.searched = entry.keep = true;
  registry.entries.push_back(entry);
  entry.label = u"Names"; entry.path = u"names";
  entry.names = core::EdictRegistryNames::kNamesOnly;
  registry.entries.push_back(entry);
  const QString registry_path = directory + "/dict.cfg";
  qt::write_edict_registry_file(registry_path, registry);
  qt::MainWindow window;
  require(window.load_edict_configuration(registry_path, qt::OpenMode::kNonInteractive), "Resource load failed");
  auto preferences = window.application_settings();
  preferences.dictionary.automatic_search = false;
  preferences.dictionary_extra_exclusions = 0x80000000U;
  require(window.apply_application_settings(preferences), "Initial preferences failed");
  auto* action = window.findChild<QAction*>("edictLookupAction");
  require(action, "Dictionary action is missing"); action->trigger();
  auto* lookup = dynamic_cast<qt::EdictLookupDialog*>(window.findChild<QDialog*>("edictLookupDialog"));
  require(lookup, "Lookup did not open");
  auto* results = lookup->findChild<QTextEdit*>("edictResults");
  auto* query = lookup->findChild<QLineEdit*>("edictQuery");
  auto* options = lookup->findChild<QToolButton*>("edictOptions");
  require(results && query && options, "Lookup controls missing");
  lookup->set_query(U"cat");
  require(lookup->search() && lookup->report().results.size() == 22, "Default filters changed results");

  for (std::size_t i = 0; i < core::kEdictCategoryTags.size(); ++i) {
    const auto bit = std::uint32_t{1} << (i + 4);
    const QPointer<QTextDocument> previous(results->document());
    const auto history = window.query_histories().dictionary.entries();
    const int anchor = results->textCursor().anchor(), position = results->textCursor().position();
    const auto edit_options = [&](bool accept) {
      bool visited = false;
      QTimer::singleShot(0, &window, [&] {
        auto* dialog = window.findChild<QDialog*>("applicationSettingsDialog");
        auto* list = dialog ? dialog->findChild<QListWidget*>("settingsDictionaryCategories") : nullptr;
        if (!list || list->count() != 21) { if (dialog) dialog->reject(); return; }
        for (int row = 0; row < list->count(); ++row) {
          require(list->item(row)->data(Qt::UserRole).toUInt() == (std::uint32_t{1} << (row + 4)),
                  "UI tag order differs from legacy wire bits");
          list->item(row)->setCheckState(row == static_cast<int>(i) ? Qt::Checked : Qt::Unchecked);
        }
        list->scrollToItem(list->item(static_cast<int>(i)));
        if (i == 20 && accept) {
          auto* scroll = dialog->findChild<QScrollArea*>("settingsDictionaryScroll");
          scroll->ensureWidgetVisible(list);
          QApplication::processEvents();
          dialog->grab().save("dictionary-category-options.png");
        }
        visited = true;
        if (accept) dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
        else dialog->reject();
      });
      options->click();
      require(visited, "Category Options was not accessible");
    };
    const auto before = qt::write_application_settings(window.application_settings());
    edit_options(false);
    require(qt::write_application_settings(window.application_settings()) == before,
            "Cancelled category settings changed policy");
    edit_options(true);
    require(window.application_settings().dictionary.category_exclusions == bit &&
        window.application_settings().dictionary_extra_exclusions == 0x80000000U &&
        previous && results->document() == previous && results->textCursor().anchor() == anchor &&
        results->textCursor().position() == position && query->text() == QStringLiteral("cat") &&
        window.query_histories().dictionary.entries() == history,
        "Category acceptance changed the current query, results, history or unrelated mask");
    require(lookup->search() && lookup->report().results.size() == 21, "A native category filter is not active");
    for (const auto& result : lookup->report().results)
      require(result.result.record.definitions != std::vector<std::u32string>{
          U"(" + std::u32string(core::kEdictCategoryTags[i]) + U") cat tagged"},
          "Native results retained the excluded category");
    require(lookup->insert_selected() && !window.active_editor()->toPlainText().isEmpty(),
            "Filtering lost canonical insertion ownership");
    window.findChild<QAction*>("undoAction")->trigger();
    require(!window.document_modified() && window.active_editor()->toPlainText().isEmpty(),
            "Filtered insertion failed one-step undo");

    qt::EdictResourceSearchOptions names_options;
    names_options.personal_names = names_options.place_names = true;
    names_options.search.name_filter.category_exclusions = bit;
    const auto report = qt::search_edict_resources(*window.edict_resources(), directory,
        {'c', 'a', 't'}, names_options);
    int name_passes = 0;
    for (const auto& result : report.results) {
      if (result.registry_index != 1) continue;
      ++name_passes;
      for (const auto& definition : result.result.record.definitions)
        require(definition != U"(" + std::u32string(core::kEdictCategoryTags[i]) + U") cat classified",
                "Optional personal/place pass lost category exclusions");
    }
    require(name_passes == 2, "Category filters suppressed an optional names pass");
  }
  preferences = window.application_settings();
  preferences.dictionary.category_exclusions = core::kEdictCategoryMask;
  query->deselect(); query->setCursorPosition(query->text().size());
  QKeyEvent k(QEvent::KeyPress, Qt::Key_K, Qt::NoModifier, "k");
  QApplication::sendEvent(query, &k);
  const auto saved_history = window.query_histories().dictionary.entries();
  const QPointer<QTextDocument> saved_results(results->document());
  require(window.apply_application_settings(preferences) && query->text() == QStringLiteral("cat") &&
              saved_results && results->document() == saved_results &&
              window.query_histories().dictionary.entries() == saved_history,
          "Applying combined exclusions disturbed pending input, results or history");
  QKeyEvent a(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, "a");
  QApplication::sendEvent(query, &a);
  require(query->text() == QStringLiteral("cat\u304b"), "Category settings lost pending kana");
  auto invalid = preferences;
  invalid.dictionary.category_exclusions |= 0x80000000U;
  require(!window.apply_application_settings(invalid) &&
              window.application_settings().dictionary.category_exclusions == core::kEdictCategoryMask,
          "Invalid category settings changed the live policy");
  lookup->set_query(U"cat");
  require(lookup->search() &&
              lookup->report().results.size() == 1 &&
              lookup->report().results.front().result.record.definitions ==
                  std::vector<std::u32string>{U"cat ordinary"}, "Combined exclusions dropped unrelated content");
  const QString path = directory + "/settings.cfg";
  require(window.save_application_settings(path), "Category settings could not be saved");
  qt::MainWindow restored;
  require(restored.load_application_settings(path) &&
              restored.application_settings().dictionary.category_exclusions == core::kEdictCategoryMask &&
              restored.application_settings().dictionary_extra_exclusions == 0x80000000U,
          "A new owner lost persisted category settings");
  const QPointer<QTextDocument> final_document(results->document());
  lookup->set_query(U"ca");
  require(!lookup->search() && final_document && results->document() == final_document,
          "Failed filtered search discarded previous results");
  bool confirmed = false;
  QTimer::singleShot(0, &window, [&] {
    auto* box = window.findChild<QMessageBox*>();
    if (box) { confirmed = true; box->button(QMessageBox::Yes)->click(); }
  });
  window.findChild<QAction*>("defaultSettingsAction")->trigger();
  require(confirmed && window.application_settings().dictionary.category_exclusions == 0 &&
              window.application_settings().dictionary_extra_exclusions == 0x80000000U,
          "Defaults did not reset known categories while preserving unknown bits");
  qt::EdictResourceSearchOptions invalid_search;
  invalid_search.search.name_filter.category_exclusions = 0x02000000U;
  bool rejected = false;
  try { (void)qt::search_edict_resources({}, directory, {'c', 'a', 't'}, invalid_search); }
  catch (const core::EdictSearchError&) { rejected = true; }
  require(rejected, "Empty resources bypassed category-mask validation");
}
}  // namespace

int main(int argc, char** argv) {
  QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
  QApplication app(argc, argv);
  app.setQuitOnLastWindowClosed(false);
  try {
    QTemporaryDir directory;
    require(directory.isValid(), "Temporary directory unavailable");
    run(directory.path());
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n'; return 1;
  }
  return 0;
}
