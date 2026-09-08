// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwp_editor.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include <QAbstractTextDocumentLayout>
#include <QColor>
#include <QCoreApplication>
#include <QFontMetricsF>
#include <QImage>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMimeData>
#include <QPaintEvent>
#include <QPainter>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStatusTipEvent>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>

#include "jwpqt/core/jwp_text_codec.h"
#include "japanese_fonts.h"

namespace jwpqt::qt {
namespace {

class DocumentStateGuard {
 public:
  explicit DocumentStateGuard(QTextDocument* document)
      : document_(document),
        modified_(document->isModified()),
        undo_enabled_(document->isUndoRedoEnabled()),
        blocker_(document) {
    document_->setUndoRedoEnabled(false);
  }

  ~DocumentStateGuard() {
    document_->setUndoRedoEnabled(undo_enabled_);
    document_->setModified(modified_);
  }

  DocumentStateGuard(const DocumentStateGuard&) = delete;
  DocumentStateGuard& operator=(const DocumentStateGuard&) = delete;

 private:
  QTextDocument* document_;
  bool modified_;
  bool undo_enabled_;
  QSignalBlocker blocker_;
};

template <typename Update>
void preserve_document_state(QTextDocument* document, Update&& update) {
  const DocumentStateGuard guard(document);
  std::forward<Update>(update)();
}

QString to_qstring(std::u32string_view text) {
  QString result;
  for (const char32_t code_point : text) {
    if (code_point > 0x10ffffU ||
        (code_point >= 0xd800U && code_point <= 0xdfffU)) {
      throw std::invalid_argument("JWP text contains an invalid Unicode scalar");
    }
    if (code_point <= 0xffffU) {
      result.append(QChar(static_cast<char16_t>(code_point)));
      continue;
    }
    const std::uint32_t value = static_cast<std::uint32_t>(code_point) - 0x10000U;
    result.append(QChar(static_cast<char16_t>(0xd800U + (value >> 10U))));
    result.append(QChar(static_cast<char16_t>(0xdc00U + (value & 0x3ffU))));
  }
  return result;
}

std::u32string checked_input_text(const QString& text) {
  std::u32string scalars;
  for (qsizetype i = 0; i < text.size(); ++i) {
    const QChar character = text[i];
    if (character.isHighSurrogate()) {
      if (++i == text.size() || !text[i].isLowSurrogate())
        throw std::invalid_argument("Invalid Unicode input");
      scalars.push_back(QChar::surrogateToUcs4(character, text[i]));
    } else if (character.isLowSurrogate()) {
      throw std::invalid_argument("Invalid Unicode input");
    } else {
      scalars.push_back(character.unicode());
    }
  }
  return scalars;
}

}  // namespace

QMimeData* JwpEditor::createMimeDataFromSelection() const {
  // Qt's fragment MIME is lazy. Preserve all native/rich representations before
  // replacing plain text (whose default conversion folds nonbreaking spaces).
  std::unique_ptr<QMimeData> original(QTextEdit::createMimeDataFromSelection());
  auto result = std::make_unique<QMimeData>();
  for (const auto& format : original->formats()) result->setData(format, original->data(format));
  QString text = textCursor().selectedText();
  text.replace(QChar::ParagraphSeparator, QLatin1Char('\n'));
  text.replace(QChar::LineSeparator, QLatin1Char('\n'));
  result->setText(text);
  result->setProperty("jwpqtInternalCopy", true);
  if (text.isEmpty() || !clipboard_bitmap_enabled(*this)) return result.release();
  try {
    if (text.size() > 262144) throw std::runtime_error("selection exceeds 262144 character positions");
    (void)checked_input_text(text);
    const QFont font = japanese_font(*this, JapaneseFontRole::kBitmap);
    QImage metrics(1, 1, QImage::Format_RGB32);
    QTextDocument bitmap;
    bitmap.documentLayout()->setPaintDevice(&metrics);
    bitmap.setDefaultFont(font);
    bitmap.setDocumentMargin(2);
    bitmap.setPlainText(text);
    const qreal old_unit = QFontMetricsF(document()->defaultFont()).horizontalAdvance(QStringLiteral("\u3000"));
    const qreal new_unit = QFontMetricsF(font, &metrics).horizontalAdvance(QStringLiteral("\u3000"));
    const qreal source_width = document()->textWidth() > 0 ? document()->textWidth() : viewport()->width();
    const qreal width = source_width * (old_unit > 0 ? new_unit / old_unit : 1);
    if (!std::isfinite(width)) throw std::runtime_error("invalid bitmap line width");
    bitmap.setTextWidth(qBound<qreal>(1, width, 8192));
    const qreal w = std::ceil(bitmap.idealWidth()), h = std::ceil(bitmap.size().height());
    if (!std::isfinite(w) || !std::isfinite(h) || w > 8192 || h > 8192 || w * h > 16777216)
      throw std::runtime_error("bitmap exceeds 8192 pixels per edge or 16 million pixels");
    QImage image(qMax(1, static_cast<int>(w)), qMax(1, static_cast<int>(h)), QImage::Format_RGB32);
    if (image.isNull()) throw std::runtime_error("could not allocate clipboard bitmap");
    image.fill(Qt::white);
    QPainter painter(&image);
    QAbstractTextDocumentLayout::PaintContext context;
    context.palette.setColor(QPalette::Text, Qt::black);
    bitmap.documentLayout()->draw(&painter, context);
    painter.end();
    result->setImageData(image);
  } catch (const std::exception& error) {
    const auto message = tr("Clipboard text copied; bitmap omitted: %1").arg(QString::fromUtf8(error.what()));
    // Report after QTextEdit's copy/cut call finishes, never from its MIME callback.
    QTimer::singleShot(0, this, [this, message] {
      QStatusTipEvent event(message);
      QCoreApplication::sendEvent(window(), &event);
    });
  }
  return result.release();
}

QString document_plain_text(const QTextDocument& document) {
  // Qt's toPlainText also changes NBSP to space; only normalize line separators.
  return document.toRawText()
      .replace(QChar(QChar::ParagraphSeparator), QLatin1Char('\n'))
      .replace(QChar(QChar::LineSeparator), QLatin1Char('\n'));
}

JwpEditor::JwpEditor(QWidget* parent) : QTextEdit(parent) {
  setAcceptRichText(false);
}

void JwpEditor::insert_composed_text(std::u32string_view text, bool allow_overwrite) {
  if (isReadOnly() || text.empty()) return;
  const QString inserted = to_qstring(text);
  QTextCursor cursor = input_cursor(text, allow_overwrite);
  cursor.insertText(inserted);
  setTextCursor(cursor);
  ensureCursorVisible();
}

QTextCursor JwpEditor::input_cursor(std::u32string_view text, bool allow_overwrite) const {
  QTextCursor cursor = textCursor();
  const auto splits_scalar = [this](int position) {
    return position > 0 && document()->characterAt(position).isLowSurrogate() &&
           document()->characterAt(position - 1).isHighSurrogate();
  };
  if (splits_scalar(cursor.selectionStart()) || splits_scalar(cursor.selectionEnd()))
    throw std::invalid_argument("Composed input cannot split a Unicode scalar");
  if (!overwriteMode() || !allow_overwrite || cursor.hasSelection()) return cursor;

  // Composed input bypasses QTextEdit's typed-key overwrite handling. Never eat
  // a paragraph break or half of a supplementary character.
  int end = cursor.position();
  const int paragraph_end = cursor.block().position() + cursor.block().length() - 1;
  for (char32_t character : text) {
    if (end == paragraph_end || character == U'\n' || character == U'\r' ||
        character == U'\u2028' || character == U'\u2029') break;
    const QChar replaced = document()->characterAt(end++);
    if (replaced.isHighSurrogate() && end < paragraph_end &&
        document()->characterAt(end).isLowSurrogate()) ++end;
  }
  cursor.setPosition(end, QTextCursor::KeepAnchor);
  return cursor;
}

void JwpEditor::inputMethodEvent(QInputMethodEvent* event) {
  // IME commits ignore Qt's overwrite flag. Preserve explicit IME replacement
  // ranges and preedit attributes, supplying a range only for ordinary commits.
  const bool selection_attribute = std::any_of(event->attributes().begin(), event->attributes().end(),
      [](const auto& attribute) { return attribute.type == QInputMethodEvent::Selection; });
  if (!overwriteMode() || isReadOnly() || event->commitString().isEmpty() ||
      event->replacementStart() != 0 || event->replacementLength() != 0 ||
      textCursor().hasSelection() || selection_attribute) {
    QTextEdit::inputMethodEvent(event);
    return;
  }
  try {
    const QTextCursor range = input_cursor(checked_input_text(event->commitString()), true);
    QInputMethodEvent adjusted(event->preeditString(), event->attributes());
    adjusted.setCommitString(event->commitString(), 0, range.selectionEnd() - range.selectionStart());
    QTextEdit::inputMethodEvent(&adjusted);
    event->setAccepted(adjusted.isAccepted());
  } catch (const std::exception&) {
    event->ignore();
  }
}

void JwpEditor::keyPressEvent(QKeyEvent* event) {
  const QString text = event->text();
  const bool tab = event->key() == Qt::Key_Tab && event->modifiers() == Qt::NoModifier;
  if (!overwriteMode() || isReadOnly() || text.isEmpty() ||
      (event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) ||
      (!text.front().isPrint() && !text.front().isSurrogate() &&
       text.front().category() != QChar::Other_Format && !tab)) {
    QTextEdit::keyPressEvent(event);
    return;
  }
  // Qt's default overwrite exposes separate deletion/insertion undo commands.
  // Use the same atomic replacement for ordinary keys and composed kana.
  try {
    insert_composed_text(checked_input_text(text));
    event->accept();
  } catch (const std::exception&) {
    event->ignore();
  }
}

int JwpEditor::character_page_width() const {
  if (!isVisible()) {
    return 0;
  }
  const qreal width = static_cast<qreal>(viewport()->width()) -
                      2.0 * document()->documentMargin();
  if (width <= 0.0) {
    return 0;
  }
  return std::max(1, static_cast<int>(std::floor(width / indent_unit())));
}

void JwpEditor::apply_jwp_layout(const core::JwpDocument& jwp_document) {
  if (jwp_document.paragraphs.empty()) {
    clear_jwp_layout();
    return;
  }
  if (document()->blockCount() !=
      static_cast<int>(jwp_document.paragraphs.size())) {
    throw std::invalid_argument(
        "JWP paragraph count does not match the editor document");
  }

  const qreal unit = indent_unit();
  preserve_document_state(document(), [this, &jwp_document, unit] {
    QTextBlock block = document()->begin();
    bool follows_page_break = false;
    for (const core::JwpParagraph& paragraph : jwp_document.paragraphs) {
      QTextBlockFormat format;
      format.setLeftMargin(static_cast<qreal>(paragraph.left_indent) * unit);
      format.setRightMargin(static_cast<qreal>(paragraph.right_indent) * unit);
      format.setTextIndent(static_cast<qreal>(paragraph.first_indent) * unit);
      format.setLineHeight(std::max<std::int16_t>(paragraph.line_spacing, 1),
                           QTextBlockFormat::ProportionalHeight);
      format.setProperty(kPageBreakProperty, paragraph.page_break);
      if (follows_page_break) {
        format.setPageBreakPolicy(QTextFormat::PageBreak_AlwaysBefore);
      }
      QTextCursor cursor(block);
      cursor.setBlockFormat(format);
      follows_page_break = paragraph.page_break;
      block = block.next();
    }
  });
  viewport()->update();
}

void JwpEditor::clear_jwp_layout() {
  preserve_document_state(document(), [this] {
    for (QTextBlock block = document()->begin(); block.isValid();
         block = block.next()) {
      QTextCursor cursor(block);
      cursor.setBlockFormat(QTextBlockFormat{});
    }
  });
  viewport()->update();
}

void JwpEditor::apply_kanji_colors(
    const core::JwpDocument& jwp_document,
    const core::KanjiColorList& color_list,
    const core::KanjiColorPolicy& policy, core::LegacyCodePage code_page) {
  set_kanji_color_selections(
      prepare_kanji_colors(jwp_document, color_list, policy, code_page));
}

QList<QTextEdit::ExtraSelection> JwpEditor::prepare_kanji_colors(
    const core::JwpDocument& jwp_document,
    const core::KanjiColorList& color_list,
    const core::KanjiColorPolicy& policy,
    core::LegacyCodePage code_page) const {
  if (jwp_document.paragraphs.empty()) {
    return {};
  }
  if (jwp_document.paragraphs.size() >
      static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    throw std::length_error("JWP paragraph count exceeds Qt limits");
  }
  if (document()->blockCount() !=
      static_cast<int>(jwp_document.paragraphs.size())) {
    throw std::invalid_argument(
        "JWP paragraph count does not match the editor document");
  }

  QList<QTextEdit::ExtraSelection> selections;
  QTextBlock block = document()->begin();
  for (const core::JwpParagraph& paragraph : jwp_document.paragraphs) {
    const std::u32string decoded =
        core::decode_jwp_text(paragraph.text, code_page);
    const QString expected = to_qstring(decoded);
    if (block.text() != expected) {
      throw std::invalid_argument(
          "JWP paragraph text does not match the editor document");
    }
    if (expected.size() >
        static_cast<qsizetype>(std::numeric_limits<int>::max() -
                               block.position())) {
      throw std::length_error("JWP paragraph exceeds Qt position limits");
    }

    int offset = 0;
    std::optional<core::RgbColor> active_color;
    int active_begin = 0;
    for (std::size_t index = 0; index < paragraph.text.size(); ++index) {
      const std::optional<core::RgbColor> color = core::kanji_foreground_color(
          paragraph.text[index], color_list.contains(paragraph.text[index]),
          policy);
      if (color != active_color) {
        if (active_color.has_value()) {
          QTextEdit::ExtraSelection selection;
          selection.cursor = QTextCursor(document());
          selection.cursor.setPosition(block.position() + active_begin);
          selection.cursor.setPosition(block.position() + offset,
                                       QTextCursor::KeepAnchor);
          selection.format.setForeground(QColor(
              active_color->red, active_color->green, active_color->blue));
          selections.push_back(std::move(selection));
        }
        active_color = color;
        active_begin = offset;
      }
      offset += decoded[index] <= 0xffffU ? 1 : 2;
    }
    if (active_color.has_value()) {
      QTextEdit::ExtraSelection selection;
      selection.cursor = QTextCursor(document());
      selection.cursor.setPosition(block.position() + active_begin);
      selection.cursor.setPosition(block.position() + offset,
                                   QTextCursor::KeepAnchor);
      selection.format.setForeground(QColor(
          active_color->red, active_color->green, active_color->blue));
      selections.push_back(std::move(selection));
    }
    block = block.next();
  }

  return selections;
}

void JwpEditor::set_kanji_color_selections(
    QList<QTextEdit::ExtraSelection> selections) {
  kanji_color_selections_ = std::move(selections);
  update_extra_selections();
}

void JwpEditor::clear_kanji_colors() {
  kanji_color_selections_.clear();
  update_extra_selections();
}

void JwpEditor::set_transient_extra_selections(
    const QList<QTextEdit::ExtraSelection>& selections) {
  transient_extra_selections_ = selections;
  update_extra_selections();
}

void JwpEditor::update_extra_selections() {
  QList<QTextEdit::ExtraSelection> combined = kanji_color_selections_;
  combined.append(transient_extra_selections_);
  QTextEdit::setExtraSelections(combined);
}

void JwpEditor::paintEvent(QPaintEvent* event) {
  QTextEdit::paintEvent(event);

  QPainter painter(viewport());
  const qreal inset = 12.0;
  QAbstractTextDocumentLayout* layout = document()->documentLayout();
  const QPointF scroll(horizontalScrollBar()->value(),
                       verticalScrollBar()->value());
  const int hit = layout->hitTest(
      QPointF(scroll.x(), scroll.y() + event->rect().top()), Qt::FuzzyHit);
  QTextBlock block = hit >= 0 ? document()->findBlock(hit) : document()->begin();
  if (block.previous().isValid()) {
    block = block.previous();
  }
  for (; block.isValid(); block = block.next()) {
    const QRectF geometry = layout->blockBoundingRect(block).translated(-scroll);
    if (geometry.top() > event->rect().bottom()) {
      break;
    }
    if (geometry.bottom() < event->rect().top() ||
        !block.blockFormat().property(kPageBreakProperty).toBool()) {
      continue;
    }
    const qreal center = geometry.center().y();
    const QRectF bar(inset, center - 2.0,
                     std::max<qreal>(0.0, viewport()->width() - 2.0 * inset),
                     4.0);
    painter.fillRect(bar, palette().mid());
  }
}

qreal JwpEditor::indent_unit() const {
  const QFontMetricsF metrics(font());
  const qreal ideographic_space = metrics.horizontalAdvance(QChar(0x3000));
  return ideographic_space > 0.0 ? ideographic_space : metrics.height();
}

}  // namespace jwpqt::qt
