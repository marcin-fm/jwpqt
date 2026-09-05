// SPDX-License-Identifier: GPL-2.0-or-later

#include "edict_user_dictionary_dialog.h"

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

using jwpqt::core::EdictUserDictionary;
using jwpqt::core::EdictUserDictionaryError;
using jwpqt::core::EdictUserEntry;
using jwpqt::core::LegacyCodePage;
using jwpqt::qt::EdictUserDictionaryDialog;

class PromptTestDialog : public EdictUserDictionaryDialog {
 public:
  using EdictUserDictionaryDialog::EdictUserDictionaryDialog;

  std::optional<EdictUserEntry> prompt(
      const std::optional<EdictUserEntry>& initial) {
    return prompt_for_entry(initial);
  }
};

void require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

EdictUserEntry entry(jwpqt::core::JwpText reading, std::u32string meaning,
                     jwpqt::core::JwpText headword = {}) {
  return jwpqt::core::make_edict_user_entry(
      std::move(reading), std::move(headword), std::move(meaning));
}

void test_editing_and_save() {
  const EdictUserEntry first = entry({0x242b}, U"first");
  const EdictUserEntry second = entry({0x2422}, U"second");
  const EdictUserEntry replacement = entry({0x2424}, U"replacement");
  EdictUserDictionary saved;
  int saves = 0;
  EdictUserDictionaryDialog dialog(
      EdictUserDictionary::from_entries({first}), LegacyCodePage::k1252,
      [&](EdictUserDictionary dictionary) {
        ++saves;
        saved = std::move(dictionary);
        return true;
      });

  require(dialog.add_entry(second) == 1 && dialog.entries().size() == 2,
          "Dialog did not append a user dictionary entry");
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
  const EdictUserEntry first = entry({0x2422}, U"first");
  const EdictUserEntry imported = entry({0x242b}, U"imported", {0x3021});
  std::optional<EdictUserEntry> inserted;
  EdictUserDictionaryDialog dialog(
      EdictUserDictionary::from_entries({first}), LegacyCodePage::k1252,
      [](EdictUserDictionary) { return false; },
      [&](const EdictUserEntry& value) { inserted = value; });
  dialog.append_dictionary(EdictUserDictionary::from_entries({imported}));
  require(dialog.entries().size() == 2,
          "Dialog did not append an imported dictionary");

  QListWidget* list =
      dialog.findChild<QListWidget*>(QStringLiteral("edictUserEntries"));
  require(list != nullptr && list->count() == 2,
          "Dialog did not render imported entries");
  list->setCurrentRow(1);
  require(dialog.insert_selected() && inserted == imported,
          "Dialog did not insert the selected user entry");

  const std::vector<EdictUserEntry> before = dialog.entries();
  require(!dialog.save_changes() && dialog.entries() == before,
          "Failed dictionary save changed the working entries");
}

void test_invalid_edit_is_atomic() {
  const EdictUserEntry first = entry({0x2422}, U"first");
  EdictUserDictionaryDialog dialog(
      EdictUserDictionary::from_entries({first}), LegacyCodePage::k1252,
      [](EdictUserDictionary) { return true; });
  const std::vector<EdictUserEntry> before = dialog.entries();
  bool rejected = false;
  try {
    dialog.replace_entry(0, EdictUserEntry{{}, {}, U"bad"});
  } catch (const EdictUserDictionaryError&) {
    rejected = true;
  }
  require(rejected && dialog.entries() == before,
          "Invalid dialog edit partially changed the working dictionary");
  rejected = false;
  try {
    dialog.add_entry(entry({0x2424}, U"\U0001f600"));
  } catch (const EdictUserDictionaryError&) {
    rejected = true;
  }
  require(rejected && dialog.entries() == before,
          "Unrepresentable dialog entry partially changed the working copy");
}

void test_imported_empty_meaning_round_trip() {
  const EdictUserEntry imported{{}, {0x2422}, {}};
  PromptTestDialog dialog(EdictUserDictionary::from_entries({imported}),
                          LegacyCodePage::k1252,
                          [](EdictUserDictionary) { return true; });
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
  const std::optional<EdictUserEntry> edited = dialog.prompt(imported);
  require(edited.has_value() && *edited == imported,
          "Confirming an imported empty meaning changed its wire semantics");
}

void test_callback_exceptions_are_contained() {
  const EdictUserEntry first = entry({0x2422}, U"first");
  EdictUserDictionaryDialog dialog(
      EdictUserDictionary::from_entries({first}), LegacyCodePage::k1252,
      [](EdictUserDictionary) -> bool { throw 7; },
      [](const EdictUserEntry&) { throw 9; });
  QListWidget* list =
      dialog.findChild<QListWidget*>(QStringLiteral("edictUserEntries"));
  QLabel* status =
      dialog.findChild<QLabel*>(QStringLiteral("edictUserStatus"));
  require(list != nullptr && status != nullptr,
          "Callback failure fixture has no dialog controls");
  list->setCurrentRow(0);
  require(!dialog.insert_selected() && !status->text().isEmpty(),
          "Nonstandard insert failure escaped the Qt callback boundary");
  require(!dialog.save_changes() && !status->text().isEmpty(),
          "Nonstandard save failure escaped the Qt callback boundary");
}

}  // namespace

int main(int argc, char** argv) {
  QApplication application(argc, argv);
  try {
    test_editing_and_save();
    test_import_insert_and_failed_save();
    test_invalid_edit_is_atomic();
    test_imported_empty_meaning_round_trip();
    test_callback_exceptions_are_contained();
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << "edict_user_dictionary_dialog_test: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
