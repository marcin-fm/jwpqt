// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <iostream>

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QTabWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDialog>
#include <QFile>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QTimer>

#include "file_io.h"
#include "application_settings_dialog.h"
#include "jwpqt/core/jwp_configuration.h"
#include "jwpqt/core/kanji_info.h"
#include "kanji_code_lookup_dialog.h"
#include "kanji_count_dialog.h"
#include "kanji_info_dialog.h"
#include "kanji_lookup_dialog.h"
#include "kanji_reading_lookup_dialog.h"
#include "main_window.h"
#include "jwp_editor.h"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

void append_u16(QByteArray& bytes, quint16 value) {
  bytes.append(static_cast<char>(value & 0xffU));
  bytes.append(static_cast<char>((value >> 8U) & 0xffU));
}

void append_u32(QByteArray& bytes, quint32 value) {
  for (int shift = 0; shift < 32; shift += 8)
    bytes.append(static_cast<char>((value >> shift) & 0xffU));
}

void put_u16(QByteArray& bytes, qsizetype offset, quint16 value) {
  bytes[offset] = static_cast<char>(value & 0xffU);
  bytes[offset + 1] = static_cast<char>((value >> 8U) & 0xffU);
}

void put_u32(QByteArray& bytes, qsizetype offset, quint32 value) {
  for (int shift = 0; shift < 32; shift += 8)
    bytes[offset + shift / 8] =
        static_cast<char>((value >> shift) & 0xffU);
}

void write_bytes(const QString& path, const QByteArray& bytes) {
  QFile file(path);
  require(file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
              file.write(bytes) == bytes.size(),
          "Could not write kanji integration fixture");
}

void write_database(const QString& path) {
  QByteArray bytes;
  append_u32(bytes, jwpqt::core::kKanjiInfoMagic);
  append_u32(bytes, 0x08U);
  append_u16(bytes, 1U);
  append_u16(bytes, 0x3021U);
  bytes.resize(28, '\0');
  put_u16(bytes, 12, 23U | (3U << 8U) | (1U << 13U));
  put_u16(bytes, 14, (1U << 4U) | (1U << 8U) | (2U << 11U));
  put_u16(bytes, 16, 3U);
  put_u16(bytes, 18, 1U);
  put_u32(bytes, 24, 28U << 8U);
  bytes.append("tree\0", 5);
  bytes.append("\x37\0", 2);
  append_u16(bytes, 0U);
  append_u32(bytes,
             (2U << 17U) | (4U << 22U) | (5U << 27U));
  append_u32(bytes, 7U | (1234U << 6U) | (5U << 20U));
  bytes.append('k');
  append_u16(bytes, 0x3021);
  bytes.append('\0');
  write_bytes(path, bytes);
}

void write_lookup_lists(const QString& path, std::size_t groups,
                        jwpqt::core::JisCode code) {
  QByteArray bytes(static_cast<qsizetype>(groups * 4U), '\0');
  std::size_t offset = groups * 4U;
  for (std::size_t group = 0; group < groups; ++group) {
    const quint16 count = group == 0 ? 1U : 0U;
    bytes[static_cast<qsizetype>(group * 4U)] =
        static_cast<char>(offset & 0xffU);
    bytes[static_cast<qsizetype>(group * 4U + 1U)] =
        static_cast<char>((offset >> 8U) & 0xffU);
    bytes[static_cast<qsizetype>(group * 4U + 2U)] =
        static_cast<char>(count & 0xffU);
    bytes[static_cast<qsizetype>(group * 4U + 3U)] =
        static_cast<char>((count >> 8U) & 0xffU);
    if (count != 0) {
      bytes.append(static_cast<char>(code & 0xffU));
      bytes.append(static_cast<char>((code >> 8U) & 0xffU));
      offset += 2U;
    }
  }
  write_bytes(path, bytes);
}

