// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwp_editor.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

#include <QAbstractTextDocumentLayout>
#include <QFontMetricsF>
#include <QPaintEvent>
#include <QPainter>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>

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

}  // namespace

JwpEditor::JwpEditor(QWidget* parent) : QTextEdit(parent) {
  setAcceptRichText(false);
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
