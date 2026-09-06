// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <iostream>

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QFile>
#include <QListWidget>
#include <QPushButton>
#include <QTemporaryDir>

#include "file_io.h"
#include "jwpqt/core/kanji_info.h"
#include "kanji_code_lookup_dialog.h"
#include "kanji_count_dialog.h"
#include "kanji_info_dialog.h"
#include "kanji_lookup_dialog.h"
#include "kanji_reading_lookup_dialog.h"
#include "main_window.h"

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
  put_u16(bytes, 12, 23U | (3U << 8U));
  put_u16(bytes, 14, (1U << 4U) | (1U << 8U) | (2U << 11U));
  put_u16(bytes, 16, 3U);
  put_u16(bytes, 18, 1U);
  put_u32(bytes, 24, 28U << 8U);
  bytes.append("tree\0", 5);
  append_u16(bytes, 0U);
  append_u32(bytes,
             (2U << 17U) | (4U << 22U) | (5U << 27U));
  append_u32(bytes, 7U | (1234U << 6U) | (5U << 20U));
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
              .size() == 1,
          "Kanji information action created duplicate dialogs");

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
  require(count_dialog != nullptr && count_dialog->count() &&
              count_dialog->results().size() == 1 &&
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
  radical_action->trigger();
  QApplication::processEvents();
  auto* radical_dialog = dynamic_cast<jwpqt::qt::KanjiLookupDialog*>(
      window.findChild<QDialog*>(QStringLiteral("kanjiLookupDialog")));
  require(radical_dialog != nullptr,
          "Radical lookup action did not open its dialog");
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
              window.kanji_info_database() == nullptr && !action->isEnabled() &&
              !radical_action->isEnabled() &&
               !skip_action->isEnabled() &&
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

}  // namespace

int main(int argc, char* argv[]) {
  QApplication application(argc, argv);
  QTemporaryDir directory(QStringLiteral("/srv/tmp/jwpqt-kanji-ui-XXXXXX"));
  require(directory.isValid(), "Could not create kanji integration directory");
  test_integration(directory.path());
  return EXIT_SUCCESS;
}
