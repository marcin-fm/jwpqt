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
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStatusTipEvent>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFormat>
#include <QTimer>

#include "jwpqt/core/line_relaxation.h"
#include "jwpqt/core/jwp_text_codec.h"
#include "japanese_fonts.h"
#include "jwp_text_drawing.h"

namespace jwpqt::qt {
namespace {

constexpr int kRelaxedMarginCharacter = QTextFormat::UserProperty + 0x4a02;

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
  const auto options = clipboard_bitmap_options(*this);
  if (text.isEmpty() || !options.enabled) return result.release();
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
    const int selection_start = textCursor().selectionStart();
    const int selection_end = textCursor().selectionEnd();
    for (auto block = document()->findBlock(selection_start); block.isValid() && block.position() < selection_end; block = block.next())
      for (auto it = block.begin(); !it.atEnd(); ++it) {
        const auto part = it.fragment();
        const int first = std::max(selection_start, part.position());
        const int last = std::min(selection_end, part.position() + part.length());
        const int kind = part.charFormat().intProperty(kJwpCharacterKind);
        if (first >= last || !kind) continue;
        QTextCursor range(&bitmap); range.setPosition(first - selection_start);
        range.setPosition(last - selection_start, QTextCursor::KeepAnchor);
        QTextCharFormat format; format.setProperty(kJwpCharacterKind, kind);
        format.setFontFamilies(jwp_representation_font(font, kind).families());
        range.mergeCharFormat(format);
      }
    if (options.colors && kanji_list_coloring_) {
      const int start = textCursor().selectionStart(), end = textCursor().selectionEnd();
      for (const auto& color : kanji_color_selections_) {
        const int first = std::max(start, color.cursor.selectionStart());
        const int last = std::min(end, color.cursor.selectionEnd());
        if (first >= last) continue;
        QTextCursor range(&bitmap);
        range.setPosition(first - start);
        range.setPosition(last - start, QTextCursor::KeepAnchor);
        QTextCharFormat format;
        format.setForeground(color.format.foreground());
        range.mergeCharFormat(format);
      }
    }
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
    if (!options.vertical) {
      bitmap.documentLayout()->draw(&painter, context);
    } else {
      painter.setPen(Qt::black);
      for (QTextBlock block = bitmap.begin(); block.isValid(); block = block.next()) {
        const auto foreground = [&block](int at) {
          QTextCursor cursor(block);
          cursor.setPosition(block.position() + at);
          cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
          const auto brush = cursor.charFormat().foreground();
          return brush.style() == Qt::NoBrush ? QColor(Qt::black) : brush.color();
        };
        draw_jwp_text_layout(painter, *block.layout(), block.text(),
            bitmap.documentLayout()->blockBoundingRect(block).topLeft(), true,
            color_code_page_, foreground, [&block](int at) {
              QTextCursor cursor(block); cursor.setPosition(block.position() + at);
              cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
              return cursor.charFormat().intProperty(kJwpCharacterKind);
            });
      }
    }
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

JwpEditor::JwpEditor(QWidget* parent)
    : QTextEdit(parent), selection_scroll_timer_(new QTimer(this)) {
  setAcceptRichText(false);
  selection_scroll_timer_->setSingleShot(false);
  connect(selection_scroll_timer_, &QTimer::timeout, this,
          [this] { scroll_mouse_selection(); });
}

void JwpEditor::set_selection_autoscroll(bool enabled, int interval_ms) {
  if (interval_ms < 0 || interval_ms > 10000)
    throw std::out_of_range("Selection autoscroll delay is outside 0..10000 ms");
  selection_autoscroll_ = enabled;
  selection_scroll_interval_ = interval_ms;
  if (!enabled) stop_mouse_autoscroll();
  else if (selection_scroll_timer_->isActive())
    selection_scroll_timer_->setInterval(std::max(1, interval_ms));
}

bool JwpEditor::selection_autoscroll_enabled() const noexcept {
  return selection_autoscroll_;
}

int JwpEditor::selection_autoscroll_interval() const noexcept {
  return selection_scroll_interval_;
}

void JwpEditor::mousePressEvent(QMouseEvent* event) {
  stop_mouse_autoscroll();
  QTextEdit::mousePressEvent(event);
  mouse_selecting_ = event->button() == Qt::LeftButton;
}

void JwpEditor::mouseMoveEvent(QMouseEvent* event) {
  if (!mouse_selecting_ || !(event->buttons() & Qt::LeftButton)) {
    stop_mouse_autoscroll();
    QTextEdit::mouseMoveEvent(event);
    return;
  }

  const int width = viewport()->width();
  const int height = viewport()->height();
  if (width <= 0 || height <= 0) {
    stop_mouse_autoscroll();
    event->accept();
    return;
  }
  const int x = static_cast<int>(event->position().x());
  const int y = static_cast<int>(event->position().y());
  selection_scroll_x_ = std::clamp(x, 0, width - 1);
  if (y < 0 || y >= height) {
    extend_mouse_selection(selection_scroll_x_, std::clamp(y, 0, height - 1));
  } else {
    QTextEdit::mouseMoveEvent(event);
  }

  const int edge = std::max(1, fontMetrics().height() / 3);
  auto* scroll = verticalScrollBar();
  int direction = 0;
  if (y < edge && scroll->value() > scroll->minimum()) direction = -1;
  else if (y > height - edge && scroll->value() < scroll->maximum()) direction = 1;
  if (!selection_autoscroll_ || direction == 0) {
    stop_mouse_autoscroll();
    event->accept();
    return;
  }

  const bool begin = direction != selection_scroll_direction_ ||
      !selection_scroll_timer_->isActive();
  selection_scroll_direction_ = direction;
  if (begin) scroll_mouse_selection();
  selection_scroll_timer_->start(std::max(1, selection_scroll_interval_));
  event->accept();
}

void JwpEditor::mouseReleaseEvent(QMouseEvent* event) {
  stop_mouse_autoscroll();
  mouse_selecting_ = false;
  QTextEdit::mouseReleaseEvent(event);
}

void JwpEditor::extend_mouse_selection(int x, int y) {
  QTextCursor selection = textCursor();
  const int anchor = selection.anchor();
  const int position = cursorForPosition(QPoint(x, y)).position();
  selection.setPosition(anchor);
  selection.setPosition(position, QTextCursor::KeepAnchor);
  setTextCursor(selection);
}

void JwpEditor::scroll_mouse_selection() {
  if (!selection_autoscroll_ || !mouse_selecting_ ||
      selection_scroll_direction_ == 0) {
    stop_mouse_autoscroll();
    return;
  }
  auto* scroll = verticalScrollBar();
  const int before = scroll->value();
  scroll->triggerAction(selection_scroll_direction_ < 0
                            ? QAbstractSlider::SliderSingleStepSub
                            : QAbstractSlider::SliderSingleStepAdd);
  if (scroll->value() == before) {
    stop_mouse_autoscroll();
    return;
  }
  extend_mouse_selection(selection_scroll_x_,
                         selection_scroll_direction_ < 0 ? 0
                                                         : viewport()->height() - 1);
}

void JwpEditor::stop_mouse_autoscroll() {
  selection_scroll_timer_->stop();
  selection_scroll_direction_ = 0;
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

void JwpEditor::set_character_line_width(std::optional<int> characters) {
  if (characters.has_value() && (*characters < 1 || *characters > 1000)) {
    throw std::out_of_range("Document line width is outside 1..1000 characters");
  }
  if (!characters.has_value()) {
    character_line_width_.reset();
    setLineWrapMode(QTextEdit::WidgetWidth);
    return;
  }
  const qreal pixels = 2.0 * document()->documentMargin() +
                       static_cast<qreal>(*characters) * indent_unit();
  if (!std::isfinite(pixels) || pixels > std::numeric_limits<int>::max()) {
    throw std::overflow_error("Document line width exceeds Qt limits");
  }
  character_line_width_ = characters;
  setLineWrapMode(QTextEdit::FixedPixelWidth);
  setLineWrapColumnOrWidth(std::max(1, static_cast<int>(std::ceil(pixels))));
}

std::optional<int> JwpEditor::configured_character_line_width() const noexcept {
  return character_line_width_;
}

int JwpEditor::character_page_width() const {
  if (character_line_width_.has_value()) return *character_line_width_;
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

void JwpEditor::apply_jwp_fonts(const core::JwpDocument& source, core::LegacyCodePage code_page) {
  preserve_document_state(document(), [&] { apply_jwp_character_fonts(*document(), source, code_page); });
}

void JwpEditor::apply_margin_relaxation(const core::JwpDocument& source,
                                        core::LegacyCodePage code_page,
                                        bool punctuation,
                                        bool small_kana) {
  if (source.paragraphs.size() !=
      static_cast<std::size_t>(document()->blockCount())) {
    throw std::invalid_argument(
        "JWP relaxation paragraph count does not match the editor document");
  }

  constexpr qreal kMetricScale = 64.0;
  const auto fixed_metric = [](qreal value) {
    if (!std::isfinite(value) || value <= 0.0 || value > 1'000'000.0) {
      throw std::length_error("JWP relaxation metric is out of range");
    }
    return std::max<std::int64_t>(
        1, static_cast<std::int64_t>(std::llround(value * kMetricScale)));
  };
  const std::int64_t cell = fixed_metric(indent_unit());
  const qreal dynamic_width = std::max<qreal>(
      1.0, viewport()->width() - 2.0 * document()->documentMargin());
  const std::int64_t base_width = character_line_width_.has_value()
      ? static_cast<std::int64_t>(*character_line_width_) * cell
      : fixed_metric(dynamic_width);

  struct PlannedCharacter {
    int position;
    int length;
  };
  std::vector<PlannedCharacter> planned;
  QTextBlock block = document()->begin();
  for (std::size_t paragraph_index = 0;
       paragraph_index < source.paragraphs.size();
       ++paragraph_index, block = block.next()) {
    const core::JwpParagraph& paragraph = source.paragraphs[paragraph_index];
    const std::u32string decoded =
        core::decode_jwp_text(paragraph.text, code_page);
    if (block.text() != to_qstring(decoded)) {
      throw std::invalid_argument(
          "JWP relaxation text does not match the editor document");
    }

    std::vector<std::int64_t> advances;
    advances.reserve(paragraph.text.size());
    for (std::size_t index = 0; index < paragraph.text.size(); ++index) {
      if (paragraph.text[index] == static_cast<core::JisCode>('\t')) {
        advances.push_back(1);
      } else if (paragraph.text[index] > 0xff) {
        advances.push_back(cell);
      } else {
        advances.push_back(fixed_metric(QFontMetricsF(jwp_representation_font(
            document()->defaultFont(), 1))
                                                    .horizontalAdvance(to_qstring(
                                                        std::u32string_view(decoded)
                                                            .substr(index, 1)))));
      }
    }

    core::LineRelaxationOptions options;
    const std::int64_t left =
        static_cast<std::int64_t>(paragraph.left_indent) * cell;
    const std::int64_t right =
        static_cast<std::int64_t>(paragraph.right_indent) * cell;
    const std::int64_t first =
        static_cast<std::int64_t>(paragraph.first_indent) * cell;
    options.first_line_width =
        std::max<std::int64_t>(1, base_width - left - right - first);
    options.continuation_line_width =
        std::max<std::int64_t>(1, base_width - left - right);
    options.jis_advance = cell;
    options.punctuation = punctuation;
    options.small_kana = small_kana;
    const auto indices =
        core::plan_line_relaxation(paragraph.text, advances, options);

    int utf16_offset = 0;
    std::size_t token = 0;
    for (const std::size_t index : indices) {
      while (token < index) {
        utf16_offset += decoded[token] <= 0xffffU ? 1 : 2;
        ++token;
      }
      if (index == 0) continue;
      const int previous_length = decoded[index - 1] <= 0xffffU ? 1 : 2;
      const int current_length = decoded[index] <= 0xffffU ? 1 : 2;
      planned.push_back({block.position() + utf16_offset - previous_length,
                         previous_length + current_length});
    }
  }

  preserve_document_state(document(), [&] {
    struct ExistingFormat {
      int position;
      int length;
      QTextCharFormat format;
    };
    std::vector<ExistingFormat> existing;
    for (QTextBlock current = document()->begin(); current.isValid();
         current = current.next()) {
      for (auto fragment = current.begin(); !fragment.atEnd(); ++fragment) {
        const auto part = fragment.fragment();
        if (!part.charFormat().hasProperty(kRelaxedMarginCharacter)) continue;
        existing.push_back({part.position(), part.length(), part.charFormat()});
      }
    }
    for (ExistingFormat& item : existing) {
      QTextCursor cursor(document());
      cursor.setPosition(item.position);
      cursor.setPosition(item.position + item.length, QTextCursor::KeepAnchor);
      QTextCharFormat& format = item.format;
      const bool relaxed = format.hasProperty(kRelaxedMarginCharacter);
      format.clearProperty(kRelaxedMarginCharacter);
      if (relaxed) format.clearProperty(QTextFormat::FontStretch);
      format.clearProperty(QTextFormat::FontLetterSpacing);
      format.clearProperty(QTextFormat::FontLetterSpacingType);
      if (relaxed) format.clearProperty(QTextFormat::ForegroundBrush);
      cursor.setCharFormat(format);
    }
    for (const PlannedCharacter& character : planned) {
      QTextCursor cursor(document());
      cursor.setPosition(character.position);
      cursor.setPosition(character.position + character.length,
                         QTextCursor::KeepAnchor);
      QTextCharFormat format;
      format.setProperty(kRelaxedMarginCharacter, character.position);
      format.setFontStretch(1);
      format.setFontLetterSpacingType(QFont::AbsoluteSpacing);
      format.setFontLetterSpacing(-1.0);
      format.setForeground(Qt::transparent);
      cursor.mergeCharFormat(format);
    }
  });
  viewport()->update();
}

void JwpEditor::clear_jwp_layout() {
  preserve_document_state(document(), [this] {
    for (QTextBlock block = document()->begin(); block.isValid(); block = block.next()) {
      QList<QTextCursor> marked;
      for (auto it = block.begin(); !it.atEnd(); ++it) {
        const auto part = it.fragment();
        if (!part.charFormat().hasProperty(kJwpCharacterKind) &&
            !part.charFormat().hasProperty(kRelaxedMarginCharacter))
          continue;
        QTextCursor cursor(document()); cursor.setPosition(part.position());
        cursor.setPosition(part.position() + part.length(), QTextCursor::KeepAnchor); marked.push_back(cursor);
      }
      for (auto cursor : marked) {
        auto format = cursor.charFormat();
        format.clearProperty(kJwpCharacterKind);
        if (format.hasProperty(kRelaxedMarginCharacter)) {
          format.clearProperty(kRelaxedMarginCharacter);
          format.clearProperty(QTextFormat::FontStretch);
          format.clearProperty(QTextFormat::FontLetterSpacing);
          format.clearProperty(QTextFormat::FontLetterSpacingType);
          format.clearProperty(QTextFormat::ForegroundBrush);
        }
        format.clearProperty(QTextFormat::FontFamilies); cursor.setCharFormat(format);
      }
    }
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
  auto colors = prepare_kanji_colors(jwp_document, color_list, policy, code_page);
  kanji_list_coloring_ = policy.list_mode != core::KanjiListColorMode::kOff;
  color_code_page_ = code_page;
  set_kanji_color_selections(std::move(colors));
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
  kanji_list_coloring_ = false;
  color_code_page_ = core::kDefaultLegacyCodePage;
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
  painter.setClipRect(event->rect());
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
    if (geometry.bottom() < event->rect().top()) {
      continue;
    }
    for (auto fragment = block.begin(); !fragment.atEnd(); ++fragment) {
      const auto part = fragment.fragment();
      if (!part.charFormat().hasProperty(kRelaxedMarginCharacter)) continue;
      const int group_start =
          part.charFormat().intProperty(kRelaxedMarginCharacter);
      const auto original_font = [&](int position) {
        QTextCursor cursor(document());
        cursor.setPosition(position);
        cursor.movePosition(QTextCursor::NextCharacter,
                            QTextCursor::KeepAnchor);
        QFont font = cursor.charFormat().font().resolve(document()->defaultFont());
        font.setStretch(QFont::Unstretched);
        font.setLetterSpacing(QFont::AbsoluteSpacing, 0.0);
        return font;
      };
      QTextCursor group_cursor(document());
      group_cursor.setPosition(group_start);
      qreal draw_x = cursorRect(group_cursor).left();
      for (int prior = group_start; prior < part.position();) {
        const int length = prior + 1 < document()->characterCount() &&
                                   document()->characterAt(prior).isHighSurrogate() &&
                                   document()->characterAt(prior + 1).isLowSurrogate()
                               ? 2
                               : 1;
        QString character(document()->characterAt(prior));
        if (length == 2) character.append(document()->characterAt(prior + 1));
        draw_x += QFontMetricsF(original_font(prior)).horizontalAdvance(character);
        prior += length;
      }
      const QTextCursor selection = textCursor();
      int position = part.position();
      const QString text = part.text();
      for (qsizetype index = 0; index < text.size();) {
        const int length = text[index].isHighSurrogate() &&
                                   index + 1 < text.size() &&
                                   text[index + 1].isLowSurrogate()
                               ? 2
                               : 1;
        const QFont font = original_font(position);
        const QFontMetricsF metrics(font);
        painter.setFont(font);
        QTextCursor cursor(document());
        cursor.setPosition(group_start);
        const QRect cursor_rect = cursorRect(cursor);
        const bool selected = selection.hasSelection() &&
            selection.selectionStart() <= position &&
            position < selection.selectionEnd();
        const QColor paper = palette().color(
            selected ? QPalette::Highlight : QPalette::Base);
        const QColor ink = palette().color(
            selected ? QPalette::HighlightedText : QPalette::Text);
        const QString character = text.mid(index, length);
        const qreal width = metrics.horizontalAdvance(character);
        painter.fillRect(QRectF(draw_x, cursor_rect.top(), width,
                               metrics.height()), paper);
        painter.setPen(ink);
        painter.drawText(QPointF(draw_x, cursor_rect.top() + metrics.ascent()),
                         character);
        draw_x += width;
        position += length;
        index += length;
      }
    }
    if (!block.blockFormat().property(kPageBreakProperty).toBool()) continue;
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
