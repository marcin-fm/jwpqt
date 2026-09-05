// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <iostream>

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QFile>
#include <QTemporaryDir>

#include "file_io.h"
#include "jwpqt/core/kanji_info.h"
#include "kanji_info_dialog.h"
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

void write_bytes(const QString& path, const QByteArray& bytes) {
  QFile file(path);
  require(file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
              file.write(bytes) == bytes.size(),
          "Could not write kanji integration fixture");
}

void write_database(const QString& path) {
  QByteArray bytes;
  append_u32(bytes, jwpqt::core::kKanjiInfoMagic);
  append_u32(bytes, 0U);
  append_u16(bytes, 1U);
  append_u16(bytes, 0x3021U);
  bytes.append(12, '\0');
  append_u32(bytes, 28U << 8U);
  write_bytes(path, bytes);
}

void test_integration(const QString& directory) {
  const QString info_path = directory + QStringLiteral("/kanjinfo.dat");
  write_database(info_path);
  jwpqt::core::JwpDocument document;
  document.paragraphs = {jwpqt::core::JwpParagraph{{0x3021U}}};
  const QString document_path = directory + QStringLiteral("/info.jwp");
  jwpqt::qt::write_jwp_file(document_path, document);

  jwpqt::qt::MainWindow window;
  require(window.load_kanji_info(
              info_path, jwpqt::qt::OpenMode::kNonInteractive) &&
              window.kanji_info_database() != nullptr &&
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

  write_bytes(info_path, QByteArray("bad"));
  require(!window.load_kanji_info(
              info_path, jwpqt::qt::OpenMode::kNonInteractive) &&
              window.kanji_info_database() != nullptr &&
              window.findChild<QDialog*>(QStringLiteral("kanjiInfoDialog")) ==
                  dialog,
          "Malformed kanji information reload discarded working state");
  require(QFile::remove(info_path), "Could not remove database fixture");
  require(window.load_kanji_info(
              info_path, jwpqt::qt::OpenMode::kNonInteractive) &&
              window.kanji_info_database() == nullptr && !action->isEnabled() &&
              window.findChild<QDialog*>(QStringLiteral("kanjiInfoDialog")) ==
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
