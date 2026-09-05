// SPDX-License-Identifier: GPL-2.0-or-later

#include "wnn_user_dictionary_dialog.h"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include <QApplication>
#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTimer>

namespace {

using jwpqt::core::JwpText;
using jwpqt::core::WnnUserDictionary;
using jwpqt::core::WnnUserDictionaryError;
using jwpqt::core::WnnUserEntry;
using jwpqt::qt::WnnUserDictionaryDialog;

class PromptTestDialog : public WnnUserDictionaryDialog {
 public:
  using WnnUserDictionaryDialog::WnnUserDictionaryDialog;

  std::optional<WnnUserEntry> prompt(
      const std::optional<WnnUserEntry>& initial) {
    return prompt_for_entry(initial);
  }
};

void require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

WnnUserEntry entry(JwpText reading, JwpText candidate) {
  return WnnUserEntry{std::move(reading), '*', {std::move(candidate)}};
}

void test_editing_and_save() {
  const WnnUserEntry first = entry({0x242b}, {0x3021});
  const WnnUserEntry second = entry({0x2422}, {0x3022});
  const WnnUserEntry replacement = entry({0x2424}, {0x3023});
  WnnUserDictionary saved;
  int saves = 0;
  WnnUserDictionaryDialog dialog(
      WnnUserDictionary::from_entries({first}),
      [&](WnnUserDictionary dictionary) {
        ++saves;
        saved = std::move(dictionary);
        return true;
      });

  require(dialog.add_entry(second) == 1 && dialog.entries().size() == 2,
          "Dialog did not append a user entry");
  require(dialog.move_entry_up(1) && !dialog.move_entry_up(0) &&
              dialog.entries()[0] == second,
          "Dialog did not preserve explicit move semantics");
  dialog.replace_entry(1, replacement);
  dialog.sort_entries();
  require(dialog.entries().size() == 2,
          "Dialog sort changed the entry count");
  dialog.erase_entry(1);
  require(dialog.entries().size() == 1 && dialog.save_changes() &&
              saves == 1 && saved.entries() == dialog.entries(),
          "Dialog did not publish its complete edited dictionary");
}

void test_import_insert_and_failed_save() {
  const WnnUserEntry first = entry({0x2422}, {0x3021});
  const WnnUserEntry imported = entry({0x242b}, {0x3022});
  std::optional<WnnUserEntry> inserted;
  WnnUserDictionaryDialog dialog(
      WnnUserDictionary::from_entries({first}),
      [](WnnUserDictionary) { return false; },
      [&](const WnnUserEntry& value) { inserted = value; });
  dialog.append_dictionary(WnnUserDictionary::from_entries({imported}));
  require(dialog.entries().size() == 2,
          "Dialog did not append an imported dictionary");

  QListWidget* list =
      dialog.findChild<QListWidget*>(QStringLiteral("wnnUserEntries"));
  require(list != nullptr && list->count() == 2,
          "Dialog did not render imported entries");
  list->setCurrentRow(1);
  require(dialog.insert_selected() && inserted == imported,
          "Dialog did not insert the selected user entry");

  const std::vector<WnnUserEntry> before = dialog.entries();
  require(!dialog.save_changes() && dialog.entries() == before,
          "Failed dictionary save changed the working entries");
}

void test_invalid_edit_is_atomic() {
  const WnnUserEntry first = entry({0x2422}, {0x3021});
  WnnUserDictionaryDialog dialog(WnnUserDictionary::from_entries({first}),
                                 [](WnnUserDictionary) { return true; });
  const std::vector<WnnUserEntry> before = dialog.entries();
  bool rejected = false;
  try {
    dialog.replace_entry(0, WnnUserEntry{{0x2422}, '?', {{0x3022}}});
  } catch (const WnnUserDictionaryError&) {
    rejected = true;
  }
  require(rejected && dialog.entries() == before,
          "Invalid dialog edit partially changed the working dictionary");
}

void test_imported_inflection_round_trip() {
  const WnnUserEntry imported{{0x2422, 0x246b},
                              '1',
                              {{0x3021, 0x246b}, {0x3022}}};
  PromptTestDialog dialog(WnnUserDictionary::from_entries({imported}),
                          [](WnnUserDictionary) { return true; });
  QTimer::singleShot(0, [] {
    QWidget* modal = QApplication::activeModalWidget();
    if (modal == nullptr) {
      return;
    }
    QDialogButtonBox* buttons = modal->findChild<QDialogButtonBox*>();
    if (buttons != nullptr) {
      buttons->button(QDialogButtonBox::Ok)->click();
    }
  });
  const std::optional<WnnUserEntry> edited = dialog.prompt(imported);
  require(edited.has_value() && *edited == imported,
          "Confirming an imported inflected entry changed its wire semantics");
}

void test_insert_exception_is_contained() {
  const WnnUserEntry first = entry({0x2422}, {0x3021});
  WnnUserDictionaryDialog dialog(
      WnnUserDictionary::from_entries({first}),
      [](WnnUserDictionary) { return true; },
      [](const WnnUserEntry&) { throw std::runtime_error("insert exploded"); });
  QListWidget* list =
      dialog.findChild<QListWidget*>(QStringLiteral("wnnUserEntries"));
  QLabel* status =
      dialog.findChild<QLabel*>(QStringLiteral("wnnUserStatus"));
  require(list != nullptr && status != nullptr,
          "Insert failure fixture has no dialog controls");
  list->setCurrentRow(0);
  require(!dialog.insert_selected() &&
              status->text().contains(QStringLiteral("insert exploded")) &&
              dialog.entries() == std::vector<WnnUserEntry>{first},
          "Insert callback failure escaped or changed the working dictionary");
}

}  // namespace

int main(int argc, char** argv) {
  QApplication application(argc, argv);
  try {
    test_editing_and_save();
    test_import_insert_and_failed_save();
    test_invalid_edit_is_atomic();
    test_imported_inflection_round_trip();
    test_insert_exception_is_contained();
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << "wnn_user_dictionary_dialog_test: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