void test_integration(const QString& directory) {
  const QString info_path = directory + QStringLiteral("/kanjinfo.dat");
  write_database(info_path);
  const QString radical_path = directory + QStringLiteral("/radical.dat");
  const QString stroke_path = directory + QStringLiteral("/stroke.dat");
  write_lookup_lists(radical_path, jwpqt::core::kRadicalListGroups, 0x3021U);
  write_lookup_lists(stroke_path, jwpqt::core::kStrokeListGroups, 0x3021U);
  jwpqt::core::JwpDocument document;
  document.paragraphs = {jwpqt::core::JwpParagraph{{0x3021U}}};
  const QString document_path = directory + QStringLiteral("/info.jwp");
  jwpqt::qt::write_jwp_file(document_path, document);

  jwpqt::qt::MainWindow window;
  require(window.load_kanji_info(
              info_path, jwpqt::qt::OpenMode::kNonInteractive) &&
              window.kanji_info_database() != nullptr &&
              window.load_kanji_lookup(
                  radical_path, stroke_path,
                  directory + QStringLiteral("/missing-radicals.bmp"),
                  jwpqt::qt::OpenMode::kNonInteractive) &&
              window.has_kanji_lookup() &&
              window.open_jwp_path(document_path),
          "Could not load native kanji information integration");
  QAction* action =
      window.findChild<QAction*>(QStringLiteral("kanjiInfoAction"));
  require(action != nullptr && action->isEnabled() &&
              action->shortcut() == QKeySequence(QStringLiteral("Ctrl+I")),
          "Kanji information action is unavailable");
  action->trigger();
  QApplication::processEvents();
  auto* dialog = dynamic_cast<jwpqt::qt::KanjiInfoDialog*>(
      window.findChild<QDialog*>(QStringLiteral("kanjiInfoDialog")));
  require(dialog != nullptr && dialog->code() == 0x3021U,
          "Kanji information action did not display the caret character");
  action->trigger();
  QApplication::processEvents();
  require(window.findChildren<QDialog*>(QStringLiteral("kanjiInfoDialog"))
              .size() == 2 && dialog->isVisible() && dialog->code() == 0x3021U,
          "Repeated Character Information did not create independent windows");

  QAction* skip_action =
      window.findChild<QAction*>(QStringLiteral("skipLookupAction"));
  QAction* four_corner_action =
      window.findChild<QAction*>(QStringLiteral("fourCornerLookupAction"));
  QAction* bushu_action =
      window.findChild<QAction*>(QStringLiteral("bushuLookupAction"));
  QAction* spahn_action =
      window.findChild<QAction*>(QStringLiteral("spahnLookupAction"));
  QAction* stroke_bushu_action = window.findChild<QAction*>(
      QStringLiteral("strokeBushuLookupAction"));
  QAction* reading_action = window.findChild<QAction*>(
      QStringLiteral("kanjiReadingLookupAction"));
  QAction* index_action = window.findChild<QAction*>(QStringLiteral("indexLookupAction"));
  QAction* count_action =
      window.findChild<QAction*>(QStringLiteral("kanjiCountAction"));
  require(skip_action != nullptr && skip_action->isEnabled() &&
              skip_action->shortcut() ==
                   QKeySequence(QStringLiteral("Ctrl+Alt+S")) &&
              four_corner_action != nullptr &&
              four_corner_action->isEnabled() &&
              four_corner_action->shortcut() ==
                  QKeySequence(QStringLiteral("Ctrl+4")) &&
              bushu_action != nullptr && bushu_action->isEnabled() &&
              bushu_action->shortcut() ==
                  QKeySequence(QStringLiteral("Ctrl+Shift+L")) &&
              spahn_action != nullptr && spahn_action->isEnabled() &&
               spahn_action->shortcut() ==
                    QKeySequence(QStringLiteral("Ctrl+Alt+H")) &&
               stroke_bushu_action != nullptr &&
               stroke_bushu_action->isEnabled() &&
               stroke_bushu_action->shortcut() ==
                   QKeySequence(QStringLiteral("Ctrl+Shift+B")) &&
                reading_action != nullptr && reading_action->isEnabled() &&
               reading_action->shortcut() ==
                   QKeySequence(QStringLiteral("Ctrl+Shift+R")) &&
               count_action != nullptr && count_action->isEnabled() &&
               count_action->shortcut() ==
                   QKeySequence(QStringLiteral("Ctrl+Shift+K")),
           "Kanji lookup actions are unavailable");
  skip_action->trigger();
  QApplication::processEvents();
  auto* code_dialog = dynamic_cast<jwpqt::qt::KanjiCodeLookupDialog*>(
      window.findChild<QDialog*>(QStringLiteral("kanjiCodeLookupDialog")));
  require(code_dialog != nullptr,
          "SKIP action did not open the code lookup dialog");
  require(index_action && index_action->isEnabled() &&
              index_action->shortcut() == QKeySequence(QStringLiteral("Ctrl+Shift+I")),
          "Index Lookup menu action is unavailable");
  index_action->trigger();
  code_dialog->set_index_query({jwpqt::core::KanjiIndexType::kNelson, 0, 0});
  require(code_dialog->search_index() && code_dialog->results().size() == 1 &&
              window.findChildren<QDialog*>(QStringLiteral("kanjiCodeLookupDialog")).size() == 1,
          "Index Lookup did not share the native lookup window");
  jwpqt::core::KanjiSkipQuery skip;
  skip.type = {1, 1};
  skip.first = {2, 2};
  skip.second = {3, 3};
  code_dialog->set_skip_query(skip);
  require(code_dialog->search_skip() && code_dialog->results().size() == 1 &&
              code_dialog->results()[0].code == 0x3021U,
          "Integrated SKIP lookup returned wrong results");
  auto* code_results = code_dialog->findChild<QListWidget*>(
      QStringLiteral("kanjiCodeResults"));
  require(code_results != nullptr && code_results->count() == 1,
          "Integrated code lookup has no result list");
  code_results->item(0)->setSelected(true);
  code_dialog->findChild<QPushButton*>(QStringLiteral("kanjiCodeInsert"))
      ->click();
  require(window.current_jwp_document()->paragraphs[0].text.size() == 2,
          "SKIP lookup insertion did not mutate the JWP document");
  four_corner_action->trigger();
  QApplication::processEvents();
  require(window.findChildren<QDialog*>(QStringLiteral("kanjiCodeLookupDialog"))
              .size() == 1,
          "Four-corner action created a duplicate lookup dialog");
  jwpqt::core::KanjiFourCornerQuery corner;
  corner.digits = {1, 2, 3, 4, 5};
  code_dialog->set_four_corner_query(corner);
  require(code_dialog->search_four_corner() &&
              code_dialog->results().size() == 1,
          "Integrated four-corner lookup returned wrong results");

  bushu_action->trigger();
  QApplication::processEvents();
  jwpqt::core::KanjiBushuQuery bushu;
  bushu.radical = {22, 22};
  bushu.strokes = {3, 3};
  bushu.classical = false;
  code_dialog->set_bushu_query(bushu);
  require(code_dialog->search_bushu() && code_dialog->results().size() == 1,
          "Integrated Bushu lookup returned wrong results");

  stroke_bushu_action->trigger();
  QApplication::processEvents();
  require(code_dialog->search_stroke_bushu() &&
              code_dialog->results().size() == 1 &&
              window.findChildren<QDialog*>(
                        QStringLiteral("kanjiCodeLookupDialog"))
                      .size() == 1,
          "Integrated Stroke/Bushu lookup failed or created a duplicate dialog");

  spahn_action->trigger();
  QApplication::processEvents();
  jwpqt::core::KanjiSpahnQuery spahn;
  spahn.radical_strokes = {2, 2};
  spahn.radical = {4, 4};
  spahn.other_strokes = {5, 5};
  spahn.index = {7, 7};
  code_dialog->set_spahn_query(spahn);
  require(code_dialog->search_spahn() && code_dialog->results().size() == 1 &&
              window.findChildren<QDialog*>(
                        QStringLiteral("kanjiCodeLookupDialog"))
                      .size() == 1,
          "Integrated Spahn lookup returned wrong results or duplicate dialog");

  auto* overwrite = window.findChild<QAction*>(QStringLiteral("overwriteModeAction"));
  overwrite->setChecked(true);
  reading_action->trigger();
  QApplication::processEvents();
  auto* reading_dialog = dynamic_cast<jwpqt::qt::KanjiReadingLookupDialog*>(
      window.findChild<QDialog*>(QStringLiteral("kanjiReadingLookupDialog")));
  require(reading_dialog != nullptr,
          "Reading action did not open the reading lookup dialog");
  jwpqt::core::KanjiReadingQuery reading;
  reading.kind = jwpqt::core::KanjiReadingKind::kMeaning;
  reading.text = U"tree";
  reading_dialog->set_query(reading);
  auto* reading_query = reading_dialog->findChild<QLineEdit*>(QStringLiteral("kanjiReadingQuery"));
  const auto before_query = *window.current_jwp_document();
  reading_query->setText(QStringLiteral("ABC"));
  reading_query->setCursorPosition(1);
  QKeyEvent typed(QEvent::KeyPress, Qt::Key_X, Qt::NoModifier, QStringLiteral("X"));
  QApplication::sendEvent(reading_query, &typed);
  require(reading_query->text() == QStringLiteral("AXC") &&
              *window.current_jwp_document() == before_query,
          "Reading query did not inherit overwrite without touching its document");
  reading_query->undo();
  QKeyEvent toggle(QEvent::KeyPress, Qt::Key_Insert, Qt::NoModifier);
  QApplication::sendEvent(reading_query, &toggle);
  require(reading_query->text() == QStringLiteral("ABC") && !overwrite->isChecked() &&
              !window.active_editor()->overwriteMode(),
          "Reading query Insert did not update the shared runtime mode");
  reading_dialog->set_query(reading);
  require(reading_dialog->search() && reading_dialog->results().size() == 1,
          "Integrated reading lookup returned wrong results");
  auto* reading_results = reading_dialog->findChild<QListWidget*>(
      QStringLiteral("kanjiReadingResults"));
  require(reading_results != nullptr && reading_results->count() == 1,
          "Integrated reading lookup has no result list");
  reading_results->item(0)->setSelected(true);
  reading_dialog
      ->findChild<QPushButton*>(QStringLiteral("kanjiReadingInsert"))
      ->click();
  require(window.current_jwp_document()->paragraphs[0].text.size() == 3,
          "Reading lookup insertion did not mutate the JWP document");

  count_action->trigger();
  QApplication::processEvents();
  auto* count_dialog = dynamic_cast<jwpqt::qt::KanjiCountDialog*>(
      window.findChild<QDialog*>(QStringLiteral("kanjiCountDialog")));
  jwpqt::qt::KanjiCountDisplayOptions count_options;
  count_options.frequency = false;
  if (count_dialog != nullptr)
    count_dialog->set_display_options(count_options);
  require(count_dialog != nullptr, "Count Kanji action did not open its dialog");
  require(count_dialog->count(), "Count Kanji could not refresh its snapshots");
  require(count_dialog->results().size() == 1 &&
              count_dialog->results()[0].code == 0x3021U &&
              count_dialog->results()[0].count == 3,
          "Count Kanji action did not report the current document");
  auto* count_results = count_dialog->findChild<QListWidget*>(
      QStringLiteral("kanjiCountResults"));
  require(count_results != nullptr && count_results->count() == 1,
          "Integrated Count Kanji has no result list");
  count_results->item(0)->setSelected(true);
  count_dialog->findChild<QPushButton*>(QStringLiteral("kanjiCountInsert"))
      ->click();
  require(window.current_jwp_document()->paragraphs[0].text.size() == 4,
          "Count Kanji insertion did not mutate the JWP document");
  QAction* undo = window.findChild<QAction*>(QStringLiteral("undoAction"));
  require(undo != nullptr && undo->isEnabled(),
          "Count Kanji insertion did not create a history entry");
  undo->trigger();
  require(window.current_jwp_document()->paragraphs[0].text.size() == 3,
          "Count Kanji insertion could not be undone");

  QAction* radical_action =
      window.findChild<QAction*>(QStringLiteral("radicalLookupAction"));
  require(radical_action != nullptr && radical_action->isEnabled() &&
              radical_action->shortcut() == QKeySequence(Qt::Key_F5),
          "Radical lookup action is unavailable");
  const auto before_radical_open = *window.current_jwp_document();
  const bool before_radical_undo = undo->isEnabled();
  radical_action->trigger();
  QApplication::processEvents();
  auto* radical_dialog = dynamic_cast<jwpqt::qt::KanjiLookupDialog*>(
      window.findChild<QDialog*>(QStringLiteral("kanjiLookupDialog")));
  require(radical_dialog != nullptr,
           "Radical lookup action did not open its dialog");
  require(!radical_dialog->selected_radicals().empty() &&
              *window.current_jwp_document() == before_radical_open && undo->isEnabled() == before_radical_undo,
          "Opening radical lookup did not seed the current kanji without editing its document");
  radical_dialog->set_selected_radicals({0});
  require(radical_dialog->search() &&
              radical_dialog->result_codes() ==
                  std::vector<jwpqt::core::JisCode>{0x3021U},
          "Integrated radical lookup returned wrong results");
  auto* radical_results = radical_dialog->findChild<QListWidget*>(
      QStringLiteral("kanjiLookupResults"));
  radical_results->item(0)->setSelected(true);
  radical_dialog->findChild<QPushButton*>(QStringLiteral("kanjiLookupInsert"))
      ->click();
  require(window.current_jwp_document()->paragraphs[0].text.size() == 4,
          "Radical lookup insertion did not mutate the JWP document");

  require(window.new_document_tab(false) == 1, "Could not create Unicode lookup target");
  auto* unicode = window.active_editor();
  unicode->insertPlainText(QString::fromStdU32String(U"\U0001f600X"));
  require(window.save_as_path(directory + QStringLiteral("/unicode-lookups.txt"),
                              jwpqt::core::TextEncoding::kUtf8),
          "Could not establish Unicode lookup baseline");
  const auto before_lookup = unicode->toPlainText();
  for (auto* lookup_action : {skip_action, four_corner_action, bushu_action,
                             stroke_bushu_action, spahn_action, index_action,
                             reading_action, radical_action, count_action})
    require(lookup_action->isEnabled(), "Unicode text disabled an available lookup action");
  code_results->item(0)->setSelected(true);
  for (auto* button : {
           code_dialog->findChild<QPushButton*>(QStringLiteral("kanjiCodeInsert")),
           reading_dialog->findChild<QPushButton*>(QStringLiteral("kanjiReadingInsert")),
           radical_dialog->findChild<QPushButton*>(QStringLiteral("kanjiLookupInsert")),
           count_dialog->findChild<QPushButton*>(QStringLiteral("kanjiCountInsert"))}) {
    QTextCursor selected = unicode->textCursor();
    selected.setPosition(2); selected.setPosition(3, QTextCursor::KeepAnchor);
    unicode->setTextCursor(selected);
    require(button != nullptr && button->isEnabled(), "Lookup has no available insert control");
    button->click();
    require(!window.is_jwp_document() && unicode->toPlainText() ==
                QString::fromStdU32String(U"\U0001f600\u4e9c"),
            "A modeless lookup inserted into its old document instead of the Unicode selection");
    undo->trigger();
    require(unicode->toPlainText() == before_lookup && !window.document_modified(),
            "Lookup insertion did not preserve Unicode undo and saved state");
  }
  jwpqt::qt::MainWindow unicode_lookup;
  require(unicode_lookup.new_document_tab(false) == 1 &&
              unicode_lookup.load_kanji_info(info_path) &&
              unicode_lookup.load_kanji_lookup(radical_path, stroke_path,
                  directory + QStringLiteral("/missing-radicals.bmp")),
          "Could not initialize lookup resources for Unicode-only editing");
  for (const char* name : {"skipLookupAction", "fourCornerLookupAction", "bushuLookupAction",
                           "strokeBushuLookupAction", "spahnLookupAction", "indexLookupAction",
                           "kanjiReadingLookupAction", "radicalLookupAction", "jisTableAction"}) {
    auto* lookup_action = unicode_lookup.findChild<QAction*>(QString::fromLatin1(name));
    require(lookup_action != nullptr && lookup_action->isEnabled(),
            "A fresh Unicode document disabled lookup");
    lookup_action->trigger();
  }
  for (const char* name : {"kanjiCodeLookupDialog", "kanjiReadingLookupDialog",
                           "kanjiLookupDialog", "jisTableDialog"})
    require(unicode_lookup.findChild<QDialog*>(QString::fromLatin1(name)) != nullptr,
            "A lookup command did not open in Unicode-only editing");
  require(window.activate_document(0), "Could not restore original metadata fixture");

  write_bytes(radical_path, QByteArray("bad"));
  require(!window.load_kanji_lookup(
              radical_path, stroke_path,
              directory + QStringLiteral("/missing-radicals.bmp"),
              jwpqt::qt::OpenMode::kNonInteractive) &&
              window.has_kanji_lookup() &&
              window.findChild<QDialog*>(QStringLiteral("kanjiLookupDialog")) ==
                  radical_dialog,
          "Malformed radical reload discarded working lookup state");

  write_bytes(info_path, QByteArray("bad"));
  require(!window.load_kanji_info(
              info_path, jwpqt::qt::OpenMode::kNonInteractive) &&
              window.kanji_info_database() != nullptr &&
              window.findChild<QDialog*>(QStringLiteral("kanjiInfoDialog")) ==
                  dialog &&
              window.findChild<QDialog*>(
                  QStringLiteral("kanjiCodeLookupDialog")) == code_dialog &&
               window.findChild<QDialog*>(
                   QStringLiteral("kanjiReadingLookupDialog")) ==
                   reading_dialog &&
               window.findChild<QDialog*>(QStringLiteral("kanjiCountDialog")) ==
                   count_dialog,
           "Malformed kanji information reload discarded working state");
  require(QFile::remove(info_path), "Could not remove database fixture");
  require(window.load_kanji_info(
              info_path, jwpqt::qt::OpenMode::kNonInteractive) &&
              window.kanji_info_database() == nullptr && action->isEnabled() &&
              !radical_action->isEnabled() &&
               !skip_action->isEnabled() &&
               !index_action->isEnabled() &&
               !four_corner_action->isEnabled() &&
               !bushu_action->isEnabled() && !spahn_action->isEnabled() &&
               !reading_action->isEnabled() && count_action->isEnabled() &&
              window.findChild<QDialog*>(QStringLiteral("kanjiInfoDialog")) ==
                  nullptr &&
              window.findChild<QDialog*>(
                  QStringLiteral("kanjiCodeLookupDialog")) == nullptr &&
               window.findChild<QDialog*>(
                   QStringLiteral("kanjiReadingLookupDialog")) == nullptr &&
               window.findChild<QDialog*>(QStringLiteral("kanjiCountDialog")) ==
                   nullptr,
           "Absent database reload retained stale kanji information state");
}

