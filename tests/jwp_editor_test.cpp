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
#include <QImage>
#include <QPalette>
#include <QScrollBar>
#include <QSizeF>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>

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

}  // namespace

int main(int argc, char** argv) {
  QApplication application(argc, argv);
  try {
    test_paragraph_layout();
    test_clear_and_validation();
    test_page_break_marker_painting();
    test_kanji_colors_follow_raw_tokens();
    test_kanji_color_validation_is_atomic();
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
