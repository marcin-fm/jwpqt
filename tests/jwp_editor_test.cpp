// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwp_editor.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

#include <QApplication>
#include <QAbstractTextDocumentLayout>
#include <QEventLoop>
#include <QImage>
#include <QMouseEvent>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QPalette>
#include <QScrollBar>
#include <QSizeF>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
#include <QTimer>

#include "jwpqt/core/jwp_text_codec.h"

namespace {

void require(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

bool close_to(qreal left, qreal right) {
  return std::abs(left - right) < 0.01;
}

void test_paragraph_layout() {
  jwpqt::qt::JwpEditor editor;
  editor.setLineWrapMode(QTextEdit::FixedPixelWidth);
  editor.setLineWrapColumnOrWidth(180);
  editor.setPlainText(QStringLiteral(
      "first first first first first first\n"
      "second second second second second second\n\nthird"));

  jwpqt::core::JwpDocument document;
  document.paragraphs.resize(4);
  document.paragraphs[0].left_indent = 2;
  document.paragraphs[0].right_indent = 3;
  document.paragraphs[0].first_indent = -1;
  document.paragraphs[0].line_spacing = 125;
  document.paragraphs[1].line_spacing = 80;
  document.paragraphs[2].page_break = true;

  editor.document()->setModified(true);
  editor.apply_jwp_layout(document);
  require(editor.document()->isModified(),
          "Applying JWP layout changed the modified state");

  const QTextBlock first = editor.document()->begin();
  const QTextBlockFormat first_format = first.blockFormat();
  const qreal unit = first_format.leftMargin() / 2.0;
  require(unit > 0.0, "JWP indent unit was not positive");
  require(close_to(first_format.rightMargin(), 3.0 * unit),
          "Right indent did not use JWP character units");
  require(close_to(first_format.textIndent(), -unit),
          "First-line indent did not use JWP character units");
  require(first_format.lineHeightType() ==
              QTextBlockFormat::ProportionalHeight &&
              close_to(first_format.lineHeight(), 125.0),
          "JWP proportional line spacing was not applied");

  const QTextBlock compact = first.next();
  const QTextBlock page_break = compact.next();
  const QTextBlockFormat page_format = page_break.blockFormat();
  require(page_format.property(jwpqt::qt::JwpEditor::kPageBreakProperty)
              .toBool(),
          "JWP hard page break marker was not retained");
  const QTextBlock last = page_break.next();
  require(last.blockFormat().pageBreakPolicy().testFlag(
              QTextFormat::PageBreak_AlwaysBefore),
          "JWP hard page break was not exposed to paged layout");
  require(close_to(compact.blockFormat().lineHeight(), 80.0),
          "Compact paragraph line spacing was not applied");

  editor.document()->documentLayout()->documentSize();
  require(first.layout()->lineCount() > 1 && compact.layout()->lineCount() > 1 &&
              last.layout()->lineCount() > 0,
          "Qt did not wrap the JWP paragraph fixtures");
  require(first.layout()->lineAt(0).x() > compact.layout()->lineAt(0).x(),
          "JWP first-line and left indents did not affect text geometry");
  const qreal expanded_step = first.layout()->lineAt(1).y() -
                              first.layout()->lineAt(0).y();
  const qreal compact_step = compact.layout()->lineAt(1).y() -
                             compact.layout()->lineAt(0).y();
  require(expanded_step > compact_step,
          "JWP proportional spacing did not affect wrapped-line geometry (" +
              std::to_string(expanded_step) + " vs " +
              std::to_string(compact_step) + ")");
  std::unique_ptr<QTextDocument> paged(editor.document()->clone());
  paged->setPageSize(QSizeF(600.0, 1000.0));
  const QTextBlock paged_last =
      paged->begin().next().next().next();
  require(paged->pageCount() == 2,
          "JWP hard page break did not affect rich-document pagination (pages=" +
              std::to_string(paged->pageCount()) + ", page=" +
              std::to_string(paged->pageSize().width()) + "x" +
              std::to_string(paged->pageSize().height()) +
              ", last-y=" +
              std::to_string(paged->documentLayout()
                                 ->blockBoundingRect(paged_last)
                                 .top()) +
              ")");
}

void test_clear_and_validation() {
  jwpqt::qt::JwpEditor editor;
  editor.setPlainText(QStringLiteral("one\ntwo"));

  jwpqt::core::JwpDocument document;
  document.paragraphs.resize(2);
  document.paragraphs[0].left_indent = 4;
  document.paragraphs[1].page_break = true;
  editor.apply_jwp_layout(document);
  editor.clear_jwp_layout();

  for (QTextBlock block = editor.document()->begin(); block.isValid();
       block = block.next()) {
    const QTextBlockFormat format = block.blockFormat();
    require(close_to(format.leftMargin(), 0.0) &&
                !format.property(jwpqt::qt::JwpEditor::kPageBreakProperty)
                     .toBool(),
            "Clearing JWP layout left paragraph metadata behind");
  }

  document.paragraphs.push_back({});
  try {
    editor.apply_jwp_layout(document);
    require(false, "Mismatched JWP paragraph count was accepted");
  } catch (const std::invalid_argument&) {
  }

  document.paragraphs.clear();
  editor.apply_jwp_layout(document);
}

void test_page_break_marker_painting() {
  jwpqt::qt::JwpEditor editor;
  editor.resize(360, 140);
  QPalette palette = editor.palette();
  const QColor marker(255, 0, 255);
  palette.setColor(QPalette::Base, Qt::white);
  palette.setColor(QPalette::Text, Qt::black);
  palette.setColor(QPalette::Mid, marker);
  editor.setPalette(palette);

  QString text;
  jwpqt::core::JwpDocument document;
  constexpr int kPageBreakBlock = 24;
  constexpr int kBlockCount = 32;
  document.paragraphs.resize(kBlockCount);
  for (int index = 0; index < kBlockCount; ++index) {
    if (index > 0) {
      text += QLatin1Char('\n');
    }
    if (index != kPageBreakBlock) {
      text += QStringLiteral("visible line");
    }
  }
  document.paragraphs[kPageBreakBlock].page_break = true;
  editor.setPlainText(text);
  editor.apply_jwp_layout(document);
  editor.show();

  QTextCursor cursor(
      editor.document()->findBlockByNumber(kPageBreakBlock));
  editor.setTextCursor(cursor);
  editor.ensureCursorVisible();
  QApplication::processEvents();
  require(editor.verticalScrollBar()->value() > 0,
          "Page-break paint fixture did not scroll the editor");

  const QImage image = editor.viewport()->grab().toImage();
  int marker_pixels = 0;
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      if (image.pixelColor(x, y).rgb() == marker.rgb()) {
        ++marker_pixels;
      }
    }
  }
  require(marker_pixels > editor.viewport()->width(),
          "Scrolled JWP page-break marker was not painted");
}

