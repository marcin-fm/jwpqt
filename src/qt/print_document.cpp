// SPDX-License-Identifier: GPL-2.0-or-later
#include "print_document.h"
#include "jwp_text_drawing.h"
#include "japanese_fonts.h"
#include "jwpqt/core/jwp_text_codec.h"
#include "jwpqt/core/jis_unicode.h"
#include "text_bridge.h"
#include <algorithm>
#include <cmath>
#include <iterator>
#include <vector>
#include <QAbstractTextDocumentLayout>
#include <QFileInfo>
#include <QFile>
#include <QSaveFile>
#include <QTemporaryFile>
#include <QScopeGuard>
#include <QFontMetricsF>
#include <QImage>
#include <QPainter>
#include <QPaintEngine>
#include <QPrinter>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextLayout>
#include <QTextBoundaryFinder>
#include <QVector>

namespace jwpqt::qt {
namespace {
void validate_margin(float value) {
  if (!std::isfinite(value) || value < 0 || value > 10)
    throw PrintDocumentError("JWP print margin is outside the supported range");
}
QString raw_text(const QTextDocument& document) {
  return document.toRawText().replace(QChar::ParagraphSeparator, QLatin1Char('\n'));
}
}

void configure_printer_for_jwp(QPrinter& printer, const core::JwpDocument& document) {
  for (float value : document.margins) validate_margin(value);
  auto layout = printer.pageLayout();
  layout.setMode(QPageLayout::StandardMode);
  layout.setOrientation(document.landscape ? QPageLayout::Landscape : QPageLayout::Portrait);
  layout.setUnits(QPageLayout::Inch);
  if (!layout.setMargins({document.margins[0], document.margins[2], document.margins[1], document.margins[3]}) ||
      !printer.setPageLayout(layout))
    throw PrintDocumentError("JWP print margins do not fit the selected page");
}

struct PrintLayout::Data {
  QImage metrics{1, 1, QImage::Format_ARGB32};
  std::unique_ptr<QTextDocument> document;
  std::optional<core::JwpDocument> jwp;
  PrintOptions options;
  QSizeF paper;
  QRectF body;
  int pages = 0;
  struct NativeBlock { QTextBlock block; std::unique_ptr<QTextLayout> layout; };
  std::vector<NativeBlock> native_blocks;

  QVector<qreal> grid(QTextLayout& layout, const QString& text,
                     const std::function<int(int)>& representation, qreal* width = nullptr,
                     int only_line = -1, bool need_offsets = true) const {
    QVector<qreal> offsets(need_offsets ? text.size() : 0);
    const qreal unit = QFontMetricsF(options.font, &metrics).horizontalAdvance(QStringLiteral("\u3000"));
    if (!std::isfinite(unit) || unit <= 0 || unit * 64 > 65536)
      throw PrintDocumentError("Invalid Japanese print cell width");
    const int cell = qRound(unit * 64);
    QTextBoundaryFinder boundaries(QTextBoundaryFinder::Grapheme, text);
    if (width) *width = 0;
    for (int n = 0; n < layout.lineCount(); ++n) {
      if (only_line >= 0 && n != only_line) continue;
      const auto line = layout.lineAt(n);
      const int end = line.textStart() + line.textLength();
      core::JwpText codes;
      std::vector<int> advances, starts;
      for (int at = line.textStart(); at < end;) {
        boundaries.setPosition(at);
        int next = boundaries.toNextBoundary();
        if (next <= at || next > end) next = end;
        if (representation) for (int p = at + 1; p < next; ++p)
          if (representation(p) != representation(at)) { next = p; break; }
        const auto scalar = from_qstring(text.mid(at, next - at)).front();
        const int kind = representation ? representation(at) : 0;
        const auto code = kind == 2 ? core::unicode_to_jis_x0208(scalar)
            : kind == 1 ? std::optional<core::JisCode>{}
                        : core::unicode_to_jwp_code(scalar, options.code_page);
        codes.push_back(code && *code >= 256 ? *code : scalar == '\t' ? '\t' : scalar == ' ' ? ' ' : '?');
        const qreal advance = std::abs(line.cursorToX(next) - line.cursorToX(at)) * 64;
        if (!std::isfinite(advance) || advance > 65536)
          throw PrintDocumentError("Print character advance exceeds its limit");
        advances.push_back(qRound(advance));
        starts.push_back(at); at = next;
      }
      std::vector<int> positions;
      try { positions = core::print_grid_positions(codes, advances, cell,
          options.formatting.justify_ascii, end == text.size()); }
      catch (const std::invalid_argument& error) { throw PrintDocumentError(error.what()); }
      if (need_offsets) for (std::size_t i = 0; i < starts.size(); ++i)
        offsets[starts[i]] = line.x() + positions[i] / 64.0 - line.cursorToX(starts[i]);
      const qreal advance = positions.back() / 64.0;
      if (width) *width = std::max(*width, line.x() + advance);
    }
    return offsets;
  }