jwpqt::qt::KanjiInfoDialog* request_information(QTextEdit& editor, int position,
                         QContextMenuEvent::Reason reason = QContextMenuEvent::Mouse,
                         Qt::KeyboardModifiers modifiers = Qt::NoModifier,
                         bool enabled = true) {
  const auto previous = QApplication::topLevelWidgets();
  const QString text = editor.toPlainText();
  QPoint point = editor.viewport()->rect().bottomRight();
  if (position >= 0) {
    QTextCursor cursor(editor.document());
    cursor.setPosition(position);
    const QRect first = editor.cursorRect(cursor);
    cursor.setPosition(position + (text.at(position).isHighSurrogate() ? 2 : 1));
    const QRect next = editor.cursorRect(cursor);
    point = QPoint((first.x() * 3 + next.x()) / 4, first.center().y());
  }
  const QPoint global = editor.viewport()->mapToGlobal(point);
  if (reason == QContextMenuEvent::Mouse) {
    QMouseEvent press(QEvent::MouseButtonPress, point, global, Qt::RightButton,
                       Qt::RightButton, modifiers);
    QApplication::sendEvent(editor.viewport(), &press);
  }
  if (!modifiers.testFlag(Qt::ShiftModifier)) {
    QTimer::singleShot(0, &editor, [enabled] {
      auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
      require(menu != nullptr, "Right-click did not open a native context menu");
      QAction* info = menu->findChild<QAction*>(QStringLiteral("characterInfoContextAction"));
      require(info && info->isEnabled() == enabled && menu->actions().size() > 3,
              "Context menu lost standard actions or uses the wrong character target");
      if (enabled) {
        menu->setActiveAction(info);
        QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(menu, &enter);
      } else {
        menu->close();
      }
    });
  }
  QContextMenuEvent context(reason, point, global, modifiers);
  QApplication::sendEvent(reason == QContextMenuEvent::Keyboard ? &editor : editor.viewport(),
                          &context);
  if (reason == QContextMenuEvent::Mouse) {
    QMouseEvent release(QEvent::MouseButtonRelease, point, global, Qt::RightButton,
                         Qt::NoButton, modifiers);
    QApplication::sendEvent(editor.viewport(), &release);
  }
  QApplication::processEvents();
  if (enabled) {
    for (auto* widget : QApplication::topLevelWidgets()) {
      auto* dialog = dynamic_cast<jwpqt::qt::KanjiInfoDialog*>(widget);
      if (dialog != nullptr && !previous.contains(dialog)) return dialog;
    }
    require(false, "Information request did not create a separate modeless window");
  }
  return nullptr;
}