std::optional<QColor> foreground_at(const jwpqt::qt::JwpEditor& editor,
                                    int position) {
  for (const QTextEdit::ExtraSelection& selection : editor.extraSelections()) {
    if (selection.cursor.selectionStart() <= position &&
        position < selection.cursor.selectionEnd() &&
        selection.format.foreground().style() != Qt::NoBrush) {
      return selection.format.foreground().color();
    }
  }
  return std::nullopt;
}

void test_kanji_colors_follow_raw_tokens() {
  jwpqt::core::JwpDocument document;
  document.paragraphs.resize(2);
  document.paragraphs[0].text = {'A', 0x2422, 0x3021, 0x5021, 0x3022};
  document.paragraphs[0].left_indent = 2;
  document.paragraphs[1].text = {0x5022};

  jwpqt::qt::JwpEditor editor;
  const std::u32string first =
      jwpqt::core::decode_jwp_text(document.paragraphs[0].text);
  const std::u32string second =
      jwpqt::core::decode_jwp_text(document.paragraphs[1].text);
  editor.setPlainText(QString::fromUcs4(first.data(),
                                        static_cast<qsizetype>(first.size())) +
                      QLatin1Char('\n') +
                      QString::fromUcs4(second.data(),
                                        static_cast<qsizetype>(second.size())));
  editor.apply_jwp_layout(document);
  editor.document()->setModified(true);
  QTextCursor undo_cursor(editor.document());
  undo_cursor.movePosition(QTextCursor::End);
  undo_cursor.insertText(QStringLiteral("x"));
  undo_cursor.deletePreviousChar();
  require(editor.document()->isUndoAvailable(),
          "Kanji-color fixture did not create Qt undo history");
  QTextCursor selected(editor.document());
  selected.setPosition(2);
  selected.setPosition(5, QTextCursor::KeepAnchor);
  editor.setTextCursor(selected);

  jwpqt::core::KanjiColorList list;
  list.add(0x3021);
  list.add(0x5021);
  jwpqt::core::KanjiColorPolicy policy;
  policy.list_mode = jwpqt::core::KanjiListColorMode::kMatch;
  policy.list_color = {200, 10, 20};
  policy.colorize_uncommon = true;
  policy.uncommon_color = {10, 150, 30};
  editor.apply_kanji_colors(document, list, policy);

  require(editor.document()->isModified(),
          "Applying kanji colors changed the modified state");
  require(editor.textCursor().selectionStart() == 2 &&
              editor.textCursor().selectionEnd() == 5,
          "Applying kanji colors changed the editor selection");
  require(!foreground_at(editor, 0).has_value() &&
              !foreground_at(editor, 1).has_value(),
          "ASCII or kana received list coloring");
  require(foreground_at(editor, 2) == QColor(200, 10, 20) &&
              foreground_at(editor, 3) == QColor(200, 10, 20),
          "Listed common/uncommon kanji did not use list color");
  require(!foreground_at(editor, 4).has_value(),
          "Unlisted common kanji was colored in match mode");
  require(foreground_at(editor, 6) == QColor(10, 150, 30),
          "Unlisted uncommon kanji did not use uncommon color");
  require(editor.document()->begin().blockFormat().leftMargin() > 0.0,
          "Applying kanji colors damaged paragraph layout");
  require(editor.document()->isUndoAvailable(),
          "Applying kanji colors discarded Qt undo history");

  policy.list_mode = jwpqt::core::KanjiListColorMode::kNoMatch;
  editor.apply_kanji_colors(document, list, policy);
  require(!foreground_at(editor, 2).has_value() &&
              foreground_at(editor, 4) == QColor(200, 10, 20) &&
              foreground_at(editor, 6) == QColor(200, 10, 20),
          "Non-match mode did not update foreground precedence");

  QTextEdit::ExtraSelection transient;
  transient.cursor = QTextCursor(editor.document());
  transient.cursor.setPosition(2);
  transient.cursor.setPosition(3, QTextCursor::KeepAnchor);
  transient.format.setBackground(Qt::yellow);
  editor.set_transient_extra_selections({transient});
  require(editor.extraSelections().size() == 4,
          "Transient selection did not coexist with kanji color spans");

  editor.clear_kanji_colors();
  require(!foreground_at(editor, 2).has_value() &&
              !foreground_at(editor, 4).has_value() &&
              editor.extraSelections().size() == 1 &&
              editor.document()->isModified(),
          "Clearing kanji colors changed state, lost overlays, or left color");
  editor.set_transient_extra_selections({});
  require(editor.extraSelections().isEmpty(),
          "Clearing transient selections left an overlay");
}

