// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>

#include <QApplication>
#include <QAbstractTextDocumentLayout>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QInputMethodEvent>
#include <QGlyphRun>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollBar>
#include <QTableWidget>
#include <QTextEdit>
#include <QTextBlock>
#include <QTextLayout>
#include <QTimer>

#include "character_context_menu.h"
#include "jwpqt/core/kanji_info.h"
#include "kanji_info_dialog.h"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

void append_u16(std::string& bytes, std::uint16_t value) {
  bytes.push_back(static_cast<char>(value & 0xffU));
  bytes.push_back(static_cast<char>((value >> 8U) & 0xffU));
}

void append_u32(std::string& bytes, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8)
    bytes.push_back(static_cast<char>((value >> shift) & 0xffU));
}

void put_u16(std::string& bytes, std::size_t offset, std::uint16_t value) {
  bytes[offset] = static_cast<char>(value & 0xffU);
  bytes[offset + 1] = static_cast<char>((value >> 8U) & 0xffU);
}

void put_u32(std::string& bytes, std::size_t offset, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8)
    bytes[offset + shift / 8] = static_cast<char>((value >> shift) & 0xffU);
}

jwpqt::core::KanjiInfoDatabase database() {
  constexpr std::size_t variable = 28;
  std::string bytes;
  append_u32(bytes, jwpqt::core::kKanjiInfoMagic);
  append_u32(bytes, 0U);
  append_u16(bytes, 1U);
  append_u16(bytes, 0x3021U);
  bytes.resize(variable, '\0');
  put_u16(bytes, 12, 87U | (13U << 8U) | (1U << 13U));
  put_u16(bytes, 14, 4U | (2U << 4U) | (2U << 8U) | (4U << 11U));
  put_u16(bytes, 16, 9U | (1U << 5U) | (1U << 6U) | (1U << 11U));
  put_u16(bytes, 18, (2492U << 1U) | 1U);
  put_u16(bytes, 20, (2829U << 1U) | 1U);
  put_u16(bytes, 22, 1927U);
  put_u32(bytes, 24, (static_cast<std::uint32_t>(variable) << 8U) | 61U);
  for (const auto* text : {"ae", "ai", "love", "affection <&>"}) {
    bytes.append(text);
    bytes.push_back('\0');
  }
  bytes.append("\x22\0", 2);
  bytes.append("\x23\0", 2);
  bytes.append("\x22\0", 2);
  append_u16(bytes, 10947U);
  append_u32(bytes, 4U | (1123U << 4U) | (4U << 17U) | (8U << 22U) | (10U << 27U));
  append_u32(bytes, 1U | (2044U << 6U) | (7U << 20U) | (6U << 24U));
  for (const auto reference : {
           jwpqt::core::KanjiInfoCode{'F', 640}, {'I', 259}, {'Q', 1234},
           {'k', 0x3021}, {'j', 0x3445}, {'z', (2U << 13U) | (2U << 10U) | 67U}}) {
    bytes.push_back(reference.kind);
    append_u16(bytes, reference.value);
  }
  bytes.push_back('\0');
  return jwpqt::core::KanjiInfoDatabase::parse(bytes);
}