  QString header(const core::JwpText& source, int page, QVector<int>* kinds = nullptr) const {
    const QString input = to_qstring(core::decode_jwp_text(source, options.code_page));
    if (input.size() > 65535) throw PrintDocumentError("Print header exceeds its limit");
    QString output;
    const auto append = [&](const QString& text, int kind) {
      output += text;
      if (kinds) for (qsizetype n = 0; n < text.size(); ++n) kinds->push_back(kind);
    };
    const auto append_raw = [&](const core::JwpText& raw, core::LegacyCodePage code_page) {
      const auto decoded = core::decode_jwp_text(raw, code_page);
      for (std::size_t n = 0; n < raw.size(); ++n)
        append(to_qstring(std::u32string(1, decoded[n])), raw[n] < 0x100 ? 1 : 2);
    };
    for (int i = 0; i < input.size(); ++i) {
      if (input[i] != QLatin1Char('&') || i + 1 == input.size()) {
        append(QString(input[i]), source[i] < 0x100 ? 1 : 2); continue;
      }
      const QChar code = input[++i];
      const auto summary = [&](int n) { append_raw(jwp->summary[n], options.code_page); };
      switch (code.toUpper().unicode()) {
        case 'A': summary(2); break;
        case 'C': summary(4); break;
        case 'K': summary(3); break;
        case 'L': summary(0); break;
        case 'S': summary(1); break;
        case 'D': case 'T': {
          const auto date = options.time.date(); const auto time = options.time.time();
          append_raw(core::expand_print_pattern(options.formatting,
              code.toUpper() == QLatin1Char('T'), date.year(), date.month(), date.day(), time.hour(), time.minute()),
              options.format_code_page);
          break;
        }
        case 'F': append(options.file_name, 0); break;
        case 'N': append(QFileInfo(options.file_name).fileName(), 0); break;
        case 'P': append(QString::number(page), 1); break;
        case '&': append(QStringLiteral("&"), 1); break;
        default: append(QStringLiteral("&"), 1); append(QString(code), source[i] < 0x100 ? 1 : 2);
      }
      if (output.size() > 65535) throw PrintDocumentError("Expanded print header exceeds its limit");
    }
    return output;
  }