void test_kanji_color_validation_is_atomic() {
  jwpqt::qt::JwpEditor editor;
  editor.setPlainText(QStringLiteral("plain"));
  QTextCursor cursor(editor.document());
  cursor.setPosition(0);
  cursor.setPosition(1, QTextCursor::KeepAnchor);
  QTextEdit::ExtraSelection original;
  original.cursor = cursor;
  original.format.setForeground(QColor(40, 50, 60));
  editor.set_transient_extra_selections({original});

  jwpqt::core::JwpDocument mismatch;
  mismatch.paragraphs.resize(1);
  mismatch.paragraphs[0].text = {0x3021};
  try {
    editor.apply_kanji_colors(mismatch, {}, {});
    require(false, "Mismatched JWP text was accepted for coloring");
  } catch (const std::invalid_argument&) {
  }
  require(foreground_at(editor, 0) == QColor(40, 50, 60) &&
              editor.extraSelections().size() == 1,
          "Failed color validation partially changed the document");

  mismatch.paragraphs.push_back({});
  try {
    editor.apply_kanji_colors(mismatch, {}, {});
    require(false, "Mismatched JWP block count was accepted for coloring");
  } catch (const std::invalid_argument&) {
  }

  jwpqt::core::JwpDocument empty;
  editor.apply_kanji_colors(empty, {}, {});
  require(foreground_at(editor, 0) == QColor(40, 50, 60) &&
              editor.extraSelections().size() == 1,
          "Empty JWP document cleared unrelated transient coloring");
}