void test_dialog() {
  const auto source = database();
  std::u32string inserted;
  jwpqt::qt::KanjiInfoDialog dialog(&source, [](char32_t) {}, nullptr,
      [&](std::u32string text) {
    inserted += text;
    return true;
  });
  require(dialog.set_code(0x3021U) && dialog.code() == 0x3021U,
          "Kanji information dialog did not accept a database code");
  auto* character =
      dialog.findChild<QLabel*>(QStringLiteral("kanjiInfoCharacter"));
  auto* fields =
      dialog.findChild<QTableWidget*>(QStringLiteral("kanjiInfoFields"));
  auto* readings =
      dialog.findChild<QTextEdit*>(QStringLiteral("kanjiInfoReadings"));
  auto* insert = dialog.findChild<QPushButton*>(QStringLiteral("kanjiInfoInsert"));
  auto* more = dialog.findChild<QPushButton*>(QStringLiteral("kanjiInfoMore"));
  auto* clipboard = dialog.findChild<QPushButton*>(QStringLiteral("kanjiInfoClipboard"));
  require(character != nullptr && character->text() == QStringLiteral("\u4e9c") &&
              fields != nullptr && fields->rowCount() == 15 &&
              readings != nullptr && readings->isReadOnly() &&
              readings->toPlainText() == QStringLiteral(
                  "-- meanings --\nlove\naffection <&>\n-- on-yomi --\n\u30a2\n"
                  "-- kun-yomi --\n\u3043\n-- nanori --\n\u3042") &&
              insert && !insert->isEnabled() && more && more->isEnabled() && clipboard,
          "Kanji information dialog rendered incomplete record data");
  const auto field = [fields](const QString& label) {
    for (int row = 0; row < fields->rowCount(); ++row)
      if (fields->item(row, 0)->text() == label) return fields->item(row, 1)->text();
    return QString();
  };
  require(field(QStringLiteral("Type")) == QStringLiteral("Kanji (common, I)") &&
              field(QStringLiteral("JIS Code")) == QStringLiteral("3021 (B0A1)") &&
              field(QStringLiteral("Shift-JIS")) == QStringLiteral("889F") &&
              field(QStringLiteral("Unicode")) == QStringLiteral("U+4E9C") &&
              field(QStringLiteral("Frequency")) == QStringLiteral("640") &&
              field(QStringLiteral("Bushu")).contains(QStringLiteral("87 (61)")) &&
              field(QStringLiteral("Bushu")).contains(QStringLiteral("\u5fc3")) &&
              field(QStringLiteral("Halpern / SKIP")) == QStringLiteral("2492    2-4-9") &&
              field(QStringLiteral("Spahn")) == QStringLiteral("4i10.1    259") &&
              field(QStringLiteral("Four Corners")) == QStringLiteral("2044.7    1234.6") &&
              field(QStringLiteral("Morohashi")) == QStringLiteral("10947    4.1123") &&
              field(QStringLiteral("Nelson")) == QStringLiteral("2829    1927"),
          "Character information lost named metadata or compound references");
  dialog.show();
  QApplication::processEvents();
  more->click();
  auto* extra = dialog.findChild<QTextEdit*>(QStringLiteral("kanjiInfoReferences"));
  require(extra && extra->isVisible() &&
              extra->toPlainText().contains(QStringLiteral("JIS X 0208 reference: 3021  \u4e9c")) &&
              extra->toPlainText().contains(QStringLiteral("JIS X 0212 reference: 3445")) &&
              extra->toPlainText().contains(QStringLiteral("Alternate SKIP: 2-2-3 (Stroke)")),
          "More Info omitted or misinterpreted cross-references");

  QTextCursor selection(readings->document());
  selection.setPosition(readings->toPlainText().indexOf(QStringLiteral("love")));
  selection.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, 4);
  readings->setTextCursor(selection);
  require(insert->isEnabled(), "Selecting readings did not enable Insert to File");
  for (const bool dark : {true, false, true}) {
    QPalette palette = dialog.palette();
    palette.setColor(QPalette::Base, dark ? QColor("#151515") : QColor(Qt::white));
    palette.setColor(QPalette::Text, dark ? QColor(Qt::white) : QColor(Qt::black));
    dialog.setPalette(palette);
    QApplication::processEvents();
    QTextCursor heading(readings->document());
    heading.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
    require(heading.charFormat().foreground().color() ==
                (dark ? QColor("#ff8080") : QColor("#b00020")) &&
                readings->textCursor().selectedText() == QStringLiteral("love") &&
                dialog.code() == 0x3021,
            "Changing the theme left unreadable headings or reset the selected reading");
  }
  insert->click();
  require(inserted == U"love", "Insert to File inserted the glyph instead of selected text");
  const QPointF center(character->rect().center());
  QMouseEvent double_click(QEvent::MouseButtonDblClick, center,
      character->mapToGlobal(center.toPoint()), Qt::LeftButton,
      Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(character, &double_click);
  require(inserted == U"love\u4e9c", "Double-clicking the large glyph did not insert it");

  QApplication::clipboard()->setText(QStringLiteral("\u3057 rest"));
  clipboard->click();
  require(dialog.code() == 0x2437 && !more->isEnabled() && !extra->isVisible() &&
              readings->toPlainText() == QStringLiteral("-- romaji --\nshi\nsi") &&
              field(QStringLiteral("Type")) == QStringLiteral("Hiragana") &&
              !insert->isEnabled(),
          "Clipboard kana information retained stale kanji metadata");
  QApplication::clipboard()->clear();
  clipboard->click();
  require(dialog.code() == 0x2437 && !dialog.set_code(0x222f) && dialog.code() == 0x2437,
          "Invalid input destroyed the displayed character");
  require(dialog.set_code(0x3022) && !more->isEnabled() && readings->toPlainText().isEmpty(),
          "Missing metadata prevented basic mapped character information");

  jwpqt::qt::KanjiInfoDialog basic(nullptr, [](char32_t) {});
  auto* basic_text = basic.findChild<QTextEdit*>(QStringLiteral("kanjiInfoReadings"));
  require(basic.set_character(U'A') && basic.code() == 0x41 &&
              basic.set_code(0x2574) && basic_text->toPlainText().endsWith(QStringLiteral("vu")) &&
              basic.set_code(0x2621) && basic_text->toPlainText() == QStringLiteral("alpha") &&
              basic.set_code(0x2721) && basic_text->toPlainText() == QStringLiteral("a") &&
              basic.set_character(0x1f600) && basic.character() == 0x1f600 &&
              basic.code() == 0 && basic_text->toPlainText().isEmpty() &&
              !basic.set_character(0xd800) && basic.character() == 0x1f600,
           "Basic character information requires kanji metadata or loses Unicode scalars");

  for (bool invalid_reading : {false, true}) {
    std::string bytes(28, '\0');
    put_u32(bytes, 0, jwpqt::core::kKanjiInfoMagic);
    put_u16(bytes, 8, 1);
    put_u16(bytes, 10, 0x3021);
    if (invalid_reading) {
      put_u16(bytes, 12, 1U << 13U);
      put_u32(bytes, 24, 28U << 8U);
      bytes.append("\x7e\0", 2);
    }
    const auto damaged = jwpqt::core::KanjiInfoDatabase::parse(bytes);
    jwpqt::qt::KanjiInfoDialog damaged_dialog(&damaged, [](char32_t) {});
    require(damaged_dialog.set_code(0x2437), "Could not prepare malformed metadata test");
    try {
      require(damaged_dialog.set_character(0x4e9c) &&
                  damaged_dialog.code() == 0x3021 &&
                  damaged_dialog.findChild<QTextEdit*>(QStringLiteral("kanjiInfoReadings"))
                      ->toPlainText().isEmpty() &&
                  !damaged_dialog.findChild<QPushButton*>(QStringLiteral("kanjiInfoMore"))
                      ->isEnabled() &&
                  damaged_dialog.findChild<QLabel*>(QStringLiteral("kanjiInfoStatus"))
                      ->text().contains(QStringLiteral("Could not read kanji metadata")),
               "Malformed metadata did not produce basic character codes and a diagnostic");
    } catch (const std::exception&) {
      require(false, "Malformed kanji metadata escaped the information window");
    }
    require(damaged_dialog.set_code(0x2437) &&
                !damaged_dialog.findChild<QLabel*>(QStringLiteral("kanjiInfoStatus"))
                    ->text().contains(QStringLiteral("Could not read kanji metadata")),
             "Character information retained a stale metadata failure");
  }
}