  void draw_layout(QPainter& painter, QTextLayout& layout, const QString& text,
                   const QPointF& origin, bool vertical,
                   const std::function<int(int)>& representation = {}) const {
    std::function<QColor(int)> foreground;
    if (options.colors && options.color_policy.list_mode != core::KanjiListColorMode::kOff) {
      foreground = [&](int position) {
        const auto unicode = text[position].isHighSurrogate() && position + 1 < text.size()
            ? QChar::surrogateToUcs4(text[position], text[position + 1]) : text[position].unicode();
        const auto jis = core::unicode_to_jis_x0208(unicode);
        const auto color = jis ? core::kanji_foreground_color(*jis, options.color_list.contains(*jis), options.color_policy) : std::nullopt;
        return color ? QColor(color->red, color->green, color->blue) : QColor(Qt::black);
      };
    }
    qreal width = 0;
    const auto offsets = jwp ? grid(layout, text, representation, &width) : QVector<qreal>{};
    if (jwp && origin.x() + width > paper.width())
      throw PrintDocumentError("Print grid line exceeds the paper width; use a smaller font or wider page");
    std::function<qreal(int)> shift;
    if (jwp) shift = [&offsets](int at) { return offsets.at(at); };
    draw_jwp_text_layout(painter, layout, text, origin, vertical, options.code_page, foreground, representation, shift);
  }
};

PrintLayout::PrintLayout(const QTextDocument& source, const QPageLayout& page,
                         const core::JwpDocument* jwp, PrintOptions options) : data_(std::make_unique<Data>()) {
  auto& d = *data_;
  if (!page.isValid() || source.characterCount() > 33554432)
    throw PrintDocumentError("Invalid or oversized print document");
  d.metrics.setDotsPerMeterX(2835); d.metrics.setDotsPerMeterY(2835);
  d.options = std::move(options);
  try { core::validate_print_formatting(d.options.formatting); }
  catch (const std::invalid_argument& error) { throw PrintDocumentError(error.what()); }
  if (!d.options.time.isValid()) throw PrintDocumentError("Invalid print date or time");
  if (jwp) d.jwp = *jwp;
  d.paper = page.fullRect(QPageLayout::Point).size();
  d.body = page.paintRect(QPageLayout::Point);
  if (!std::isfinite(d.paper.width()) || !std::isfinite(d.paper.height()) ||
      d.paper.width() > 14400 || d.paper.height() > 14400 ||
      !std::isfinite(d.body.width()) || !std::isfinite(d.body.height()) || d.body.width() < 36 || d.body.height() < 36)
    throw PrintDocumentError("Print margins leave too little space for text");
  if (d.options.font.pointSizeF() <= 0) d.options.font.setPointSizeF(12);
  if (d.options.font.pointSizeF() < 1 || d.options.font.pointSizeF() > 144)
    throw PrintDocumentError("Print font size is outside the supported range");
  d.options.font = ensure_ascii_font(d.options.font);
  if (jwp) d.options.font.setKerning(false);
  d.document.reset(source.clone());
  if (to_qstring(from_qstring(raw_text(source))) != raw_text(source))
    throw PrintDocumentError("Invalid Unicode in print document");
  d.document->setUndoRedoEnabled(false);
  if (d.options.selection) {
    const auto [begin, end] = *d.options.selection;
    const auto split = [&](int p) { return p > 0 && source.characterAt(p).isLowSurrogate() && source.characterAt(p - 1).isHighSurrogate(); };
    if (begin < 0 || begin >= end || end > source.characterCount() - 1 || split(begin) || split(end))
      throw PrintDocumentError("Invalid print selection");
    QTextCursor cursor(d.document.get());
    cursor.setPosition(end); cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor); cursor.removeSelectedText();
    cursor.setPosition(0); cursor.setPosition(begin, QTextCursor::KeepAnchor); cursor.removeSelectedText();
    auto format = cursor.blockFormat(); format.setPageBreakPolicy(QTextFormat::PageBreak_Auto); cursor.setBlockFormat(format);
  }
  const qreal old_unit = QFontMetricsF(source.defaultFont()).horizontalAdvance(QStringLiteral("\u3000"));
  const qreal new_unit = QFontMetricsF(d.options.font, &d.metrics).horizontalAdvance(QStringLiteral("\u3000"));
  const qreal scale = old_unit > 0 ? new_unit / old_unit : 1;
  d.document->documentLayout()->setPaintDevice(&d.metrics);
  d.document->setDefaultFont(d.options.font);
  struct FormatRange { int begin; int end; QTextCharFormat format; };
  std::vector<FormatRange> formats;
  for (auto block = d.document->begin(); block.isValid(); block = block.next())
    for (auto fragment = block.begin(); !fragment.atEnd(); ++fragment) {
      if (formats.size() == 100000) throw PrintDocumentError("Too many print formatting runs");
      const auto part = fragment.fragment();
      auto format = part.charFormat();
      QFont font = format.font().resolve(d.options.font);
      font.setFamilies(jwp_representation_font(d.options.font, format.intProperty(kJwpCharacterKind)).families());
      font.setPointSizeF(d.options.font.pointSizeF());
      if (jwp) { font.setKerning(false); font.setLetterSpacing(QFont::AbsoluteSpacing, 0); }
      format.setFont(font); format.setForeground(Qt::black); format.setBackground(Qt::NoBrush);
      formats.push_back({part.position(), part.position() + part.length(), format});
    }
  for (const auto& range : formats) {
    QTextCursor cursor(d.document.get()); cursor.setPosition(range.begin); cursor.setPosition(range.end, QTextCursor::KeepAnchor);
    cursor.setCharFormat(range.format);
  }
  for (auto block = d.document->begin(); block.isValid(); block = block.next()) {
    QTextCursor cursor(block); auto format = block.blockFormat();
    format.setLeftMargin(format.leftMargin() * scale); format.setRightMargin(format.rightMargin() * scale);
    format.setTextIndent(format.textIndent() * scale);
    format.setTopMargin(format.topMargin() * scale); format.setBottomMargin(format.bottomMargin() * scale);
    if (format.lineHeightType() == QTextBlockFormat::FixedHeight || format.lineHeightType() == QTextBlockFormat::MinimumHeight ||
        format.lineHeightType() == QTextBlockFormat::LineDistanceHeight)
      format.setLineHeight(format.lineHeight() * scale, format.lineHeightType());
    cursor.setBlockFormat(format);
  }
  d.document->setDocumentMargin(0);
  if (jwp) {
    auto text_options = d.document->defaultTextOption();
    text_options.setTabStopDistance(new_unit);
    d.document->setDefaultTextOption(text_options);
  }
  d.document->setPageSize(d.body.size());
  d.pages = jwp ? 0 : d.document->pageCount();
  if (jwp) {
    qreal y = 0, last_bottom = 0;
    std::size_t work = 0;
    for (auto block = d.document->begin(); block.isValid(); block = block.next()) {
      const auto format = block.blockFormat();
      if ((format.pageBreakPolicy() & QTextFormat::PageBreak_AlwaysBefore) && y > 0)
        y = std::ceil(y / d.body.height()) * d.body.height();
      y += format.topMargin();
      auto layout = std::make_unique<QTextLayout>(block.text(), d.options.font, &d.metrics);
      auto text_option = d.document->defaultTextOption();
      text_option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
      layout->setTextOption(text_option);
      QList<QTextLayout::FormatRange> ranges;
      for (auto fragment = block.begin(); !fragment.atEnd(); ++fragment) {
        const auto part = fragment.fragment();
        ranges.push_back({part.position() - block.position(), part.length(), part.charFormat()});
      }
      layout->setFormats(ranges);
      const auto kind = [block](int at) {
        QTextCursor cursor(block); cursor.setPosition(block.position() + at);
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        return cursor.charFormat().intProperty(kJwpCharacterKind);
      };
      layout->beginLayout();
      for (int n = 0;; ++n) {
        auto line = layout->createLine(); if (!line.isValid()) break;
        const qreal left = format.leftMargin() + (n == 0 ? format.textIndent() : 0);
        const qreal available = d.body.width() - left - format.rightMargin();
        if (!std::isfinite(available) || available < new_unit)
          throw PrintDocumentError("Print paragraph margins leave no Japanese cell");
        const auto fits = [&](qreal width) {
          line.setLineWidth(width);
          work += line.textLength();
          if (work > 100000000) throw PrintDocumentError("Print grid layout work limit exceeded");
          qreal actual = 0; (void)d.grid(*layout, block.text(), kind, &actual, n, false);
          return actual <= available + 0.01;
        };
        if (!fits(available)) {
          qreal low = 0, high = available;
          if (!fits(0)) throw PrintDocumentError("A print character does not fit the paragraph");
          for (int step = 0; step < 20; ++step) {
            const qreal middle = (low + high) / 2;
            if (fits(middle)) low = middle; else high = middle;
          }
          (void)fits(low);
        }
        if (line.height() > d.body.height()) throw PrintDocumentError("Print line exceeds page height");
        if (std::fmod(y, d.body.height()) + line.height() > d.body.height())
          y = std::ceil(y / d.body.height()) * d.body.height();
        line.setPosition({left, y}); last_bottom = y + line.height();
        y += std::max(qreal(1), format.lineHeight(line.height(), 1));
        if (last_bottom > d.body.height() * 10000) throw PrintDocumentError("Print page count exceeds its limit");
      }
      layout->endLayout();
      y += format.bottomMargin();
      if (format.pageBreakPolicy() & QTextFormat::PageBreak_AlwaysAfter)
        y = std::ceil(y / d.body.height()) * d.body.height();
      d.native_blocks.push_back({block, std::move(layout)});
    }
    d.pages = std::max(1, static_cast<int>(std::ceil(last_bottom / d.body.height())));
  }
  if (d.pages < 1 || d.pages > 10000) throw PrintDocumentError("Print page count exceeds its limit");
  if (static_cast<qint64>(d.pages) * d.document->characterCount() > 100000000)
    throw PrintDocumentError("Print layout exceeds its work limit; use a smaller selection");
  // Validate every expansion before a printer or PDF output is opened.
  if (d.jwp) for (const auto& set : d.jwp->headers) for (const auto& text : set) (void)d.header(text, d.pages);
}
PrintLayout::~PrintLayout() = default;
int PrintLayout::page_count() const { return data_->pages; }
QSizeF PrintLayout::page_size() const { return data_->paper; }
QString PrintLayout::text() const { return raw_text(*data_->document); }

