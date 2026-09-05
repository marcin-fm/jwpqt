// SPDX-License-Identifier: GPL-2.0-or-later

#include "edict_lookup_dialog.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>

#include "jwpqt/core/jwp_text_codec.h"

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

  auto* list = dialog.findChild<QListWidget*>(QStringLiteral("edictResults"));
  auto* status = dialog.findChild<QLabel*>(QStringLiteral("edictStatus"));
  require(list != nullptr && status != nullptr && list->count() == 2 &&
              list->item(0)->text() ==
                  QStringLiteral("\u3042 [\u3044] /cat/feline/") &&
              list->item(0)->toolTip() == QStringLiteral("Main"),
          "Dictionary results were not rendered in search order");
  require(status->text().contains(QStringLiteral("2 matches")) &&
              status->text().contains(QStringLiteral("3 rejected")) &&
              status->text().contains(QStringLiteral("broken: unavailable")) &&
              !status->text().contains(QStringLiteral("hidden")),
          "Dictionary status did not expose the bounded visible diagnostics");

  list->item(1)->setSelected(true);
  dialog.copy_selected();
  require(QApplication::clipboard()->text() ==
              QStringLiteral("\u3042 [\u3044] /cat/feline/\nAlice /name/"),
          "Dictionary result copy did not preserve selected row order");
  list->setCurrentRow(0);
  require(dialog.insert_selected() &&
              inserted == U"\u3042 [\u3044] /cat/feline/",
          "Dictionary insertion callback did not receive the full result row");
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
}

}  // namespace

int main(int argc, char** argv) {
  QApplication application(argc, argv);
  try {
    test_search_render_status_copy_and_insert();
    test_empty_invalid_and_failed_search_are_contained();
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << "edict_lookup_dialog_test: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