void test_lookup_preferences(const QString& directory) {
  using namespace jwpqt;
  const auto settings = qt::read_application_settings(
      "auto_lookup=false\nRareKanjiLast=no\nFutureOption=42\n"
      "Bushu_MatchNelson=false\nbushu_classical=true\nFlexibleKunReadings=no\n"
      "reading_word=true\nMatch_SKIP_Miscodings=true\nIndexType=20\nreading_type=3\n"
      "no_variants=true\nDeemphasizeRareRadicals=true\n");
  require(!settings.automatic_kanji_lookup && !settings.rare_kanji_last && settings.unapplied.size() == 1,
          "Lookup aliases/defaults/retained fields are wrong");
  try {
    (void)qt::read_application_settings("rare_last=maybe\nRareKanjiLast=true\n");
    require(false, "Invalid overridden lookup setting was accepted");
  } catch (const core::JwpConfigurationError&) {}
  require(!settings.bushu_nelson && settings.bushu_classical && !settings.flexible_kun &&
          settings.partial_meanings && settings.skip_miscodes && settings.index_type == 20 && settings.reading_type == 3,
          "Search preference aliases did not decode");
  for (const auto* invalid : {"IndexType=21\nindex_type=0\n", "ReadingType=-1\n", "reading_type=7\n",
                             "FlexibleKunReadings=maybe\nreading_kun=true\n"}) {
    try { (void)qt::read_application_settings(invalid); require(false, "Invalid lookup preference was accepted"); }
    catch (const core::JwpConfigurationError&) {}
  }
  const auto canonical = qt::write_application_settings(settings);
  require(qt::write_application_settings(qt::read_application_settings(canonical)) == canonical,
          "Lookup setting serialization is not canonical");
  qt::ApplicationSettingsDialog options(settings);
  auto* automatic = options.findChild<QCheckBox*>("settingsLookupAutomatic");
  auto* rare = options.findChild<QCheckBox*>("settingsLookupRareLast");
  require(automatic && rare && !automatic->isChecked() && !rare->isChecked(), "Lookup Options are missing");
  auto* preferred_index = options.findChild<QComboBox*>("settingsIndexType");
  auto* preferred_reading = options.findChild<QComboBox*>("settingsReadingType");
  auto* flexible = options.findChild<QCheckBox*>("settingsFlexibleKun");
  require(preferred_index && preferred_reading && flexible && preferred_index->currentIndex() == 20 &&
          preferred_reading->currentIndex() == 3 && !flexible->isChecked(), "Lookup default Options are missing");
  preferred_index->setCurrentIndex(5); flexible->setChecked(true);
  automatic->setChecked(true);
  options.reject();
  require(!options.settings().automatic_kanji_lookup, "Cancelled lookup Options changed settings");
  require(options.settings().index_type == 20 && !options.settings().flexible_kun, "Cancel changed lookup defaults");
  auto* buttons = options.findChild<QDialogButtonBox*>();
  buttons->button(QDialogButtonBox::Ok)->click();
  require(options.settings().automatic_kanji_lookup && !options.settings().rare_kanji_last,
          "Accepted lookup Options lost values");
  require(options.settings().index_type == 5 && options.settings().flexible_kun, "Accepted lookup defaults were lost");
  auto* option_tabs = options.findChild<QTabWidget*>();
  require(option_tabs, "Options has no pages");
  for (int i = 0; i < option_tabs->count(); ++i)
    if (option_tabs->tabText(i) == "Kanji Lookup") option_tabs->setCurrentIndex(i);
  options.show();
  QApplication::processEvents();
  require(options.grab().save(QDir::current().filePath("lookup-preferences.png")), "Could not capture lookup Options");
  options.hide();

  qt::MainWindow window;
  write_database(directory + "/lookup-info.dat");
  write_lookup_lists(directory + "/lookup-radicals.dat", core::kRadicalListGroups, 0x3021U);
  write_lookup_lists(directory + "/lookup-strokes.dat", core::kStrokeListGroups, 0x3021U);
  core::JwpDocument document;
  document.paragraphs = {core::JwpParagraph{{0x3021U}}};
  qt::write_jwp_file(directory + "/lookup-doc.jwp", document);
  require(window.load_kanji_info(directory + "/lookup-info.dat", qt::OpenMode::kNonInteractive) &&
              window.load_kanji_lookup(directory + "/lookup-radicals.dat", directory + "/lookup-strokes.dat", {},
                                       qt::OpenMode::kNonInteractive) &&
              window.open_jwp_path(directory + "/lookup-doc.jwp"), "Lookup settings fixture failed");
  auto supported = settings;
  supported.source.clear(); // The explicit unsupported-field consent is tested by project tests.
  supported.unapplied.clear();
  require(window.apply_application_settings(supported), "Could not apply shared lookup settings");
  const auto original = *window.current_jwp_document();
  window.findChild<QAction*>("radicalLookupAction")->trigger();
  window.findChild<QAction*>("skipLookupAction")->trigger();
  auto* radial = dynamic_cast<qt::KanjiLookupDialog*>(window.findChild<QDialog*>("kanjiLookupDialog"));
  auto* code = dynamic_cast<qt::KanjiCodeLookupDialog*>(window.findChild<QDialog*>("kanjiCodeLookupDialog"));
  require(radial && code, "Lookup windows did not open");
  auto* miscodes = code->findChild<QCheckBox*>("skipMisclassifications");
  require(miscodes && !miscodes->isEnabled() && miscodes->isChecked() &&
          window.application_settings().skip_miscodes,
          "Unavailable SKIP references erased the preference or enabled unsupported search");
  auto* variants = code->findChild<QCheckBox*>("spahnVariants");
  auto* stroke_variants = code->findChild<QCheckBox*>("strokeBushuVariants");
  require(variants && stroke_variants && !variants->isChecked() && !stroke_variants->isChecked(),
          "Reduced radical preference was not applied");
  variants->click();
  require(!window.application_settings().reduce_radical_choices && stroke_variants->isChecked(),
          "User variant preference did not synchronize");
  stroke_variants->click();
  require(window.application_settings().reduce_radical_choices && !variants->isChecked(),
          "Stroke variant preference did not synchronize");
  auto* nelson = code->findChild<QCheckBox*>("bushuNelson");
  auto* classical = code->findChild<QCheckBox*>("bushuClassical");
  auto* stroke_nelson = code->findChild<QCheckBox*>("strokeBushuNelson");
  auto* index_kind = code->findChild<QComboBox*>("kanjiIndexType");
  require(nelson && classical && stroke_nelson && index_kind && !nelson->isChecked() && classical->isChecked() &&
          !stroke_nelson->isChecked(), "Bushu defaults were not applied to both pages");
  nelson->click();
  require(window.application_settings().bushu_nelson && stroke_nelson->isChecked() &&
          window.application_settings().index_type == 20, "Bushu edit lost synchronization or unavailable index preference");
  index_kind->setCurrentIndex(1);
  QMetaObject::invokeMethod(index_kind, "activated", Qt::DirectConnection, Q_ARG(int, 1));
  require(window.application_settings().index_type == 1, "Index choice was not remembered");
  window.findChild<QAction*>("kanjiReadingLookupAction")->trigger();
  auto* reading = dynamic_cast<qt::KanjiReadingLookupDialog*>(window.findChild<QDialog*>("kanjiReadingLookupDialog"));
  require(reading, "Reading preferences fixture did not open");
  auto* reading_kind = reading->findChild<QComboBox*>("kanjiReadingKind");
  auto* query_edit = reading->findChild<QLineEdit*>("kanjiReadingQuery");
  auto* reading_flexible = reading->findChild<QCheckBox*>("kanjiReadingFlexibleKun");
  require(reading_kind->currentData().toInt() == 3 && !reading_flexible->isChecked() &&
          reading->findChild<QCheckBox*>("kanjiReadingPartialWords")->isChecked(), "Reading defaults did not reach controls");
  reading->set_search_preferences(false, true, 6);
  require(reading_kind->findData(6) < 0 && reading_kind->currentData().toInt() == 2,
          "Unavailable reading type did not use a visible fallback");
  reading_flexible->click();
  require(window.application_settings().reading_type == 6, "Fallback overwrote the saved reading type");
  reading->set_search_preferences(false, true, 3);
  query_edit->setText("tree");
  require(reading->search() && !reading->results().empty(), "Reading defaults did not reach search");
  const auto reading_results = reading->results();
  auto* reading_list = reading->findChild<QListWidget*>("kanjiReadingResults");
  const int reading_row = reading_list->currentRow();
  reading_kind->setCurrentIndex(2);
  QMetaObject::invokeMethod(reading_kind, "activated", Qt::DirectConnection, Q_ARG(int, 2));
  query_edit->clear();
  QKeyEvent pending(QEvent::KeyPress, Qt::Key_N, Qt::NoModifier, "n");
  QApplication::sendEvent(query_edit, &pending);
  require(query_edit->text().isEmpty(), "Pending reading fixture unexpectedly committed");
  auto changed_defaults = window.application_settings();
  changed_defaults.reading_type = 3; changed_defaults.flexible_kun = true;
  require(window.apply_application_settings(changed_defaults) && query_edit->text().isEmpty() &&
          reading->results() == reading_results && reading_list->currentRow() == reading_row,
          "Preference application flushed pending input or changed results");
  QKeyEvent finish(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, "a");
  QApplication::sendEvent(query_edit, &finish);
  require(query_edit->text() == QString(QChar(0x306a)), "Applying reading type lost local kana composition");
  reading_kind->setCurrentIndex(2);
  QMetaObject::invokeMethod(reading_kind, "activated", Qt::DirectConnection, Q_ARG(int, 2));
  reading_flexible->click();
  require(!window.application_settings().flexible_kun && window.application_settings().reading_type == 2,
          "Reading control changes were not remembered");
  auto* radial_auto = radial->findChild<QCheckBox*>("kanjiLookupAutoSearch");
  auto* code_auto = code->findChild<QCheckBox*>("kanjiCodeAutoSearch");
  require(radial_auto && code_auto && !radial_auto->isChecked() && !code_auto->isChecked(),
          "New lookup did not adopt saved Auto preference");
  radial_auto->setChecked(true);
  require(window.application_settings().automatic_kanji_lookup && code_auto->isChecked(),
          "Radical Auto did not update global and code lookup state");
  code_auto->setChecked(false);
  require(!window.application_settings().automatic_kanji_lookup && !radial_auto->isChecked(),
          "Code Auto did not update radical state");
  require(!radial->findChild<QTimer*>("kanjiLookupSearchTimer")->isActive(),
          "Disabling shared Auto left a pending radical search");
  require(radial->search(), "Could not populate saved preference fixture");
  auto* list = radial->findChild<QListWidget*>("kanjiLookupResults");
  const auto values = radial->result_codes();
  const auto row = list->currentRow();
  auto next = window.application_settings();
  next.automatic_kanji_lookup = true;
  next.rare_kanji_last = true;
  require(window.apply_application_settings(next) && radial_auto->isChecked() && code_auto->isChecked() &&
              radial->result_codes() == values && list->currentRow() == row &&
              *window.current_jwp_document() == original, "Preferences altered results or document");
  bool reentered = false;
  const auto connection = QObject::connect(list, &QListWidget::itemSelectionChanged, &window, [&] {
    if (reentered) return;
    reentered = true;
    auto newer = window.application_settings();
    newer.automatic_kanji_lookup = true;
    require(window.apply_application_settings(newer), "Reentrant lookup preference update failed");
  });
  radial_auto->setChecked(false);
  QObject::disconnect(connection);
  require(reentered && window.application_settings().automatic_kanji_lookup && radial_auto->isChecked() &&
              code_auto->isChecked(), "Older Auto notification overwrote a reentrant settings update");
  code_auto->setChecked(false);
  require(window.save_application_settings(directory + "/lookup.cfg") &&
              window.save_project_path(directory + "/lookup.jpr", false), "Could not persist lookup preferences");
  qt::MainWindow restarted;
  require(restarted.load_application_settings(directory + "/lookup.cfg") &&
              !restarted.application_settings().automatic_kanji_lookup && restarted.application_settings().rare_kanji_last,
          "Restart lost lookup preferences");
  qt::MainWindow project;
  require(project.open_project_path(directory + "/lookup.jpr") &&
              !project.application_settings().automatic_kanji_lookup && project.application_settings().rare_kanji_last,
           "Project lost lookup preferences");
  for (const auto* saved : {&restarted.application_settings(), &project.application_settings()})
    require(saved->index_type == 1 && saved->reading_type == 2 && saved->bushu_nelson &&
            saved->bushu_classical && !saved->flexible_kun && saved->partial_meanings && saved->skip_miscodes,
            "Restart/project lost lookup search preferences");
  require(restarted.application_settings().reduce_radical_choices && project.application_settings().reduce_radical_choices &&
          restarted.application_settings().deemphasize_rare_radicals && project.application_settings().deemphasize_rare_radicals,
          "Restart/project lost radical appearance preferences");
}

