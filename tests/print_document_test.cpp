// SPDX-License-Identifier: GPL-2.0-or-later

#include <cmath>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <string_view>

#include <QApplication>
#include <QFile>
#include <QPageLayout>
#include <QPageRanges>
#include <QPrinter>
#include <QTemporaryDir>
#include <QTextDocument>
#include <QTextCursor>
#include <QPainter>
#include <QFontMetricsF>
#include <QProcess>
#include <QStandardPaths>
#include <QDir>
#include <QAction>
#include <QTimer>
#include <QPointer>
#include <QTextEdit>
#include <QPrintPreviewWidget>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include "main_window.h"
#include "application_settings_dialog.h"
#include "file_io.h"
#include "text_bridge.h"
#include "jwpqt/core/jwp_text_codec.h"

#include "print_document.h"

namespace {

void require(bool condition, std::string_view message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

void require_error(const std::function<void()>& operation,
                   std::string_view message) {
  try {
    operation();
  } catch (const jwpqt::qt::PrintDocumentError&) {
    return;
  }
  require(false, message);
}

void test_pdf_print(const QString& directory) {
  const QString path = directory + QStringLiteral("/document.pdf");
  QPrinter printer(QPrinter::HighResolution);
  printer.setOutputFormat(QPrinter::PdfFormat);
  printer.setOutputFileName(path);

  jwpqt::core::JwpDocument layout;
  layout.margins = {0.5F, 0.75F, 1.0F, 1.25F};
  layout.landscape = true;
  QTextDocument source;
  source.setPlainText(QStringLiteral("Native print \u65e5\u672c\nSecond line"));
  source.setModified(true);

  jwpqt::qt::print_document(printer, source, &layout);
  require(source.isModified(), "Printing changed the source document state");
  require(printer.pageLayout().orientation() == QPageLayout::Landscape,
          "Printing did not apply JWP landscape orientation");
  const QMarginsF margins =
      printer.pageLayout().margins(QPageLayout::Inch);
  require(std::abs(margins.left() - 0.5) < 0.01 &&
              std::abs(margins.right() - 0.75) < 0.01 &&
              std::abs(margins.top() - 1.0) < 0.01 &&
              std::abs(margins.bottom() - 1.25) < 0.01,
          "Printing applied JWP margins in the wrong order");
  QFile output(path);
  require(output.open(QIODevice::ReadOnly), "PDF print output was not created");
  require(output.size() > 500 && output.read(4) == QByteArray("%PDF", 4),
          "PDF print output is incomplete");
}

void test_invalid_layout() {
  QPrinter printer;
  jwpqt::core::JwpDocument layout;
  layout.margins[0] = std::numeric_limits<float>::quiet_NaN();
  QTextDocument source;
  require_error(
      [&] { jwpqt::qt::print_document(printer, source, &layout); },
      "Printing accepted an invalid JWP margin");
  QPageLayout page(QPageSize(QPageSize::A4), QPageLayout::Portrait, {72, 72, 72, 72}, QPageLayout::Point);
  source.setPlainText(QString::fromUcs4(U"A\U0001f600B"));
  jwpqt::qt::PrintOptions options;
  options.selection = {{1, 2}};
  require_error([&] { jwpqt::qt::PrintLayout invalid(source, page, nullptr, options); },
                "Printing accepted a split Unicode scalar");
}

QByteArray bytes(const QString& path) {
  QFile file(path); require(file.open(QIODevice::ReadOnly), "Could not read PDF fixture"); return file.readAll();
}
QString pdf_text(const QString& path) {
  require(!QStandardPaths::findExecutable(QStringLiteral("pdftotext")).isEmpty(),
          "PDF verification requires pdftotext (poppler-utils)");
  QProcess process;
  process.start(QStringLiteral("pdftotext"), {QStringLiteral("-layout"), path, QStringLiteral("-")});
  require(process.waitForFinished(30000) && process.exitCode() == 0, "PDF text extraction failed");
  const QString result = QString::fromUtf8(process.readAllStandardOutput());
  require(!result.trimmed().isEmpty(), "A nonempty print fixture produced no PDF text");
  return result;
}

void test_layout_and_ranges(const QString& directory) {
  using namespace jwpqt;
  QTextDocument source;
  source.setDefaultFont(QFont(QStringLiteral("Noto Sans CJK JP"), 12));
  QTextCursor cursor(&source);
  cursor.insertText(QStringLiteral("FIRST \u65e5\u672c \u00a0\U0001f600"));
  QTextBlockFormat new_page; new_page.setPageBreakPolicy(QTextFormat::PageBreak_AlwaysBefore);
  cursor.insertBlock(new_page); cursor.insertText(QStringLiteral("SECOND"));
  cursor.insertBlock(new_page); cursor.insertText(QStringLiteral("THIRD"));
  source.setModified(true);
  const auto original = source.toRawText();
  const auto undo = source.availableUndoSteps();
  core::JwpDocument metadata;
  metadata.margins = {1, 1, 1, 1};
  auto raw = [](std::u32string_view value) { return core::encode_jwp_text(value, core::LegacyCodePage::k1252); };
  metadata.summary[2] = raw(U"AUTHOR");
  metadata.headers[0][0] = raw(U"ODD &P &A &N && &z");
  metadata.headers[1][0] = raw(U"EVEN &P");
  metadata.headers[2][2] = raw(U"FOOT &P &D &T");
  metadata.headers[3][2] = raw(U"EVENFOOT &P");
  metadata.separate_left_right_headers = true;
  metadata.suppress_first_page_headers = true;
  qt::PrintOptions options;
  options.font = source.defaultFont(); options.file_name = QStringLiteral("/documents/name.jwp");
  options.time = QDateTime(QDate(2024, 2, 3), QTime(14, 5));
  QPrinter printer(QPrinter::HighResolution);
  printer.setOutputFormat(QPrinter::PdfFormat);
  const QString path = directory + QStringLiteral("/pages.pdf");
  printer.setOutputFileName(path);
  qt::configure_printer_for_jwp(printer, metadata);
  qt::PrintLayout plan(source, printer.pageLayout(), &metadata, options);
  require(plan.page_count() == 3 && plan.text().startsWith(QStringLiteral("FIRST")) &&
              plan.text().contains(QChar(0xa0)), "Print pagination or Unicode text changed");
  QImage image(plan.page_size().toSize() * 2, QImage::Format_ARGB32); image.fill(Qt::white);
  { QPainter painter(&image); painter.scale(2, 2); plan.paint_page(painter, 3); }
  require(image.save(QDir::currentPath() + QStringLiteral("/print-horizontal.png")), "Print capture failed");
  qt::print_document(printer, source, &metadata, options);
  const QString complete = pdf_text(path);
  if (!complete.isEmpty()) {
    require(complete.contains(QStringLiteral("FIRST")) && complete.contains(QStringLiteral("SECOND")) &&
                complete.contains(QStringLiteral("THIRD")), "PDF omitted document pages");
    require(!complete.contains(QStringLiteral("ODD 1")) && complete.contains(QStringLiteral("EVEN 2")) &&
                complete.contains(QStringLiteral("ODD 3 AUTHOR name.jwp & &z")) &&
                complete.contains(QStringLiteral("FOOT 3 2024/02/03 14:05")), "PDF header/footer expansion or parity is wrong");
  }
  printer.setPrintRange(QPrinter::PageRange); printer.setFromTo(2, 3);
  printer.setPageOrder(QPrinter::LastPageFirst);
  qt::print_document(printer, source, &metadata, options);
  const auto reverse = pdf_text(path);
  if (!reverse.isEmpty()) require(!reverse.contains(QStringLiteral("FIRST")) && reverse.indexOf(QStringLiteral("THIRD")) <
      reverse.indexOf(QStringLiteral("SECOND")), "PDF page range or reverse order failed");
  const auto saved = bytes(path);
  QPageRanges disjoint; disjoint.addPage(1); disjoint.addPage(3);
  printer.setPageRanges(disjoint);
  qt::print_document(printer, source, &metadata, options);
  const auto sparse = pdf_text(path);
  require(sparse.contains(QStringLiteral("FIRST")) && sparse.contains(QStringLiteral("THIRD")) &&
      !sparse.contains(QStringLiteral("SECOND")), "Disjoint page ranges included an unrequested page");
  printer.setPrintRange(QPrinter::AllPages); printer.setCopyCount(2); printer.setCollateCopies(true);
  qt::print_document(printer, source, &metadata, options);
  auto copies = pdf_text(path);
  if (!copies.isEmpty()) require(copies.count(QStringLiteral("FIRST")) == 2 && copies.count(QStringLiteral("THIRD")) == 2,
                                 std::string("PDF copy count was ignored: ") + copies.toStdString());
  require(copies.indexOf(QStringLiteral("FIRST")) < copies.lastIndexOf(QStringLiteral("THIRD")),
          "Collated PDF copies were not complete document sets");
  printer.setCollateCopies(false);
  qt::print_document(printer, source, &metadata, options);
  copies = pdf_text(path);
  require(copies.lastIndexOf(QStringLiteral("THIRD")) < copies.indexOf(QStringLiteral("SECOND")) &&
      copies.lastIndexOf(QStringLiteral("SECOND")) < copies.indexOf(QStringLiteral("FIRST")),
      "Uncollated PDF copies did not repeat each page in the requested order");
  printer.setCopyCount(1); printer.setPrintRange(QPrinter::PageRange);
  // Re-establish the prior output before exercising non-destructive failures.
  QFile reset(path); require(reset.open(QIODevice::WriteOnly) && reset.write(saved) == saved.size(), "Could not reset PDF fixture"); reset.close();
  printer.setFromTo(9, 10);
  require_error([&] { qt::print_document(printer, source, &metadata, options); }, "Out-of-range printing succeeded");
  require(bytes(path) == saved, "Invalid range destroyed the previous PDF");
  printer.setPrintRange(QPrinter::AllPages); printer.setPageOrder(QPrinter::FirstPageFirst);
  options.progress = [](int page, int) { return page == 0; };
  require_error([&] { qt::print_document(printer, source, &metadata, options); }, "Print cancellation was ignored");
  require(bytes(path) == saved && printer.outputFileName() == path, "Cancelled printing replaced output or printer target");
  options.progress = [](int page, int count) { return page < count; };
  require_error([&] { qt::print_document(printer, source, &metadata, options); }, "Final cancellation was ignored");
  require(bytes(path) == saved, "Final cancellation published a completed but unwanted PDF");
  options.progress = {};
  auto fields = metadata; fields.headers = {}; fields.suppress_first_page_headers = false;
  fields.summary = {raw(U"L"), raw(U"S"), raw(U"A"), raw(U"K"), raw(U"C")};
  fields.headers[0][0] = raw(U"&l &s &k &c &f &n &a && &x &");
  auto field_options = options; field_options.file_name = QStringLiteral("/d/n");
  qt::print_document(printer, source, &fields, field_options);
  require(pdf_text(path).contains(QStringLiteral("L S K C /d/n n A & &x &")),
          "Header fields, lowercase aliases or literal ampersands changed");
  require(reset.open(QIODevice::WriteOnly) && reset.write(saved) == saved.size(), "Could not restore header fixture");
  reset.close();
  metadata.headers[0][0] = raw(U"This header cannot fit"); metadata.margins[2] = 0;
  metadata.suppress_first_page_headers = false;
  require_error([&] { qt::print_document(printer, source, &metadata, options); }, "Invalid header geometry was accepted");
  require(bytes(path) == saved, "Invalid header destroyed the previous PDF");
  require(source.toRawText() == original && source.isModified() && source.availableUndoSteps() == undo,
          "Printing mutated source text, modification or undo state");
}

void test_selection_and_vertical(const QString& directory) {
  using namespace jwpqt;
  QTextDocument source;
  source.setDefaultFont(QFont(QStringLiteral("Noto Sans CJK JP"), 12));
  source.setPlainText(QStringLiteral("BEFORE\nSELECTED \u65e5\u672c\u8a9e ABC office \u30fc\uff08\uff09\nAFTER"));
  QPrinter printer(QPrinter::HighResolution); printer.setOutputFormat(QPrinter::PdfFormat);
  const QString path = directory + QStringLiteral("/selection.pdf"); printer.setOutputFileName(path);
  core::JwpDocument metadata; metadata.margins = {1, 1, 1, 1}; metadata.vertical = true;
  qt::PrintOptions options; options.font = source.defaultFont(); options.selection = {{7, source.toPlainText().indexOf(QStringLiteral("\nAFTER"))}};
  printer.setPrintRange(QPrinter::Selection);
  qt::print_document(printer, source, &metadata, options);
  const auto text = pdf_text(path);
  if (!text.isEmpty()) require(text.contains(QStringLiteral("SELECTED")) &&
      text.normalized(QString::NormalizationForm_KC).contains(QStringLiteral("office")) &&
      !text.contains(QStringLiteral("BEFORE")) && !text.contains(QStringLiteral("AFTER")),
      std::string("Vertical selection leaked content or duplicated Latin glyphs: ") + text.toStdString());
  qt::PrintLayout vertical(source, printer.pageLayout(), &metadata, options);
  require(vertical.text().startsWith(QStringLiteral("SELECTED")) && !vertical.text().contains(QStringLiteral("AFTER")),
          "Selection layout does not own exactly the selected text");
  QImage image(vertical.page_size().toSize() * 2, QImage::Format_ARGB32); image.fill(Qt::white);
  { QPainter painter(&image); painter.scale(2, 2); vertical.paint_page(painter, 1); }
  require(image.save(QDir::currentPath() + QStringLiteral("/print-vertical.png")), "Vertical capture failed");
  metadata.vertical = false;
  qt::PrintLayout horizontal(source, printer.pageLayout(), &metadata, options);
  QImage unrotated(image.size(), image.format()); unrotated.fill(Qt::white);
  { QPainter painter(&unrotated); painter.scale(2, 2); horizontal.paint_page(painter, 1); }
  require(image != unrotated, "Vertical output did not rotate any Japanese glyphs");
  source.setPlainText(QStringLiteral("CHANGED"));
  require(vertical.text().startsWith(QStringLiteral("SELECTED")), "Print plan retained live document state");

  QTextDocument hanging;
  hanging.setDefaultFont(options.font);
  QTextCursor cursor(&hanging);
  auto format = cursor.blockFormat();
  format.setTextIndent(-QFontMetricsF(options.font).horizontalAdvance(QStringLiteral("\u3000")));
  cursor.setBlockFormat(format); cursor.insertText(QStringLiteral("HANGING"));
  options.selection.reset();
  qt::PrintLayout indented(hanging, printer.pageLayout(), nullptr, options);
  QImage indent_image(indented.page_size().toSize(), QImage::Format_ARGB32); indent_image.fill(Qt::white);
  { QPainter painter(&indent_image); indented.paint_page(painter, 1); }
  const auto body = printer.pageLayout().paintRect(QPageLayout::Point);
  bool hanging_ink = false;
  for (int y = body.top(); y < body.top() + 40; ++y)
    for (int x = body.left() - 30; x < body.left(); ++x)
      hanging_ink |= indent_image.pixelColor(x, y) != QColor(Qt::white);
  require(hanging_ink, "Printing clipped a hanging indent at the nominal text margin");
}

class PrintWindow final : public jwpqt::qt::MainWindow {
 public:
  std::function<bool(QPrinter&)> prompt;
  std::function<bool(QPrinter&)> setup;
  protected:
  bool prompt_for_print(QPrinter& printer) override { return prompt(printer); }
  bool prompt_for_printer_setup(QPrinter& printer) override { return setup(printer); }
};

void test_window_workflow(const QString& directory) {
  using namespace jwpqt;
  const QString source_path = directory + QStringLiteral("/snapshot.txt");
  qt::write_text_file(source_path, {U"ORIGINAL CONTENT", core::TextEncoding::kUtf8, false});
  PrintWindow window;
  require(window.open_path(source_path, core::TextEncoding::kUtf8), "Could not load print snapshot");
  auto* editor = window.findChild<QTextEdit*>();
  auto* action = window.findChild<QAction*>(QStringLiteral("printAction"));
  require(editor && action, "Print command unavailable");
  QTextCursor selected(editor->document()); selected.setPosition(9); selected.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
  editor->setTextCursor(selected);
  const QString path = directory + QStringLiteral("/snapshot.pdf");
  window.prompt = [&](QPrinter& printer) {
    printer.setOutputFormat(QPrinter::PdfFormat); printer.setOutputFileName(path); printer.setPrintRange(QPrinter::Selection);
    editor->selectAll(); editor->insertPlainText(QStringLiteral("EXTERNAL CHANGE"));
    action->trigger(); // A print prompt cannot recursively start a second job.
    return true;
  };
  action->trigger();
  const auto text = pdf_text(path);
  if (!text.isEmpty()) require(text.contains(QStringLiteral("CONTENT")) && !text.contains(QStringLiteral("ORIGINAL")) &&
      !text.contains(QStringLiteral("EXTERNAL")), "Modal printing did not use its frozen selection");
  require(editor->toPlainText() == QStringLiteral("EXTERNAL CHANGE"), "Printing reverted a newer editor change");
  window.prompt = [&](QPrinter& printer) { printer.setOutputFileName(source_path); return true; };
  const auto source_bytes = bytes(source_path);
  action->trigger(); require(bytes(source_path) == source_bytes, "Printing overwrote an open document");

  const QString late_path = directory + QStringLiteral("/opened-during-print.txt");
  qt::write_text_file(late_path, {U"KEEP THIS FILE", core::TextEncoding::kUtf8, false});
  const auto late_bytes = bytes(late_path);
  window.prompt = [&](QPrinter& printer) {
    printer.setOutputFormat(QPrinter::PdfFormat); printer.setOutputFileName(late_path);
    printer.setPrintRange(QPrinter::AllPages);
    QTimer::singleShot(0, &window, [&] {
      require(window.open_path(late_path, core::TextEncoding::kUtf8), "Could not open concurrent print target");
    });
    return true;
  };
  action->trigger();
  require(bytes(late_path) == late_bytes, "Printing overwrote a destination opened during progress");
  require(window.open_path(source_path, core::TextEncoding::kUtf8), "Could not restore preview source");
  editor = window.findChild<QTextEdit*>();
  editor->selectAll(); editor->insertPlainText(QStringLiteral("EXTERNAL CHANGE"));

  QTimer timer;
  bool preview_seen = false;
  QObject::connect(&timer, &QTimer::timeout, [&] {
    auto* preview = window.findChild<QDialog*>(QStringLiteral("printPreviewDialog"));
    if (!preview) return;
    require(preview->findChild<QPrintPreviewWidget*>()->pageCount() > 0 &&
        preview->findChild<QPushButton*>(QStringLiteral("printFromPreview"))->isEnabled(), "Preview rendering failed");
    preview_seen = true; timer.stop();
    preview->grab().save(QDir::currentPath() + QStringLiteral("/print-preview.png"));
    preview->reject();
  });
  timer.start(100);
  window.findChild<QAction*>(QStringLiteral("printPreviewAction"))->trigger();
  require(preview_seen && editor->toPlainText() == QStringLiteral("EXTERNAL CHANGE"), "Preview failed or changed the editor");
  require(bytes(source_path) == source_bytes && bytes(late_path) == late_bytes,
          "Preview wrote to a remembered printer destination");

  const QString preview_output = directory + QStringLiteral("/from-preview.pdf");
  window.prompt = [&](QPrinter& printer) {
    printer.setOutputFormat(QPrinter::PdfFormat); printer.setOutputFileName(preview_output);
    printer.setPrintRange(QPrinter::AllPages); return true;
  };
  QObject::disconnect(&timer, nullptr, nullptr, nullptr);
  QObject::connect(&timer, &QTimer::timeout, [&] {
    auto* preview = window.findChild<QDialog*>(QStringLiteral("printPreviewDialog"));
    if (!preview) return;
    timer.stop();
    editor->selectAll(); editor->insertPlainText(QStringLiteral("CHANGED WHILE PREVIEWING"));
    preview->findChild<QPushButton*>(QStringLiteral("printFromPreview"))->click();
  });
  timer.start(100);
  window.findChild<QAction*>(QStringLiteral("printPreviewAction"))->trigger();
  const auto preview_text = pdf_text(preview_output);
  require(preview_text.contains(QStringLiteral("EXTERNAL CHANGE")) &&
      !preview_text.contains(QStringLiteral("CHANGED WHILE")) &&
      editor->toPlainText() == QStringLiteral("CHANGED WHILE PREVIEWING"),
      "Print from preview discarded its snapshot or reverted a newer edit");

  auto* doomed = new PrintWindow;
  QPointer<PrintWindow> weak(doomed);
  doomed->prompt = [doomed](QPrinter&) { delete doomed; return true; };
  doomed->findChild<QAction*>(QStringLiteral("printAction"))->trigger();
  require(!weak, "Print owner deletion test failed");

  auto settings = qt::read_application_settings("print_font.size=95\nprint_font.automatic=false\nprint_font.name=\"Noto Sans CJK JP\"\n");
  require(settings.print_font.size == 95 && !settings.print_font.automatic &&
      qt::read_application_settings(qt::write_application_settings(settings)).print_font.size == 95,
      "Print font did not preserve tenths of a point");
  require(window.apply_application_settings(settings), "Print settings could not be applied");
  qt::ApplicationSettingsDialog options(settings);
  auto* size = options.findChild<QDoubleSpinBox*>(QStringLiteral("settingsPrintSize"));
  auto* automatic = options.findChild<QCheckBox*>(QStringLiteral("settingsPrintAutomatic"));
  require(size && automatic && size->value() == 9.5, "Printing settings controls missing");
  size->setValue(14.2); automatic->setChecked(true);
  QMetaObject::invokeMethod(options.findChild<QDialogButtonBox*>(), "accepted", Qt::DirectConnection);
  require(options.settings().print_font.size == 142 && options.settings().print_font.automatic,
          "Printing settings acceptance lost physical units");
  bool rejected = false;
  try { (void)qt::read_application_settings("Print.Size=bad\nprint_font.size=120\n"); } catch (const std::exception&) { rejected = true; }
  require(rejected, "A later print size hid an invalid earlier setting");

  const auto before_setup = *window.current_jwp_document();
  window.setup = [&](QPrinter& printer) {
    auto layout = printer.pageLayout(); layout.setUnits(QPageLayout::Inch);
    layout.setOrientation(QPageLayout::Landscape);
    require(layout.setMargins({0.5, 0.75, 1.25, 1.5}) && printer.setPageLayout(layout), "Invalid setup fixture");
    return true;
  };
  window.findChild<QAction*>(QStringLiteral("printerSetupAction"))->trigger();
  const auto configured = *window.current_jwp_document();
  require(configured.landscape && configured.margins == std::array<float, 4>{0.5, 1.25, 0.75, 1.5},
          "Printer Setup lost accepted document margins or orientation");
  window.findChild<QAction*>(QStringLiteral("undoAction"))->trigger();
  require(*window.current_jwp_document() == before_setup, "Printer Setup was not one undoable metadata change");
  window.setup = [&](QPrinter&) {
    require(window.open_path(late_path, core::TextEncoding::kUtf8), "Could not change setup target");
    return true;
  };
  window.findChild<QAction*>(QStringLiteral("printerSetupAction"))->trigger();
  require(window.current_jwp_document()->margins == std::array<float, 4>{1, 1, 1, 1} &&
      !window.document_modified(), "Printer Setup applied stale settings to a replacement document");
}

}  // namespace

int main(int argc, char* argv[]) {
  QApplication application(argc, argv);
  QTemporaryDir directory(QStringLiteral("/srv/tmp/jwpqt-print-XXXXXX"));
  require(directory.isValid(), "Could not create print test directory");
  test_pdf_print(directory.path());
  test_invalid_layout();
  test_layout_and_ranges(directory.path());
  test_selection_and_vertical(directory.path());
  test_window_workflow(directory.path());
  return EXIT_SUCCESS;
}