void test_composed_overwrite() {
  jwpqt::qt::JwpEditor editor;
  const QString original = QStringLiteral("A\U0001f600B\nC");
  editor.setPlainText(original);
  editor.setOverwriteMode(true);
  const auto select = [&](int first, int last) {
    QTextCursor cursor(editor.document());
    cursor.setPosition(first);
    cursor.setPosition(last, QTextCursor::KeepAnchor);
    editor.setTextCursor(cursor);
  };
  select(1, 1);
  editor.insert_composed_text(U"\u304d\u3083");
  require(editor.toPlainText() == QStringLiteral("A\u304d\u3083\nC") &&
              editor.textCursor().position() == 3,
          "Composed overwrite split a scalar or lost its caret");
  editor.undo();
  require(editor.toPlainText() == original && !editor.document()->isModified(),
          "Composed overwrite did not retain one-step undo and its baseline");
  editor.redo();
  editor.insert_composed_text(U"X");
  require(editor.toPlainText() == QStringLiteral("A\u304d\u3083X\nC"),
          "Composed overwrite consumed a paragraph break");

  editor.setPlainText(original);
  select(1, 3);
  editor.insert_composed_text(U"xy");
  require(editor.toPlainText() == QStringLiteral("AxyB\nC"),
          "Composed selection replacement consumed following text");
  editor.insert_composed_text(U"z", false);
  require(editor.toPlainText() == QStringLiteral("AxyzB\nC"),
          "Later composition events overwrote the replaced selection's suffix");
  editor.setPlainText(QStringLiteral("e\u0300B"));
  editor.insert_composed_text(U"X");
  require(editor.toPlainText() == QStringLiteral("X\u0300B"),
          "Composed overwrite replaced more than one legacy scalar");

  editor.setPlainText(original);
  select(2, 2);
  try {
    editor.insert_composed_text(U"X");
    require(false, "Split-surrogate composed insertion was accepted");
  } catch (const std::invalid_argument&) {}
  select(1, 1);
  try {
    editor.insert_composed_text(std::u32string(1, static_cast<char32_t>(0xd800)));
    require(false, "Invalid composed Unicode was accepted");
  } catch (const std::invalid_argument&) {}
  editor.setReadOnly(true);
  editor.insert_composed_text(U"X");
  require(editor.toPlainText() == original && !editor.document()->isUndoAvailable(),
          "Rejected composed input changed the document or history");
  editor.setReadOnly(false);
  editor.setOverwriteMode(false);
  editor.insert_composed_text(U"X");
  require(editor.toPlainText() == QStringLiteral("AX\U0001f600B\nC"),
          "Composed insert mode unexpectedly overwrote text");

  editor.resize(180, 120);
  editor.show();
  editor.set_character_line_width(2);
  editor.setPlainText(QStringLiteral("\u3042\u3044\u3046\u3048"));
  QCoreApplication::processEvents();
  QTextCursor wrapped(editor.document());
  wrapped.movePosition(QTextCursor::EndOfLine);
  require(wrapped.position() > 0 && wrapped.position() < 4,
          "Visual-line overwrite fixture did not wrap");
  const int boundary = wrapped.position();
  editor.setTextCursor(wrapped);
  editor.setOverwriteMode(true);
  editor.insert_composed_text(U"X");
  QString expected = QStringLiteral("\u3042\u3044\u3046\u3048");
  expected.insert(boundary, QLatin1Char('X'));
  require(editor.toPlainText() == expected,
          "Overwrite replaced text beyond a visual-line end");
  editor.undo();
  require(editor.toPlainText() == QStringLiteral("\u3042\u3044\u3046\u3048"),
          "Visual-line insertion was not one undo transaction");

  editor.setPlainText(QStringLiteral("\u3042\u3044\u3046\u3048"));
  QCoreApplication::processEvents();
  wrapped = QTextCursor(editor.document());
  wrapped.movePosition(QTextCursor::EndOfLine);
  const int typed_boundary = wrapped.position();
  editor.setTextCursor(wrapped);
  QKeyEvent typed(QEvent::KeyPress, Qt::Key_Y, Qt::NoModifier,
                  QStringLiteral("Y"));
  QApplication::sendEvent(&editor, &typed);
  expected = QStringLiteral("\u3042\u3044\u3046\u3048");
  expected.insert(typed_boundary, QLatin1Char('Y'));
  require(editor.toPlainText() == expected,
          "Typed overwrite replaced text beyond a visual-line end");

  editor.setPlainText(QStringLiteral("\u3042\u3044\u3046\u3048"));
  QCoreApplication::processEvents();
  wrapped = QTextCursor(editor.document());
  wrapped.movePosition(QTextCursor::EndOfLine);
  const int ime_boundary = wrapped.position();
  editor.setTextCursor(wrapped);
  QInputMethodEvent wrapped_commit;
  wrapped_commit.setCommitString(QStringLiteral("Z"));
  QApplication::sendEvent(&editor, &wrapped_commit);
  expected = QStringLiteral("\u3042\u3044\u3046\u3048");
  expected.insert(ime_boundary, QLatin1Char('Z'));
  require(editor.toPlainText() == expected,
          "Input method overwrite replaced text beyond a visual-line end");
  editor.set_character_line_width(std::nullopt);
  editor.hide();

  editor.setPlainText(QStringLiteral("ABC"));
  editor.setOverwriteMode(true);
  select(1, 1);
  QInputMethodEvent preedit(QStringLiteral("\u3042"), {});
  QApplication::sendEvent(&editor, &preedit);
  require(editor.toPlainText() == QStringLiteral("ABC") && !editor.document()->isModified(),
          "Overwrite preedit changed document text");
  QInputMethodEvent commit;
  commit.setCommitString(QStringLiteral("\u3042"));
  QApplication::sendEvent(&editor, &commit);
  require(editor.toPlainText() == QStringLiteral("A\u3042C"),
          "Input method commit ignored overwrite mode");
  editor.undo();
  require(editor.toPlainText() == QStringLiteral("ABC") && !editor.document()->isModified(),
          "Input method overwrite lost one-step undo");
  select(1, 2);
  QInputMethodEvent selected_commit;
  selected_commit.setCommitString(QStringLiteral("xy"));
  QApplication::sendEvent(&editor, &selected_commit);
  require(editor.toPlainText() == QStringLiteral("AxyC"),
          "Input method commit overwrote outside its selection");
  editor.undo();
  select(2, 2);
  QInputMethodEvent replacement;
  replacement.setCommitString(QStringLiteral("Z"), -1, 1);
  QApplication::sendEvent(&editor, &replacement);
  require(editor.toPlainText() == QStringLiteral("AZC"),
          "Overwrite mode changed an explicit input method replacement");
  editor.undo();
  select(1, 1);
  QInputMethodEvent continued(QStringLiteral("\u3046"), {});
  continued.setCommitString(QStringLiteral("\u3044"));
  QApplication::sendEvent(&editor, &continued);
  require(editor.toPlainText() == QStringLiteral("A\u3044C") &&
              editor.textCursor().block().layout()->preeditAreaText() == QStringLiteral("\u3046"),
          "Input method overwrite lost continuing preedit");
  QInputMethodEvent finish;
  finish.setCommitString(QStringLiteral("\u3048"));
  QApplication::sendEvent(&editor, &finish);
  require(editor.toPlainText() == QStringLiteral("A\u3044\u3048"),
          "Continued input method overwrite used a stale cursor");
  editor.setPlainText(original);
  select(1, 1);
  QInputMethodEvent invalid;
  invalid.setCommitString(QString(QChar(0xd800)));
  QApplication::sendEvent(&editor, &invalid);
  require(editor.toPlainText() == original && !editor.document()->isUndoAvailable(),
          "Malformed input method commit damaged the document");
  QKeyEvent format(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier,
                   QStringLiteral("\ufeff\u00a0"));
  QApplication::sendEvent(&editor, &format);
  require(jwpqt::qt::document_plain_text(*editor.document()) == QStringLiteral("A\ufeff\u00a0\nC"),
          "Typed format characters bypassed scalar-safe document overwrite");
  editor.undo();
  require(editor.toPlainText() == original && !editor.document()->isUndoAvailable(),
          "Typed format-character overwrite was not one undo transaction");
  for (const char16_t scalar : {char16_t{0xd800}, char16_t{0xdc00}}) {
    QKeyEvent malformed(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier,
                        QString(QChar(scalar)));
    QApplication::sendEvent(&editor, &malformed);
    require(editor.toPlainText() == original && !editor.document()->isUndoAvailable(),
            "Malformed typed Unicode changed the document");
  }
}

