// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <iostream>

#include <QApplication>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>

#include "kanji_count_dialog.h"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

jwpqt::core::JwpDocument document(std::initializer_list<std::uint16_t> text) {
  jwpqt::core::JwpDocument value;
  value.paragraphs.resize(1);
  value.paragraphs[0].text.assign(text);
  return value;
}

void test_dialog() {
  const auto first = document({0x3021, 0x3021, 0x3022, 0x0020});
  const auto second = document({0x3022, 0x3022, 0x3022});
  jwpqt::core::KanjiColorList color_list;
  require(color_list.add(0x3021), "Could not create count-dialog color list");
  std::u32string inserted;
  jwpqt::core::JisCode shown = 0;
  jwpqt::qt::KanjiCountDialog dialog(
      {&first, &second}, color_list, nullptr,
      [&](std::u32string text) { inserted = std::move(text); },
      [&](jwpqt::core::JisCode code) { shown = code; });
  require(dialog.count() && dialog.results().size() == 2 &&
              dialog.results()[0].code == 0x3021 &&
              dialog.results()[0].count == 2,
          "Count dialog returned wrong current-document results");

  jwpqt::qt::KanjiCountDisplayOptions options;
  options.all_documents = true;
  options.filter = jwpqt::core::KanjiCountFilter::kExcludeColorList;
  options.frequency = true;
  dialog.set_display_options(options);
  require(dialog.count() && dialog.results().size() == 1 &&
              dialog.results()[0].code == 0x3022 &&
              dialog.results()[0].count == 4,
          "Count dialog did not apply all-document/list filtering");
  auto* results =
      dialog.findChild<QListWidget*>(QStringLiteral("kanjiCountResults"));
  auto* status =
      dialog.findChild<QLabel*>(QStringLiteral("kanjiCountStatus"));
  require(results != nullptr && status != nullptr && results->count() == 1 &&
              status->text().contains(QStringLiteral("7 characters")),
          "Count dialog did not publish rows or summary");
  results->item(0)->setSelected(true);
  dialog.findChild<QPushButton*>(QStringLiteral("kanjiCountInsert"))->click();
  require(!inserted.empty(), "Count dialog did not invoke insertion callback");
  require(shown == 0, "Information callback ran without information data");
  options.filter = static_cast<jwpqt::core::KanjiCountFilter>(99);
  try {
    dialog.set_display_options(options);
  } catch (const jwpqt::core::KanjiInfoError&) {
    return;
  }
  require(false, "Count dialog accepted an invalid filter");
}

}  // namespace

int main(int argc, char* argv[]) {
  QApplication application(argc, argv);
  test_dialog();
  return EXIT_SUCCESS;
}
