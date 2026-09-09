// SPDX-License-Identifier: GPL-2.0-or-later
#include "print_document.h"
#include "japanese_fonts.h"
#include "jwpqt/core/jwp_text_codec.h"
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
#include <QTextBoundaryFinder>
#include <QTextCursor>
#include <QTextLayout>

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

  QString header(const core::JwpText& source, int page) const {
    const QString input = to_qstring(core::decode_jwp_text(source, options.code_page));
    if (input.size() > 65535) throw PrintDocumentError("Print header exceeds its limit");
    QString output;
    for (int i = 0; i < input.size(); ++i) {
      if (input[i] != QLatin1Char('&') || i + 1 == input.size()) { output += input[i]; continue; }
      const QChar code = input[++i];
      const auto summary = [&](int n) { return to_qstring(core::decode_jwp_text(jwp->summary[n], options.code_page)); };
      switch (code.toUpper().unicode()) {
        case 'A': output += summary(2); break;
        case 'C': output += summary(4); break;
        case 'K': output += summary(3); break;
        case 'L': output += summary(0); break;
        case 'S': output += summary(1); break;
        case 'D': output += options.time.toString(QStringLiteral("yyyy/MM/dd")); break;
        case 'T': output += options.time.toString(QStringLiteral("HH:mm")); break;
        case 'F': output += options.file_name; break;
        case 'N': output += QFileInfo(options.file_name).fileName(); break;
        case 'P': output += QString::number(page); break;
        case '&': output += QLatin1Char('&'); break;
        default: output += QLatin1Char('&'); output += code;
      }
      if (output.size() > 65535) throw PrintDocumentError("Expanded print header exceeds its limit");
    }
    return output;
  }

  void draw_layout(QPainter& painter, QTextLayout& layout, const QString& text,
                   const QPointF& origin, bool vertical) const {
    // JWP's physical vertical output counter-rotates Japanese glyphs, not the
    // page or Latin runs. The paper is read after a clockwise quarter turn.
    static constexpr core::JisCode no_rotate[] = {
      0x213b,0x213c,0x2141,0x2142,0x2143,0x2144,0x2145,0x214a,0x214b,
      0x214c,0x214d,0x214e,0x214f,0x2150,0x2151,0x2152,0x2153,0x2154,0x2155,
      0x2156,0x2157,0x2158,0x2159,0x215a,0x215b,0x2161,0x2162,0x2163,0x2164,
      0x2165,0x2166,0x2167,0x222a,0x222b,0x222e,0x2127};
    QTextBoundaryFinder boundaries(QTextBoundaryFinder::Grapheme, text);
    const auto rotates = [&](const QString& cluster) {
      const auto scalars = from_qstring(cluster);
      auto jis = scalars.empty() ? std::optional<core::JisCode>{} : core::unicode_to_jwp_code(scalars[0], options.code_page);
      return jis && *jis >= 0x2100 &&
          std::find(std::begin(no_rotate), std::end(no_rotate), *jis) == std::end(no_rotate);
    };
    for (int line_number = 0; line_number < layout.lineCount(); ++line_number) {
      const auto line = layout.lineAt(line_number);
      if (painter.hasClipping()) {
        const qreal middle = line.rect().translated(origin).center().y();
        if (middle < painter.clipBoundingRect().top() || middle >= painter.clipBoundingRect().bottom()) continue;
      }
      if (!vertical) { line.draw(&painter, origin); continue; }
      const int end = line.textStart() + line.textLength();
      for (int at = line.textStart(); at < end;) {
        boundaries.setPosition(at);
        int next = boundaries.toNextBoundary();
        if (next <= at || next > end) next = end;
        const QString cluster = text.mid(at, next - at);
        const bool rotate = rotates(cluster);
        if (rotate) {
          const qreal x = line.cursorToX(at);
          const qreal width = std::abs(line.cursorToX(next) - x);
          const QPointF center = origin + QPointF(x + width / 2, line.y() + line.height() / 2);
          painter.save();
          painter.translate(center);
          painter.rotate(-90);
          painter.translate(-center);
          for (const auto& run : line.glyphRuns(at, next - at)) painter.drawGlyphRun(origin, run);
          painter.restore();
        } else {
          while (next < end) {
            boundaries.setPosition(next);
            const int after = boundaries.toNextBoundary();
            if (after <= next || after > end || rotates(text.mid(next, after - next))) break;
            next = after;
          }
          // Retrieve original shaped glyphs, retaining ligatures/bidi positions.
          for (const auto& run : line.glyphRuns(at, next - at)) painter.drawGlyphRun(origin, run);
        }
        at = next;
      }
    }
  }
};

PrintLayout::PrintLayout(const QTextDocument& source, const QPageLayout& page,
                         const core::JwpDocument* jwp, PrintOptions options) : data_(std::make_unique<Data>()) {
  auto& d = *data_;
  if (!page.isValid() || source.characterCount() > 33554432)
    throw PrintDocumentError("Invalid or oversized print document");
  d.metrics.setDotsPerMeterX(2835); d.metrics.setDotsPerMeterY(2835);
  d.options = std::move(options);
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
      font.setFamilies(d.options.font.families()); font.setPointSizeF(d.options.font.pointSizeF());
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
  d.document->setPageSize(d.body.size());
  d.pages = d.document->pageCount();
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
  for (auto block = d.document->begin(); block.isValid(); block = block.next()) {
    const auto rect = d.document->documentLayout()->blockBoundingRect(block).translated(origin);
    if (rect.intersects(d.body)) d.draw_layout(painter, *block.layout(), block.text(), rect.topLeft(), vertical);
  }
  painter.restore();
  if (!d.jwp || (page == 1 && d.jwp->suppress_first_page_headers)) return;
  const int parity = d.jwp->separate_left_right_headers && page % 2 == 0 ? 1 : 0;
  for (int footer = 0; footer < 2; ++footer) for (int alignment = 0; alignment < 3; ++alignment) {
    const QString text = d.header(d.jwp->headers[parity + footer * 2][alignment], page);
    if (text.isEmpty()) continue;
    QTextLayout header(text, d.options.font, &d.metrics);
    header.beginLayout(); auto line = header.createLine(); line.setLineWidth(1000000); header.endLayout();
    const qreal x = d.body.left() + (d.body.width() - line.naturalTextWidth()) * alignment / 2;
    const qreal y = footer ? d.body.bottom() + line.height() : d.body.top() - 2 * line.height();
    if (x < 0 || x + line.naturalTextWidth() > d.paper.width() || y < 0 || y + line.height() > d.paper.height())
      throw PrintDocumentError("Header or footer does not fit the page margins");
    painter.save(); painter.setPen(Qt::black); d.draw_layout(painter, header, text, {x, y}, vertical); painter.restore();
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