void wait_for_events(int milliseconds) {
  QEventLoop loop;
  QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
  loop.exec();
}

void send_mouse(jwpqt::qt::JwpEditor& editor, QEvent::Type type,
                const QPoint& position, Qt::MouseButton button,
                Qt::MouseButtons buttons) {
  QMouseEvent event(type, QPointF(position),
                    QPointF(editor.viewport()->mapToGlobal(position)),
                    button, buttons, Qt::NoModifier);
  QApplication::sendEvent(editor.viewport(), &event);
}

void test_selection_autoscroll() {
  jwpqt::qt::JwpEditor editor;
  editor.resize(240, 120);
  QString text;
  for (int i = 0; i < 100; ++i)
    text += QStringLiteral("selection row %1\n").arg(i);
  editor.setPlainText(text);
  editor.show();
  QApplication::processEvents();
  require(editor.verticalScrollBar()->maximum() > 0,
          "Autoscroll fixture does not overflow its viewport");

  const QPoint start(8, 8);
  const QPoint edge(8, editor.viewport()->height() - 1);
  editor.set_selection_autoscroll(false, 20);
  send_mouse(editor, QEvent::MouseButtonPress, start,
             Qt::LeftButton, Qt::LeftButton);
  send_mouse(editor, QEvent::MouseMove, edge,
             Qt::NoButton, Qt::LeftButton);
  const int disabled_position = editor.verticalScrollBar()->value();
  wait_for_events(80);
  require(editor.verticalScrollBar()->value() == disabled_position,
          "Disabled selection autoscroll continued moving the document");
  send_mouse(editor, QEvent::MouseButtonRelease, edge,
             Qt::LeftButton, Qt::NoButton);

  editor.verticalScrollBar()->setValue(0);
  editor.set_selection_autoscroll(true, 20);
  send_mouse(editor, QEvent::MouseButtonPress, start,
             Qt::LeftButton, Qt::LeftButton);
  send_mouse(editor, QEvent::MouseMove, edge,
             Qt::NoButton, Qt::LeftButton);
  const int first_position = editor.verticalScrollBar()->value();
  wait_for_events(90);
  require(first_position > 0 &&
              editor.verticalScrollBar()->value() > first_position &&
              editor.textCursor().hasSelection(),
          "Enabled selection autoscroll did not repeat or extend the selection");
  send_mouse(editor, QEvent::MouseButtonRelease, edge,
             Qt::LeftButton, Qt::NoButton);
  const int released_position = editor.verticalScrollBar()->value();
  wait_for_events(60);
  require(editor.verticalScrollBar()->value() == released_position,
          "Selection autoscroll continued after mouse release");

  editor.verticalScrollBar()->setValue(editor.verticalScrollBar()->maximum());
  const QPoint lower_start(8, editor.viewport()->height() - 8);
  const QPoint top_edge(8, 0);
  send_mouse(editor, QEvent::MouseButtonPress, lower_start,
             Qt::LeftButton, Qt::LeftButton);
  send_mouse(editor, QEvent::MouseMove, top_edge,
             Qt::NoButton, Qt::LeftButton);
  const int upward_first = editor.verticalScrollBar()->value();
  wait_for_events(90);
  require(upward_first < editor.verticalScrollBar()->maximum() &&
              editor.verticalScrollBar()->value() < upward_first,
          "Selection autoscroll did not repeat upward");
  send_mouse(editor, QEvent::MouseButtonRelease, top_edge,
             Qt::LeftButton, Qt::NoButton);
  require(editor.selection_autoscroll_enabled() &&
              editor.selection_autoscroll_interval() == 20,
          "Autoscroll editor policy was not retained");
  bool rejected = false;
  try {
    editor.set_selection_autoscroll(true, 10001);
  } catch (const std::out_of_range&) {
    rejected = true;
  }
  require(rejected && editor.selection_autoscroll_interval() == 20,
          "Invalid autoscroll delay changed the editor policy");
}

