// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include <QApplication>
#include <QClipboard>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>

#include "edict_results_window.h"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

jwpqt::qt::EdictResourceSearchResult result(
    QString label, std::u32string headword, std::u32string definition) {
  jwpqt::qt::EdictResourceSearchResult value;
  value.label = std::move(label);
  value.result.record.headword = std::move(headword);
  value.result.record.definitions.push_back(std::move(definition));
  return value;
}

void test_accumulate_copy_insert_and_clear() {
  jwpqt::qt::EdictResultsWindow window;
  jwpqt::qt::EdictResourceSearchReport first;
  first.results.push_back(result(QStringLiteral("One"), U"cat", U"first"));
  first.rejected = 2;
  jwpqt::qt::EdictResourceFailure visible;
  visible.message = QStringLiteral("missing");
  first.failures.push_back(visible);
  jwpqt::qt::EdictResourceFailure quiet;
  quiet.quiet = true;
  quiet.message = QStringLiteral("hidden");
  first.failures.push_back(quiet);
  window.append_report(std::move(first));

  jwpqt::qt::EdictResourceSearchReport second;
  auto dog = result(QStringLiteral("Two"), U"dog", U"second");
  dog.result.record.readings = {U"hound"};
  second.results.push_back(std::move(dog));
  second.rejected = 3;
  window.append_report(std::move(second));

  QListWidget* list =
      window.findChild<QListWidget*>(QStringLiteral("edictResultsList"));
  QLabel* status =
      window.findChild<QLabel*>(QStringLiteral("edictResultsStatus"));
  QPushButton* copy =
      window.findChild<QPushButton*>(QStringLiteral("edictResultsCopy"));
  QPushButton* insert =
      window.findChild<QPushButton*>(QStringLiteral("edictResultsInsert"));
  require(list != nullptr && status != nullptr && copy != nullptr &&
              insert != nullptr && window.result_count() == 2 &&
              list->count() == 2,
          "Results window did not append reports");
  require(status->text().contains(QStringLiteral("2 result")) &&
              status->text().contains(QStringLiteral("5 rejected")) &&
              status->text().contains(QStringLiteral("1 resource error")),
          "Results window status did not accumulate report counts");
  require(list->item(0)->toolTip() == QStringLiteral("One") &&
              list->item(1)->text() ==
                  QStringLiteral("dog [hound] /second/"),
          "Results window did not render record/source metadata");

  std::u32string inserted;
  window.set_insert_handler([&inserted](const std::u32string& text) {
    inserted = text;
    return true;
  });
  list->item(1)->setSelected(true);
  list->item(0)->setSelected(true);
  QApplication::processEvents();
  copy->click();
  require(QApplication::clipboard()->text() ==
              QStringLiteral("cat /first/\ndog [hound] /second/"),
          "Results window did not copy selected rows in display order");
  insert->click();
  require(inserted == U"cat /first/\ndog [hound] /second/",
          "Results window did not insert selected rows in display order");

  window.clear_results();
  require(window.result_count() == 0 && list->count() == 0 &&
              !copy->isEnabled() && !insert->isEnabled() &&
              status->text().contains(QStringLiteral("0 result")),
          "Results window did not reset accumulated state");
}

void test_insert_failures_are_contained() {
  jwpqt::qt::EdictResultsWindow window;
  jwpqt::qt::EdictResourceSearchReport report;
  report.results.push_back(result(QStringLiteral("One"), U"cat", U"first"));
  window.append_report(std::move(report));
  QListWidget* list =
      window.findChild<QListWidget*>(QStringLiteral("edictResultsList"));
  QLabel* status =
      window.findChild<QLabel*>(QStringLiteral("edictResultsStatus"));
  QPushButton* insert =
      window.findChild<QPushButton*>(QStringLiteral("edictResultsInsert"));
  require(list != nullptr && status != nullptr && insert != nullptr,
          "Results window failure controls are missing");
  list->item(0)->setSelected(true);

  window.set_insert_handler([](const std::u32string&) -> bool {
    throw std::runtime_error("expected failure");
  });
  insert->click();
  require(status->text().contains(QStringLiteral("expected failure")),
          "Standard insert failure escaped or was not displayed");
  window.set_insert_handler([](const std::u32string&) -> bool { throw 7; });
  insert->click();
  require(status->text().contains(QStringLiteral("unknown error")),
          "Unknown insert failure escaped the UI boundary");
}

}  // namespace

int main(int argc, char** argv) {
  QApplication application(argc, argv);
  try {
    test_accumulate_copy_insert_and_clear();
    test_insert_failures_are_contained();
  } catch (const std::exception& error) {
    std::cerr << "edict_results_window_test: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