void PrintLayout::paint_page(QPainter& painter, int page) const {
  auto& d = *data_;
  if (!painter.isActive() || page < 1 || page > d.pages) throw PrintDocumentError("Invalid print page");
  const bool vertical = d.jwp && d.jwp->vertical;
  painter.save();
  painter.setPen(Qt::black);
  // Preserve hanging indents outside the nominal text margin, but not paper.
  painter.setClipRect(QRectF(0, d.body.top(), d.paper.width(), d.body.height()));
  const QPointF origin(d.body.left(), d.body.top() - (page - 1) * d.body.height());
  if (d.jwp) for (const auto& item : d.native_blocks) {
    d.draw_layout(painter, *item.layout, item.block.text(), origin, vertical, [block = item.block](int at) {
      QTextCursor cursor(block); cursor.setPosition(block.position() + at);
      cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
      return cursor.charFormat().intProperty(kJwpCharacterKind);
    });
  }
  else for (auto block = d.document->begin(); block.isValid(); block = block.next()) {
    const auto rect = d.document->documentLayout()->blockBoundingRect(block).translated(origin);
    if (rect.intersects(d.body)) d.draw_layout(painter, *block.layout(), block.text(), rect.topLeft(), vertical, [block](int at) {
      QTextCursor cursor(block); cursor.setPosition(block.position() + at);
      cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
      return cursor.charFormat().intProperty(kJwpCharacterKind);
    });
  }
  painter.restore();
  if (!d.jwp || (page == 1 && d.jwp->suppress_first_page_headers)) return;
  const int parity = d.jwp->separate_left_right_headers && page % 2 == 0 ? 1 : 0;
  for (int footer = 0; footer < 2; ++footer) for (int alignment = 0; alignment < 3; ++alignment) {
    QVector<int> kinds;
    const QString text = d.header(d.jwp->headers[parity + footer * 2][alignment], page, &kinds);
    if (text.isEmpty()) continue;
    QTextLayout header(text, d.options.font, &d.metrics);
    QList<QTextLayout::FormatRange> formats;
    for (int first = 0; first < kinds.size();) {
      int end = first + 1;
      while (end < kinds.size() && kinds[end] == kinds[first]) ++end;
      QTextCharFormat format; format.setFontFamilies(jwp_representation_font(d.options.font, kinds[first]).families());
      formats.push_back({first, end - first, format}); first = end;
    }
    header.setFormats(formats);
    header.beginLayout(); auto line = header.createLine(); line.setLineWidth(1000000); header.endLayout();
    const auto& position = d.options.formatting.position;
    const qreal unit = QFontMetricsF(d.options.font, &d.metrics).horizontalAdvance(QStringLiteral("\u3000"));
    const qreal left = d.body.left() - unit * position[0] / 100;
    const qreal width = d.body.width() + unit * (position[0] + position[1]) / 100;
    qreal text_width = line.naturalTextWidth();
    (void)d.grid(header, text, [&kinds](int at) { return kinds.at(at); }, &text_width);
    const qreal x = left + (width - text_width) * alignment / 2;
    const qreal y = footer ? d.body.bottom() + line.height() * position[3] / 100
        : d.body.top() - line.height() * (100 + position[2]) / 100;
    if (x < 0 || x + text_width > d.paper.width() || y < 0 || y + line.height() > d.paper.height())
      throw PrintDocumentError("Header or footer does not fit the page margins");
    painter.save(); painter.setPen(Qt::black);
    d.draw_layout(painter, header, text, {x, y}, vertical, [&kinds](int at) { return kinds.at(at); });
    painter.restore();
  }
}