void test_mouse_hold_popup() {
  jwpqt::qt::JwpEditor editor;
  editor.resize(240, 120);
  editor.setPlainText(QStringLiteral("hold popup"));
  editor.show();
  QApplication::processEvents();

  int calls = 0;
  QPoint held_position;
  QPoint held_global_position;
  editor.set_mouse_hold_handler(
      [&](const QPoint& position, const QPoint& global_position) {
        ++calls;
        held_position = position;
        held_global_position = global_position;
      });
  const int saved_interval = QApplication::doubleClickInterval();
  QApplication::setDoubleClickInterval(25);
  const QPoint start(12, 12);
  send_mouse(editor, QEvent::MouseButtonPress, start,
             Qt::LeftButton, Qt::LeftButton);
  wait_for_events(60);
  require(calls == 1 && held_position == start &&
              held_global_position == editor.viewport()->mapToGlobal(start),
          "Stationary left-button hold did not invoke the popup handler");
  send_mouse(editor, QEvent::MouseButtonRelease, start,
             Qt::LeftButton, Qt::NoButton);

  send_mouse(editor, QEvent::MouseButtonPress, start,
             Qt::LeftButton, Qt::LeftButton);
  send_mouse(editor, QEvent::MouseButtonRelease, start,
             Qt::LeftButton, Qt::NoButton);
  wait_for_events(60);
  require(calls == 1, "Released left click invoked the hold popup");

  send_mouse(editor, QEvent::MouseButtonPress, start,
             Qt::LeftButton, Qt::LeftButton);
  const QPoint moved = start +
      QPoint(QApplication::startDragDistance() + 1, 0);
  send_mouse(editor, QEvent::MouseMove, moved,
             Qt::NoButton, Qt::LeftButton);
  wait_for_events(60);
  send_mouse(editor, QEvent::MouseButtonRelease, moved,
             Qt::LeftButton, Qt::NoButton);
  QApplication::setDoubleClickInterval(saved_interval);
  require(calls == 1, "Dragged left press invoked the hold popup");
}

