// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include <QApplication>
#include <QClipboard>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>

#include "jis_table_dialog.h"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

void test_codes_and_callbacks() {
  std::optional<jwpqt::core::JisTableEntry> inserted;
  jwpqt::core::JisCode shown = 0;
  jwpqt::qt::JisTableDialog dialog(
      [&](const auto& entry) { inserted = entry; },
      [&](jwpqt::core::JisCode code) { shown = code; });
  require(dialog.set_jis(0x2422U) && dialog.current().has_value(),
          "Native JIS table rejected assigned hiragana");
  require(dialog.findChild<QLineEdit*>(QStringLiteral("jisTableJis"))->text() ==
                  QStringLiteral("2422") &&
              dialog.findChild<QLineEdit*>(QStringLiteral("jisTableEuc"))
                      ->text() == QStringLiteral("A4A2") &&
              dialog.findChild<QLineEdit*>(QStringLiteral("jisTableShiftJis"))
                      ->text() == QStringLiteral("82A0") &&
              dialog.findChild<QLineEdit*>(QStringLiteral("jisTableUnicode"))
                      ->text() == QStringLiteral("3042"),
          "Native JIS table did not synchronize character codes");
  dialog.findChild<QPushButton*>(QStringLiteral("jisTableCopy"))->click();
  dialog.findChild<QPushButton*>(QStringLiteral("jisTableInsert"))->click();
  dialog.findChild<QPushButton*>(QStringLiteral("jisTableInfo"))->click();
  require(QApplication::clipboard()->text() == QStringLiteral("\u3042") &&
              inserted.has_value() && inserted->jis == 0x2422U &&
              shown == 0x2422U,
          "Native JIS table callbacks received the wrong character");
  require(dialog.set_unicode(U'\u65e5') &&
              dialog.current()->unicode == U'\u65e5',
          "Native JIS table did not select a Unicode character");
}

void test_grid_and_invalid_input() {
  jwpqt::qt::JisTableDialog dialog({}, {});
  auto* table = dialog.findChild<QTableWidget*>(QStringLiteral("jisTableGrid"));
  auto* jis = dialog.findChild<QLineEdit*>(QStringLiteral("jisTableJis"));
  require(table != nullptr && table->rowCount() == 6 &&
              table->columnCount() == 16 && table->item(0, 1) != nullptr,
          "Native JIS table grid has the wrong geometry or content");
  dialog.show();
  QApplication::processEvents();
  require(table->font().pixelSize() == 16 &&
              table->mapTo(&dialog, QPoint()).x() > jis->mapTo(&dialog, QPoint()).x(),
          "Character table does not place its readable grid beside the codes");
  const auto before = dialog.current();
  jis->setText(QStringLiteral("FFFF"));
  QMetaObject::invokeMethod(jis, "editingFinished", Qt::DirectConnection);
  auto* status =
      dialog.findChild<QLabel*>(QStringLiteral("jisTableStatus"));
  require(before.has_value() && dialog.current().has_value() &&
              dialog.current()->jis == before->jis &&
              status->text().contains(QStringLiteral("assigned")),
          "Invalid native JIS input replaced the prior character");
}

void test_callback_failures_are_contained() {
  jwpqt::qt::JisTableDialog dialog(
      [](const auto&) { throw std::runtime_error("insert failed"); },
      [](auto) { throw 7; });
  dialog.findChild<QPushButton*>(QStringLiteral("jisTableInsert"))->click();
  auto* status =
      dialog.findChild<QLabel*>(QStringLiteral("jisTableStatus"));
  require(status->text() == QStringLiteral("insert failed"),
          "JIS insert exception escaped or lost its message");
  dialog.findChild<QPushButton*>(QStringLiteral("jisTableInfo"))->click();
  require(status->text().contains(QStringLiteral("failed")),
          "Unknown JIS information exception was not contained");
}

}  // namespace

int main(int argc, char* argv[]) {
  QApplication application(argc, argv);
  test_codes_and_callbacks();
  test_grid_and_invalid_input();
  test_callback_failures_are_contained();
  return EXIT_SUCCESS;
}