void test_character_context(const QString& directory) {
  const QString info_path = directory + QStringLiteral("/context-info.dat");
  write_database(info_path);
  jwpqt::core::JwpDocument source;
  source.paragraphs = {jwpqt::core::JwpParagraph{{'A', 0x3021, 0x2437, 0xe9, 0x2574}}};
  const QString path = directory + QStringLiteral("/context.jwp");
  jwpqt::qt::write_jwp_file(path, source);
  jwpqt::qt::MainWindow window;
  require(window.load_kanji_info(info_path, jwpqt::qt::OpenMode::kNonInteractive) &&
              window.open_jwp_path(path), "Could not load context workflow fixture");
  window.show();
  QApplication::processEvents();
  auto* editor = window.findChild<QTextEdit*>();
  require(editor != nullptr, "Context workflow has no document editor");
  QTextCursor selection(editor->document());
  selection.setPosition(1);
  selection.setPosition(0, QTextCursor::KeepAnchor);
  editor->setTextCursor(selection);
  auto* undo = window.findChild<QAction*>(QStringLiteral("undoAction"));
  const auto pristine = [&] {
    return window.current_jwp_document()->paragraphs[0].text == source.paragraphs[0].text &&
        !window.document_modified() && !undo->isEnabled();
  };
  const auto unchanged = [&] {
    return editor->textCursor().position() == 0 && editor->textCursor().anchor() == 1 &&
        pristine();
  };
  auto* dialog = request_information(*editor, 1);
  require(dialog && dialog->code() == 0x3021 && unchanged(),
          "Editor right-click ignored the pointer or changed selection/document/history");
  auto* readings = dialog->findChild<QTextEdit*>(QStringLiteral("kanjiInfoReadings"));
  QTextCursor retained_selection(readings->document());
  retained_selection.setPosition(readings->toPlainText().indexOf(QStringLiteral("tree")));
  retained_selection.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, 4);
  readings->setTextCursor(retained_selection);
  auto* kana = request_information(*readings, readings->toPlainText().indexOf(QChar(0x30b7)));
  auto* kana_text = kana->findChild<QTextEdit*>(QStringLiteral("kanjiInfoReadings"));
  require(kana->code() == 0x2537 && unchanged() && kana != dialog &&
              kana_text->toPlainText().contains(QStringLiteral("shi\nsi")) &&
              dialog->code() == 0x3021 && dialog->isVisible() &&
              readings->textCursor().selectedText() == QStringLiteral("tree"),
          "Reading navigation replaced its source window or lost its selection");
  auto* latin = request_information(*kana_text,
      kana_text->toPlainText().indexOf(QStringLiteral("shi")) + 1);
  require(latin->character() == U'h' && unchanged() && kana->code() == 0x2537 &&
              window.findChildren<QDialog*>(QStringLiteral("kanjiInfoDialog")).size() == 3 &&
              kana->parentWidget() == &window && latin->parentWidget() == &window,
          "Nested information windows are not independent main-window children");
  auto* code_page = request_information(*editor, 3, QContextMenuEvent::Mouse, Qt::ShiftModifier);
  require(code_page->code() == 0xe9 && code_page->character() == U'\u00e9' && unchanged(),
          "Shift-right-click lost the original native code-page byte");
  auto* keyboard = request_information(*editor, -1, QContextMenuEvent::Keyboard);
  require(keyboard->character() == U'A' && unchanged(),
          "Keyboard context information ignored the selected character");
  request_information(*editor, -1, QContextMenuEvent::Mouse, Qt::NoModifier, false);
  require(keyboard->character() == U'A' && unchanged() &&
              window.findChildren<QDialog*>(QStringLiteral("kanjiInfoDialog")).size() == 5,
          "Blank-area context menu inspected the caret instead of the pointer");

  dialog->findChild<QPushButton*>(QStringLiteral("kanjiInfoMore"))->click();
  auto* extra = dialog->findChild<QTextEdit*>(QStringLiteral("kanjiInfoReferences"));
  require(extra && extra->isVisible(), "More Info did not expose its reference pane");
  auto* cross_reference = request_information(*extra,
      extra->toPlainText().indexOf(QChar(0x4e9c)));
  require(cross_reference->code() == 0x3021 && extra->isVisible() && unchanged() &&
              cross_reference != dialog && dialog->code() == 0x3021,
          "JIS cross-reference navigation replaced or closed its source window");
  auto* pane_keyboard = request_information(*readings, -1, QContextMenuEvent::Keyboard);
  require(pane_keyboard->character() == U't' && unchanged() && dialog->code() == 0x3021,
          "Reading-pane keyboard context did not open a separate information window");
  QApplication::clipboard()->setText(QStringLiteral("\u3042"));
  kana->findChild<QPushButton*>(QStringLiteral("kanjiInfoClipboard"))->click();
  require(kana->character() == U'\u3042' && dialog->code() == 0x3021 &&
              latin->character() == U'h' && unchanged(),
          "From Clipboard changed a different information window or the document");

  QTextCursor reading_selection(readings->document());
  reading_selection.setPosition(readings->toPlainText().indexOf(QStringLiteral("tree")));
  reading_selection.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, 4);
  readings->setTextCursor(reading_selection);
  dialog->findChild<QPushButton*>(QStringLiteral("kanjiInfoInsert"))->click();
  require(editor->toPlainText().startsWith(QStringLiteral("tree\u4e9c")) &&
              undo->isEnabled(), "Character Information insertion did not reach native history");
  undo->trigger();
  require(pristine() && editor->textCursor().position() == 0,
          "Undo did not restore the pre-information document and caret");
  selection.setPosition(1);
  selection.setPosition(0, QTextCursor::KeepAnchor);
  editor->setTextCursor(selection);
  auto* glyph = dialog->findChild<QLabel*>(QStringLiteral("kanjiInfoCharacter"));
  const QPoint center = glyph->rect().center();
  QMouseEvent double_click(QEvent::MouseButtonDblClick, center, glyph->mapToGlobal(center),
                            Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(glyph, &double_click);
  require(window.current_jwp_document()->paragraphs[0].text.front() == 0x3021 &&
              undo->isEnabled(), "Large character insertion bypassed the native document");
  undo->trigger();
  require(pristine() && editor->textCursor().position() == 0,
          "Large character insertion could not be undone exactly");
  selection.setPosition(1);
  selection.setPosition(0, QTextCursor::KeepAnchor);
  editor->setTextCursor(selection);

  QPointer<QDialog> closed_dialog = dialog;
  QPointer<QTextEdit> closed_more = extra;
  dialog->findChild<QPushButton*>(QStringLiteral("kanjiInfoDone"))->click();
  QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  require(!closed_dialog && !closed_more && kana->isVisible() && latin->isVisible() &&
              cross_reference->isVisible(),
          "Closing an originating viewer closed its independently opened information windows");
  std::vector<QPointer<QDialog>> old_dialogs;
  for (auto* view : window.findChildren<QDialog*>(QStringLiteral("kanjiInfoDialog")))
    old_dialogs.emplace_back(view);
  require(window.load_kanji_info(directory + QStringLiteral("/no-context-info.dat"),
                                 jwpqt::qt::OpenMode::kNonInteractive),
          "Could not unload metadata with multiple viewers open");
  for (const auto& view : old_dialogs)
    require(!view, "Metadata reload retained a viewer holding the previous database");
  dialog = request_information(*editor, 2);
  require(dialog && dialog->code() == 0x2437 && unchanged(),
          "Basic character navigation stopped after unloading kanji metadata");
  dialog->close();
  QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  window.findChild<QAction*>(QStringLiteral("newTextDocumentAction"))->trigger();
  editor = window.active_editor();
  editor->insertPlainText(QStringLiteral("x\U0001f600\u3042"));
  selection = editor->textCursor();
  selection.setPosition(0);
  editor->setTextCursor(selection);
  dialog = request_information(*editor, 1);
  require(dialog && dialog->character() == 0x1f600 && dialog->code() == 0 &&
              editor->textCursor().position() == 0 &&
              editor->toPlainText() == QStringLiteral("x\U0001f600\u3042"),
           "Unicode text context split a surrogate pair or changed the document");

  jwpqt::core::JwpDocument reading;
  reading.paragraphs = {jwpqt::core::JwpParagraph{{0x2422}}};
  const QString preview_path = directory + QStringLiteral("/context-preview.jwp");
  const QString wnn_path = directory + QStringLiteral("/context-wnn");
  jwpqt::qt::write_jwp_file(preview_path, reading);
  write_bytes(wnn_path + QStringLiteral(".dix"),
              QByteArray::fromHex("a280807700000000"));
  write_bytes(wnn_path + QStringLiteral(".dat"),
              QByteArray::fromHex("a22ab0a1b0a1b0a10a"));
  require(window.open_jwp_path(preview_path) &&
              window.load_wnn_resources(wnn_path + QStringLiteral(".dix"),
                  wnn_path + QStringLiteral(".dat"),
                  directory + QStringLiteral("/context-user.sel"),
                  jwpqt::qt::OpenMode::kNonInteractive),
           "Could not prepare character information during a conversion preview");
  editor->selectAll();
  require(window.convert_selection() && window.conversion_active() &&
              editor->toPlainText() == QStringLiteral("\u4e9c\u4e9c\u4e9c"),
           "Conversion context fixture did not produce a longer displayed candidate");
  const auto candidate_document = *window.current_jwp_document();
  const bool candidate_modified = window.document_modified();
  const QTextCursor candidate_selection = editor->textCursor();
  for (int position : {0, 2}) {
    dialog = request_information(*editor, position);
    require(dialog && dialog->code() == 0x3021 &&
                window.conversion_active() &&
                window.document_modified() == candidate_modified &&
                *window.current_jwp_document() == candidate_document &&
                editor->textCursor().position() == candidate_selection.position() &&
                editor->textCursor().anchor() == candidate_selection.anchor() &&
                editor->toPlainText() == QStringLiteral("\u4e9c\u4e9c\u4e9c"),
             "Character information changed or committed the conversion preview");
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  QApplication application(argc, argv);
  QTemporaryDir directory(QStringLiteral("/srv/tmp/jwpqt-kanji-ui-XXXXXX"));
  require(directory.isValid(), "Could not create kanji integration directory");
  test_integration(directory.path());
  test_lookup_preferences(directory.path());
  test_character_context(directory.path());
  return EXIT_SUCCESS;
}