void test_control_line_scroll() {
  using namespace jwpqt::qt;
  JwpEditor editor;
  editor.resize(280, 120);
  QString text;
  for (int i = 0; i < 40; ++i)
    text += QStringLiteral("line %1\n").arg(i);
  editor.setPlainText(text);
  editor.show();
  QApplication::processEvents();
  require(editor.verticalScrollBar()->maximum() > 0,
          "Control-scroll fixture does not overflow its viewport");

  editor.verticalScrollBar()->setValue(
      editor.verticalScrollBar()->maximum() / 2);
  QTextCursor selection(editor.document());
  selection.setPosition(20);
  selection.setPosition(36, QTextCursor::KeepAnchor);
  editor.setTextCursor(selection);
  const int position = editor.textCursor().position();
  const int anchor = editor.textCursor().anchor();
  const int before = editor.verticalScrollBar()->value();
  editor.scroll_view_line(1);
  require(editor.verticalScrollBar()->value() > before &&
              editor.textCursor().position() == position &&
              editor.textCursor().anchor() == anchor,
          "Control-scroll changed the selection or did not move the viewport");
  const int down = editor.verticalScrollBar()->value();
  editor.scroll_view_line(-1);
  require(editor.verticalScrollBar()->value() < down &&
              editor.textCursor().position() == position &&
              editor.textCursor().anchor() == anchor,
          "Reverse control-scroll changed the selection or did not move the viewport");

  bool rejected = false;
  try {
    editor.scroll_view_line(0);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  require(rejected, "Invalid control-scroll direction was accepted");
}

void test_character_line_width() {
  jwpqt::qt::JwpEditor editor;
  editor.setPlainText(QStringLiteral("one two three four five six seven"));
  editor.document()->setModified(false);
  editor.set_character_line_width(12);
  require(editor.configured_character_line_width() == 12 &&
              editor.character_page_width() == 12 &&
              editor.lineWrapMode() == QTextEdit::FixedPixelWidth &&
              editor.lineWrapColumnOrWidth() > 0 &&
              !editor.document()->isModified(),
          "Fixed character width did not configure the editor without modifying text");
  const int width = editor.lineWrapColumnOrWidth();
  bool rejected = false;
  try {
    editor.set_character_line_width(1001);
  } catch (const std::out_of_range&) {
    rejected = true;
  }
  require(rejected && editor.configured_character_line_width() == 12 &&
              editor.lineWrapColumnOrWidth() == width,
          "Invalid character width changed the editor policy");
  editor.set_character_line_width(std::nullopt);
  require(!editor.configured_character_line_width().has_value() &&
              editor.lineWrapMode() == QTextEdit::WidgetWidth &&
              !editor.document()->isModified(),
          "Dynamic character width did not restore viewport wrapping");
}

void test_margin_relaxation() {
  using namespace jwpqt;
  core::JwpDocument source;
  source.paragraphs.resize(1);
  source.paragraphs[0].text =
      {0x467c, 0x467c, 0x467c, 0x467c, 0x2123};
  const std::u32string decoded =
      core::decode_jwp_text(source.paragraphs[0].text);
  qt::JwpEditor editor;
  editor.setPlainText(QString::fromUcs4(
      decoded.data(), static_cast<qsizetype>(decoded.size())));
  editor.apply_jwp_layout(source);
  editor.apply_jwp_fonts(source, core::kDefaultLegacyCodePage);
  editor.set_character_line_width(4);
  editor.document()->setModified(true);

  editor.apply_margin_relaxation(source, core::kDefaultLegacyCodePage, true,
                                 true);
  const QTextBlock block = editor.document()->begin();
  QTextCursor first_period(editor.document());
  first_period.setPosition(4);
  first_period.setPosition(5, QTextCursor::KeepAnchor);
  require(block.layout()->lineAt(0).textLength() == 5 &&
              editor.toPlainText().size() == 5 &&
              editor.document()->isModified(),
          "Closing punctuation did not hang beyond the right margin: " +
              std::to_string(block.layout()->lineAt(0).textLength()) + "/" +
              std::to_string(first_period.charFormat().fontStretch()) +
              "/" + std::to_string(editor.lineWrapColumnOrWidth()) + "/" +
              std::to_string(block.layout()->lineAt(0).naturalTextWidth()));
  require(first_period.charFormat().fontStretch() == 1,
          "Planned punctuation was not marked for custom rendering");
  QPalette palette = editor.palette();
  palette.setColor(QPalette::Base, Qt::white);
  palette.setColor(QPalette::Text, Qt::black);
  editor.setPalette(palette);
  editor.resize(180, 80);
  editor.show();
  QApplication::processEvents();
  QTextCursor pair_start(editor.document());
  pair_start.setPosition(3);
  const QRect pair_rect = editor.cursorRect(pair_start);
  const QImage rendering = editor.viewport()->grab().toImage();
  int painted_pixels = 0;
  const QRect sample(pair_rect.left(), pair_rect.top(),
                     std::min(40, rendering.width() - pair_rect.left()),
                     std::min(editor.fontMetrics().height() + 4,
                              rendering.height() - pair_rect.top()));
  for (int y = sample.top(); y < sample.bottom(); ++y)
    for (int x = sample.left(); x < sample.right(); ++x)
      if (rendering.pixelColor(x, y).value() < 128) ++painted_pixels;
  require(painted_pixels > 20 &&
              rendering.save(QStringLiteral("margin-relaxation.png")),
          "Relaxed margin glyphs were not painted at their full size");

  editor.apply_margin_relaxation(source, core::kDefaultLegacyCodePage, false,
                                 false);
  require(block.layout()->lineAt(0).textLength() < 5 &&
              first_period.charFormat().fontStretch() != 1 &&
              editor.toPlainText().size() == 5 &&
              editor.document()->isModified(),
          "Disabling margin relaxation did not restore ordinary wrapping");
  editor.clear_jwp_layout();
  require(first_period.charFormat().fontStretch() != 1,
          "Clearing JWP presentation retained margin relaxation");

  source.paragraphs[0].text.push_back(0x2122);
  const std::u32string adjacent_decoded =
      core::decode_jwp_text(source.paragraphs[0].text);
  qt::JwpEditor adjacent_editor;
  adjacent_editor.setPlainText(QString::fromUcs4(
      adjacent_decoded.data(), static_cast<qsizetype>(adjacent_decoded.size())));
  adjacent_editor.apply_jwp_layout(source);
  adjacent_editor.apply_jwp_fonts(source, core::kDefaultLegacyCodePage);
  adjacent_editor.set_character_line_width(4);
  adjacent_editor.apply_margin_relaxation(
      source, core::kDefaultLegacyCodePage, true, true);
  QTextCursor first_closing(adjacent_editor.document());
  first_closing.setPosition(4);
  first_closing.setPosition(5, QTextCursor::KeepAnchor);
  QTextCursor second_closing(adjacent_editor.document());
  second_closing.setPosition(5);
  second_closing.setPosition(6, QTextCursor::KeepAnchor);
  require(first_closing.charFormat().fontStretch() == 1 &&
              second_closing.charFormat().fontStretch() != 1 &&
              adjacent_editor.toPlainText() ==
                  QString::fromUcs4(adjacent_decoded.data(),
                                    static_cast<qsizetype>(
                                        adjacent_decoded.size())) &&
              source.paragraphs[0].text.back() == 0x2122,
          "Adjacent closing marks changed the one-character planner or "
          "document identity");
}

}  // namespace

int main(int argc, char** argv) {
  QApplication application(argc, argv);
  try {
    test_paragraph_layout();
    test_clear_and_validation();
    test_page_break_marker_painting();
    test_kanji_colors_follow_raw_tokens();
    test_kanji_color_validation_is_atomic();
    test_composed_overwrite();
    test_selection_autoscroll();
    test_mouse_hold_popup();
    test_control_line_scroll();
    test_character_line_width();
    test_margin_relaxation();
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