QPoint character_point(QTextEdit& editor, int position, int width = 1,
                       bool right_half = false) {
  QTextCursor cursor(editor.document());
  cursor.setPosition(position);
  const QRect first = editor.cursorRect(cursor);
  cursor.setPosition(position + width);
  const QRect last = editor.cursorRect(cursor);
  const int left = std::min(first.left(), last.left());
  const int span = std::abs(first.left() - last.left());
  return QPoint(left + span * (right_half ? 3 : 1) / 4, first.center().y());
}

void test_character_targeting() {
  using jwpqt::qt::character_target;
  QTextEdit editor;
  require(!character_target(editor), "Empty document unexpectedly supplied a character");
  QFont font = editor.font();
  font.setPointSize(24);
  editor.setFont(font);
  editor.resize(480, 220);
  editor.document()->setDocumentMargin(17);
  editor.setPlainText(QStringLiteral("ab\u611b\u304b\U0001f600z"));
  editor.show();
  QApplication::processEvents();
  const QString original = editor.toPlainText();
  const char32_t expected[] = {U'a', U'b', U'\u611b', U'\u304b', 0x1f600, U'z'};
  int position = 0;
  for (const auto character : expected) {
    const int width = character > 0xffff ? 2 : 1;
    for (const bool right : {false, true}) {
      const auto hit = character_target(
          editor, character_point(editor, position, width, right));
      require(hit && hit->position == position && hit->character == character,
              "Character hit selected a neighboring glyph or surrogate half");
    }
    position += width;
  }
  require(!character_target(editor, QPoint(2, 2)) &&
              !character_target(editor, editor.viewport()->rect().bottomRight()),
          "Blank viewport area acquired an unrelated character");
  QTextCursor cursor(editor.document());
  cursor.setPosition(6);
  cursor.setPosition(4, QTextCursor::KeepAnchor);
  editor.setTextCursor(cursor);
  require(character_target(editor)->character == 0x1f600 &&
              character_target(editor, character_point(editor, 2))->character ==
                  U'\u611b' &&
              editor.textCursor().anchor() == 6 &&
              editor.textCursor().position() == 4 && editor.toPlainText() == original,
          "Character targeting moved a selection or ignored keyboard targeting");

  for (const auto direction : {Qt::LeftToRight, Qt::RightToLeft}) {
    editor.setLayoutDirection(direction);
    editor.setLineWrapMode(QTextEdit::NoWrap);
    editor.setPlainText(QString(40, QChar('x')) + QStringLiteral("\u611b") +
                        QString(40, QChar('x')));
    cursor = editor.textCursor();
    cursor.setPosition(41);
    editor.setTextCursor(cursor);
    editor.ensureCursorVisible();
    QApplication::processEvents();
    require(editor.horizontalScrollBar()->maximum() > 0,
            "Character hit test did not exercise horizontal scrolling");
    const auto hit = character_target(editor, character_point(editor, 40));
    require(hit && hit->position == 40 && hit->character == U'\u611b',
            "Scrolled character hit used the wrong document origin");
  }
  editor.setLayoutDirection(Qt::LeftToRight);
  editor.setLineWrapMode(QTextEdit::WidgetWidth);
  editor.resize(150, 160);
  editor.setPlainText(QString(80, QChar(0x611b)));
  cursor = editor.textCursor();
  cursor.setPosition(40);
  editor.setTextCursor(cursor);
  editor.ensureCursorVisible();
  QApplication::processEvents();
  const auto hit = character_target(editor, character_point(editor, 40));
  require(editor.verticalScrollBar()->value() > 0 && hit && hit->position == 40,
          "Wrapped/scrolled character hit used the wrong line");

  editor.resize(640, 220);
  editor.setLineWrapMode(QTextEdit::NoWrap);
  for (const auto& mixed : {QStringLiteral("ab \u05d0\u05d1 12 cd \u611b"),
                            QStringLiteral("\u05d0\u05d1 ab 12 \u611b")}) {
    editor.setPlainText(mixed);
    QApplication::processEvents();
    const auto block = editor.document()->firstBlock();
    const auto line = block.layout()->lineAt(0);
    const auto origin = editor.document()->documentLayout()->blockBoundingRect(block).topLeft();
    for (int i = 0; i < mixed.size(); ++i) {
      if (mixed.at(i).isSpace()) continue;
      const auto glyphs = line.glyphRuns(i, 1);
      require(!glyphs.empty(), "Bidirectional fixture has no rendered glyph");
      const auto bounds = glyphs.front().boundingRect();
      for (const qreal fraction : {0.25, 0.75}) {
        const QPoint point = (origin + QPointF(bounds.left() + bounds.width() * fraction,
                                               bounds.center().y())).toPoint();
        const auto target = character_target(editor, point);
        require(target && target->position == i && target->character == mixed.at(i).unicode(),
                "Mixed-direction character targeting crossed a visual run boundary");
      }
    }
  }

  editor.setPlainText(QStringLiteral("abc"));
  cursor = editor.textCursor();
  cursor.setPosition(1);
  editor.setTextCursor(cursor);
  QInputMethodEvent preedit(QStringLiteral("\u611b\u3057"), {});
  QApplication::sendEvent(&editor, &preedit);
  QApplication::processEvents();
  require(!editor.textCursor().block().layout()->preeditAreaText().isEmpty() &&
              !character_target(editor) &&
              !character_target(editor, character_point(editor, 1)),
          "Uncommitted IME text was mistaken for a stored character");
  QInputMethodEvent commit;
  commit.setCommitString(QStringLiteral("\u611b"));
  QApplication::sendEvent(&editor, &commit);
  require(character_target(editor, character_point(editor, 1))->character == U'\u611b',
          "Committed IME text could not be inspected");
}

}  // namespace

int main(int argc, char* argv[]) {
  QApplication application(argc, argv);
  test_dialog();
  test_character_targeting();
  return EXIT_SUCCESS;
}