void print_document(QPrinter& printer, const QTextDocument& source,
                    const core::JwpDocument* jwp_document, PrintOptions options) {
  if (jwp_document) configure_printer_for_jwp(printer, *jwp_document);
  if (options.preview && printer.paintEngine()->type() != QPaintEngine::Picture)
    throw PrintDocumentError("Preview requires a preview paint device");
  if (printer.printRange() != QPrinter::Selection) options.selection.reset();
  else if (!options.selection) throw PrintDocumentError("There is no selected text to print");
  PrintLayout layout(source, printer.pageLayout(), jwp_document, options);
  if (printer.printRange() == QPrinter::CurrentPage)
    throw PrintDocumentError("Choose a page range instead of an unspecified current page");
  std::vector<int> pages;
  for (int page = 1; page <= layout.page_count(); ++page) {
    if (printer.printRange() == QPrinter::PageRange) {
      const auto ranges = printer.pageRanges();
      if (!ranges.isEmpty() ? !ranges.contains(page) : (page < printer.fromPage() || page > printer.toPage())) continue;
    }
    pages.push_back(page);
  }
  if (pages.empty()) throw PrintDocumentError("The requested page range is outside the document");
  if (!options.preview && printer.pageOrder() == QPrinter::LastPageFirst) std::reverse(pages.begin(), pages.end());
  const int copies = options.preview || (printer.outputFormat() != QPrinter::PdfFormat && printer.supportsMultipleCopies())
      ? 1 : printer.copyCount();
  if (copies < 1 || copies > 100 || pages.size() * copies > 10000)
    throw PrintDocumentError("Print copies exceed the page limit");
  if (static_cast<qint64>(layout.text().size()) * pages.size() * copies > 100000000)
    throw PrintDocumentError("Print job exceeds its work limit; use fewer copies or pages");
  // Render once to a bounded scratch surface to catch header geometry errors
  // before opening an output destination. Actual output retains vector text.
  QImage scratch(1, 1, QImage::Format_ARGB32); QPainter validation(&scratch);
  for (int page : pages) layout.paint_page(validation, page);
  const bool full_page = printer.fullPage();
  const QString destination = printer.outputFileName();
  const auto restore = qScopeGuard([&] { printer.setFullPage(full_page); printer.setOutputFileName(destination); });
  std::unique_ptr<QTemporaryFile> staged;
  if (!options.preview && printer.outputFormat() == QPrinter::PdfFormat) {
    if (destination.isEmpty()) throw PrintDocumentError("Choose a PDF output file");
    staged = std::make_unique<QTemporaryFile>(QFileInfo(destination).path() + QStringLiteral("/.jwpqt-print-XXXXXX.pdf"));
    if (!staged->open()) throw PrintDocumentError("Could not stage PDF output");
    staged->close(); printer.setOutputFileName(staged->fileName());
  }
  if (options.progress && !options.progress(0, pages.size() * copies)) throw PrintDocumentError("Printing cancelled");
  printer.setFullPage(true);
  QPainter painter;
  if (!painter.begin(&printer)) throw PrintDocumentError("Could not start printer output");
  int emitted = 0;
  try {
    for (int n = 0; n < static_cast<int>(pages.size()) * copies; ++n) {
      if (options.progress && !options.progress(n, pages.size() * copies)) throw PrintDocumentError("Printing cancelled");
      if (printer.printerState() == QPrinter::Aborted) throw PrintDocumentError("Printing cancelled");
      if (emitted++ && !printer.newPage()) throw PrintDocumentError("Could not start the next print page");
      const int page = printer.collateCopies() ? pages[n % pages.size()] : pages[n / copies];
      painter.save(); painter.scale(printer.resolution() / 72.0, printer.resolution() / 72.0);
      layout.paint_page(painter, page); painter.restore();
    }
    if (!painter.end()) throw PrintDocumentError("Could not finish printer output");
    if (printer.printerState() == QPrinter::Error) throw PrintDocumentError("Printer reported an output error");
    if (options.progress && !options.progress(emitted, pages.size() * copies))
      throw PrintDocumentError("Printing cancelled");
  } catch (...) { printer.abort(); if (painter.isActive()) painter.end(); throw; }
  if (staged) {
    QFile input(staged->fileName()); QSaveFile output(destination);
    if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly))
      throw PrintDocumentError("Could not publish PDF output");
    while (!input.atEnd()) {
      const QByteArray bytes = input.read(65536);
      if (input.error() != QFileDevice::NoError || bytes.isEmpty() || output.write(bytes) != bytes.size())
        throw PrintDocumentError("Could not copy completed PDF output");
    }
    if (!output.commit()) throw PrintDocumentError("Could not commit completed PDF output");
  }
}
}  // namespace jwpqt::qt
